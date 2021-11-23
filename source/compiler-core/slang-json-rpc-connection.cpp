// slang-json-rpc-connection.cpp
#include "slang-json-rpc-connection.h"

#include "../core/slang-string-util.h"
#include "../core/slang-process-util.h"

#include "slang-json-rpc.h"
#include "slang-json-native.h"

namespace Slang {

SlangResult JSONRPCConnection::init(HTTPPacketConnection* connection, Process* process)
{
    m_connection = connection;
    m_process = process;

    m_sourceManager.initialize(nullptr, nullptr);
    m_diagnosticSink.init(&m_sourceManager, &JSONLexer::calcLexemeLocation);
    m_container.setSourceManager(&m_sourceManager);

    return SLANG_OK;
}

SlangResult JSONRPCConnection::initWithStdStreams(Process* process)
{
    RefPtr<Stream> stdinStream, stdoutStream;

    Process::getStdStream(Process::StreamType::StdIn, stdinStream);
    Process::getStdStream(Process::StreamType::StdOut, stdoutStream);

    RefPtr<BufferedReadStream> readStream(new BufferedReadStream(stdinStream));

    RefPtr<HTTPPacketConnection> connection = new HTTPPacketConnection(readStream, stdoutStream);
    return init(connection, process);
}

void JSONRPCConnection::clearBuffers()
{
    m_sourceManager.reset();
    m_diagnosticSink.reset();
    m_container.reset();
    m_jsonRoot.reset();
}

bool JSONRPCConnection::isActive()
{
    return m_connection->isActive() && (m_process == nullptr || !m_process->isTerminated());
}

JSONValue JSONRPCConnection::getMessageId()
{
    SLANG_ASSERT(hasMessage());
    return JSONRPCUtil::getId(&m_container, m_jsonRoot);
}

void JSONRPCConnection::disconnect()
{
    if (m_process)
    {
        if (!m_process->isTerminated())
        {
            if (m_connection)
            {
                // Send. If succeeded, wait 
                if (SLANG_SUCCEEDED(sendCall(UnownedStringSlice::fromLiteral("quit"))))
                {
                    // Wait for termination
                    m_process->waitForTermination(m_terminationTimeOutInMs);
                }
            }

            if (!m_process->isTerminated())
            {
                // Okay, just try terminating
                m_process->waitForTermination(m_terminationTimeOutInMs);
            }

            // Okay just kill it then
            if (!m_process->isTerminated())
            {
                m_process->kill(-1);
            }
        }
        m_process.setNull();
    }

    m_connection.setNull();
}

SlangResult JSONRPCConnection::sendRPC(const RttiInfo* rttiInfo, const void* data)
{
    // Convert to JSON
    NativeToJSONConverter converter(&m_container, &m_diagnosticSink);
    JSONValue value;

    SLANG_RETURN_ON_FAIL(converter.convert(rttiInfo, data, value));

    // Convert to text
    JSONWriter writer(JSONWriter::IndentationStyle::Allman);

    m_container.traverseRecursively(value, &writer);
    const StringBuilder& builder = writer.getBuilder();
    return m_connection->write(builder.getBuffer(), builder.getLength());
}

SlangResult JSONRPCConnection::sendError(JSONRPC::ErrorCode code)
{
    return sendError(code, m_diagnosticSink.outputBuffer.getUnownedSlice());
}

SlangResult JSONRPCConnection::sendError(JSONRPC::ErrorCode errorCode, const UnownedStringSlice& msg)
{
    JSONRPCErrorResponse errorResponse;
    errorResponse.error.code = Int(errorCode);
    errorResponse.error.message = msg;

    // TODO(JS):
    // This is only appropriate if the sendError is for the current input message.
    // We might want to add function that the client uses, which take the id as a parameter.

    if (m_jsonRoot.isValid())
    {
        errorResponse.id = JSONRPCUtil::getId(&m_container, m_jsonRoot);
    }
    else
    {
        // If we don't have valid json, we set the id to be null per the spec
        errorResponse.id = JSONValue::makeNull();
    }

    return sendRPC(&errorResponse);
}

SlangResult JSONRPCConnection::toNativeOrSendError(const JSONValue& value, const RttiInfo* info, void* dst)
{
    m_diagnosticSink.outputBuffer.Clear();
    if (SLANG_FAILED(JSONRPCUtil::convertToNative(&m_container, value, &m_diagnosticSink, info, dst)))
    {
        return sendError(JSONRPC::ErrorCode::InvalidRequest);
    }
    return SLANG_OK;
}

SlangResult JSONRPCConnection::sendCall(const UnownedStringSlice& method, const JSONValue& id)
{
    JSONRPCCall call;
    call.id = id;
    call.method = method;

    SLANG_RETURN_ON_FAIL(sendRPC(&call));
    return SLANG_OK;
}

SlangResult JSONRPCConnection::sendResult(const RttiInfo* rttiInfo, const void* result, const JSONValue& id)
{
    JSONResultResponse response;
    response.id = id;

    NativeToJSONConverter converter(&m_container, &m_diagnosticSink);
    SLANG_RETURN_ON_FAIL(converter.convert(rttiInfo, result, response.result));

    // Send the RPC
    SLANG_RETURN_ON_FAIL(sendRPC(&response));
    return SLANG_OK;
}

SlangResult JSONRPCConnection::sendCall(const UnownedStringSlice& method, const RttiInfo* argsRttiInfo, const void* args, const JSONValue& id)
{
    JSONRPCCall call;
    call.id = id;
    call.method = method;

    // Convert the args/params
    NativeToJSONConverter converter(&m_container, &m_diagnosticSink);
    SLANG_RETURN_ON_FAIL(converter.convert(argsRttiInfo, args, call.params));

    // Send the RPC
    SLANG_RETURN_ON_FAIL(sendRPC(&call));
    return SLANG_OK;
}

SlangResult JSONRPCConnection::waitForResult(Int timeOutInMs)
{
    SLANG_RETURN_ON_FAIL(m_connection->waitForResult(timeOutInMs));
    return tryReadMessage();
}

SlangResult JSONRPCConnection::tryReadMessage()
{
    m_jsonRoot.reset();

    SLANG_RETURN_ON_FAIL(m_connection->update());
    if (!m_connection->hasContent())
    {
        return SLANG_OK;
    }

    auto content = m_connection->getContent();
    UnownedStringSlice slice((const char*)content.begin(), content.getCount());

    clearBuffers();

    {
        const SlangResult res = JSONRPCUtil::parseJSON(slice, &m_container, &m_diagnosticSink, m_jsonRoot);

        // Consume that content/packet
        m_connection->consumeContent();
        if (SLANG_FAILED(res))
        {
            return sendError(JSONRPC::ErrorCode::ParseError);
        }
    }

    return SLANG_OK;
}

JSONRPCMessageType JSONRPCConnection::getMessageType()
{
    return JSONRPCUtil::getMessageType(&m_container, m_jsonRoot);
}

SlangResult JSONRPCConnection::getMessage(const RttiInfo* rttiInfo, void* out)
{
    if (!hasMessage())
    {
        return SLANG_FAIL;
    }

    m_diagnosticSink.outputBuffer.Clear();
    JSONToNativeConverter converter(&m_container, &m_diagnosticSink);

    // Get the RPC response
    JSONResultResponse resultResponse;
    SLANG_RETURN_ON_FAIL(converter.convert(m_jsonRoot, &resultResponse));

    // Convert the result in the response
    SLANG_RETURN_ON_FAIL(converter.convert(resultResponse.result, rttiInfo, out));
    return SLANG_OK;
}

SlangResult JSONRPCConnection::getMessageOrSendError(const RttiInfo* rttiInfo, void* out)
{
    if (!hasMessage())
    {
        return SLANG_FAIL;
    }

    const auto res = getMessage(rttiInfo, out);
    if (SLANG_FAILED(res))
    {
        return sendError(JSONRPC::ErrorCode::ParseError);
    }
    return res;
}

SlangResult JSONRPCConnection::getRPC(const RttiInfo* rttiInfo, void* out)
{
    if (!hasMessage())
    {
        return SLANG_FAIL;
    }

    m_diagnosticSink.outputBuffer.Clear();
    JSONToNativeConverter converter(&m_container, &m_diagnosticSink);

    // Convert the result in the response
    SLANG_RETURN_ON_FAIL(converter.convert(m_jsonRoot, rttiInfo, out));
    return SLANG_OK;
}

SlangResult JSONRPCConnection::getRPCOrSendError(const RttiInfo* rttiInfo, void* out)
{
    if (!hasMessage())
    {
        return SLANG_FAIL;
    }

    const auto res = getRPC(rttiInfo, out);
    if (SLANG_FAILED(res))
    {
        return sendError(JSONRPC::ErrorCode::ParseError);
    }
    return res;
}

} // namespcae Slang
