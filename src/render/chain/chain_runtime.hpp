#pragma once

#include "chain_controls.hpp"
#include "chain_executor.hpp"
#include "chain_resources.hpp"
#include "filter_controls.hpp"
#include "vulkan_context.hpp"

#include <filesystem>
#include <memory>
#include <util/config.hpp>
#include <util/error.hpp>
#include <vector>
#include <vulkan/vulkan.hpp>

namespace goggles::render {

class ShaderRuntime;

struct FilterChainPaths {
    std::filesystem::path shader_dir;
    std::filesystem::path cache_dir;
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

private:
    ChainRuntime() = default;

    std::unique_ptr<ShaderRuntime> m_shader_runtime;
    std::unique_ptr<ChainResources> m_resources;
    ChainExecutor m_executor;
    ChainControls m_controls;
    bool m_prechain_policy_enabled = true;
    bool m_effect_stage_policy_enabled = true;
};

} // namespace goggles::render
