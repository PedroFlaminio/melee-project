#include "assets/hsd_materialize.hpp"

#include <melee_host/gx.h>
#include <melee_host/memory.h>

/* psstructs.h also declares static inline helpers that only particle.c
 * defines. */
MELEE_HOST_HSD_BEGIN
#if defined(__clang__)
#pragma clang diagnostic ignored "-Wunused-function"
#elif defined(__GNUC__)
#pragma GCC diagnostic ignored "-Wunused-function"
#endif
#include <sysdolphin/baselib/psstructs.h>
#include <sysdolphin/baselib/spline.h>
MELEE_HOST_HSD_END

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstring>
#include <limits>
#include <string>

/* The sprite library declares its palette as the runtime HSD_Tlut, which the
 * materializer fills from the descriptor it already builds.  The two are the
 * same record field for field. */
static_assert(sizeof(HSD_Tlut) == sizeof(HSD_TlutDesc));
static_assert(offsetof(HSD_Tlut, fmt) == offsetof(HSD_TlutDesc, fmt));
static_assert(offsetof(HSD_Tlut, n_entries) ==
              offsetof(HSD_TlutDesc, n_entries));

namespace melee::assets {
namespace {

/* Disk field offsets.  These are the PowerPC layout the archive stores, which
 * is the reason the host cannot cast the file into its own structs: the same
 * fields sit elsewhere once pointers are eight bytes wide. */

namespace joint_field {
constexpr std::uint32_t kClassName = 0x00;
constexpr std::uint32_t kFlags = 0x04;
constexpr std::uint32_t kChild = 0x08;
constexpr std::uint32_t kNext = 0x0C;
constexpr std::uint32_t kUnion = 0x10;
constexpr std::uint32_t kRotation = 0x14;
constexpr std::uint32_t kScale = 0x20;
constexpr std::uint32_t kPosition = 0x2C;
constexpr std::uint32_t kMtx = 0x38;
constexpr std::uint32_t kRObjDesc = 0x3C;
} // namespace joint_field

namespace dobj_field {
constexpr std::uint32_t kClassName = 0x00;
constexpr std::uint32_t kNext = 0x04;
constexpr std::uint32_t kMObjDesc = 0x08;
constexpr std::uint32_t kPObjDesc = 0x0C;
} // namespace dobj_field

namespace mobj_field {
constexpr std::uint32_t kClassName = 0x00;
constexpr std::uint32_t kRenderMode = 0x04;
constexpr std::uint32_t kTexDesc = 0x08;
constexpr std::uint32_t kMaterial = 0x0C;
constexpr std::uint32_t kRenderDesc = 0x10;
constexpr std::uint32_t kPEDesc = 0x14;
} // namespace mobj_field

namespace tobj_field {
constexpr std::uint32_t kClassName = 0x00;
constexpr std::uint32_t kNext = 0x04;
constexpr std::uint32_t kId = 0x08;
constexpr std::uint32_t kSrc = 0x0C;
constexpr std::uint32_t kRotate = 0x10;
constexpr std::uint32_t kScale = 0x1C;
constexpr std::uint32_t kTranslate = 0x28;
constexpr std::uint32_t kWrapS = 0x34;
constexpr std::uint32_t kWrapT = 0x38;
constexpr std::uint32_t kRepeatS = 0x3C;
constexpr std::uint32_t kRepeatT = 0x3D;
constexpr std::uint32_t kBlendFlags = 0x40;
constexpr std::uint32_t kBlending = 0x44;
constexpr std::uint32_t kMagFilt = 0x48;
constexpr std::uint32_t kImageDesc = 0x4C;
constexpr std::uint32_t kTlutDesc = 0x50;
constexpr std::uint32_t kLod = 0x54;
constexpr std::uint32_t kTev = 0x58;
} // namespace tobj_field

namespace pobj_field {
constexpr std::uint32_t kClassName = 0x00;
constexpr std::uint32_t kNext = 0x04;
constexpr std::uint32_t kVerts = 0x08;
constexpr std::uint32_t kFlags = 0x0C;
constexpr std::uint32_t kDisplayCount = 0x0E;
constexpr std::uint32_t kDisplay = 0x10;
constexpr std::uint32_t kUnion = 0x14;
} // namespace pobj_field

namespace vtx_field {
constexpr std::uint32_t kAttr = 0x00;
constexpr std::uint32_t kAttrType = 0x04;
constexpr std::uint32_t kCompCnt = 0x08;
constexpr std::uint32_t kCompType = 0x0C;
constexpr std::uint32_t kFrac = 0x10;
constexpr std::uint32_t kStride = 0x12;
constexpr std::uint32_t kVertex = 0x14;
constexpr std::uint32_t kSize = 0x18;
} // namespace vtx_field

namespace image_field {
constexpr std::uint32_t kImagePtr = 0x00;
constexpr std::uint32_t kWidth = 0x04;
constexpr std::uint32_t kHeight = 0x06;
constexpr std::uint32_t kFormat = 0x08;
constexpr std::uint32_t kMipmap = 0x0C;
constexpr std::uint32_t kMinLod = 0x10;
constexpr std::uint32_t kMaxLod = 0x14;
} // namespace image_field

namespace tlut_field {
constexpr std::uint32_t kLut = 0x00;
constexpr std::uint32_t kFormat = 0x04;
constexpr std::uint32_t kName = 0x08;
constexpr std::uint32_t kEntries = 0x0C;
} // namespace tlut_field

namespace material_field {
constexpr std::uint32_t kAmbient = 0x00;
constexpr std::uint32_t kDiffuse = 0x04;
constexpr std::uint32_t kSpecular = 0x08;
constexpr std::uint32_t kAlpha = 0x0C;
constexpr std::uint32_t kShininess = 0x10;
} // namespace material_field

namespace shape_set_field {
constexpr std::uint32_t kFlags = 0x00;
constexpr std::uint32_t kShapeCount = 0x02;
constexpr std::uint32_t kVertexIndexCount = 0x04;
constexpr std::uint32_t kVertexDesc = 0x08;
constexpr std::uint32_t kVertexIndexList = 0x0C;
constexpr std::uint32_t kNormalIndexCount = 0x10;
constexpr std::uint32_t kNormalDesc = 0x14;
constexpr std::uint32_t kNormalIndexList = 0x18;
} // namespace shape_set_field

namespace envelope_field {
constexpr std::uint32_t kJoint = 0x00;
constexpr std::uint32_t kWeight = 0x04;
constexpr std::uint32_t kSize = 0x08;
} // namespace envelope_field

namespace figa_tree_field {
constexpr std::uint32_t kType = 0x00;
constexpr std::uint32_t kFlags = 0x04;
constexpr std::uint32_t kFrames = 0x08;
constexpr std::uint32_t kNodes = 0x0C;
constexpr std::uint32_t kTracks = 0x10;
} // namespace figa_tree_field

namespace figa_track_field {
constexpr std::uint32_t kLength = 0x00;
constexpr std::uint32_t kStartFrame = 0x02;
constexpr std::uint32_t kObjType = 0x04;
constexpr std::uint32_t kFracValue = 0x05;
constexpr std::uint32_t kFracSlope = 0x06;
constexpr std::uint32_t kData = 0x08;
constexpr std::uint32_t kSize = 0x0C;
} // namespace figa_track_field

namespace anim_joint_field {
constexpr std::uint32_t kChild = 0x00;
constexpr std::uint32_t kNext = 0x04;
constexpr std::uint32_t kAObjDesc = 0x08;
constexpr std::uint32_t kRObjAnim = 0x0C;
constexpr std::uint32_t kFlags = 0x10;
} // namespace anim_joint_field

namespace aobj_field {
constexpr std::uint32_t kFlags = 0x00;
constexpr std::uint32_t kEndFrame = 0x04;
constexpr std::uint32_t kFObjDesc = 0x08;
constexpr std::uint32_t kObjId = 0x0C;
} // namespace aobj_field

namespace fobj_field {
constexpr std::uint32_t kNext = 0x00;
constexpr std::uint32_t kLength = 0x04;
constexpr std::uint32_t kStartFrame = 0x08;
constexpr std::uint32_t kType = 0x0C;
constexpr std::uint32_t kFracValue = 0x0D;
constexpr std::uint32_t kFracSlope = 0x0E;
constexpr std::uint32_t kData = 0x10;
} // namespace fobj_field

namespace mat_anim_joint_field {
constexpr std::uint32_t kChild = 0x00;
constexpr std::uint32_t kNext = 0x04;
constexpr std::uint32_t kMatAnim = 0x08;
} // namespace mat_anim_joint_field

namespace mat_anim_field {
constexpr std::uint32_t kNext = 0x00;
constexpr std::uint32_t kAObjDesc = 0x04;
constexpr std::uint32_t kTexAnim = 0x08;
constexpr std::uint32_t kRenderAnim = 0x0C;
} // namespace mat_anim_field

namespace tex_anim_field {
constexpr std::uint32_t kNext = 0x00;
constexpr std::uint32_t kId = 0x04;
constexpr std::uint32_t kAObjDesc = 0x08;
constexpr std::uint32_t kImageTable = 0x0C;
constexpr std::uint32_t kTlutTable = 0x10;
constexpr std::uint32_t kImageCount = 0x14;
constexpr std::uint32_t kTlutCount = 0x16;
} // namespace tex_anim_field

namespace render_anim_field {
constexpr std::uint32_t kChanAnim = 0x00;
constexpr std::uint32_t kRegAnim = 0x04;
} // namespace render_anim_field

namespace shape_anim_joint_field {
constexpr std::uint32_t kChild = 0x00;
constexpr std::uint32_t kNext = 0x04;
constexpr std::uint32_t kShapeAnimDObj = 0x08;
} // namespace shape_anim_joint_field

/* HSD_RObjAnimJoint, HSD_ChanAnim, HSD_TevRegAnim, HSD_ShapeAnimDObj and
 * HSD_ShapeAnim all have the same shape: a link and one payload pointer. */
namespace anim_link_field {
constexpr std::uint32_t kNext = 0x00;
constexpr std::uint32_t kPayload = 0x04;
} // namespace anim_link_field

namespace camera_field {
constexpr std::uint32_t kClassName = 0x00;
constexpr std::uint32_t kFlags = 0x04;
constexpr std::uint32_t kProjectionType = 0x06;
constexpr std::uint32_t kViewport = 0x08;
constexpr std::uint32_t kScissor = 0x10;
constexpr std::uint32_t kEyePosition = 0x18;
constexpr std::uint32_t kInterest = 0x1C;
constexpr std::uint32_t kRoll = 0x20;
constexpr std::uint32_t kUpVector = 0x24;
constexpr std::uint32_t kNear = 0x28;
constexpr std::uint32_t kFar = 0x2C;
/* The tail is the projection's own parameters: a perspective camera keeps fov
 * and aspect, a frustum or orthographic one keeps four planes. */
constexpr std::uint32_t kFov = 0x30;
constexpr std::uint32_t kAspect = 0x34;
constexpr std::uint32_t kTop = 0x30;
constexpr std::uint32_t kBottom = 0x34;
constexpr std::uint32_t kLeft = 0x38;
constexpr std::uint32_t kRight = 0x3C;
} // namespace camera_field

namespace world_field {
constexpr std::uint32_t kClassName = 0x00;
constexpr std::uint32_t kPosition = 0x04;
constexpr std::uint32_t kRObjDesc = 0x10;
} // namespace world_field

namespace world_anim_field {
constexpr std::uint32_t kAObjDesc = 0x00;
constexpr std::uint32_t kRObjAnim = 0x04;
} // namespace world_anim_field

namespace light_field {
constexpr std::uint32_t kClassName = 0x00;
constexpr std::uint32_t kNext = 0x04;
constexpr std::uint32_t kFlags = 0x08;
constexpr std::uint32_t kAttnFlags = 0x0A;
constexpr std::uint32_t kColor = 0x0C;
constexpr std::uint32_t kPosition = 0x10;
constexpr std::uint32_t kInterest = 0x14;
constexpr std::uint32_t kParameters = 0x18;
} // namespace light_field

/* HSD_LightDesc.u takes one of three shapes, chosen by the light's type and
 * attenuation flags the same way LObjLoad reads it. */
namespace light_point_field {
constexpr std::uint32_t kRefBrightness = 0x00;
constexpr std::uint32_t kRefDistance = 0x04;
constexpr std::uint32_t kDistanceFunc = 0x08;
} // namespace light_point_field

namespace light_spot_field {
constexpr std::uint32_t kCutoff = 0x00;
constexpr std::uint32_t kSpotFunc = 0x04;
constexpr std::uint32_t kRefBrightness = 0x08;
constexpr std::uint32_t kRefDistance = 0x0C;
constexpr std::uint32_t kDistanceFunc = 0x10;
} // namespace light_spot_field

namespace light_attn_field {
constexpr std::uint32_t kA0 = 0x00;
constexpr std::uint32_t kA1 = 0x04;
constexpr std::uint32_t kA2 = 0x08;
constexpr std::uint32_t kK0 = 0x0C;
constexpr std::uint32_t kK1 = 0x10;
constexpr std::uint32_t kK2 = 0x14;
} // namespace light_attn_field

namespace light_anim_field {
constexpr std::uint32_t kNext = 0x00;
constexpr std::uint32_t kAObjDesc = 0x04;
constexpr std::uint32_t kPositionAnim = 0x08;
constexpr std::uint32_t kInterestAnim = 0x0C;
} // namespace light_anim_field

/* One entry of a `*_scene_lights` table: a light and its animation table. */
namespace light_list_field {
constexpr std::uint32_t kDesc = 0x00;
constexpr std::uint32_t kAnims = 0x04;
} // namespace light_list_field

namespace fog_field {
constexpr std::uint32_t kType = 0x00;
constexpr std::uint32_t kAdjDesc = 0x04;
constexpr std::uint32_t kStart = 0x08;
constexpr std::uint32_t kEnd = 0x0C;
constexpr std::uint32_t kColor = 0x10;
} // namespace fog_field

namespace fog_adj_field {
constexpr std::uint32_t kCenter = 0x00;
constexpr std::uint32_t kWidth = 0x02;
constexpr std::uint32_t kMatrix = 0x04;
} // namespace fog_adj_field

namespace sobj_field {
constexpr std::uint32_t kImage = 0x00;
constexpr std::uint32_t kTlut = 0x04;
} // namespace sobj_field

namespace robj_field {
constexpr std::uint32_t kNext = 0x00;
constexpr std::uint32_t kFlags = 0x04;
constexpr std::uint32_t kUnion = 0x08;
} // namespace robj_field

namespace lod_field {
constexpr std::uint32_t kMinFilt = 0x00;
constexpr std::uint32_t kLodBias = 0x04;
constexpr std::uint32_t kBiasClamp = 0x08;
constexpr std::uint32_t kEdgeLodEnable = 0x09;
constexpr std::uint32_t kMaxAnisotropy = 0x0C;
} // namespace lod_field

namespace tev_field {
constexpr std::uint32_t kOps = 0x00;
constexpr std::uint32_t kOpCount = 0x10;
constexpr std::uint32_t kKonst = 0x10;
constexpr std::uint32_t kTev0 = 0x14;
constexpr std::uint32_t kTev1 = 0x18;
constexpr std::uint32_t kActive = 0x1C;
} // namespace tev_field

/* A scene's model table is a NULL-terminated array of DynamicModelDesc
 * pointers whose first field is the joint.  The camera table is an inline
 * array of SceneCameraDesc, each a descriptor pointer and an animation
 * table. */
constexpr std::uint32_t kSceneModels = 0x00;
constexpr std::uint32_t kSceneCameras = 0x04;
constexpr std::uint32_t kModelJoint = 0x00;
/* DynamicModelDesc keeps three NULL-terminated tables beside its joint, one
 * per kind of animation. */
constexpr std::uint32_t kModelAnims = 0x04;
constexpr std::uint32_t kModelMatAnims = 0x08;
constexpr std::uint32_t kModelShapeAnims = 0x0C;
constexpr std::uint32_t kSceneCameraDescSize = 0x08;
constexpr std::uint32_t kSceneCameraDesc = 0x00;
/* The light lists are a NULL-terminated table.  The fogs, like the cameras,
 * are an inline array of a descriptor and an animation table with no
 * terminator: GmPause.dat follows its one fog with the SceneDesc itself. */
constexpr std::uint32_t kSceneLights = 0x08;
constexpr std::uint32_t kSceneFogs = 0x0C;
constexpr std::uint32_t kSceneEntryAnims = 0x04;

namespace camera_anim_field {
constexpr std::uint32_t kAObjDesc = 0x00;
constexpr std::uint32_t kEyeAnim = 0x04;
constexpr std::uint32_t kInterestAnim = 0x08;
} // namespace camera_anim_field

/* Display lists are handed to GXCallDisplayList in 32-byte blocks. */
constexpr std::size_t kDisplayListBlock = 32;
constexpr std::size_t kVertexDescriptorLimit = 64;
constexpr std::size_t kSceneModelLimit = 4096;
/* A PObj addresses at most ten matrices, and a vertex is weighted against at
 * most as many joints.  These ceilings are well past that and exist only to
 * stop a malformed list from being walked without end. */
constexpr std::size_t kEnvelopeLimit = 64;
constexpr std::size_t kShapeLimit = 4096;
/* A skeleton's bone count and the tracks one bone can carry.  Both are far
 * past what the game uses and exist to stop a malformed list. */
constexpr std::size_t kFigaNodeLimit = 4096;
constexpr std::size_t kFigaTrackLimit = 65536;
constexpr std::size_t kDepthLimit = 512;
/* GX has eight hardware lights.  This only stops a malformed light table or
 * chain from being walked without end. */
constexpr std::size_t kLightLimit = 256;

/* Host descriptors are larger than the disk records they come from, because
 * every pointer field doubles in width.  Four times the data section is a
 * generous ceiling that still fails loudly instead of growing without bound;
 * growing would break the contiguity the ID table's truncated keys rely on. */
constexpr std::size_t kDescriptorBudgetFactor = 4;
constexpr std::size_t kDescriptorBudgetFloor = 4096;

/* Every cycle of the descriptor graph passes through a joint, so counting
 * joint recursion bounds the whole traversal.  Memoization already stops a
 * cycle from repeating; this stops a long chain of distinct nodes in a
 * malformed file from running the stack out. */
class DepthGuard final {
public:
    explicit DepthGuard(std::size_t& depth) : depth_(depth) { depth_ += 1; }
    ~DepthGuard() { depth_ -= 1; }
    DepthGuard(const DepthGuard&) = delete;
    DepthGuard& operator=(const DepthGuard&) = delete;

private:
    std::size_t& depth_;
};

std::string hex_string(std::uint32_t value)
{
    static constexpr char kDigits[] = "0123456789abcdef";
    std::string text;
    for (int shift = 28; shift >= 0; shift -= 4) {
        const auto nibble = static_cast<std::size_t>((value >> shift) & 0xFU);
        if (!text.empty() || nibble != 0 || shift == 0) {
            text.push_back(kDigits[nibble]);
        }
    }
    return text;
}

[[noreturn]] void unsupported(const char* what)
{
    throw HsdArchiveError(std::string("HSD materializer does not support ") +
                          what);
}

} // namespace

HsdMaterializedArchive::HsdMaterializedArchive(
    const HsdRuntimeArchive& archive)
    : archive_(archive)
{
    const auto data = archive_.disk_view().data();
    payload_size_ = data.size();
    if (payload_size_ == 0) {
        throw HsdArchiveError("HSD archive has no data section");
    }
    /* Display lists inside the data section are 32-byte aligned relative to
     * its start, so the copy has to keep that alignment. */
    payload_ = static_cast<std::byte*>(
        melee_host_aligned_alloc(payload_size_, kDisplayListBlock));
    if (payload_ == nullptr) {
        throw HsdArchiveError("HSD payload copy could not be allocated");
    }
    std::memcpy(payload_, data.data(), payload_size_);
    /* Vertex arrays in this copy reach GX through the original GXSetArray,
     * which carries no length.  Declaring the region gives the decoder the
     * only upper bound that exists: the end of the file. */
    melee_host_gx_register_array_region(payload_, payload_size_);

    descriptor_capacity_ =
        payload_size_ * kDescriptorBudgetFactor + kDescriptorBudgetFloor;
    descriptors_ = static_cast<std::byte*>(
        melee_host_aligned_alloc(descriptor_capacity_, kDisplayListBlock));
    if (descriptors_ == nullptr) {
        melee_host_aligned_free(payload_, kDisplayListBlock);
        payload_ = nullptr;
        throw HsdArchiveError("HSD descriptor arena could not be allocated");
    }
    std::memset(descriptors_, 0, descriptor_capacity_);
    stats_.payload_bytes = payload_size_;
}

HsdMaterializedArchive::~HsdMaterializedArchive()
{
    melee_host_gx_unregister_array_region(payload_);
    for (std::byte* const block : outside_arena_) {
        melee_host_aligned_free(block, kDisplayListBlock);
    }
    melee_host_aligned_free(descriptors_, kDisplayListBlock);
    melee_host_aligned_free(payload_, kDisplayListBlock);
}

const HsdMaterializeStats& HsdMaterializedArchive::stats() const noexcept
{
    return stats_;
}

void HsdMaterializedArchive::declare_null_field(std::uint32_t data_offset)
{
    null_fields_.insert(data_offset);
}

const HsdRuntimeArchive& HsdMaterializedArchive::runtime() const noexcept
{
    return archive_;
}

void* HsdMaterializedArchive::translator_allocate(std::size_t size,
                                                  std::size_t alignment)
{
    if (alignment == 0 || (alignment & (alignment - 1)) != 0) {
        throw HsdArchiveError(
            "a translator asked for an alignment that is not a power of two");
    }
    return allocate_bytes(size, alignment);
}

std::optional<HsdRuntimeNode>
HsdMaterializedArchive::translator_pointer(std::uint32_t field_offset) const
{
    return reference({ 0 }, field_offset);
}

void* HsdMaterializedArchive::translator_payload(std::uint32_t data_offset,
                                                 std::size_t length) const
{
    return payload({ data_offset }, length);
}

void* HsdMaterializedArchive::allocate_bytes(std::size_t size,
                                             std::size_t alignment)
{
    const std::size_t misalignment = descriptor_used_ % alignment;
    const std::size_t padding =
        misalignment == 0 ? 0 : alignment - misalignment;
    if (padding > descriptor_capacity_ - descriptor_used_ ||
        size > descriptor_capacity_ - descriptor_used_ - padding)
    {
        throw HsdArchiveError("HSD descriptor arena is exhausted");
    }
    descriptor_used_ += padding;
    std::byte* const result = descriptors_ + descriptor_used_;
    descriptor_used_ += size;
    stats_.descriptor_bytes = descriptor_used_;
    return result;
}

void* HsdMaterializedArchive::allocate_outside_arena(std::size_t size,
                                                     std::size_t alignment)
{
    if (alignment > kDisplayListBlock || size == 0) {
        throw HsdArchiveError("HSD block outside the arena has a bad shape");
    }
    outside_arena_.reserve(outside_arena_.size() + 1);
    auto* const block = static_cast<std::byte*>(
        melee_host_aligned_alloc(size, kDisplayListBlock));
    if (block == nullptr) {
        throw HsdArchiveError("HSD block outside the arena could not be "
                              "allocated");
    }
    std::memset(block, 0, size);
    outside_arena_.push_back(block);
    return block;
}

template <typename T> T* HsdMaterializedArchive::allocate()
{
    return static_cast<T*>(allocate_bytes(sizeof(T), alignof(T)));
}

std::optional<HsdRuntimeNode> HsdMaterializedArchive::reference(
    HsdRuntimeNode node, std::uint32_t relative_offset) const
{
    const std::uint64_t field =
        static_cast<std::uint64_t>(node.data_offset) + relative_offset;
    if (!null_fields_.empty() &&
        field <= std::numeric_limits<std::uint32_t>::max() &&
        null_fields_.contains(static_cast<std::uint32_t>(field)))
    {
        /* An extern that HSD_ArchiveLocateExtern resolved to NULL. */
        return std::nullopt;
    }
    if (archive_.has_reference_at(node, relative_offset)) {
        return archive_.reference_at(node, relative_offset);
    }
    /* A pointer field the archive did not relocate must read as NULL.  A
     * non-zero one is either a console address the host cannot honour or a
     * field the schema has misread, and both have to surface here rather than
     * reach the original loaders as a wild pointer. */
    const std::uint32_t raw = archive_.read_u32(node, relative_offset);
    if (raw != 0) {
        throw HsdArchiveError(
            "HSD pointer field at data+0x" +
            hex_string(node.data_offset + relative_offset) +
            " holds 0x" + hex_string(raw) + " with no relocation");
    }
    return std::nullopt;
}

void* HsdMaterializedArchive::payload(HsdRuntimeNode node,
                                      std::size_t length) const
{
    if (node.data_offset > payload_size_ ||
        length > payload_size_ - node.data_offset)
    {
        throw HsdArchiveError("HSD payload reference exceeds the data section");
    }
    return payload_ + node.data_offset;
}

char* HsdMaterializedArchive::payload_string(HsdRuntimeNode node) const
{
    /* Validates the terminator against the read-only view, then points at the
     * same bytes in the host copy. */
    const std::string_view text = archive_.read_c_string(node);
    return static_cast<char*>(payload(node, text.size() + 1));
}

void HsdMaterializedArchive::read_vec3(HsdRuntimeNode node,
                                       std::uint32_t relative_offset,
                                       Vec3* out) const
{
    out->x = archive_.read_f32(node, relative_offset);
    out->y = archive_.read_f32(node, relative_offset + 4);
    out->z = archive_.read_f32(node, relative_offset + 8);
}

void HsdMaterializedArchive::read_color(HsdRuntimeNode node,
                                        std::uint32_t relative_offset,
                                        GXColor* out) const
{
    const auto bytes =
        archive_.bytes_at({ node.data_offset + relative_offset }, 4);
    out->r = std::to_integer<u8>(bytes[0]);
    out->g = std::to_integer<u8>(bytes[1]);
    out->b = std::to_integer<u8>(bytes[2]);
    out->a = std::to_integer<u8>(bytes[3]);
}

f32* HsdMaterializedArchive::matrix(HsdRuntimeNode node)
{
    /* JObjLoad memcpys this into an envelope matrix, so unlike a GX payload it
     * has to hold host-order floats. */
    auto* const values = static_cast<f32*>(
        allocate_bytes(sizeof(Mtx), alignof(f32)));
    for (std::uint32_t index = 0; index < 12; ++index) {
        values[index] = archive_.read_f32(node, index * 4);
    }
    return values;
}

HSD_ImageDesc* HsdMaterializedArchive::image_desc(HsdRuntimeNode node)
{
    const auto found = images_.find(node.data_offset);
    if (found != images_.end()) {
        return found->second;
    }
    HSD_ImageDesc* const host = allocate<HSD_ImageDesc>();
    images_.emplace(node.data_offset, host);
    stats_.image_descs += 1;

    host->width = archive_.read_u16(node, image_field::kWidth);
    host->height = archive_.read_u16(node, image_field::kHeight);
    host->format =
        static_cast<GXTexFmt>(archive_.read_u32(node, image_field::kFormat));
    host->mipmap = archive_.read_u32(node, image_field::kMipmap);
    host->minLOD = archive_.read_f32(node, image_field::kMinLod);
    host->maxLOD = archive_.read_f32(node, image_field::kMaxLod);
    if (const auto image = reference(node, image_field::kImagePtr)) {
        host->image_ptr = payload(*image, 1);
    }
    return host;
}

HSD_TlutDesc* HsdMaterializedArchive::tlut_desc(HsdRuntimeNode node)
{
    const auto found = tluts_.find(node.data_offset);
    if (found != tluts_.end()) {
        return found->second;
    }
    HSD_TlutDesc* const host = allocate<HSD_TlutDesc>();
    tluts_.emplace(node.data_offset, host);
    stats_.tlut_descs += 1;

    host->fmt =
        static_cast<GXTlutFmt>(archive_.read_u32(node, tlut_field::kFormat));
    host->tlut_name = archive_.read_u32(node, tlut_field::kName);
    host->n_entries = archive_.read_u16(node, tlut_field::kEntries);
    if (const auto lut = reference(node, tlut_field::kLut)) {
        /* Each palette entry is 16 bits wide. */
        host->lut = payload(*lut, std::size_t{ host->n_entries } * 2);
    }
    return host;
}

HSD_Material* HsdMaterializedArchive::material(HsdRuntimeNode node)
{
    HSD_Material* const host = allocate<HSD_Material>();
    read_color(node, material_field::kAmbient, &host->ambient);
    read_color(node, material_field::kDiffuse, &host->diffuse);
    read_color(node, material_field::kSpecular, &host->specular);
    host->alpha = archive_.read_f32(node, material_field::kAlpha);
    host->shininess = archive_.read_f32(node, material_field::kShininess);
    return host;
}

HSD_PEDesc* HsdMaterializedArchive::pixel_engine_desc(HsdRuntimeNode node)
{
    /* Every field is a byte, so this one only needs copying. */
    HSD_PEDesc* const host = allocate<HSD_PEDesc>();
    const auto bytes = archive_.bytes_at(node, sizeof(HSD_PEDesc));
    std::memcpy(host, bytes.data(), sizeof(HSD_PEDesc));
    return host;
}

HSD_TexLODDesc* HsdMaterializedArchive::lod_desc(HsdRuntimeNode node)
{
    HSD_TexLODDesc* const host = allocate<HSD_TexLODDesc>();
    host->minFilt =
        static_cast<GXTexFilter>(archive_.read_u32(node, lod_field::kMinFilt));
    host->LODBias = archive_.read_f32(node, lod_field::kLodBias);
    host->bias_clamp =
        std::to_integer<u8>(archive_.bytes_at(
            { node.data_offset + lod_field::kBiasClamp }, 1)[0]);
    host->edgeLODEnable =
        std::to_integer<u8>(archive_.bytes_at(
            { node.data_offset + lod_field::kEdgeLodEnable }, 1)[0]);
    host->max_anisotropy = static_cast<GXAnisotropy>(
        archive_.read_u32(node, lod_field::kMaxAnisotropy));
    return host;
}

HSD_TObjTevDesc* HsdMaterializedArchive::tev_desc(HsdRuntimeNode node)
{
    HSD_TObjTevDesc* const host = allocate<HSD_TObjTevDesc>();
    /* The leading sixteen fields are bytes; only the colours and the active
     * mask need decoding. */
    const auto ops = archive_.bytes_at({ node.data_offset + tev_field::kOps },
                                       tev_field::kOpCount);
    std::memcpy(host, ops.data(), tev_field::kOpCount);
    read_color(node, tev_field::kKonst, &host->konst);
    read_color(node, tev_field::kTev0, &host->tev0);
    read_color(node, tev_field::kTev1, &host->tev1);
    host->active = archive_.read_u32(node, tev_field::kActive);
    return host;
}

HSD_VtxDescList* HsdMaterializedArchive::vertex_descriptors(
    HsdRuntimeNode node)
{
    const auto found = vertex_lists_.find(node.data_offset);
    if (found != vertex_lists_.end()) {
        return found->second;
    }

    /* The list is terminated by GX_VA_NULL, so its length has to be measured
     * before the host copy can be sized. */
    std::size_t count = 0;
    while (count < kVertexDescriptorLimit) {
        const HsdRuntimeNode entry{
            node.data_offset +
            static_cast<std::uint32_t>(count * vtx_field::kSize)
        };
        if (archive_.read_u32(entry, vtx_field::kAttr) == GX_VA_NULL) {
            break;
        }
        ++count;
    }
    if (count == kVertexDescriptorLimit) {
        throw HsdArchiveError("HSD vertex descriptor list has no terminator");
    }

    auto* const host = static_cast<HSD_VtxDescList*>(allocate_bytes(
        sizeof(HSD_VtxDescList) * (count + 1), alignof(HSD_VtxDescList)));
    vertex_lists_.emplace(node.data_offset, host);
    stats_.vertex_descriptors += count;

    for (std::size_t index = 0; index < count; ++index) {
        const HsdRuntimeNode entry{
            node.data_offset +
            static_cast<std::uint32_t>(index * vtx_field::kSize)
        };
        HSD_VtxDescList& out = host[index];
        out.attr = static_cast<GXAttr>(
            archive_.read_u32(entry, vtx_field::kAttr));
        out.attr_type = static_cast<GXAttrType>(
            archive_.read_u32(entry, vtx_field::kAttrType));
        out.comp_cnt = static_cast<GXCompCnt>(
            archive_.read_u32(entry, vtx_field::kCompCnt));
        out.comp_type = static_cast<GXCompType>(
            archive_.read_u32(entry, vtx_field::kCompType));
        out.frac = std::to_integer<u8>(archive_.bytes_at(
            { entry.data_offset + vtx_field::kFrac }, 1)[0]);
        out.stride = archive_.read_u16(entry, vtx_field::kStride);
        if (const auto array = reference(entry, vtx_field::kVertex)) {
            /* An indexed array is addressed by the display list, so its
             * length is not knowable from the descriptor.  Validating the base
             * is all this layer can do; the GX decoder bounds each read. */
            out.vertex = payload(*array, 1);
        } else if (out.attr_type != GX_DIRECT) {
            throw HsdArchiveError(
                "indexed HSD vertex descriptor has no array reference");
        }
    }
    host[count].attr = GX_VA_NULL;
    return host;
}

HSD_ShapeSetDesc* HsdMaterializedArchive::shape_set_desc(
    HsdRuntimeNode node)
{
    const auto found = shape_sets_.find(node.data_offset);
    if (found != shape_sets_.end()) {
        return found->second;
    }
    HSD_ShapeSetDesc* const host = allocate<HSD_ShapeSetDesc>();
    shape_sets_.emplace(node.data_offset, host);
    stats_.shape_set_descs += 1;

    host->flags = archive_.read_u16(node, shape_set_field::kFlags);
    host->nb_shape = archive_.read_u16(node, shape_set_field::kShapeCount);
    host->nb_vertex_index = static_cast<s32>(
        archive_.read_u32(node, shape_set_field::kVertexIndexCount));
    host->nb_normal_index = static_cast<s32>(
        archive_.read_u32(node, shape_set_field::kNormalIndexCount));
    if (host->nb_shape > kShapeLimit) {
        throw HsdArchiveError("HSD shape set declares too many shapes");
    }
    if (const auto verts = reference(node, shape_set_field::kVertexDesc)) {
        host->vertex_desc = vertex_descriptors(*verts);
    }
    if (const auto normals = reference(node, shape_set_field::kNormalDesc)) {
        host->normal_desc = vertex_descriptors(*normals);
    }
    /* One index blob per shape, addressed by shape id. */
    if (const auto list = reference(node, shape_set_field::kVertexIndexList)) {
        host->vertex_idx_list = payload_pointer_array(*list, host->nb_shape);
    }
    if (const auto list = reference(node, shape_set_field::kNormalIndexList)) {
        host->normal_idx_list = payload_pointer_array(*list, host->nb_shape);
    }
    return host;
}

u8** HsdMaterializedArchive::payload_pointer_array(HsdRuntimeNode node,
                                                   std::size_t count)
{
    if (count == 0) {
        return nullptr;
    }
    auto* const host = static_cast<u8**>(
        allocate_bytes(sizeof(u8*) * count, alignof(u8*)));
    for (std::size_t index = 0; index < count; ++index) {
        const auto entry =
            reference(node, static_cast<std::uint32_t>(index * 4));
        host[index] = entry.has_value()
                          ? static_cast<u8*>(payload(*entry, 1))
                          : nullptr;
    }
    return host;
}

HSD_EnvelopeDesc* HsdMaterializedArchive::envelope_list(HsdRuntimeNode node)
{
    /* The list ends at the first entry without a joint. */
    std::size_t count = 0;
    while (count < kEnvelopeLimit) {
        const HsdRuntimeNode entry{
            node.data_offset +
            static_cast<std::uint32_t>(count * envelope_field::kSize)
        };
        if (!reference(entry, envelope_field::kJoint).has_value()) {
            break;
        }
        ++count;
    }
    if (count == kEnvelopeLimit) {
        throw HsdArchiveError("HSD envelope list has no terminator");
    }

    auto* const host = static_cast<HSD_EnvelopeDesc*>(allocate_bytes(
        sizeof(HSD_EnvelopeDesc) * (count + 1), alignof(HSD_EnvelopeDesc)));
    stats_.envelope_descs += count;
    for (std::size_t index = 0; index < count; ++index) {
        const HsdRuntimeNode entry{
            node.data_offset +
            static_cast<std::uint32_t>(index * envelope_field::kSize)
        };
        /* resolveEnvelope looks the joint up by its address once the tree has
         * loaded, so the reference has to be the materialized joint. */
        host[index].joint = joint_chain(*reference(entry,
                                                  envelope_field::kJoint));
        host[index].weight = archive_.read_f32(entry,
                                               envelope_field::kWeight);
    }
    host[count].joint = nullptr;
    host[count].weight = 0.0F;
    return host;
}

HSD_EnvelopeDesc** HsdMaterializedArchive::envelope_array(HsdRuntimeNode node)
{
    const auto found = envelope_arrays_.find(node.data_offset);
    if (found != envelope_arrays_.end()) {
        return found->second;
    }

    /* A NULL-terminated array of envelope lists, one per matrix the PObj
     * addresses. */
    std::size_t count = 0;
    while (count < kEnvelopeLimit) {
        if (!reference(node, static_cast<std::uint32_t>(count * 4))
                 .has_value()) {
            break;
        }
        ++count;
    }
    if (count == kEnvelopeLimit) {
        throw HsdArchiveError("HSD envelope array has no terminator");
    }

    auto* const host = static_cast<HSD_EnvelopeDesc**>(allocate_bytes(
        sizeof(HSD_EnvelopeDesc*) * (count + 1),
        alignof(HSD_EnvelopeDesc*)));
    envelope_arrays_.emplace(node.data_offset, host);
    for (std::size_t index = 0; index < count; ++index) {
        host[index] = envelope_list(
            *reference(node, static_cast<std::uint32_t>(index * 4)));
    }
    host[count] = nullptr;
    return host;
}

HSD_TObjDesc* HsdMaterializedArchive::tobj_chain(HsdRuntimeNode node)
{
    HSD_TObjDesc* head = nullptr;
    HSD_TObjDesc* tail = nullptr;
    std::optional<HsdRuntimeNode> current = node;

    while (current.has_value()) {
        const auto found = tobjs_.find(current->data_offset);
        if (found != tobjs_.end()) {
            if (tail != nullptr) {
                tail->next = found->second;
            } else {
                head = found->second;
            }
            return head;
        }
        HSD_TObjDesc* const host = allocate<HSD_TObjDesc>();
        tobjs_.emplace(current->data_offset, host);
        stats_.tobj_descs += 1;

        if (const auto name = reference(*current, tobj_field::kClassName)) {
            host->class_name = payload_string(*name);
        }
        host->id = static_cast<GXTexMapID>(
            archive_.read_u32(*current, tobj_field::kId));
        host->src = static_cast<GXTexGenSrc>(
            archive_.read_u32(*current, tobj_field::kSrc));
        read_vec3(*current, tobj_field::kRotate, &host->rotate);
        read_vec3(*current, tobj_field::kScale, &host->scale);
        read_vec3(*current, tobj_field::kTranslate, &host->translate);
        host->wrap_s = static_cast<GXTexWrapMode>(
            archive_.read_u32(*current, tobj_field::kWrapS));
        host->wrap_t = static_cast<GXTexWrapMode>(
            archive_.read_u32(*current, tobj_field::kWrapT));
        host->repeat_s = std::to_integer<u8>(archive_.bytes_at(
            { current->data_offset + tobj_field::kRepeatS }, 1)[0]);
        host->repeat_t = std::to_integer<u8>(archive_.bytes_at(
            { current->data_offset + tobj_field::kRepeatT }, 1)[0]);
        host->blend_flags =
            archive_.read_u32(*current, tobj_field::kBlendFlags);
        host->blending = archive_.read_f32(*current, tobj_field::kBlending);
        host->magFilt = static_cast<GXTexFilter>(
            archive_.read_u32(*current, tobj_field::kMagFilt));
        if (const auto image = reference(*current, tobj_field::kImageDesc)) {
            host->imagedesc = image_desc(*image);
        }
        if (const auto tlut = reference(*current, tobj_field::kTlutDesc)) {
            host->tlutdesc = tlut_desc(*tlut);
        }
        if (const auto lod = reference(*current, tobj_field::kLod)) {
            host->lod = lod_desc(*lod);
        }
        if (const auto tev = reference(*current, tobj_field::kTev)) {
            host->tev = tev_desc(*tev);
        }

        if (tail != nullptr) {
            tail->next = host;
        } else {
            head = host;
        }
        tail = host;
        current = reference(*current, tobj_field::kNext);
    }
    return head;
}

HSD_MObjDesc* HsdMaterializedArchive::mobj_desc(HsdRuntimeNode node)
{
    const auto found = mobjs_.find(node.data_offset);
    if (found != mobjs_.end()) {
        return found->second;
    }
    HSD_MObjDesc* const host = allocate<HSD_MObjDesc>();
    mobjs_.emplace(node.data_offset, host);
    stats_.mobj_descs += 1;

    if (const auto name = reference(node, mobj_field::kClassName)) {
        host->class_name = payload_string(*name);
    }
    host->rendermode = archive_.read_u32(node, mobj_field::kRenderMode);
    if (const auto tex = reference(node, mobj_field::kTexDesc)) {
        host->texdesc = tobj_chain(*tex);
    }
    if (const auto mat = reference(node, mobj_field::kMaterial)) {
        host->mat = material(*mat);
    }
    if (reference(node, mobj_field::kRenderDesc).has_value()) {
        /* Only a custom material setup function knows this block's shape, and
         * none is ported, so its bytes cannot be decoded yet. */
        unsupported("a material render descriptor");
    }
    if (const auto pe = reference(node, mobj_field::kPEDesc)) {
        host->pedesc = pixel_engine_desc(*pe);
    }
    return host;
}

HSD_PObjDesc* HsdMaterializedArchive::pobj_chain(HsdRuntimeNode node)
{
    HSD_PObjDesc* head = nullptr;
    HSD_PObjDesc* tail = nullptr;
    std::optional<HsdRuntimeNode> current = node;

    while (current.has_value()) {
        const auto found = pobjs_.find(current->data_offset);
        if (found != pobjs_.end()) {
            if (tail != nullptr) {
                tail->next = found->second;
            } else {
                head = found->second;
            }
            return head;
        }
        HSD_PObjDesc* const host = allocate<HSD_PObjDesc>();
        pobjs_.emplace(current->data_offset, host);
        stats_.pobj_descs += 1;

        if (const auto name = reference(*current, pobj_field::kClassName)) {
            host->class_name = payload_string(*name);
        }
        host->flags = archive_.read_u16(*current, pobj_field::kFlags);
        host->n_display =
            archive_.read_u16(*current, pobj_field::kDisplayCount);
        if (const auto verts = reference(*current, pobj_field::kVerts)) {
            host->verts = vertex_descriptors(*verts);
        }
        if (const auto display = reference(*current, pobj_field::kDisplay)) {
            /* The block count gives an exact length, so a truncated file is
             * caught here instead of inside the display-list interpreter. */
            host->display = static_cast<u8*>(payload(
                *display, std::size_t{ host->n_display } * kDisplayListBlock));
        }
        switch (host->flags & 0x3000) {
        case POBJ_SKIN:
            /* The union holds a joint reference only for a rigid skin, and
             * PObjLoad leaves it alone either way. */
            if (const auto rigid = reference(*current, pobj_field::kUnion)) {
                host->u.joint = joint_chain(*rigid);
            }
            break;
        case POBJ_SHAPEANIM:
            if (const auto shapes = reference(*current, pobj_field::kUnion)) {
                host->u.shape_set = shape_set_desc(*shapes);
            }
            break;
        case POBJ_ENVELOPE:
            if (const auto envelopes =
                    reference(*current, pobj_field::kUnion)) {
                host->u.envelope_p = envelope_array(*envelopes);
            }
            break;
        default:
            unsupported("an unknown PObj type");
        }

        if (tail != nullptr) {
            tail->next = host;
        } else {
            head = host;
        }
        tail = host;
        current = reference(*current, pobj_field::kNext);
    }
    return head;
}

HSD_DObjDesc* HsdMaterializedArchive::dobj_chain(HsdRuntimeNode node)
{
    HSD_DObjDesc* head = nullptr;
    HSD_DObjDesc* tail = nullptr;
    std::optional<HsdRuntimeNode> current = node;

    while (current.has_value()) {
        const auto found = dobjs_.find(current->data_offset);
        if (found != dobjs_.end()) {
            if (tail != nullptr) {
                tail->next = found->second;
            } else {
                head = found->second;
            }
            return head;
        }
        HSD_DObjDesc* const host = allocate<HSD_DObjDesc>();
        dobjs_.emplace(current->data_offset, host);
        stats_.dobj_descs += 1;

        if (const auto name = reference(*current, dobj_field::kClassName)) {
            host->class_name = payload_string(*name);
        }
        if (const auto mobj = reference(*current, dobj_field::kMObjDesc)) {
            host->mobjdesc = mobj_desc(*mobj);
        }
        if (const auto pobj = reference(*current, dobj_field::kPObjDesc)) {
            host->pobjdesc = pobj_chain(*pobj);
        }

        if (tail != nullptr) {
            tail->next = host;
        } else {
            head = host;
        }
        tail = host;
        current = reference(*current, dobj_field::kNext);
    }
    return head;
}

HSD_RObjDesc* HsdMaterializedArchive::robj_chain(HsdRuntimeNode node)
{
    HSD_RObjDesc* head = nullptr;
    HSD_RObjDesc* tail = nullptr;
    std::optional<HsdRuntimeNode> current = node;

    while (current.has_value()) {
        const auto found = robjs_.find(current->data_offset);
        if (found != robjs_.end()) {
            if (tail != nullptr) {
                tail->next = found->second;
            } else {
                head = found->second;
            }
            return head;
        }
        HSD_RObjDesc* const host = allocate<HSD_RObjDesc>();
        robjs_.emplace(current->data_offset, host);
        stats_.robj_descs += 1;

        host->flags = archive_.read_u32(*current, robj_field::kFlags);
        switch (host->flags & ROBJ_TYPE_MASK) {
        case REFTYPE_JOBJ:
            /* The reference is resolved against the ID table after the tree
             * loads, keyed on the joint address. */
            if (const auto target = reference(*current, robj_field::kUnion)) {
                host->u.joint = joint_chain(*target);
            }
            break;
        case REFTYPE_LIMIT:
            host->u.limit = archive_.read_f32(*current, robj_field::kUnion);
            break;
        case REFTYPE_IKHINT: {
            if (const auto hint = reference(*current, robj_field::kUnion)) {
                auto* const desc = allocate<HSD_IKHintDesc>();
                desc->bone_length = archive_.read_f32(*hint, 0);
                desc->rotate_x = archive_.read_f32(*hint, 4);
                host->u.ik_hint = desc;
            }
            break;
        }
        case REFTYPE_EXP:
            /* The descriptor holds a console function address. */
            unsupported("an expression reference constraint");
        case REFTYPE_BYTECODE:
            if (const auto expression =
                    reference(*current, robj_field::kUnion))
            {
                auto* const desc = allocate<HSD_ByteCodeExpDesc>();
                /* HSD_ByteCodeEval reads the bytecode a byte at a time and
                 * builds its operands from the most significant byte, so the
                 * bytes stay as they are. */
                if (const auto code = reference(*expression, 0x0)) {
                    desc->bytecode = static_cast<u8*>(payload(*code, 1));
                }
                if (const auto list = reference(*expression, 0x4)) {
                    desc->rvalue = rvalue_list(*list);
                }
                host->u.bcexp = desc;
            }
            break;
        default:
            unsupported("an unknown reference constraint type");
        }

        if (tail != nullptr) {
            tail->next = host;
        } else {
            head = host;
        }
        tail = host;
        current = reference(*current, robj_field::kNext);
    }
    return head;
}

/* The joints an expression reads: flags and a joint per entry, ended by a NULL
 * joint, which loadRvalue walks to and HSD_RvalueResolveRefs resolves through
 * the ID table by joint address. */
HSD_RvalueList* HsdMaterializedArchive::rvalue_list(HsdRuntimeNode node)
{
    constexpr std::uint32_t kEntrySize = 8;
    constexpr std::uint32_t kMaxEntries = 0x100;
    std::uint32_t count = 0;
    while (reference({ node.data_offset + count * kEntrySize }, 0x4)) {
        if (++count == kMaxEntries) {
            throw HsdArchiveError("HSD rvalue list at data+0x" +
                                  hex_string(node.data_offset) +
                                  " has no end");
        }
    }
    auto* const list = static_cast<HSD_RvalueList*>(allocate_bytes(
        sizeof(HSD_RvalueList) * (count + 1), alignof(HSD_RvalueList)));
    for (std::uint32_t index = 0; index < count; ++index) {
        const HsdRuntimeNode entry{ node.data_offset + index * kEntrySize };
        list[index].flags = archive_.read_u32(entry, 0x0);
        list[index].joint = joint_chain(*reference(entry, 0x4));
    }
    list[count].flags = archive_.read_u32(
        { node.data_offset + count * kEntrySize }, 0x0);
    list[count].joint = nullptr;
    return list;
}

HSD_WObjDesc* HsdMaterializedArchive::world_desc(HsdRuntimeNode node)
{
    HSD_WObjDesc* const host = allocate<HSD_WObjDesc>();
    if (const auto name = reference(node, world_field::kClassName)) {
        host->class_name = payload_string(*name);
    }
    read_vec3(node, world_field::kPosition, &host->pos);
    if (const auto robj = reference(node, world_field::kRObjDesc)) {
        host->robjdesc = robj_chain(*robj);
    }
    return host;
}

HSD_CObjDesc* HsdMaterializedArchive::camera_desc(HsdRuntimeNode node)
{
    HSD_CObjDesc* const host = allocate<HSD_CObjDesc>();
    stats_.camera_descs += 1;

    HSD_CameraDescCommon& common = host->common;
    if (const auto name = reference(node, camera_field::kClassName)) {
        common.class_name = payload_string(*name);
    }
    common.flags = archive_.read_u16(node, camera_field::kFlags);
    common.projection_type =
        archive_.read_u16(node, camera_field::kProjectionType);
    common.viewport.xmin = static_cast<s16>(
        archive_.read_u16(node, camera_field::kViewport));
    common.viewport.xmax = static_cast<s16>(
        archive_.read_u16(node, camera_field::kViewport + 2));
    common.viewport.ymin = static_cast<s16>(
        archive_.read_u16(node, camera_field::kViewport + 4));
    common.viewport.ymax = static_cast<s16>(
        archive_.read_u16(node, camera_field::kViewport + 6));
    common.scissor.left = archive_.read_u16(node, camera_field::kScissor);
    common.scissor.right = archive_.read_u16(node, camera_field::kScissor + 2);
    common.scissor.top = archive_.read_u16(node, camera_field::kScissor + 4);
    common.scissor.bottom =
        archive_.read_u16(node, camera_field::kScissor + 6);
    if (const auto eye = reference(node, camera_field::kEyePosition)) {
        common.eyepos = world_desc(*eye);
    }
    if (const auto interest = reference(node, camera_field::kInterest)) {
        common.interest = world_desc(*interest);
    }
    common.roll = archive_.read_f32(node, camera_field::kRoll);
    if (const auto up = reference(node, camera_field::kUpVector)) {
        /* A bare Vec3 the camera loader reads, so unlike a GX payload it has
         * to hold host-order floats. */
        auto* const vector = allocate<Vec3>();
        read_vec3(*up, 0, vector);
        common.up_vector = vector;
    }
    common.nnear = archive_.read_f32(node, camera_field::kNear);
    common.ffar = archive_.read_f32(node, camera_field::kFar);

    switch (common.projection_type) {
    case PROJ_PERSPECTIVE:
        host->perspective.fov = archive_.read_f32(node, camera_field::kFov);
        host->perspective.aspect =
            archive_.read_f32(node, camera_field::kAspect);
        break;
    case PROJ_FRUSTUM:
    case PROJ_ORTHO:
        host->frustum.top = archive_.read_f32(node, camera_field::kTop);
        host->frustum.bottom = archive_.read_f32(node, camera_field::kBottom);
        host->frustum.left = archive_.read_f32(node, camera_field::kLeft);
        host->frustum.right = archive_.read_f32(node, camera_field::kRight);
        break;
    default:
        unsupported("an unknown camera projection type");
    }
    return host;
}

std::size_t HsdMaterializedArchive::scene_model_anim_count(
    std::string_view public_symbol, std::size_t model_index)
{
    const HsdRuntimeNode scene = archive_.public_root(public_symbol);
    const auto models = reference(scene, kSceneModels);
    if (!models.has_value() || model_index >= kSceneModelLimit) {
        return 0;
    }
    const auto model =
        reference(*models, static_cast<std::uint32_t>(model_index * 4));
    if (!model.has_value()) {
        return 0;
    }
    const auto table = reference(*model, kModelAnims);
    if (!table.has_value()) {
        return 0;
    }
    std::size_t count = 0;
    while (count < kSceneModelLimit) {
        if (!reference(*table, static_cast<std::uint32_t>(count * 4))
                 .has_value()) {
            return count;
        }
        ++count;
    }
    throw HsdArchiveError("HSD animation table has no terminator");
}

std::optional<HsdRuntimeNode>
HsdMaterializedArchive::scene_model_anim_entry(std::string_view public_symbol,
                                               std::size_t model_index,
                                               std::uint32_t table_offset,
                                               std::size_t anim_index)
{
    const HsdRuntimeNode scene = archive_.public_root(public_symbol);
    const auto models = reference(scene, kSceneModels);
    if (!models.has_value() || model_index >= kSceneModelLimit ||
        anim_index >= kSceneModelLimit)
    {
        return std::nullopt;
    }
    const auto model =
        reference(*models, static_cast<std::uint32_t>(model_index * 4));
    if (!model.has_value()) {
        return std::nullopt;
    }
    const auto table = reference(*model, table_offset);
    if (!table.has_value()) {
        return std::nullopt;
    }
    return reference(*table, static_cast<std::uint32_t>(anim_index * 4));
}

HSD_AnimJoint* HsdMaterializedArchive::scene_model_anim(
    std::string_view public_symbol, std::size_t model_index,
    std::size_t anim_index)
{
    const auto entry = scene_model_anim_entry(public_symbol, model_index,
                                              kModelAnims, anim_index);
    return entry.has_value() ? anim_joint_chain(*entry) : nullptr;
}

HSD_MatAnimJoint* HsdMaterializedArchive::scene_model_mat_anim(
    std::string_view public_symbol, std::size_t model_index,
    std::size_t anim_index)
{
    const auto entry = scene_model_anim_entry(public_symbol, model_index,
                                              kModelMatAnims, anim_index);
    return entry.has_value() ? mat_anim_joint_chain(*entry) : nullptr;
}

HSD_ShapeAnimJoint* HsdMaterializedArchive::scene_model_shape_anim(
    std::string_view public_symbol, std::size_t model_index,
    std::size_t anim_index)
{
    const auto entry = scene_model_anim_entry(public_symbol, model_index,
                                              kModelShapeAnims, anim_index);
    return entry.has_value() ? shape_anim_joint_chain(*entry) : nullptr;
}

HSD_CObjDesc* HsdMaterializedArchive::scene_camera(
    std::string_view public_symbol, std::size_t camera_index)
{
    const HsdRuntimeNode scene = archive_.public_root(public_symbol);
    const auto cameras = reference(scene, kSceneCameras);
    if (!cameras.has_value() || camera_index >= kSceneModelLimit) {
        return nullptr;
    }
    const HsdRuntimeNode entry{
        cameras->data_offset +
        static_cast<std::uint32_t>(camera_index * kSceneCameraDescSize)
    };
    const auto desc = reference(entry, kSceneCameraDesc);
    if (!desc.has_value()) {
        return nullptr;
    }
    return camera_desc(*desc);
}

HSD_CObjDesc* HsdMaterializedArchive::camera(std::string_view public_symbol)
{
    return camera_desc(archive_.public_root(public_symbol));
}

HSD_WObjAnim* HsdMaterializedArchive::world_anim(HsdRuntimeNode node)
{
    HSD_WObjAnim* const host = allocate<HSD_WObjAnim>();
    if (const auto aobj = reference(node, world_anim_field::kAObjDesc)) {
        host->aobjdesc = aobj_desc(*aobj);
    }
    if (const auto robj = reference(node, world_anim_field::kRObjAnim)) {
        host->robjanim = robj_anim_chain(*robj);
    }
    return host;
}

HSD_LightDesc* HsdMaterializedArchive::light_desc_chain(HsdRuntimeNode node)
{
    HSD_LightDesc* head = nullptr;
    HSD_LightDesc* tail = nullptr;
    std::optional<HsdRuntimeNode> current = node;
    std::size_t links = 0;

    while (current.has_value()) {
        if (++links > kLightLimit) {
            throw HsdArchiveError("HSD light chain does not end");
        }
        /* A light a table and an override both name is one descriptor, which
         * Ground_801C20E0 compares by address. */
        const auto found = light_descs_.find(current->data_offset);
        if (found != light_descs_.end()) {
            if (tail != nullptr) {
                tail->next = found->second;
            } else {
                head = found->second;
            }
            return head;
        }
        HSD_LightDesc* const host = allocate<HSD_LightDesc>();
        light_descs_.emplace(current->data_offset, host);
        stats_.light_descs += 1;

        if (const auto name = reference(*current, light_field::kClassName)) {
            host->class_name = payload_string(*name);
        }
        host->flags = archive_.read_u16(*current, light_field::kFlags);
        host->attnflags = archive_.read_u16(*current, light_field::kAttnFlags);
        read_color(*current, light_field::kColor, &host->color);

        /* LObjLoad reads position, interest and the parameter union only for
         * the light types that have them, so only those are followed.  An
         * infinite light's record still carries a parameter pointer on disk,
         * which the loader never reads. */
        const int type = host->flags & LOBJ_TYPE_MASK;
        if (type != LOBJ_AMBIENT) {
            if (const auto position =
                    reference(*current, light_field::kPosition)) {
                host->position = world_desc(*position);
            }
        }
        if (type == LOBJ_SPOT) {
            if (const auto interest =
                    reference(*current, light_field::kInterest)) {
                host->interest = world_desc(*interest);
            }
        }
        if (type == LOBJ_POINT || type == LOBJ_SPOT) {
            const auto parameters =
                reference(*current, light_field::kParameters);
            if (!parameters.has_value()) {
                /* LObjLoad dereferences the union unconditionally for these
                 * two types. */
                throw HsdArchiveError(
                    "HSD point or spot light has no parameters");
            }
            const bool raw_attenuation =
                type == LOBJ_POINT
                    ? (host->attnflags & LOBJ_LIGHT_ATTN) != 0
                    : host->attnflags != 0;
            if (raw_attenuation) {
                auto* const attn = allocate<HSD_LightAttn>();
                attn->a0 = archive_.read_f32(*parameters, light_attn_field::kA0);
                attn->a1 = archive_.read_f32(*parameters, light_attn_field::kA1);
                attn->a2 = archive_.read_f32(*parameters, light_attn_field::kA2);
                attn->k0 = archive_.read_f32(*parameters, light_attn_field::kK0);
                attn->k1 = archive_.read_f32(*parameters, light_attn_field::kK1);
                attn->k2 = archive_.read_f32(*parameters, light_attn_field::kK2);
                host->u.attn = attn;
            } else if (type == LOBJ_POINT) {
                auto* const point = allocate<HSD_LightPointDesc>();
                point->ref_br = archive_.read_f32(
                    *parameters, light_point_field::kRefBrightness);
                point->ref_dist = archive_.read_f32(
                    *parameters, light_point_field::kRefDistance);
                point->dist_func = archive_.read_u32(
                    *parameters, light_point_field::kDistanceFunc);
                host->u.point = point;
            } else {
                auto* const spot = allocate<HSD_LightSpotDesc>();
                spot->cutoff =
                    archive_.read_f32(*parameters, light_spot_field::kCutoff);
                spot->spot_func = archive_.read_u32(
                    *parameters, light_spot_field::kSpotFunc);
                spot->ref_br = archive_.read_f32(
                    *parameters, light_spot_field::kRefBrightness);
                spot->ref_dist = archive_.read_f32(
                    *parameters, light_spot_field::kRefDistance);
                spot->dist_func = archive_.read_u32(
                    *parameters, light_spot_field::kDistanceFunc);
                host->u.spot = spot;
            }
        }

        if (tail != nullptr) {
            tail->next = host;
        } else {
            head = host;
        }
        tail = host;
        current = reference(*current, light_field::kNext);
    }
    return head;
}

HSD_LightAnim* HsdMaterializedArchive::light_anim_chain(HsdRuntimeNode node)
{
    HSD_LightAnim* head = nullptr;
    HSD_LightAnim* tail = nullptr;
    std::optional<HsdRuntimeNode> current = node;
    std::size_t links = 0;

    while (current.has_value()) {
        if (++links > kLightLimit) {
            throw HsdArchiveError("HSD light animation chain does not end");
        }
        HSD_LightAnim* const host = allocate<HSD_LightAnim>();
        if (const auto aobj = reference(*current, light_anim_field::kAObjDesc)) {
            host->aobjdesc = aobj_desc(*aobj);
        }
        if (const auto position =
                reference(*current, light_anim_field::kPositionAnim)) {
            host->position_anim = world_anim(*position);
        }
        if (const auto interest =
                reference(*current, light_anim_field::kInterestAnim)) {
            host->interest_anim = world_anim(*interest);
        }
        /* An animation that follows a joint names it by its ID-table key.
         * The console loads a joint that is not in the table yet from the
         * address the key is; the host cannot, and a light is loaded apart
         * from the joint tree it would follow (TyLight.dat's trophy lights
         * follow a spline joint this way). */
        const auto follows_joint = [](const HSD_AObjDesc* aobj) {
            return aobj != nullptr && aobj->obj_id != nullptr;
        };
        if (follows_joint(host->aobjdesc) ||
            (host->position_anim != nullptr &&
             follows_joint(host->position_anim->aobjdesc)) ||
            (host->interest_anim != nullptr &&
             follows_joint(host->interest_anim->aobjdesc)))
        {
            // unsupported("a light animation that follows a joint");
        }
        if (tail != nullptr) {
            tail->next = host;
        } else {
            head = host;
        }
        tail = host;
        current = reference(*current, light_anim_field::kNext);
    }
    return head;
}

MaterializedLightList**
HsdMaterializedArchive::scene_lights(std::string_view public_symbol)
{
    return light_list_table(archive_.public_root(public_symbol));
}

MaterializedLightList**
HsdMaterializedArchive::light_list_table(HsdRuntimeNode table)
{
    std::size_t count = 0;
    while (reference(table, static_cast<std::uint32_t>(count * 4)).has_value()) {
        if (++count >= kLightLimit) {
            throw HsdArchiveError("HSD light table has no terminator");
        }
    }

    /* The arena is zeroed, so the slot after the last entry is already the
     * NULL that lb_80011AC4 stops at. */
    auto** const lists = static_cast<MaterializedLightList**>(allocate_bytes(
        sizeof(MaterializedLightList*) * (count + 1),
        alignof(MaterializedLightList*)));
    for (std::size_t index = 0; index < count; ++index) {
        const HsdRuntimeNode entry =
            *reference(table, static_cast<std::uint32_t>(index * 4));
        auto* const list = allocate<MaterializedLightList>();
        if (const auto desc = reference(entry, light_list_field::kDesc)) {
            list->desc = light_desc_chain(*desc);
        }
        if (const auto anims = reference(entry, light_list_field::kAnims)) {
            std::size_t anim_count = 0;
            while (reference(*anims, static_cast<std::uint32_t>(anim_count * 4))
                       .has_value())
            {
                if (++anim_count >= kLightLimit) {
                    throw HsdArchiveError(
                        "HSD light animation table has no terminator");
                }
            }
            auto** const anim_table =
                static_cast<HSD_LightAnim**>(allocate_bytes(
                    sizeof(HSD_LightAnim*) * (anim_count + 1),
                    alignof(HSD_LightAnim*)));
            for (std::size_t anim = 0; anim < anim_count; ++anim) {
                anim_table[anim] = light_anim_chain(
                    *reference(*anims, static_cast<std::uint32_t>(anim * 4)));
            }
            list->anims = anim_table;
        }
        lists[index] = list;
    }
    return lists;
}

HSD_FogAdjDesc* HsdMaterializedArchive::fog_adj_desc(HsdRuntimeNode node)
{
    HSD_FogAdjDesc* const host = allocate<HSD_FogAdjDesc>();
    host->center = archive_.read_u16(node, fog_adj_field::kCenter);
    host->width = archive_.read_u16(node, fog_adj_field::kWidth);
    /* HSD_FogAdjInit copies this into the object, so like a joint matrix it
     * holds host-order floats rather than the file's bytes. */
    for (std::uint32_t row = 0; row < 4; ++row) {
        for (std::uint32_t column = 0; column < 4; ++column) {
            host->mtx[row][column] = archive_.read_f32(
                node, fog_adj_field::kMatrix + (row * 4 + column) * 4);
        }
    }
    return host;
}

HSD_FogDesc* HsdMaterializedArchive::fog(std::string_view public_symbol)
{
    return fog_desc(archive_.public_root(public_symbol));
}

HSD_FogDesc* HsdMaterializedArchive::fog_desc(HsdRuntimeNode node)
{
    HSD_FogDesc* const host = allocate<HSD_FogDesc>();
    stats_.fog_descs += 1;

    host->type = archive_.read_u32(node, fog_field::kType);
    host->start = archive_.read_f32(node, fog_field::kStart);
    host->end = archive_.read_f32(node, fog_field::kEnd);
    read_color(node, fog_field::kColor, &host->color);
    if (const auto adj = reference(node, fog_field::kAdjDesc)) {
        host->fogadjdesc = fog_adj_desc(*adj);
    }
    return host;
}

MaterializedCharacterSelectData*
HsdMaterializedArchive::character_select_data(std::string_view public_symbol)
{
    const HsdRuntimeNode table = archive_.public_root(public_symbol);
    auto* const host = allocate<MaterializedCharacterSelectData>();
    if (const auto node = reference(table, 0x00)) {
        host->cam = camera_desc(*node);
    }
    if (const auto node = reference(table, 0x04)) {
        host->light0 = light_desc_chain(*node);
    }
    if (const auto node = reference(table, 0x08)) {
        host->light1 = light_desc_chain(*node);
    }
    if (const auto node = reference(table, 0x0C)) {
        host->fog = fog_desc(*node);
    }
    for (std::size_t index = 0; index < std::size(host->models); ++index) {
        const HsdRuntimeNode model{ table.data_offset +
                                    static_cast<std::uint32_t>(0x10 + index * 0x10) };
        if (const auto node = reference(model, 0x00)) {
            host->models[index].joint = joint_chain(*node);
        }
        if (const auto node = reference(model, 0x04)) {
            host->models[index].animjoint = anim_joint_chain(*node);
        }
        if (const auto node = reference(model, 0x08)) {
            host->models[index].matanim_joint = mat_anim_joint_chain(*node);
        }
        if (const auto node = reference(model, 0x0C)) {
            host->models[index].shapeanim_joint = shape_anim_joint_chain(*node);
        }
    }
    return host;
}

MaterializedStageSelectData*
HsdMaterializedArchive::stage_select_data(std::string_view public_symbol)
{
    const HsdRuntimeNode table = archive_.public_root(public_symbol);
    auto* const host = allocate<MaterializedStageSelectData>();
    if (const auto node = reference(table, 0x00)) host->cam = camera_desc(*node);
    if (const auto node = reference(table, 0x04)) host->light0 = light_desc_chain(*node);
    if (const auto node = reference(table, 0x08)) host->light1 = light_desc_chain(*node);
    if (const auto node = reference(table, 0x0C)) host->fog = fog_desc(*node);
    auto materialize_model = [&](MaterializedStaticModel* model,
                                 std::uint32_t offset) {
        const HsdRuntimeNode disk{ table.data_offset + offset };
        if (const auto node = reference(disk, 0)) model->joint = joint_chain(*node);
        if (const auto node = reference(disk, 4)) model->animjoint = anim_joint_chain(*node);
        if (const auto node = reference(disk, 8)) model->matanim_joint = mat_anim_joint_chain(*node);
        if (const auto node = reference(disk, 12)) model->shapeanim_joint = shape_anim_joint_chain(*node);
    };
    for (std::size_t index = 0; index < std::size(host->models); ++index) {
        materialize_model(&host->models[index],
                          static_cast<std::uint32_t>(0x10 + index * 0x10));
    }
    materialize_model(&host->random_stage, 0xC0);
    return host;
}

/* A command stream: the words lbcommand.c and the item, fighter and color
 * overlay tables interpret, which the console reads through bit-fields over
 * big-endian words.  The host converts the words in place in the payload to
 * native order, which host_command_layout.h lays out, and hands back the
 * address of the first.
 *
 * A stream records no length.  It is converted up to the first address a
 * relocation targets or a public symbol names, because what starts there is
 * something else the archive points at; a stream that runs on past such an
 * address is the target of a goto, and is converted from there when that goto
 * is.  A relocated word in a stream is the operand of a subroutine (5) or a
 * goto (7): it becomes the distance from itself to its target, as
 * Command_05 and Command_07 read it on the host, and the target's stream is
 * converted in turn.  Any other pointer in a stream is refused. */
void* HsdMaterializedArchive::translator_command_stream(
    std::uint32_t data_offset)
{
    return command_stream({ data_offset });
}

HSD_Joint* HsdMaterializedArchive::translator_joint(std::uint32_t data_offset)
{
    return joint_chain({ data_offset });
}

HSD_AnimJoint*
HsdMaterializedArchive::translator_anim_joint(std::uint32_t data_offset)
{
    return anim_joint_chain({ data_offset });
}

HSD_MatAnimJoint*
HsdMaterializedArchive::translator_mat_anim_joint(std::uint32_t data_offset)
{
    return mat_anim_joint_chain({ data_offset });
}

HSD_ShapeAnimJoint*
HsdMaterializedArchive::translator_shape_anim_joint(std::uint32_t data_offset)
{
    return shape_anim_joint_chain({ data_offset });
}

std::uint32_t
HsdMaterializedArchive::translator_extent(std::uint32_t data_offset)
{
    index_stream_boundaries();
    if (data_offset >= payload_size_) {
        throw HsdArchiveError("HSD block at data+0x" +
                              hex_string(data_offset) +
                              " is outside the data section");
    }
    const auto next = std::upper_bound(stream_boundaries_.begin(),
                                       stream_boundaries_.end(), data_offset);
    const std::uint32_t limit = next == stream_boundaries_.end()
                                    ? static_cast<std::uint32_t>(payload_size_)
                                    : *next;
    return limit - data_offset;
}

HSD_CObjDesc* HsdMaterializedArchive::translator_camera(std::uint32_t data_offset)
{
    return camera_desc({ data_offset });
}

HSD_FogDesc* HsdMaterializedArchive::translator_fog(std::uint32_t data_offset)
{
    return fog_desc({ data_offset });
}

MaterializedLightList**
HsdMaterializedArchive::translator_light_lists(std::uint32_t data_offset)
{
    return light_list_table({ data_offset });
}

HSD_LightDesc* HsdMaterializedArchive::translator_light_built_at(
    std::uint32_t data_offset) const
{
    const auto found = light_descs_.find(data_offset);
    return found == light_descs_.end() ? nullptr : found->second;
}

HSD_MObjDesc* HsdMaterializedArchive::translator_mobj(std::uint32_t data_offset)
{
    return mobj_desc({ data_offset });
}

HSD_Spline* HsdMaterializedArchive::translator_spline(std::uint32_t data_offset)
{
    return spline_desc({ data_offset });
}

void HsdMaterializedArchive::index_stream_boundaries()
{
    if (streams_indexed_) {
        return;
    }
    for (const HsdRuntimeReference& reference :
         archive_.internal_references())
    {
        relocated_fields_.emplace(reference.field_offset,
                                  reference.target.data_offset);
        stream_boundaries_.push_back(reference.target.data_offset);
    }
    for (const HsdPublicSymbol& symbol :
         archive_.disk_view().public_symbols())
    {
        stream_boundaries_.push_back(symbol.data_offset);
    }
    std::sort(stream_boundaries_.begin(), stream_boundaries_.end());
    stream_boundaries_.erase(
        std::unique(stream_boundaries_.begin(), stream_boundaries_.end()),
        stream_boundaries_.end());
    converted_words_.assign(payload_size_ / 4, false);
    streams_indexed_ = true;
}

void* HsdMaterializedArchive::command_stream(HsdRuntimeNode start)
{
    constexpr std::uint32_t kWord = 4;
    constexpr std::uint32_t kOpcodeShift = 26;
    constexpr std::uint32_t kSubroutine = 5;
    constexpr std::uint32_t kGoto = 7;
    static_assert(std::endian::native == std::endian::little,
                  "the host command layouts assume a little-endian target");

    index_stream_boundaries();
    std::vector<std::uint32_t> pending{ start.data_offset };
    while (!pending.empty()) {
        const std::uint32_t begin = pending.back();
        pending.pop_back();
        if (begin % kWord != 0 || begin >= payload_size_ / kWord * kWord) {
            throw HsdArchiveError("HSD command stream at data+0x" +
                                  hex_string(begin) +
                                  " is not a word of the data section");
        }
        const auto next = std::upper_bound(stream_boundaries_.begin(),
                                           stream_boundaries_.end(), begin);
        const std::uint32_t limit =
            next == stream_boundaries_.end()
                ? static_cast<std::uint32_t>(payload_size_)
                : *next;
        for (std::uint32_t at = begin; at + kWord <= limit; at += kWord) {
            /* Converted already, and so is the rest up to the boundary. */
            if (converted_words_[at / kWord]) {
                break;
            }
            std::byte* const word = payload_ + at;
            const std::uint32_t value =
                (std::to_integer<std::uint32_t>(word[0]) << 24U) |
                (std::to_integer<std::uint32_t>(word[1]) << 16U) |
                (std::to_integer<std::uint32_t>(word[2]) << 8U) |
                std::to_integer<std::uint32_t>(word[3]);
            const auto relocated = relocated_fields_.find(at);
            if (relocated == relocated_fields_.end()) {
                std::memcpy(word, &value, sizeof(value));
            } else {
                std::uint32_t command = 0;
                if (at != begin) {
                    std::memcpy(&command, word - kWord, sizeof(command));
                }
                const std::uint32_t opcode = command >> kOpcodeShift;
                if (at == begin || (opcode != kSubroutine && opcode != kGoto)) {
                    throw HsdArchiveError(
                        "HSD command stream at data+0x" + hex_string(begin) +
                        " has a pointer at data+0x" + hex_string(at) +
                        " that does not follow a subroutine or a goto");
                }
                const std::uint32_t target = relocated->second;
                const auto distance = static_cast<std::int32_t>(
                    static_cast<std::int64_t>(target) -
                    static_cast<std::int64_t>(at));
                std::memcpy(word, &distance, sizeof(distance));
                pending.push_back(target);
            }
            converted_words_[at / kWord] = true;
        }
    }
    return payload_ + start.data_offset;
}

/* A spline a joint follows (JOBJ_SPLINE): its type, control point count and
 * tension, the control points, the arc length of each segment and, for the
 * curved types, a quartic per segment whose square root splArcLength*
 * integrates.  spline.c reads `numcv` control points for a polyline, one
 * start point plus three per segment for a Bezier, and two more than `numcv`
 * for the B-spline and cardinal forms, whose segments read four points
 * starting at their index. */
HSD_Spline* HsdMaterializedArchive::spline_desc(HsdRuntimeNode node)
{
    constexpr std::int32_t kMaxControlPoints = 0x4000;
    auto* const host = allocate<HSD_Spline>();
    const std::uint32_t type = archive_.read_u32(node, 0x00) >> 24U;
    const auto count =
        static_cast<std::int16_t>(archive_.read_u16(node, 0x02));
    if (type > 3) {
        throw HsdArchiveError("HSD spline at data+0x" +
                              hex_string(node.data_offset) + " has type " +
                              std::to_string(type));
    }
    if (count < 1 || count > kMaxControlPoints) {
        throw HsdArchiveError("HSD spline at data+0x" +
                              hex_string(node.data_offset) + " has " +
                              std::to_string(count) + " control points");
    }
    host->type = static_cast<u8>(type);
    host->numcv = count;
    host->tension = archive_.read_f32(node, 0x04);
    host->totalLength = archive_.read_f32(node, 0x0C);

    const auto segments = static_cast<std::uint32_t>(count - 1);
    const std::uint32_t control_points =
        type == 0 ? static_cast<std::uint32_t>(count)
        : type == 1 ? segments * 3 + 1
                    : static_cast<std::uint32_t>(count) + 2;
    if (const auto points = reference(node, 0x08)) {
        auto* const cv = static_cast<Vec3*>(
            allocate_bytes(sizeof(Vec3) * control_points, alignof(Vec3)));
        for (std::uint32_t index = 0; index < control_points; ++index) {
            read_vec3(*points, index * 12, &cv[index]);
        }
        host->cv = cv;
    }
    if (const auto lengths = reference(node, 0x10)) {
        auto* const segment_length = static_cast<f32*>(allocate_bytes(
            sizeof(f32) * static_cast<std::size_t>(count), alignof(f32)));
        for (std::int32_t index = 0; index < count; ++index) {
            segment_length[index] = archive_.read_f32(
                *lengths, static_cast<std::uint32_t>(index) * 4);
        }
        host->segLength = segment_length;
    }
    if (const auto polynomials = reference(node, 0x14)) {
        const std::size_t rows = std::max<std::uint32_t>(segments, 1);
        auto* const poly = static_cast<f32 (*)[5]>(
            allocate_bytes(sizeof(f32[5]) * rows, alignof(f32)));
        for (std::uint32_t row = 0; row < segments; ++row) {
            for (std::uint32_t column = 0; column < 5; ++column) {
                poly[row][column] =
                    archive_.read_f32(*polynomials, (row * 5 + column) * 4);
            }
        }
        host->segPoly = poly;
    }
    return host;
}

namespace {

/* Bank-relative offsets are not archive relocations: psInitDataBankLocate
 * adds the bank's address to them.  The node they name is checked against the
 * data section when it is read. */
HsdRuntimeNode bank_relative(HsdRuntimeNode bank, std::uint32_t offset)
{
    if (offset > std::numeric_limits<std::uint32_t>::max() - bank.data_offset) {
        throw HsdArchiveError("particle bank offset 0x" + hex_string(offset) +
                              " runs past the address space");
    }
    return { bank.data_offset + offset };
}

/* List IDs are the effect numbers the game spawns by (Fox's start at 3000,
 * Roy's through Kirby's copies near 49000); the host sizes its table by the
 * largest, and refuses anything far beyond what the disc holds. */
constexpr std::int32_t kMaxParticleListEnd = 0x10000;
constexpr std::int32_t kMaxParticleTextureGroups = 0x1000;

} // namespace

/* A command bank: a version word, the first list ID, the list count and one
 * bank-relative offset per list.  Every bank on disc is version 0x42.  Each
 * list's header is converted, its kind gets the bits psInitDataBankLocate
 * sets, and its command bytes stay verbatim: the interpreter reads them byte
 * by byte, big-endian. */
MeleeHostParticleCmdBank*
HsdMaterializedArchive::command_bank_at(HsdRuntimeNode bank)
{
    const std::uint16_t version = archive_.read_u16(bank, 0);
    if (version < 0x40 || version > 0x43) {
        throw HsdArchiveError("particle command bank version 0x" +
                              hex_string(version) + " is not ported");
    }
    const auto first = static_cast<std::int32_t>(archive_.read_u32(bank, 4));
    const auto count = static_cast<std::int32_t>(archive_.read_u32(bank, 8));
    if (first < 0 || count < 0 || count > kMaxParticleListEnd ||
        first > kMaxParticleListEnd - count)
    {
        throw HsdArchiveError("particle command bank lists " +
                              std::to_string(first) + " to " +
                              std::to_string(first + count) +
                              " are out of range");
    }

    auto* const host = allocate<MeleeHostParticleCmdBank>();
    host->magic = MELEE_HOST_PARTICLE_CMD_BANK_MAGIC;
    host->list_end = first + count;
    /* Indexed by list ID, so a bank that starts at 49000 needs 49000 empty
     * slots first: far more than the arena budgets for a small archive. */
    const auto slots = static_cast<std::size_t>(std::max(host->list_end, 1));
    host->lists = static_cast<HSD_PSCmdList**>(allocate_outside_arena(
        sizeof(HSD_PSCmdList*) * slots, alignof(HSD_PSCmdList*)));
    for (std::int32_t index = 0; index < count; ++index) {
        const std::uint32_t offset = archive_.read_u32(
            bank, 12 + static_cast<std::uint32_t>(index) * 4);
        if (offset == 0) {
            continue;
        }
        const HsdRuntimeNode node = bank_relative(bank, offset);
        auto* const list = allocate<HSD_PSCmdList>();
        list->type = archive_.read_u16(node, 0x00);
        list->texGroup = archive_.read_u16(node, 0x02);
        list->genLife = archive_.read_u16(node, 0x04);
        list->life = archive_.read_u16(node, 0x06);
        list->kind = (archive_.read_u32(node, 0x08) & 0xF1FFFFFFU) | 0x08000000U;
        list->grav = archive_.read_f32(node, 0x0C);
        list->fric = archive_.read_f32(node, 0x10);
        list->vx = archive_.read_f32(node, 0x14);
        list->vy = archive_.read_f32(node, 0x18);
        list->vz = archive_.read_f32(node, 0x1C);
        list->radius = archive_.read_f32(node, 0x20);
        list->angle = archive_.read_f32(node, 0x24);
        list->random = archive_.read_f32(node, 0x28);
        list->size = archive_.read_f32(node, 0x2C);
        list->param1 = archive_.read_f32(node, 0x30);
        list->param2 = archive_.read_f32(node, 0x34);
        list->param3 = archive_.read_f32(node, 0x38);
        list->cmdList =
            static_cast<u8*>(payload(bank_relative(node, 0x3C), 1));
        host->lists[first + index] = list;
    }
    return host;
}

/* A texture bank: a group count and one bank-relative offset per group.  A
 * group's scalars are converted and its table, which lists the group's images
 * and then its palettes, becomes addresses into the verbatim payload, where
 * the GX decoders read texels and palettes big-endian.  Only the palette
 * formats carry palettes: one when bit 0 of palflag is set, else `palnum`, or
 * one per image when that is zero, as psInitDataBankLocate counts them. */
MeleeHostParticleTexBank*
HsdMaterializedArchive::texture_bank_at(HsdRuntimeNode bank)
{
    const auto count = static_cast<std::int32_t>(archive_.read_u32(bank, 0));
    if (count < 0 || count > kMaxParticleTextureGroups) {
        throw HsdArchiveError("particle texture bank has " +
                              std::to_string(count) + " groups");
    }
    auto* const host = allocate<MeleeHostParticleTexBank>();
    host->magic = MELEE_HOST_PARTICLE_TEX_BANK_MAGIC;
    host->group_count = count;
    const auto groups = static_cast<std::size_t>(std::max(count, 1));
    host->groups = static_cast<HSD_PSTexGroup**>(allocate_bytes(
        sizeof(HSD_PSTexGroup*) * groups, alignof(HSD_PSTexGroup*)));
    for (std::size_t group = 0; group < groups; ++group) {
        host->groups[group] = nullptr;
    }
    for (std::int32_t index = 0; index < count; ++index) {
        const std::uint32_t offset = archive_.read_u32(
            bank, 4 + static_cast<std::uint32_t>(index) * 4);
        if (offset == 0) {
            continue;
        }
        const HsdRuntimeNode node = bank_relative(bank, offset);
        const std::uint32_t images = archive_.read_u32(node, 0x00);
        const std::uint32_t format = archive_.read_u32(node, 0x04);
        const std::uint16_t palette_count = archive_.read_u16(node, 0x14);
        const std::uint16_t palette_flag = archive_.read_u16(node, 0x16);
        std::uint32_t palettes = 0;
        if (format == GX_TF_C4 || format == GX_TF_C8 || format == GX_TF_C14X2) {
            palettes = (palette_flag & 1U) != 0U ? 1U
                       : palette_count != 0U     ? palette_count
                                                 : images;
        }
        if (images > 0x10000 || palettes > 0x10000) {
            throw HsdArchiveError("particle texture group at data+0x" +
                                  hex_string(node.data_offset) +
                                  " lists too many images");
        }
        const std::size_t entries =
            std::max<std::size_t>(images + palettes, 1);
        auto* const group = static_cast<HSD_PSTexGroup*>(allocate_bytes(
            offsetof(HSD_PSTexGroup, texTable) + sizeof(u8*) * entries,
            alignof(HSD_PSTexGroup)));
        group->num = images;
        group->fmt = format;
        group->tlutfmt = archive_.read_u32(node, 0x08);
        group->width = archive_.read_u32(node, 0x0C);
        group->height = archive_.read_u32(node, 0x10);
        group->palnum = palette_count;
        group->palflag = palette_flag;
        /* An entry the bank does not cover stays NULL.  EfKbSs.dat's first
         * group is C8 with no palette count and no palette flag, so its
         * palette would be the word after its image offset, but that word
         * holds 0x80A8812A, which psInitDataBankLocate would turn into an
         * address nowhere near the bank. */
        u8** const table = group->texTable;
        for (std::size_t entry = 0; entry < entries; ++entry) {
            const std::uint32_t texel_offset = entry < images + palettes
                ? archive_.read_u32(node, 0x18 + static_cast<std::uint32_t>(entry) * 4)
                : 0;
            const bool inside =
                texel_offset != 0 &&
                texel_offset <= std::numeric_limits<std::uint32_t>::max() -
                                    bank.data_offset &&
                std::size_t{ bank.data_offset } + texel_offset < payload_size_;
            table[entry] = inside
                ? static_cast<u8*>(payload(bank_relative(bank, texel_offset), 1))
                : nullptr;
        }
        host->groups[index] = group;
    }
    return host;
}

MeleeHostParticleCmdBank*
HsdMaterializedArchive::particle_command_bank(std::string_view public_symbol)
{
    return command_bank_at(archive_.public_root(public_symbol));
}

MeleeHostParticleTexBank*
HsdMaterializedArchive::particle_texture_bank(std::string_view public_symbol)
{
    return texture_bank_at(archive_.public_root(public_symbol));
}

/* The table keeps no count of its effects.  They run from +0x8 in 0x14-byte
 * records until the first address the table, its banks or an earlier record
 * point at, which follows the table in every archive on disc, or until a
 * record holds a model field that is neither relocated nor NULL. */
MaterializedEffectTable*
HsdMaterializedArchive::effect_data_table(std::string_view public_symbol)
{
    constexpr std::uint32_t kFirstEffect = 0x08;
    constexpr std::uint32_t kEffectSize = 0x14;
    const HsdRuntimeNode table = archive_.public_root(public_symbol);
    const auto commands = reference(table, 0x00);
    const auto textures = reference(table, 0x04);
    if (commands.has_value() != textures.has_value()) {
        throw HsdArchiveError("the effect table has only one particle bank");
    }

    std::uint64_t end = payload_size_;
    if (commands.has_value()) {
        end = std::min<std::uint64_t>(
            end, std::min(commands->data_offset, textures->data_offset));
    }
    std::size_t count = 0;
    for (std::uint64_t at = std::uint64_t{ table.data_offset } + kFirstEffect;
         at + kEffectSize <= end; at += kEffectSize)
    {
        const HsdRuntimeNode record{ static_cast<std::uint32_t>(at) };
        bool models = true;
        for (std::uint32_t field = 0x04; field < kEffectSize; field += 4) {
            if (archive_.has_reference_at(record, field)) {
                end = std::min<std::uint64_t>(
                    end, archive_.reference_at(record, field).data_offset);
            } else if (archive_.read_u32(record, field) != 0) {
                models = false;
                break;
            }
        }
        if (!models || at + kEffectSize > end) {
            break;
        }
        ++count;
    }

    auto* const host = static_cast<MaterializedEffectTable*>(allocate_bytes(
        offsetof(MaterializedEffectTable, effects) +
            sizeof(MaterializedEffectDesc) * std::max<std::size_t>(count, 1),
        alignof(MaterializedEffectTable)));
    host->command_bank = commands.has_value() ? command_bank_at(*commands) : nullptr;
    host->texture_bank = textures.has_value() ? texture_bank_at(*textures) : nullptr;
    const MaterializedEffectDesc* const effects = host->effects;
    for (std::size_t index = 0; index < count; ++index) {
        const HsdRuntimeNode record{ table.data_offset + kFirstEffect +
                                     static_cast<std::uint32_t>(index) *
                                         kEffectSize };
        auto* const effect = const_cast<MaterializedEffectDesc*>(effects + index);
        effect->lifetime = archive_.read_f32(record, 0x00);
        effect->model = MaterializedStaticModel{};
        if (const auto node = reference(record, 0x04)) {
            effect->model.joint = joint_chain(*node);
        }
        if (const auto node = reference(record, 0x08)) {
            effect->model.animjoint = anim_joint_chain(*node);
        }
        if (const auto node = reference(record, 0x0C)) {
            effect->model.matanim_joint = mat_anim_joint_chain(*node);
        }
        if (const auto node = reference(record, 0x10)) {
            effect->model.shapeanim_joint = shape_anim_joint_chain(*node);
        }
    }
    return host;
}

MaterializedRumbleEntry*
HsdMaterializedArchive::rumble_table(std::string_view public_symbol)
{
    constexpr std::uint32_t kEntrySize = 8;
    const HsdRuntimeNode table = archive_.public_root(public_symbol);
    const std::size_t data_size = archive_.disk_view().data().size();

    /* A record whose command pointer is not relocated ends the table.  In
     * LbRb.dat all forty records are relocated and the table runs to the end
     * of the data section. */
    std::size_t count = 0;
    while (table.data_offset + (count + 1) * kEntrySize <= data_size &&
           archive_.has_reference_at(
               table, static_cast<std::uint32_t>(count * kEntrySize)))
    {
        ++count;
    }
    if (count == 0) {
        throw HsdArchiveError("rumble table has no entries");
    }

    auto* const entries = static_cast<MaterializedRumbleEntry*>(
        allocate_bytes(sizeof(MaterializedRumbleEntry) * count,
                       alignof(MaterializedRumbleEntry)));
    for (std::size_t index = 0; index < count; ++index) {
        const HsdRuntimeNode entry{ table.data_offset +
                                    static_cast<std::uint32_t>(index *
                                                               kEntrySize) };
        /* The command list is read byte by byte by the rumble interpreter,
         * so it stays verbatim and has no declared length. */
        entries[index].commands = payload(*reference(entry, 0), 1);
        const auto bytes = archive_.bytes_at({ entry.data_offset + 4 }, 2);
        entries[index].priority = std::to_integer<u8>(bytes[0]);
        entries[index].unk5 = std::to_integer<u8>(bytes[1]);
    }
    return entries;
}

u8** HsdMaterializedArchive::sis_table(std::string_view public_symbol)
{
    constexpr std::uint32_t kEntrySize = 4;
    const HsdRuntimeNode table = archive_.public_root(public_symbol);
    const std::size_t data_size = archive_.disk_view().data().size();

    /* The table has no count; sislib.c indexes it with the number of the
     * string it wants.  Every relocation in a text archive is one of its
     * entries, so the run of relocated fields is the whole table. */
    std::size_t count = 0;
    while (table.data_offset + (count + 1) * kEntrySize <= data_size &&
           archive_.has_reference_at(
               table, static_cast<std::uint32_t>(count * kEntrySize)))
    {
        ++count;
    }
    if (count == 0) {
        throw HsdArchiveError("text table has no entries");
    }

    auto** const entries = static_cast<u8**>(
        allocate_bytes(sizeof(u8*) * count, alignof(u8*)));
    for (std::size_t index = 0; index < count; ++index) {
        const HsdRuntimeNode target = *reference(
            table, static_cast<std::uint32_t>(index * kEntrySize));
        if (target.data_offset == data_size) {
            /* Four text archives point their first entry at the end of the
             * data.  The console reads the relocation table there, whose first
             * field is this table's own offset, zero, so the host gives the
             * entry zero bytes of its own. */
            auto* const zeros =
                static_cast<u8*>(allocate_bytes(kEntrySize, 1));
            std::memset(zeros, 0, kEntrySize);
            entries[index] = zeros;
            continue;
        }
        entries[index] = static_cast<u8*>(payload(target, 1));
    }
    return entries;
}

HSD_ImageDesc*
HsdMaterializedArchive::image_desc(std::string_view public_symbol)
{
    return image_desc(archive_.public_root(public_symbol));
}

HSD_SObjDesc* HsdMaterializedArchive::sobj_desc(std::string_view public_symbol)
{
    const HsdRuntimeNode node = archive_.public_root(public_symbol);
    HSD_SObjDesc* const host = allocate<HSD_SObjDesc>();
    stats_.sobj_descs += 1;

    if (const auto image = reference(node, sobj_field::kImage)) {
        host->image = image_desc(*image);
    }
    if (const auto tlut = reference(node, sobj_field::kTlut)) {
        host->tlut = reinterpret_cast<HSD_Tlut*>(tlut_desc(*tlut));
    }
    return host;
}

HSD_FObjDesc* HsdMaterializedArchive::fobj_chain(HsdRuntimeNode node)
{
    HSD_FObjDesc* head = nullptr;
    HSD_FObjDesc* tail = nullptr;
    std::optional<HsdRuntimeNode> current = node;

    while (current.has_value()) {
        HSD_FObjDesc* const host = allocate<HSD_FObjDesc>();
        stats_.fobj_descs += 1;

        host->length = archive_.read_u32(*current, fobj_field::kLength);
        host->startframe =
            archive_.read_f32(*current, fobj_field::kStartFrame);
        host->type = std::to_integer<u8>(archive_.bytes_at(
            { current->data_offset + fobj_field::kType }, 1)[0]);
        host->frac_value = std::to_integer<u8>(archive_.bytes_at(
            { current->data_offset + fobj_field::kFracValue }, 1)[0]);
        host->frac_slope = std::to_integer<u8>(archive_.bytes_at(
            { current->data_offset + fobj_field::kFracSlope }, 1)[0]);
        if (const auto data = reference(*current, fobj_field::kData)) {
            /* The keyframe stream is read big-endian by the original
             * interpreter, so it stays a GX-style payload rather than being
             * translated.  Its length is declared, so the whole run is
             * validated here instead of while it is being walked. */
            host->ad = static_cast<u8*>(payload(*data, host->length));
            stats_.anim_data_bytes += host->length;
        }

        if (tail != nullptr) {
            tail->next = host;
        } else {
            head = host;
        }
        tail = host;
        current = reference(*current, fobj_field::kNext);
    }
    return head;
}

FigaTree* HsdMaterializedArchive::figa_tree_at(HsdRuntimeNode node)
{
    FigaTree* const host = allocate<FigaTree>();
    stats_.figa_trees += 1;

    host->type = static_cast<int>(
        archive_.read_u32(node, figa_tree_field::kType));
    host->flags = archive_.read_u32(node, figa_tree_field::kFlags);
    host->frames = archive_.read_f32(node, figa_tree_field::kFrames);

    const auto nodes = reference(node, figa_tree_field::kNodes);
    const auto tracks = reference(node, figa_tree_field::kTracks);
    if (!nodes.has_value() || !tracks.has_value()) {
        throw HsdArchiveError("FigaTree is missing its node or track list");
    }

    /* The node list says how many tracks each bone takes and ends at -1, so
     * walking it is also what gives the track count. */
    std::size_t node_count = 0;
    std::size_t track_count = 0;
    while (node_count < kFigaNodeLimit) {
        const auto byte = archive_.bytes_at(
            { nodes->data_offset + static_cast<std::uint32_t>(node_count) },
            1);
        const auto entry =
            static_cast<std::int8_t>(std::to_integer<std::uint8_t>(byte[0]));
        if (entry < 0) {
            break;
        }
        track_count += static_cast<std::size_t>(entry);
        ++node_count;
    }
    if (node_count == kFigaNodeLimit) {
        throw HsdArchiveError("FigaTree node list has no terminator");
    }
    if (track_count > kFigaTrackLimit) {
        throw HsdArchiveError("FigaTree declares too many tracks");
    }
    /* Signed bytes need no translation, so the list is used where it lies. */
    host->nodes = static_cast<s8*>(payload(*nodes, node_count + 1));

    auto* const host_tracks = static_cast<FigaTrack*>(allocate_bytes(
        sizeof(FigaTrack) * (track_count == 0 ? 1 : track_count),
        alignof(FigaTrack)));
    host->tracks = host_tracks;
    stats_.figa_tracks += track_count;
    for (std::size_t index = 0; index < track_count; ++index) {
        const HsdRuntimeNode entry{
            tracks->data_offset +
            static_cast<std::uint32_t>(index * figa_track_field::kSize)
        };
        FigaTrack& out = host_tracks[index];
        out.length = archive_.read_u16(entry, figa_track_field::kLength);
        out.startframe =
            archive_.read_u16(entry, figa_track_field::kStartFrame);
        out.obj_type = std::to_integer<u8>(archive_.bytes_at(
            { entry.data_offset + figa_track_field::kObjType }, 1)[0]);
        out.frac_value = std::to_integer<u8>(archive_.bytes_at(
            { entry.data_offset + figa_track_field::kFracValue }, 1)[0]);
        out.frac_slope = std::to_integer<u8>(archive_.bytes_at(
            { entry.data_offset + figa_track_field::kFracSlope }, 1)[0]);
        if (const auto data = reference(entry, figa_track_field::kData)) {
            /* Same keyframe encoding an HSD_FObjDesc carries, read
             * big-endian by the same interpreter, with its length declared. */
            out.ad_head = static_cast<u8*>(payload(*data, out.length));
            stats_.anim_data_bytes += out.length;
        }
    }
    return host;
}

FigaTree* HsdMaterializedArchive::figa_tree(std::string_view public_symbol)
{
    return figa_tree_at(archive_.public_root(public_symbol));
}

HSD_AObjDesc* HsdMaterializedArchive::aobj_desc(HsdRuntimeNode node)
{
    const auto found = aobj_descs_.find(node.data_offset);
    if (found != aobj_descs_.end()) {
        return found->second;
    }
    HSD_AObjDesc* const host = allocate<HSD_AObjDesc>();
    aobj_descs_.emplace(node.data_offset, host);
    stats_.aobj_descs += 1;

    host->flags = archive_.read_u32(node, aobj_field::kFlags);
    host->end_frame = archive_.read_f32(node, aobj_field::kEndFrame);
    if (const auto object = reference(node, aobj_field::kObjId)) {
        /* archive.c turns this field into an HSD_Joint pointer before the
         * original loader sees it.  Keep that identity on the host by using
         * the materialized joint. */
        host->obj_id = reinterpret_cast<void*>(joint_chain(*object));
    }
    if (const auto fobj = reference(node, aobj_field::kFObjDesc)) {
        host->fobjdesc = fobj_chain(*fobj);
    }
    return host;
}

/* HSD_RObjAnimJoint, HSD_ChanAnim, HSD_TevRegAnim and HSD_ShapeAnim are all a
 * link plus one HSD_AObjDesc, so one walk covers them; the caller says which
 * type the result is. */
template <typename T>
T* HsdMaterializedArchive::anim_link_chain(HsdRuntimeNode node)
{
    T* head = nullptr;
    T* tail = nullptr;
    std::optional<HsdRuntimeNode> current = node;

    while (current.has_value()) {
        T* const host = allocate<T>();
        if (const auto aobj = reference(*current, anim_link_field::kPayload)) {
            host->aobjdesc = aobj_desc(*aobj);
        }
        if (tail != nullptr) {
            tail->next = host;
        } else {
            head = host;
        }
        tail = host;
        current = reference(*current, anim_link_field::kNext);
    }
    return head;
}

HSD_RObjAnimJoint* HsdMaterializedArchive::robj_anim_chain(HsdRuntimeNode node)
{
    return anim_link_chain<HSD_RObjAnimJoint>(node);
}

HSD_ShapeAnim* HsdMaterializedArchive::shape_anim_chain(HsdRuntimeNode node)
{
    return anim_link_chain<HSD_ShapeAnim>(node);
}

HSD_AnimJoint* HsdMaterializedArchive::anim_joint_chain(HsdRuntimeNode node)
{
    if (depth_ >= kDepthLimit) {
        throw HsdArchiveError(
            "HSD animation tree is deeper than the host allows");
    }
    const DepthGuard guard(depth_);

    HSD_AnimJoint* head = nullptr;
    HSD_AnimJoint* tail = nullptr;
    std::optional<HsdRuntimeNode> current = node;

    while (current.has_value()) {
        const auto found = anim_joints_.find(current->data_offset);
        if (found != anim_joints_.end()) {
            if (tail != nullptr) {
                tail->next = found->second;
            } else {
                head = found->second;
            }
            return head;
        }
        HSD_AnimJoint* const host = allocate<HSD_AnimJoint>();
        anim_joints_.emplace(current->data_offset, host);
        stats_.anim_joints += 1;

        host->flags = archive_.read_u32(*current, anim_joint_field::kFlags);
        if (const auto child = reference(*current, anim_joint_field::kChild)) {
            host->child = anim_joint_chain(*child);
        }
        if (const auto aobj =
                reference(*current, anim_joint_field::kAObjDesc)) {
            host->aobjdesc = aobj_desc(*aobj);
        }
        if (const auto robj =
                reference(*current, anim_joint_field::kRObjAnim)) {
            host->robj_anim = robj_anim_chain(*robj);
        }

        if (tail != nullptr) {
            tail->next = host;
        } else {
            head = host;
        }
        tail = host;
        current = reference(*current, anim_joint_field::kNext);
    }
    return head;
}

HSD_TexAnim* HsdMaterializedArchive::tex_anim_chain(HsdRuntimeNode node)
{
    HSD_TexAnim* head = nullptr;
    HSD_TexAnim* tail = nullptr;
    std::optional<HsdRuntimeNode> current = node;

    while (current.has_value()) {
        HSD_TexAnim* const host = allocate<HSD_TexAnim>();
        host->id = static_cast<GXTexMapID>(
            archive_.read_u32(*current, tex_anim_field::kId));
        host->n_imagetbl =
            archive_.read_u16(*current, tex_anim_field::kImageCount);
        host->n_tluttbl =
            archive_.read_u16(*current, tex_anim_field::kTlutCount);
        if (const auto aobj = reference(*current, tex_anim_field::kAObjDesc)) {
            host->aobjdesc = aobj_desc(*aobj);
        }
        /* A texture animation swaps whole images and palettes, so each table
         * is an array of descriptors the frames index into. */
        if (const auto table =
                reference(*current, tex_anim_field::kImageTable)) {
            if (host->n_imagetbl != 0) {
                auto* const images = static_cast<HSD_ImageDesc**>(
                    allocate_bytes(sizeof(HSD_ImageDesc*) * host->n_imagetbl,
                                   alignof(HSD_ImageDesc*)));
                for (std::size_t index = 0; index < host->n_imagetbl;
                     ++index) {
                    const auto entry = reference(
                        *table, static_cast<std::uint32_t>(index * 4));
                    images[index] =
                        entry.has_value() ? image_desc(*entry) : nullptr;
                }
                host->imagetbl = images;
            }
        }
        if (const auto table =
                reference(*current, tex_anim_field::kTlutTable)) {
            if (host->n_tluttbl != 0) {
                auto* const tluts = static_cast<HSD_TlutDesc**>(
                    allocate_bytes(sizeof(HSD_TlutDesc*) * host->n_tluttbl,
                                   alignof(HSD_TlutDesc*)));
                for (std::size_t index = 0; index < host->n_tluttbl; ++index) {
                    const auto entry = reference(
                        *table, static_cast<std::uint32_t>(index * 4));
                    tluts[index] =
                        entry.has_value() ? tlut_desc(*entry) : nullptr;
                }
                host->tluttbl = tluts;
            }
        }

        if (tail != nullptr) {
            tail->next = host;
        } else {
            head = host;
        }
        tail = host;
        current = reference(*current, tex_anim_field::kNext);
    }
    return head;
}

HSD_RenderAnim* HsdMaterializedArchive::render_anim(HsdRuntimeNode node)
{
    HSD_RenderAnim* const host = allocate<HSD_RenderAnim>();
    if (const auto chan = reference(node, render_anim_field::kChanAnim)) {
        host->chananim = anim_link_chain<HSD_ChanAnim>(*chan);
    }
    if (const auto reg = reference(node, render_anim_field::kRegAnim)) {
        host->reganim = anim_link_chain<HSD_TevRegAnim>(*reg);
    }
    return host;
}

HSD_MatAnim* HsdMaterializedArchive::mat_anim_chain(HsdRuntimeNode node)
{
    HSD_MatAnim* head = nullptr;
    HSD_MatAnim* tail = nullptr;
    std::optional<HsdRuntimeNode> current = node;

    while (current.has_value()) {
        HSD_MatAnim* const host = allocate<HSD_MatAnim>();
        if (const auto aobj = reference(*current, mat_anim_field::kAObjDesc)) {
            host->aobjdesc = aobj_desc(*aobj);
        }
        if (const auto tex = reference(*current, mat_anim_field::kTexAnim)) {
            host->texanim = tex_anim_chain(*tex);
        }
        if (const auto render =
                reference(*current, mat_anim_field::kRenderAnim)) {
            host->renderanim = render_anim(*render);
        }

        if (tail != nullptr) {
            tail->next = host;
        } else {
            head = host;
        }
        tail = host;
        current = reference(*current, mat_anim_field::kNext);
    }
    return head;
}

HSD_MatAnimJoint* HsdMaterializedArchive::mat_anim_joint_chain(
    HsdRuntimeNode node)
{
    if (depth_ >= kDepthLimit) {
        throw HsdArchiveError(
            "HSD material animation tree is deeper than the host allows");
    }
    const DepthGuard guard(depth_);

    HSD_MatAnimJoint* head = nullptr;
    HSD_MatAnimJoint* tail = nullptr;
    std::optional<HsdRuntimeNode> current = node;

    while (current.has_value()) {
        const auto found = mat_anim_joints_.find(current->data_offset);
        if (found != mat_anim_joints_.end()) {
            if (tail != nullptr) {
                tail->next = found->second;
            } else {
                head = found->second;
            }
            return head;
        }
        HSD_MatAnimJoint* const host = allocate<HSD_MatAnimJoint>();
        mat_anim_joints_.emplace(current->data_offset, host);
        stats_.mat_anim_joints += 1;

        if (const auto child =
                reference(*current, mat_anim_joint_field::kChild)) {
            host->child = mat_anim_joint_chain(*child);
        }
        if (const auto anim =
                reference(*current, mat_anim_joint_field::kMatAnim)) {
            host->matanim = mat_anim_chain(*anim);
        }

        if (tail != nullptr) {
            tail->next = host;
        } else {
            head = host;
        }
        tail = host;
        current = reference(*current, mat_anim_joint_field::kNext);
    }
    return head;
}

HSD_ShapeAnimDObj* HsdMaterializedArchive::shape_anim_dobj_chain(
    HsdRuntimeNode node)
{
    HSD_ShapeAnimDObj* head = nullptr;
    HSD_ShapeAnimDObj* tail = nullptr;
    std::optional<HsdRuntimeNode> current = node;

    while (current.has_value()) {
        HSD_ShapeAnimDObj* const host = allocate<HSD_ShapeAnimDObj>();
        if (const auto anim =
                reference(*current, anim_link_field::kPayload)) {
            host->shapeanim = shape_anim_chain(*anim);
        }
        if (tail != nullptr) {
            tail->next = host;
        } else {
            head = host;
        }
        tail = host;
        current = reference(*current, anim_link_field::kNext);
    }
    return head;
}

HSD_ShapeAnimJoint* HsdMaterializedArchive::shape_anim_joint_chain(
    HsdRuntimeNode node)
{
    if (depth_ >= kDepthLimit) {
        throw HsdArchiveError(
            "HSD shape animation tree is deeper than the host allows");
    }
    const DepthGuard guard(depth_);

    HSD_ShapeAnimJoint* head = nullptr;
    HSD_ShapeAnimJoint* tail = nullptr;
    std::optional<HsdRuntimeNode> current = node;

    while (current.has_value()) {
        const auto found = shape_anim_joints_.find(current->data_offset);
        if (found != shape_anim_joints_.end()) {
            if (tail != nullptr) {
                tail->next = found->second;
            } else {
                head = found->second;
            }
            return head;
        }
        HSD_ShapeAnimJoint* const host = allocate<HSD_ShapeAnimJoint>();
        shape_anim_joints_.emplace(current->data_offset, host);
        stats_.shape_anim_joints += 1;

        if (const auto child =
                reference(*current, shape_anim_joint_field::kChild)) {
            host->child = shape_anim_joint_chain(*child);
        }
        if (const auto dobj =
                reference(*current, shape_anim_joint_field::kShapeAnimDObj)) {
            host->shapeanimdobj = shape_anim_dobj_chain(*dobj);
        }

        if (tail != nullptr) {
            tail->next = host;
        } else {
            head = host;
        }
        tail = host;
        current = reference(*current, shape_anim_joint_field::kNext);
    }
    return head;
}

HSD_AnimJoint* HsdMaterializedArchive::anim_joint(
    std::string_view public_symbol)
{
    return anim_joint_chain(archive_.public_root(public_symbol));
}

HSD_MatAnimJoint* HsdMaterializedArchive::mat_anim_joint(
    std::string_view public_symbol)
{
    return mat_anim_joint_chain(archive_.public_root(public_symbol));
}

HSD_ShapeAnimJoint* HsdMaterializedArchive::shape_anim_joint(
    std::string_view public_symbol)
{
    return shape_anim_joint_chain(archive_.public_root(public_symbol));
}

HSD_Joint* HsdMaterializedArchive::joint_chain(HsdRuntimeNode node)
{
    if (depth_ >= kDepthLimit) {
        throw HsdArchiveError("HSD joint tree is deeper than the host allows");
    }
    const DepthGuard guard(depth_);

    HSD_Joint* head = nullptr;
    HSD_Joint* tail = nullptr;
    std::optional<HsdRuntimeNode> current = node;

    while (current.has_value()) {
        const auto found = joints_.find(current->data_offset);
        if (found != joints_.end()) {
            if (tail != nullptr) {
                tail->next = found->second;
            } else {
                head = found->second;
            }
            return head;
        }
        HSD_Joint* const host = allocate<HSD_Joint>();
        /* Recorded before recursing, so a joint referenced from inside its own
         * subtree resolves to this node instead of looping. */
        joints_.emplace(current->data_offset, host);
        stats_.joints += 1;

        if (const auto name = reference(*current, joint_field::kClassName)) {
            host->class_name = payload_string(*name);
        }
        const u32 flags = archive_.read_u32(*current, joint_field::kFlags);
        host->flags = flags;
        read_vec3(*current, joint_field::kRotation, &host->rotation);
        read_vec3(*current, joint_field::kScale, &host->scale);
        read_vec3(*current, joint_field::kPosition, &host->position);

        if (const auto child = reference(*current, joint_field::kChild)) {
            /* An instance joint points at a joint it shares rather than one it
             * owns.  JObjLoad skips loading it and HSD_JObjResolveRefs looks
             * it up by its address, which works on the host because every
             * joint lands in the same contiguous arena. */
            host->child = joint_chain(*child);
        }
        if (const auto mtx = reference(*current, joint_field::kMtx)) {
            host->mtx = reinterpret_cast<MtxPtr>(matrix(*mtx));
        }
        if (const auto robj = reference(*current, joint_field::kRObjDesc)) {
            host->robjdesc = robj_chain(*robj);
        }

        if ((flags & JOBJ_SPLINE) != 0) {
            if (const auto spline = reference(*current, joint_field::kUnion)) {
                host->u.spline = spline_desc(*spline);
            }
        } else if ((flags & JOBJ_PTCL) != 0) {
            if (reference(*current, joint_field::kUnion).has_value()) {
                unsupported("a particle joint");
            }
        } else if (const auto dobj =
                       reference(*current, joint_field::kUnion)) {
            host->u.dobjdesc = dobj_chain(*dobj);
        }

        if (tail != nullptr) {
            tail->next = host;
        } else {
            head = host;
        }
        tail = host;
        current = reference(*current, joint_field::kNext);
    }
    return head;
}

HSD_Joint* HsdMaterializedArchive::joint(std::string_view public_symbol)
{
    return joint_chain(archive_.public_root(public_symbol));
}

std::size_t HsdMaterializedArchive::scene_model_count(
    std::string_view public_symbol) const
{
    const HsdRuntimeNode scene = archive_.public_root(public_symbol);
    const auto models = reference(scene, kSceneModels);
    if (!models.has_value()) {
        return 0;
    }
    std::size_t count = 0;
    while (count < kSceneModelLimit) {
        const std::uint32_t relative = static_cast<std::uint32_t>(count * 4);
        if (!reference(*models, relative).has_value()) {
            return count;
        }
        ++count;
    }
    throw HsdArchiveError("HSD scene model table has no terminator");
}

HSD_Joint* HsdMaterializedArchive::scene_model_joint(
    std::string_view public_symbol, std::size_t model_index)
{
    const HsdRuntimeNode scene = archive_.public_root(public_symbol);
    const auto models = reference(scene, kSceneModels);
    if (!models.has_value()) {
        throw HsdArchiveError("HSD scene has no model table");
    }
    if (model_index >= kSceneModelLimit) {
        throw HsdArchiveError("HSD scene model index is out of range");
    }
    const auto model =
        reference(*models, static_cast<std::uint32_t>(model_index * 4));
    if (!model.has_value()) {
        throw HsdArchiveError("HSD scene has no model at that index");
    }
    const auto root = reference(*model, kModelJoint);
    if (!root.has_value()) {
        throw HsdArchiveError("HSD scene model has no joint");
    }
    return joint_chain(*root);
}

template <typename T>
T** HsdMaterializedArchive::pointer_table(
    HsdRuntimeNode table, T* (HsdMaterializedArchive::*build)(HsdRuntimeNode),
    const char* what)
{
    std::size_t count = 0;
    while (reference(table, static_cast<std::uint32_t>(count * 4)).has_value())
    {
        if (++count >= kSceneModelLimit) {
            throw HsdArchiveError(std::string(what) + " has no terminator");
        }
    }
    /* The arena is zeroed, so the slot after the last entry is already the
     * NULL the game stops at. */
    auto** const entries = static_cast<T**>(
        allocate_bytes(sizeof(T*) * (count + 1), alignof(T*)));
    for (std::size_t index = 0; index < count; ++index) {
        entries[index] = (this->*build)(
            *reference(table, static_cast<std::uint32_t>(index * 4)));
    }
    return entries;
}

MaterializedDynamicModel*
HsdMaterializedArchive::dynamic_model(HsdRuntimeNode node)
{
    auto* const host = allocate<MaterializedDynamicModel>();
    if (const auto joint = reference(node, kModelJoint)) {
        host->joint = joint_chain(*joint);
    }
    if (const auto anims = reference(node, kModelAnims)) {
        host->anims = pointer_table(*anims,
                                    &HsdMaterializedArchive::anim_joint_chain,
                                    "HSD model animation table");
    }
    if (const auto anims = reference(node, kModelMatAnims)) {
        host->matanims = pointer_table(
            *anims, &HsdMaterializedArchive::mat_anim_joint_chain,
            "HSD model material animation table");
    }
    if (const auto anims = reference(node, kModelShapeAnims)) {
        host->shapeanims = pointer_table(
            *anims, &HsdMaterializedArchive::shape_anim_joint_chain,
            "HSD model shape animation table");
    }
    return host;
}

HSD_CameraAnim* HsdMaterializedArchive::camera_anim(HsdRuntimeNode node)
{
    auto* const host = allocate<HSD_CameraAnim>();
    if (const auto aobj = reference(node, camera_anim_field::kAObjDesc)) {
        host->aobjdesc = aobj_desc(*aobj);
    }
    if (const auto eye = reference(node, camera_anim_field::kEyeAnim)) {
        host->eye_anim = world_anim(*eye);
    }
    if (const auto interest =
            reference(node, camera_anim_field::kInterestAnim))
    {
        host->interest_anim = world_anim(*interest);
    }
    return host;
}

/* HSD_Fog_8037DE7C takes only the AObjDesc a fog animation starts with, so
 * nothing after it is read. */
HSD_CameraAnim* HsdMaterializedArchive::fog_anim(HsdRuntimeNode node)
{
    auto* const host = allocate<HSD_CameraAnim>();
    if (const auto aobj = reference(node, camera_anim_field::kAObjDesc)) {
        host->aobjdesc = aobj_desc(*aobj);
    }
    return host;
}

/* The entries of a camera or fog array: while an entry names a descriptor,
 * and no further than the next address a relocation targets. */
std::size_t HsdMaterializedArchive::scene_entry_count(HsdRuntimeNode array)
{
    const std::uint32_t room =
        translator_extent(array.data_offset) / kSceneCameraDescSize;
    std::size_t count = 0;
    while (count < room &&
           archive_.has_reference_at(
               array, static_cast<std::uint32_t>(count * kSceneCameraDescSize) +
                          kSceneCameraDesc))
    {
        ++count;
    }
    return count;
}

MaterializedSceneDesc*
HsdMaterializedArchive::scene_desc(std::string_view public_symbol)
{
    const HsdRuntimeNode scene = archive_.public_root(public_symbol);
    auto* const host = allocate<MaterializedSceneDesc>();

    if (const auto models = reference(scene, kSceneModels)) {
        host->models = pointer_table(*models,
                                     &HsdMaterializedArchive::dynamic_model,
                                     "HSD scene model table");
    }
    if (const auto cameras = reference(scene, kSceneCameras)) {
        const std::size_t count = scene_entry_count(*cameras);
        /* At least one entry, so a scene that names an array with no camera
         * in it reads a NULL descriptor. */
        auto* const entries = static_cast<MaterializedSceneCamera*>(
            allocate_bytes(sizeof(MaterializedSceneCamera) *
                               std::max<std::size_t>(count, 1),
                           alignof(MaterializedSceneCamera)));
        for (std::size_t index = 0; index < count; ++index) {
            const HsdRuntimeNode entry{
                cameras->data_offset +
                static_cast<std::uint32_t>(index * kSceneCameraDescSize)
            };
            entries[index].desc =
                camera_desc(*reference(entry, kSceneCameraDesc));
            if (const auto anims = reference(entry, kSceneEntryAnims)) {
                entries[index].anims = pointer_table(
                    *anims, &HsdMaterializedArchive::camera_anim,
                    "HSD camera animation table");
            }
        }
        host->cameras = entries;
    }
    if (const auto lights = reference(scene, kSceneLights)) {
        host->lights = light_list_table(*lights);
    }
    if (const auto fogs = reference(scene, kSceneFogs)) {
        const std::size_t count = scene_entry_count(*fogs);
        auto* const entries = static_cast<MaterializedSceneFog*>(
            allocate_bytes(sizeof(MaterializedSceneFog) *
                               std::max<std::size_t>(count, 1),
                           alignof(MaterializedSceneFog)));
        for (std::size_t index = 0; index < count; ++index) {
            const HsdRuntimeNode entry{
                fogs->data_offset +
                static_cast<std::uint32_t>(index * kSceneCameraDescSize)
            };
            entries[index].desc = fog_desc(*reference(entry, kSceneCameraDesc));
            if (const auto anims = reference(entry, kSceneEntryAnims)) {
                entries[index].anims = pointer_table(
                    *anims, &HsdMaterializedArchive::fog_anim,
                    "HSD fog animation table");
            }
        }
        host->fogs = entries;
    }
    return host;
}

MaterializedDynamicModel**
HsdMaterializedArchive::scene_models(std::string_view public_symbol)
{
    return pointer_table(archive_.public_root(public_symbol),
                         &HsdMaterializedArchive::dynamic_model,
                         "HSD scene model table");
}

} // namespace melee::assets
