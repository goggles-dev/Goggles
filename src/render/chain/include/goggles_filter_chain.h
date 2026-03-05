#ifndef GOGGLES_FILTER_CHAIN_H
#define GOGGLES_FILTER_CHAIN_H

#ifdef __cplusplus
#include <cstddef>
#include <cstdint>
#else
#include <stddef.h>
#include <stdint.h>
#endif

#include <vulkan/vulkan.h>

#if defined(_WIN32)
#if defined(GOGGLES_CHAIN_BUILD_SHARED)
#define GOGGLES_CHAIN_API __declspec(dllexport)
#elif defined(GOGGLES_CHAIN_USE_SHARED)
#define GOGGLES_CHAIN_API __declspec(dllimport)
#endif
#elif defined(__GNUC__) && defined(GOGGLES_CHAIN_BUILD_SHARED)
#define GOGGLES_CHAIN_API __attribute__((visibility("default")))
#endif

#ifndef GOGGLES_CHAIN_API
#define GOGGLES_CHAIN_API
#endif

#ifndef GOGGLES_CHAIN_CALL
#if defined(_WIN32)
#define GOGGLES_CHAIN_CALL __cdecl
#else
#define GOGGLES_CHAIN_CALL
#endif
#endif

#if defined(_MSC_VER)
#define GOGGLES_CHAIN_DEPRECATED(msg) __declspec(deprecated(msg))
#elif defined(__clang__) || defined(__GNUC__)
#define GOGGLES_CHAIN_DEPRECATED(msg) __attribute__((deprecated(msg)))
#else
#define GOGGLES_CHAIN_DEPRECATED(msg)
#endif

#define GOGGLES_CHAIN_MAKE_VERSION(major, minor, patch)                                            \
    ((((uint32_t)(major) & 0x3ffu) << 22) | (((uint32_t)(minor) & 0x3ffu) << 12) |                 \
     ((uint32_t)(patch) & 0xfffu))

#define GOGGLES_CHAIN_API_VERSION GOGGLES_CHAIN_MAKE_VERSION(1u, 0u, 0u)
#define GOGGLES_CHAIN_ABI_VERSION 1u
#define GOGGLES_CHAIN_STRUCT_SIZE(type) ((uint32_t)sizeof(type))

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
#define GOGGLES_CHAIN_NOARGS
#define GOGGLES_CHAIN_NULLPTR nullptr
#else
#define GOGGLES_CHAIN_NOARGS void
#define GOGGLES_CHAIN_NULLPTR NULL
#endif

struct goggles_chain;
struct goggles_chain_control_snapshot;

struct GogglesChainExtent2D {
    uint32_t width;
    uint32_t height;
};

struct GogglesChainVkContext {
    VkDevice device;
    VkPhysicalDevice physical_device;
    VkQueue graphics_queue;
    uint32_t graphics_queue_family_index;
};

struct GogglesChainVkCreateInfo {
    uint32_t struct_size;
    VkFormat target_format;
    uint32_t num_sync_indices;
    const char* shader_dir_utf8;
    const char* cache_dir_utf8;
    struct GogglesChainExtent2D initial_prechain_resolution;
};

struct GogglesChainVkCreateInfoEx {
    uint32_t struct_size;
    VkFormat target_format;
    uint32_t num_sync_indices;
    const char* shader_dir_utf8;
    size_t shader_dir_len;
    const char* cache_dir_utf8;
    size_t cache_dir_len;
    struct GogglesChainExtent2D initial_prechain_resolution;
};

struct GogglesChainVkRecordInfo {
    uint32_t struct_size;
    VkCommandBuffer command_buffer;
    VkImage source_image;
    VkImageView source_view;
    struct GogglesChainExtent2D source_extent;
    VkImageView target_view;
    struct GogglesChainExtent2D target_extent;
    uint32_t frame_index;
    uint32_t scale_mode;
    uint32_t integer_scale;
};

struct GogglesChainStagePolicy {
    uint32_t struct_size;
    uint32_t enabled_stage_mask;
};

struct GogglesChainCapabilities {
    uint32_t struct_size;
    uint32_t capability_flags;
    uint32_t max_sync_indices;
};

struct GogglesChainErrorLastInfo {
    uint32_t struct_size;
    uint32_t status;
    int32_t vk_result;
    uint32_t subsystem_code;
};

struct GogglesChainControlDesc {
    uint64_t control_id;
    uint32_t stage;
    const char* name_utf8;
    const char* description_utf8;
    float current_value;
    float default_value;
    float min_value;
    float max_value;
    float step;
};

#ifdef __cplusplus
using goggles_chain_t = goggles_chain;
using goggles_chain_control_snapshot_t = goggles_chain_control_snapshot;
using goggles_chain_status_t = uint32_t;
using goggles_chain_stage_t = uint32_t;
using goggles_chain_stage_mask_t = uint32_t;
using goggles_chain_scale_mode_t = uint32_t;
using goggles_chain_capability_flags_t = uint32_t;
using goggles_chain_control_id_t = uint64_t;
using goggles_chain_extent2d_t = GogglesChainExtent2D;
using goggles_chain_vk_context_t = GogglesChainVkContext;
using goggles_chain_vk_create_info_t = GogglesChainVkCreateInfo;
using goggles_chain_vk_create_info_ex_t = GogglesChainVkCreateInfoEx;
using goggles_chain_vk_record_info_t = GogglesChainVkRecordInfo;
using goggles_chain_stage_policy_t = GogglesChainStagePolicy;
using goggles_chain_capabilities_t = GogglesChainCapabilities;
using goggles_chain_error_last_info_t = GogglesChainErrorLastInfo;
using goggles_chain_control_desc_t = GogglesChainControlDesc;
#else
typedef struct goggles_chain goggles_chain_t;
typedef struct goggles_chain_control_snapshot goggles_chain_control_snapshot_t;
typedef uint32_t goggles_chain_status_t;
typedef uint32_t goggles_chain_stage_t;
typedef uint32_t goggles_chain_stage_mask_t;
typedef uint32_t goggles_chain_scale_mode_t;
typedef uint32_t goggles_chain_capability_flags_t;
typedef uint64_t goggles_chain_control_id_t;
typedef struct GogglesChainExtent2D goggles_chain_extent2d_t;
typedef struct GogglesChainVkContext goggles_chain_vk_context_t;
typedef struct GogglesChainVkCreateInfo goggles_chain_vk_create_info_t;
typedef struct GogglesChainVkCreateInfoEx goggles_chain_vk_create_info_ex_t;
typedef struct GogglesChainVkRecordInfo goggles_chain_vk_record_info_t;
typedef struct GogglesChainStagePolicy goggles_chain_stage_policy_t;
typedef struct GogglesChainCapabilities goggles_chain_capabilities_t;
typedef struct GogglesChainErrorLastInfo goggles_chain_error_last_info_t;
typedef struct GogglesChainControlDesc goggles_chain_control_desc_t;
#endif

#define GOGGLES_CHAIN_STATUS_OK ((goggles_chain_status_t)0u)
#define GOGGLES_CHAIN_STATUS_INVALID_ARGUMENT ((goggles_chain_status_t)1u)
#define GOGGLES_CHAIN_STATUS_NOT_INITIALIZED ((goggles_chain_status_t)2u)
#define GOGGLES_CHAIN_STATUS_NOT_FOUND ((goggles_chain_status_t)3u)
#define GOGGLES_CHAIN_STATUS_PRESET_ERROR ((goggles_chain_status_t)4u)
#define GOGGLES_CHAIN_STATUS_IO_ERROR ((goggles_chain_status_t)5u)
#define GOGGLES_CHAIN_STATUS_VULKAN_ERROR ((goggles_chain_status_t)6u)
#define GOGGLES_CHAIN_STATUS_OUT_OF_MEMORY ((goggles_chain_status_t)7u)
#define GOGGLES_CHAIN_STATUS_NOT_SUPPORTED ((goggles_chain_status_t)8u)
#define GOGGLES_CHAIN_STATUS_RUNTIME_ERROR ((goggles_chain_status_t)9u)

#define GOGGLES_CHAIN_STAGE_PRECHAIN ((goggles_chain_stage_t)0u)
#define GOGGLES_CHAIN_STAGE_EFFECT ((goggles_chain_stage_t)1u)
#define GOGGLES_CHAIN_STAGE_POSTCHAIN ((goggles_chain_stage_t)2u)

#define GOGGLES_CHAIN_STAGE_MASK_PRECHAIN ((goggles_chain_stage_mask_t)(1u << 0))
#define GOGGLES_CHAIN_STAGE_MASK_EFFECT ((goggles_chain_stage_mask_t)(1u << 1))
#define GOGGLES_CHAIN_STAGE_MASK_POSTCHAIN ((goggles_chain_stage_mask_t)(1u << 2))
#define GOGGLES_CHAIN_STAGE_MASK_ALL                                                               \
    ((goggles_chain_stage_mask_t)(GOGGLES_CHAIN_STAGE_MASK_PRECHAIN |                              \
                                  GOGGLES_CHAIN_STAGE_MASK_EFFECT |                                \
                                  GOGGLES_CHAIN_STAGE_MASK_POSTCHAIN))

#define GOGGLES_CHAIN_SCALE_MODE_STRETCH ((goggles_chain_scale_mode_t)0u)
#define GOGGLES_CHAIN_SCALE_MODE_FIT ((goggles_chain_scale_mode_t)1u)
#define GOGGLES_CHAIN_SCALE_MODE_INTEGER ((goggles_chain_scale_mode_t)2u)

#define GOGGLES_CHAIN_CAPABILITY_NONE ((goggles_chain_capability_flags_t)0u)
#define GOGGLES_CHAIN_CAPABILITY_LAST_ERROR_INFO ((goggles_chain_capability_flags_t)(1u << 0))

static inline goggles_chain_vk_create_info_t
goggles_chain_vk_create_info_init(GOGGLES_CHAIN_NOARGS) {
    goggles_chain_vk_create_info_t value;
    value.struct_size = GOGGLES_CHAIN_STRUCT_SIZE(goggles_chain_vk_create_info_t);
    value.target_format = VK_FORMAT_UNDEFINED;
    value.num_sync_indices = 1u;
    value.shader_dir_utf8 = GOGGLES_CHAIN_NULLPTR;
    value.cache_dir_utf8 = GOGGLES_CHAIN_NULLPTR;
    value.initial_prechain_resolution.width = 0u;
    value.initial_prechain_resolution.height = 0u;
    return value;
}

static inline goggles_chain_vk_create_info_ex_t
goggles_chain_vk_create_info_ex_init(GOGGLES_CHAIN_NOARGS) {
    goggles_chain_vk_create_info_ex_t value;
    value.struct_size = GOGGLES_CHAIN_STRUCT_SIZE(goggles_chain_vk_create_info_ex_t);
    value.target_format = VK_FORMAT_UNDEFINED;
    value.num_sync_indices = 1u;
    value.shader_dir_utf8 = GOGGLES_CHAIN_NULLPTR;
    value.shader_dir_len = 0u;
    value.cache_dir_utf8 = GOGGLES_CHAIN_NULLPTR;
    value.cache_dir_len = 0u;
    value.initial_prechain_resolution.width = 0u;
    value.initial_prechain_resolution.height = 0u;
    return value;
}

static inline goggles_chain_vk_record_info_t
goggles_chain_vk_record_info_init(GOGGLES_CHAIN_NOARGS) {
    goggles_chain_vk_record_info_t value;
    value.struct_size = GOGGLES_CHAIN_STRUCT_SIZE(goggles_chain_vk_record_info_t);
    value.command_buffer = VK_NULL_HANDLE;
    value.source_image = VK_NULL_HANDLE;
    value.source_view = VK_NULL_HANDLE;
    value.source_extent.width = 0u;
    value.source_extent.height = 0u;
    value.target_view = VK_NULL_HANDLE;
    value.target_extent.width = 0u;
    value.target_extent.height = 0u;
    value.frame_index = 0u;
    value.scale_mode = GOGGLES_CHAIN_SCALE_MODE_STRETCH;
    value.integer_scale = 1u;
    return value;
}

static inline goggles_chain_stage_policy_t goggles_chain_stage_policy_init(GOGGLES_CHAIN_NOARGS) {
    goggles_chain_stage_policy_t value;
    value.struct_size = GOGGLES_CHAIN_STRUCT_SIZE(goggles_chain_stage_policy_t);
    value.enabled_stage_mask = GOGGLES_CHAIN_STAGE_MASK_ALL;
    return value;
}

static inline goggles_chain_capabilities_t goggles_chain_capabilities_init(GOGGLES_CHAIN_NOARGS) {
    goggles_chain_capabilities_t value;
    value.struct_size = GOGGLES_CHAIN_STRUCT_SIZE(goggles_chain_capabilities_t);
    value.capability_flags = GOGGLES_CHAIN_CAPABILITY_NONE;
    value.max_sync_indices = 0u;
    return value;
}

static inline goggles_chain_error_last_info_t
goggles_chain_error_last_info_init(GOGGLES_CHAIN_NOARGS) {
    goggles_chain_error_last_info_t value;
    value.struct_size = GOGGLES_CHAIN_STRUCT_SIZE(goggles_chain_error_last_info_t);
    value.status = GOGGLES_CHAIN_STATUS_OK;
    value.vk_result = 0;
    value.subsystem_code = 0u;
    return value;
}

GOGGLES_CHAIN_API uint32_t GOGGLES_CHAIN_CALL goggles_chain_api_version(GOGGLES_CHAIN_NOARGS);
GOGGLES_CHAIN_API uint32_t GOGGLES_CHAIN_CALL goggles_chain_abi_version(GOGGLES_CHAIN_NOARGS);
GOGGLES_CHAIN_API goggles_chain_status_t GOGGLES_CHAIN_CALL
goggles_chain_capabilities_get(goggles_chain_capabilities_t* out_caps);
GOGGLES_CHAIN_API const char* GOGGLES_CHAIN_CALL
goggles_chain_status_to_string(goggles_chain_status_t status);

GOGGLES_CHAIN_API goggles_chain_status_t GOGGLES_CHAIN_CALL goggles_chain_create_vk(
    const goggles_chain_vk_context_t* vk, const goggles_chain_vk_create_info_t* create_info,
    goggles_chain_t** out_chain);
GOGGLES_CHAIN_API goggles_chain_status_t GOGGLES_CHAIN_CALL goggles_chain_create_vk_ex(
    const goggles_chain_vk_context_t* vk, const goggles_chain_vk_create_info_ex_t* create_info,
    goggles_chain_t** out_chain);
GOGGLES_CHAIN_API goggles_chain_status_t GOGGLES_CHAIN_CALL
goggles_chain_destroy(goggles_chain_t** chain);

GOGGLES_CHAIN_API goggles_chain_status_t GOGGLES_CHAIN_CALL
goggles_chain_preset_load(goggles_chain_t* chain, const char* preset_path_utf8);
GOGGLES_CHAIN_API goggles_chain_status_t GOGGLES_CHAIN_CALL goggles_chain_preset_load_ex(
    goggles_chain_t* chain, const char* preset_path_utf8, size_t preset_path_len);
GOGGLES_CHAIN_API goggles_chain_status_t GOGGLES_CHAIN_CALL
goggles_chain_handle_resize(goggles_chain_t* chain, goggles_chain_extent2d_t new_target_extent);
GOGGLES_CHAIN_API goggles_chain_status_t GOGGLES_CHAIN_CALL
goggles_chain_record_vk(goggles_chain_t* chain, const goggles_chain_vk_record_info_t* record_info);

GOGGLES_CHAIN_API goggles_chain_status_t GOGGLES_CHAIN_CALL
goggles_chain_stage_policy_set(goggles_chain_t* chain, const goggles_chain_stage_policy_t* policy);
GOGGLES_CHAIN_API goggles_chain_status_t GOGGLES_CHAIN_CALL goggles_chain_stage_policy_get(
    const goggles_chain_t* chain, goggles_chain_stage_policy_t* out_policy);
GOGGLES_CHAIN_API goggles_chain_status_t GOGGLES_CHAIN_CALL
goggles_chain_prechain_resolution_set(goggles_chain_t* chain, goggles_chain_extent2d_t resolution);
GOGGLES_CHAIN_API goggles_chain_status_t GOGGLES_CHAIN_CALL goggles_chain_prechain_resolution_get(
    const goggles_chain_t* chain, goggles_chain_extent2d_t* out_resolution);

GOGGLES_CHAIN_API goggles_chain_status_t GOGGLES_CHAIN_CALL goggles_chain_control_list(
    const goggles_chain_t* chain, goggles_chain_control_snapshot_t** out_snapshot);
GOGGLES_CHAIN_API goggles_chain_status_t GOGGLES_CHAIN_CALL
goggles_chain_control_list_stage(const goggles_chain_t* chain, goggles_chain_stage_t stage,
                                 goggles_chain_control_snapshot_t** out_snapshot);
GOGGLES_CHAIN_API size_t GOGGLES_CHAIN_CALL
goggles_chain_control_snapshot_get_count(const goggles_chain_control_snapshot_t* snapshot);
GOGGLES_CHAIN_API const goggles_chain_control_desc_t* GOGGLES_CHAIN_CALL
goggles_chain_control_snapshot_get_data(const goggles_chain_control_snapshot_t* snapshot);
GOGGLES_CHAIN_API goggles_chain_status_t GOGGLES_CHAIN_CALL
goggles_chain_control_snapshot_destroy(goggles_chain_control_snapshot_t** snapshot);

GOGGLES_CHAIN_API goggles_chain_status_t GOGGLES_CHAIN_CALL goggles_chain_control_set_value(
    goggles_chain_t* chain, goggles_chain_control_id_t control_id, float value);
GOGGLES_CHAIN_API goggles_chain_status_t GOGGLES_CHAIN_CALL
goggles_chain_control_reset_value(goggles_chain_t* chain, goggles_chain_control_id_t control_id);
GOGGLES_CHAIN_API goggles_chain_status_t GOGGLES_CHAIN_CALL
goggles_chain_control_reset_all(goggles_chain_t* chain);
GOGGLES_CHAIN_API goggles_chain_status_t GOGGLES_CHAIN_CALL goggles_chain_error_last_info_get(
    const goggles_chain_t* chain, goggles_chain_error_last_info_t* out_info);

#undef GOGGLES_CHAIN_NOARGS
#undef GOGGLES_CHAIN_NULLPTR

#ifdef __cplusplus
}
#endif

#endif
