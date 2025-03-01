// slang-ir-lower-optional-type.cpp

#include "slang-ir-lower-optional-type.h"
#include "slang-ir.h"
#include "slang-ir-insts.h"

namespace Slang
{
    struct OptionalTypeLoweringContext
    {
        IRModule* module;
        DiagnosticSink* sink;

        SharedIRBuilder sharedBuilderStorage;

        List<IRInst*> workList;
        HashSet<IRInst*> workListSet;

        struct LoweredOptionalTypeInfo : public RefObject
        {
            IRType* optionalType = nullptr;
            IRType* valueType = nullptr;
            IRType* loweredType = nullptr;
            IRStructField* valueField = nullptr;
            IRStructField* hasValueField = nullptr;
        };
        Dictionary<IRInst*, RefPtr<LoweredOptionalTypeInfo>> mapLoweredTypeToOptionalTypeInfo;
        Dictionary<IRInst*, RefPtr<LoweredOptionalTypeInfo>> loweredOptionalTypes;

        IRType* maybeLowerOptionalType(IRBuilder* builder, IRType* type)
        {
            if (auto info = getLoweredOptionalType(builder, type))
                return info->loweredType;
            else
                return type;
        }

        LoweredOptionalTypeInfo* getLoweredOptionalType(IRBuilder* builder, IRInst* type)
        {
            if (auto loweredInfo = loweredOptionalTypes.TryGetValue(type))
                return loweredInfo->Ptr();
            if (auto loweredInfo = mapLoweredTypeToOptionalTypeInfo.TryGetValue(type))
                return loweredInfo->Ptr();

            if (!type)
                return nullptr;
            if (type->getOp() != kIROp_OptionalType)
                return nullptr;

            RefPtr<LoweredOptionalTypeInfo> info = new LoweredOptionalTypeInfo();
            info->optionalType = (IRType*)type;
            auto optionalType = cast<IROptionalType>(type);
            auto valueType = optionalType->getValueType();
            info->valueType = valueType;

            auto structType = builder->createStructType();
            info->loweredType = structType;
            builder->addNameHintDecoration(structType, UnownedStringSlice("OptionalType"));

            info->valueType = valueType;
            auto valueKey = builder->createStructKey();
            builder->addNameHintDecoration(valueKey, UnownedStringSlice("value"));
            info->valueField = builder->createStructField(structType, valueKey, (IRType*)valueType);

            auto boolType = builder->getBoolType();
            auto hasValueKey = builder->createStructKey();
            builder->addNameHintDecoration(hasValueKey, UnownedStringSlice("hasValue"));
            info->hasValueField = builder->createStructField(structType, hasValueKey, (IRType*)boolType);

            mapLoweredTypeToOptionalTypeInfo[info->loweredType] = info;
            loweredOptionalTypes[type] = info;
            return info.Ptr();
        }

        void addToWorkList(
            IRInst* inst)
        {
            for (auto ii = inst->getParent(); ii; ii = ii->getParent())
            {
                if (as<IRGeneric>(ii))
                    return;
            }

            if (workListSet.Contains(inst))
                return;

            workList.add(inst);
            workListSet.Add(inst);
        }

        void processMakeOptionalValue(IRMakeOptionalValue* inst)
        {
            IRBuilder builderStorage(sharedBuilderStorage);
            auto builder = &builderStorage;
            builder->setInsertBefore(inst);

            auto info = getLoweredOptionalType(builder, inst->getDataType());
            List<IRInst*> operands;
            operands.add(inst->getOperand(0));
            operands.add(builder->getBoolValue(true));
            auto makeStruct = builder->emitMakeStruct(info->loweredType, operands);
            inst->replaceUsesWith(makeStruct);
            inst->removeAndDeallocate();
        }

        void processMakeOptionalNone(IRMakeOptionalNone* inst)
        {
            IRBuilder builderStorage(sharedBuilderStorage);
            auto builder = &builderStorage;
            builder->setInsertBefore(inst);

            auto info = getLoweredOptionalType(builder, inst->getDataType());

            List<IRInst*> operands;
            operands.add(inst->getDefaultValue());
            operands.add(builder->getBoolValue(false));
            auto makeStruct = builder->emitMakeStruct(info->loweredType, operands);
            inst->replaceUsesWith(makeStruct);
            inst->removeAndDeallocate();
        }

        IRInst* getOptionalHasValue(IRBuilder* builder, IRInst* optionalInst)
        {
            auto loweredOptionalTypeInfo = getLoweredOptionalType(builder, optionalInst->getDataType());
            SLANG_ASSERT(loweredOptionalTypeInfo);

            auto value = builder->emitFieldExtract(
                builder->getBoolType(),
                optionalInst,
                loweredOptionalTypeInfo->hasValueField->getKey());
            return value;
        }

        void processGetOptionalHasValue(IROptionalHasValue* inst)
        {
            IRBuilder builderStorage(sharedBuilderStorage);
            auto builder = &builderStorage;
            builder->setInsertBefore(inst);

            auto optionalValue = inst->getOptionalOperand();
            auto hasVal = getOptionalHasValue(builder, optionalValue);
            inst->replaceUsesWith(hasVal);
            inst->removeAndDeallocate();
        }

        void processGetOptionalValue(IRGetOptionalValue* inst)
        {
            IRBuilder builderStorage(sharedBuilderStorage);
            auto builder = &builderStorage;
            builder->setInsertBefore(inst);

            auto base = inst->getOptionalOperand();
            auto loweredOptionalTypeInfo = getLoweredOptionalType(builder, base->getDataType());
            SLANG_ASSERT(loweredOptionalTypeInfo);
            SLANG_ASSERT(loweredOptionalTypeInfo->valueField);
            auto getElement = builder->emitFieldExtract(
                loweredOptionalTypeInfo->valueType,
                base,
                loweredOptionalTypeInfo->valueField->getKey());
            inst->replaceUsesWith(getElement);
            inst->removeAndDeallocate();
        }

        void processOptionalType(IROptionalType* inst)
        {
            IRBuilder builderStorage(sharedBuilderStorage);
            auto builder = &builderStorage;
            builder->setInsertBefore(inst);

            auto loweredOptionalTypeInfo = getLoweredOptionalType(builder, inst);
            SLANG_ASSERT(loweredOptionalTypeInfo);
            SLANG_UNUSED(loweredOptionalTypeInfo);
        }

        void processInst(IRInst* inst)
        {
            switch (inst->getOp())
            {
            case kIROp_MakeOptionalValue:
                processMakeOptionalValue((IRMakeOptionalValue*)inst);
                break;
            case kIROp_MakeOptionalNone:
                processMakeOptionalNone((IRMakeOptionalNone*)inst);
                break;
            case kIROp_OptionalHasValue:
                processGetOptionalHasValue((IROptionalHasValue*)inst);
                break;
            case kIROp_GetOptionalValue:
                processGetOptionalValue((IRGetOptionalValue*)inst);
                break;
            case kIROp_OptionalType:
                processOptionalType((IROptionalType*)inst);
                break;
            default:
                break;
            }
        }

        void processModule()
        {
            SharedIRBuilder* sharedBuilder = &sharedBuilderStorage;
            sharedBuilder->init(module);

            // Deduplicate equivalent types.
            sharedBuilder->deduplicateAndRebuildGlobalNumberingMap();

            addToWorkList(module->getModuleInst());

            while (workList.getCount() != 0)
            {
                IRInst* inst = workList.getLast();

                workList.removeLast();
                workListSet.Remove(inst);

                processInst(inst);

                for (auto child = inst->getLastChild(); child; child = child->getPrevInst())
                {
                    addToWorkList(child);
                }
            }

            // Replace all optional types with lowered struct types.
            for (auto kv : loweredOptionalTypes)
            {
                kv.Key->replaceUsesWith(kv.Value->loweredType);
            }
        }
    };
    
    void lowerOptionalType(IRModule* module, DiagnosticSink* sink)
    {
        OptionalTypeLoweringContext context;
        context.module = module;
        context.sink = sink;
        context.processModule();
    }
}
