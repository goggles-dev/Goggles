#pragma once

#include "chain_controls.hpp"
#include "chain_executor.hpp"
#include "chain_resources.hpp"

#include <cstdint>
#include <filesystem>
#include <goggles/filter_chain/filter_controls.hpp>
#include <goggles/filter_chain/result.hpp>
#include <goggles/filter_chain/scale_mode.hpp>
#include <goggles/filter_chain/vulkan_context.hpp>
#include <memory>
#include <util/diagnostics/diagnostic_policy.hpp>
#include <util/diagnostics/diagnostic_session.hpp>
#include <util/diagnostics/gpu_timestamp_pool.hpp>
#include <vector>
#include <vulkan/vulkan.hpp>

namespace goggles::render {

class ShaderRuntime;

struct FilterChainPaths {
    std::filesystem::path shader_dir;
    std::filesystem::path cache_dir;
};

struct CapturedImage {
    std::vector<std::uint8_t> rgba;
    uint32_t width = 0;
    uint32_t height = 0;
};

/// @brief Boundary filter-chain API that owns runtime internals.
class ChainRuntime {
public:
    [[nodiscard]] static auto create(const VulkanContext& vk_ctx, vk::Format swapchain_format,
                                     uint32_t num_sync_indices, const FilterChainPaths& paths,
                                     vk::Extent2D source_resolution = {0, 0})
        -> ResultPtr<ChainRuntime>;

    ~ChainRuntime();

    ChainRuntime(const ChainRuntime&) = delete;
    ChainRuntime& operator=(const ChainRuntime&) = delete;
    ChainRuntime(ChainRuntime&&) = delete;
    ChainRuntime& operator=(ChainRuntime&&) = delete;

    void shutdown();

    [[nodiscard]] auto load_preset(const std::filesystem::path& preset_path) -> Result<void>;
    [[nodiscard]] auto handle_resize(vk::Extent2D new_viewport_extent) -> Result<void>;
    [[nodiscard]] auto retarget_output(vk::Format swapchain_format) -> Result<void>;

    void record(vk::CommandBuffer cmd, vk::Image original_image, vk::ImageView original_view,
                vk::Extent2D original_extent, vk::ImageView target_view,
                vk::Extent2D viewport_extent, uint32_t frame_index,
                ScaleMode scale_mode = ScaleMode::stretch, uint32_t integer_scale = 0);

    void set_stage_policy(bool prechain_enabled, bool effect_stage_enabled);

    void set_prechain_resolution(vk::Extent2D resolution);
    [[nodiscard]] auto get_prechain_resolution() const -> vk::Extent2D;

    [[nodiscard]] auto list_controls() const -> std::vector<FilterControlDescriptor>;
    [[nodiscard]] auto list_controls(FilterControlStage stage) const
        -> std::vector<FilterControlDescriptor>;

    [[nodiscard]] auto set_control_value(FilterControlId control_id, float value) -> bool;
    [[nodiscard]] auto reset_control_value(FilterControlId control_id) -> bool;
    void reset_controls();

    void create_diagnostic_session(diagnostics::DiagnosticPolicy policy);
    void destroy_diagnostic_session();
    [[nodiscard]] auto diagnostic_session() -> diagnostics::DiagnosticSession*;
    [[nodiscard]] auto diagnostic_session() const -> const diagnostics::DiagnosticSession*;
    [[nodiscard]] auto capture_pass_output(uint32_t pass_ordinal) const -> Result<CapturedImage>;

private:
    ChainRuntime() = default;
    void sync_gpu_timestamp_pool();

    std::unique_ptr<ShaderRuntime> m_shader_runtime;
    std::unique_ptr<ChainResources> m_resources;
    std::unique_ptr<diagnostics::GpuTimestampPool> m_gpu_timestamp_pool;
    std::unique_ptr<diagnostics::DiagnosticSession> m_diagnostic_session;
    ChainExecutor m_executor;
    ChainControls m_controls;
    bool m_prechain_policy_enabled = true;
    bool m_effect_stage_policy_enabled = true;
};

} // namespace goggles::render
