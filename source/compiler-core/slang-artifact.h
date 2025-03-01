// slang-artifact.h
#ifndef SLANG_ARTIFACT_H
#define SLANG_ARTIFACT_H

#include "../core/slang-basic.h"

#include "../core/slang-castable-list.h"

namespace Slang
{


/* Simplest slice types. We can't use UnownedStringSlice etc, because they implement functionality in libraries,
and we want to use these types in headers.
If we wanted a C implementation it would be easy to use a macro to generate the functionality */

template <typename T>
struct Slice
{
    const T* begin() const { return data; }
    const T* end() const { return data + count; }

    Slice() :count(0), data(nullptr) {}
    Slice(const T* inData, Count inCount) :
        data(inData),
        count(inCount)
    {}

    const T* data;
    Count count;
};

struct ZeroTerminatedCharSlice : Slice<char>
{
    typedef Slice<char> Super;
    explicit ZeroTerminatedCharSlice(const char* in) :Super(in, ::strlen(in)) {}
    ZeroTerminatedCharSlice(const char* in, Count inCount) :Super(in, inCount) { SLANG_ASSERT(in[inCount] == 0); }
    ZeroTerminatedCharSlice() :Super("", 0) {}
};

/* As a rule of thumb, if we can define some aspect in a hierarchy then we should do so at the highest level. 
If some aspect can apply to multiple items identically we move that to a separate enum. 

NOTE!
New Kinds must be added at the end. Values can be depreciated, or disabled
but never removed, without breaking binary compatability.

Any change requires a change to SLANG_ARTIFACT_KIND
*/
enum class ArtifactKind : uint8_t
{ 
    Invalid,                    ///< Invalid
    Base,                       ///< Base kind of all valid kinds

    None,                       ///< Doesn't contain anything
    Unknown,                    ///< Unknown

    Container,                  ///< Container like types
    Zip,                        ///< Zip container
    Riff,                       ///< Riff format

    Text,                       ///< Representation is text. Encoding is utf8, unless prefixed with 'encoding'.
    
    Source,                     ///< Source (Source type is in payload)
    Assembly,                   ///< Assembly (Type is in payload)
    HumanText,                  ///< Text for human consumption

    BinaryLike,                 ///< Kinds which are 'binary like' - can be executed, linked with and so forth. 
    
    ObjectCode,                 ///< Object file
    Library,                    ///< Library (collection of object code)
    Executable,                 ///< Executable
    SharedLibrary,              ///< Shared library - can be dynamically linked
    HostCallable,               ///< Code can be executed directly on the host

    Instance,                   ///< Primary representation is an interface/class instance 
    
    CountOf,
};

/* Payload. 

SlangIR and LLVMIR can be GPU or CPU orientated, so put in own category.

NOTE!
New Payloads must be added at the end. Values can be depreciated, or disabled
but never removed, without breaking binary compatability.

Any change requires a change to SLANG_ARTIFACT_PAYLOAD
*/
enum class ArtifactPayload : uint8_t
{
    Invalid,        ///< Is invalid - indicates some kind of problem
    Base,           ///< The base of the hierarchy

    None,           ///< Doesn't have a payload
    Unknown,        ///< Unknown but probably valid
    
    Source,         ///< Source code
    
    C,              ///< C source
    Cpp,            ///< C++ source
    HLSL,           ///< HLSL source
    GLSL,           ///< GLSL source
    CUDA,           ///< CUDA source
    Metal,          ///< Metal source
    Slang,          ///< Slang source

    KernelLike,     ///< GPU Kernel like

    DXIL,           ///< DXIL 
    DXBC,           ///< DXBC
    SPIRV,          ///< SPIR-V
    PTX,            ///< PTX. NOTE! PTX is a text format, but is handable to CUDA API.
    MetalAIR,       ///< Metal AIR 
    CuBin,          ///< CUDA binary

    CPULike,        ///< CPU code
    
    UnknownCPU,     ///< CPU code for unknown/undetermined type
    X86,            ///< X86
    X86_64,         ///< X86_64
    Aarch,          ///< 32 bit arm
    Aarch64,        ///< Aarch64
    HostCPU,        ///< HostCPU
    UniversalCPU,   ///< CPU code for multiple CPU types 

    GeneralIR,      ///< General purpose IR representation (IR)

    SlangIR,        ///< Slang IR
    LLVMIR,         ///< LLVM IR

    AST,            ///< Abstract syntax tree (AST)

    SlangAST,       ///< Slang AST

    CompileResults, ///< Payload is a collection of compilation results

    MetaData,       ///< Meta data

    DebugInfo,      ///< Debugging information
    Diagnostics,    ///< Diagnostics information
    PostEmitMetadata, ///< Post emit meta data

    CountOf,
};

/* Style.

NOTE!
New Styles must be added at the end. Values can be depreciated, or disabled
but never removed, without breaking binary compatability.

Any change requires a change to SLANG_ARTIFACT_STYLE
*/
enum class ArtifactStyle : uint8_t
{
    Invalid,            ///< Invalid style (indicating an error)
    Base,
        
    Unknown,            ///< Unknown

    Kernel,             ///< Compiled as `GPU kernel` style.        
    Host,               ///< Compiled in `host` style

    CountOf,
};

typedef uint8_t ArtifactFlags;
struct ArtifactFlag
{
    enum Enum : ArtifactFlags
    {
        // Don't currently have any flags
    };
};

/**
A value type to describe aspects of the contents of an Artifact.
**/
struct ArtifactDesc
{
public:
    typedef ArtifactDesc This;

    typedef ArtifactKind Kind;
    typedef ArtifactPayload Payload;
    typedef ArtifactStyle Style;
    typedef ArtifactFlags Flags;
   
    typedef uint32_t PackedBacking;
    enum class Packed : PackedBacking;
    
        /// Get in packed format
    inline Packed getPacked() const;

    bool operator==(const This& rhs) const { return kind == rhs.kind && payload == rhs.payload && style == rhs.style && flags == rhs.flags;  }
    bool operator!=(const This& rhs) const { return !(*this == rhs); }

        /// Construct from the elements
    static This make(Kind inKind, Payload inPayload, Style inStyle = Style::Unknown, Flags flags = 0) { return This{ inKind, inPayload, inStyle, flags }; }

        /// Construct from the packed format
    inline static This make(Packed inPacked);

    Kind kind;
    Payload payload;
    Style style;
    Flags flags;
};

// --------------------------------------------------------------------------
inline ArtifactDesc::Packed ArtifactDesc::getPacked() const
{
    typedef PackedBacking IntType;
    return Packed((IntType(kind) << 24) |
        (IntType(payload) << 16) |
        (IntType(style) << 8) |
        flags);
}

// --------------------------------------------------------------------------
inline /* static */ArtifactDesc ArtifactDesc::make(Packed inPacked)
{
    const PackedBacking packed = PackedBacking(inPacked);

    This r;
    r.kind = Kind(packed >> 24);
    r.payload = Payload(uint8_t(packed >> 16));
    r.style = Style(uint8_t(packed >> 8));
    r.flags = uint8_t(packed);

    return r;
}

// Forward declare
class IFileArtifactRepresentation;
class IArtifactRepresentation;
class IArtifactList;

// Controls what items can be kept. 
enum class ArtifactKeep
{
    No,         ///< Don't keep the item
    Yes,        ///< Yes keep the final item
    All,        ///< Keep the final item and any intermediataries
};

/// True if can keep an intermediate item
SLANG_INLINE  bool canKeepIntermediate(ArtifactKeep keep) { return keep == ArtifactKeep::All; }
/// True if can keep
SLANG_INLINE bool canKeep(ArtifactKeep keep) { return Index(keep) >= Index(ArtifactKeep::Yes); }
/// Returns the keep type for an intermediate
SLANG_INLINE ArtifactKeep getIntermediateKeep(ArtifactKeep keep) { return (keep == ArtifactKeep::All) ? ArtifactKeep::All : ArtifactKeep::No; }

/* Forward define */
class IArtifactHandler;

/* The IArtifact interface is designed to represent some Artifact of compilation. It could be input to or output from a compilation.

An abstraction is desirable here, because depending on the compiler the artifact/s could be

* A file on the file system
* A blob
* Multiple files
* Some other (perhaps multiple) in memory representations 
* A name 

The artifact uses the Blob as the canonical in memory representation. 

Some downstream compilers require the artifact to be available as a file system file, or to produce
artifacts that are files. The IArtifact type allows to abstract away this difference, including the
ability to turn an in memory representation into a temporary file on the file system. 

The mechanism also allows for 'Containers' which allow for Artifacts to contain other Artifacts (amongst other things).
Those artifacts may be other files. For example a downstream compilation that produces results as well as temporary
files could be a Container containing artifacts for

* Diagnostics
* Temporary files (of known and unknown types)
* Files that contain known types
* Callable interface (an ISlangSharedLibrary)

There are several types of ways to associate data with an artifact:

* A representation
* Associated data
* A child artifact

A `representation` has to wholly represent the artifact. That representation could be a blob, a file on the file system,
an in memory representation. There are two classes of `Representation` - ones that can be turned into blobs (and therefore 
derive from IArtifactRepresentation) and ones that are in of themselves a representation (such as a blob or or ISlangSharedLibrary).

`Associated data` is information that is associated with the artifact, but isn't a (whole) representation. It could be part 
of the representation, or useful for the implementation of a representation. Could also be considered as a kind of side channel
to associate arbitrary temporary data with an artifact.

A `child artifact` belongs to the artifact, within the hierarchy of artifacts. Child artifacts are held in an IArtifactList.

More long term goals would be to

* Make Diagnostics into an interface (such it can be added to a Artifact result)
* Use Artifact and related types for downstream compiler
*/
class IArtifact : public ICastable
{
public:
    SLANG_COM_INTERFACE(0x57375e20, 0xbed, 0x42b6, { 0x9f, 0x5e, 0x59, 0x4f, 0x6, 0x2b, 0xe6, 0x90 })

    typedef ArtifactDesc Desc;

    typedef ArtifactKind Kind;
    typedef ArtifactPayload Payload;
    typedef ArtifactStyle Style;
    typedef ArtifactFlags Flags;
    typedef ArtifactKeep Keep;
    
        /// Get the Desc defining the contents of the artifact
    virtual SLANG_NO_THROW Desc SLANG_MCALL getDesc() = 0;

        /// Returns true if the artifact in principal exists
    virtual SLANG_NO_THROW bool SLANG_MCALL exists() = 0;

        /// Load as a blob
    virtual SLANG_NO_THROW SlangResult SLANG_MCALL loadBlob(Keep keep, ISlangBlob** outBlob) = 0;
    
        /// Require artifact is available as a file.
        /// NOTE! May need to serialize and write as a temporary file.
    virtual SLANG_NO_THROW SlangResult SLANG_MCALL requireFile(Keep keep, ISlangMutableFileSystem* fileSystem, IFileArtifactRepresentation** outFileRep) = 0;

        /// Load the artifact as a shared library
    virtual SLANG_NO_THROW SlangResult SLANG_MCALL loadSharedLibrary(ArtifactKeep keep, ISlangSharedLibrary** outSharedLibrary) = 0;

        /// Get the name of the artifact. This can be empty.
    virtual SLANG_NO_THROW const char* SLANG_MCALL getName() = 0;

        /// Add data associated with this artifact
    virtual SLANG_NO_THROW void SLANG_MCALL addAssociated(ICastable* castable) = 0;
        /// Find an associated item
    virtual SLANG_NO_THROW void* SLANG_MCALL SLANG_MCALL findAssociated(const Guid& unk) = 0;
        /// TODO(JS): We may want this to return nullptr if it's empty.
        /// Get the list of associated items
    virtual SLANG_NO_THROW ICastableList* SLANG_MCALL getAssociated() = 0;
        /// Find first associated that matches the predicate
    virtual SLANG_NO_THROW ICastable* SLANG_MCALL findAssociatedWithPredicate(ICastableList::FindFunc findFunc, void* data) = 0;

        /// Add a representation 
    virtual SLANG_NO_THROW void SLANG_MCALL addRepresentation(ICastable* castable) = 0;
        /// Add a representation that doesn't derive from IArtifactRepresentation
    virtual SLANG_NO_THROW void SLANG_MCALL addRepresentationUnknown(ISlangUnknown* rep) = 0;
        /// Find representation
    virtual SLANG_NO_THROW void* SLANG_MCALL findRepresentation(const Guid& guid) = 0;
        /// Find first representation that matches the predicate 
    virtual SLANG_NO_THROW ICastable* SLANG_MCALL findRepresentationWithPredicate(ICastableList::FindFunc findFunc, void* data) = 0;

        /// Get all the representations
    virtual SLANG_NO_THROW Slice<ICastable*> SLANG_MCALL getRepresentations() = 0;
        /// Get the list of all representations
    virtual SLANG_NO_THROW ICastableList* SLANG_MCALL getRepresentationList() = 0;

        /// Given a typeGuid representing the desired type get or create the representation.
        /// If found outCastable holds an entity that *must* be castable to typeGuid
    virtual SLANG_NO_THROW SlangResult SLANG_MCALL getOrCreateRepresentation(const Guid& typeGuid, ArtifactKeep keep, ICastable** outCastable) = 0;
};

class IArtifactContainer : public IArtifact
{
public:
    SLANG_COM_INTERFACE(0xa96e29bd, 0xb546, 0x4e79, { 0xa0, 0xdc, 0x67, 0x49, 0x22, 0x2c, 0x39, 0xad })

    typedef bool (*FindFunc)(IArtifact* artifact, void* data);

        /// Returns the result of expansion. Will return SLANG_E_UNINITIALIZED if expansion hasn't happened
    virtual SLANG_NO_THROW SlangResult SLANG_MCALL getExpandChildrenResult() = 0;
        /// Sets all of the children, will set the expansion state to SLANG_OK
    virtual SLANG_NO_THROW void SLANG_MCALL setChildren(IArtifact**children, Count count) = 0;
        /// Will be called implicitly on access to children
    virtual SLANG_NO_THROW SlangResult SLANG_MCALL expandChildren() = 0;
        /// Get the children, will only remain valid if no mutation of children list
    virtual SLANG_NO_THROW Slice<IArtifact*> SLANG_MCALL getChildren() = 0;
        /// Add the artifact to the list
    virtual SLANG_NO_THROW void SLANG_MCALL addChild(IArtifact* artifact) = 0;
        /// Removes the child at index, keeps other artifacts in the same order
    virtual SLANG_NO_THROW void SLANG_MCALL removeChildAt(Index index) = 0;
        /// Clear the list
    virtual SLANG_NO_THROW void SLANG_MCALL clearChildren() = 0;
        /// Find an artifact which is an exact match for the desc
    virtual SLANG_NO_THROW IArtifact* SLANG_MCALL findChildByDesc(const ArtifactDesc& desc) = 0;
        /// Find an artifact that matches desc allowing derivations. Flags is ignored
    virtual SLANG_NO_THROW IArtifact* SLANG_MCALL findChildByDerivedDesc(const ArtifactDesc& desc) = 0;
        /// Find by name
    virtual SLANG_NO_THROW IArtifact* SLANG_MCALL findChildByName(const char* name) = 0;
        /// Find via predicate function
    virtual SLANG_NO_THROW IArtifact* SLANG_MCALL findChildByPredicate(FindFunc func, void* data) = 0;
};

template <typename T>
SLANG_FORCE_INLINE T* findRepresentation(IArtifact* artifact)
{
    return reinterpret_cast<T*>(artifact->findRepresentation(T::getTypeGuid()));
}

/* The IArtifactRepresentation interface represents a single representation that can be part of an artifact. It's special in so far
as

* IArtifactRepresentation can be queried for it's underlying object class
* Can optionally serialize into a blob
*/
class IArtifactRepresentation : public ICastable
{
    SLANG_COM_INTERFACE(0x311457a8, 0x1796, 0x4ebb, { 0x9a, 0xfc, 0x46, 0xa5, 0x44, 0xc7, 0x6e, 0xa9 })

        /// Convert the instance into a serializable blob. 
        /// Returns SLANG_E_NOT_IMPLEMENTED if an implementation doesn't implement
    virtual SLANG_NO_THROW SlangResult SLANG_MCALL writeToBlob(ISlangBlob** blob) = 0;

        /// Returns true if this representation exists and is available for use.
    virtual SLANG_NO_THROW bool SLANG_MCALL exists() = 0;
};

/* Handler provides functionality external to the artifact */
class IArtifactHandler : public ICastable
{
    SLANG_COM_INTERFACE(0x6a646f57, 0xb3ac, 0x4c6a, { 0xb6, 0xf1, 0x33, 0xb6, 0xef, 0x60, 0xa6, 0xae });

        /// Given an artifact expands children
    virtual SLANG_NO_THROW SlangResult SLANG_MCALL expandChildren(IArtifactContainer* container) = 0;
        /// Given an artifact gets or creates a representation. 
    virtual SLANG_NO_THROW SlangResult SLANG_MCALL getOrCreateRepresentation(IArtifact* artifact, const Guid& guid, ArtifactKeep keep, ICastable** outCastable) = 0;
        /// Given an artifact gets a represenation of it on the file system. 
    virtual SLANG_NO_THROW SlangResult SLANG_MCALL getOrCreateFileRepresentation(IArtifact* artifact, ArtifactKeep keep, ISlangMutableFileSystem* fileSystem, IFileArtifactRepresentation** outFileRep) = 0;
};

} // namespace Slang

#endif
