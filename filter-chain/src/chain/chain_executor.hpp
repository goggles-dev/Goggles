#pragma once

#include "chain_resources.hpp"
#include "diagnostics/diagnostic_session.hpp"
#include "diagnostics/gpu_timestamp_pool.hpp"

#include <vulkan/vulkan.hpp>

namespace goggles::render {

/// @brief Records filter-chain commands into a Vulkan command buffer.
class ChainExecutor {
public:
    void record(ChainResources& resources, vk::CommandBuffer cmd, vk::Image original_image,
                vk::ImageView original_view, vk::Extent2D original_extent,
                vk::ImageView swapchain_view, vk::Extent2D viewport_extent, uint32_t frame_index,
                ScaleMode scale_mode = ScaleMode::stretch, uint32_t integer_scale = 0,
                diagnostics::DiagnosticSession* session = nullptr,
                diagnostics::GpuTimestampPool* gpu_timestamp_pool = nullptr);

private:
    struct BindPassResult {
        bool strict_fallback_forbidden = false;
    };

    struct ChainResult {
        vk::ImageView view;
        vk::Extent2D extent;
    };

    auto record_prechain(ChainResources& resources, vk::CommandBuffer cmd,
                         vk::ImageView original_view, vk::Extent2D original_extent,
                         uint32_t frame_index) -> ChainResult;
    auto record_prechain_region(ChainResources& resources, vk::CommandBuffer cmd,
                                vk::ImageView original_view, vk::Extent2D original_extent,
                                uint32_t frame_index, diagnostics::DiagnosticSession* session,
                                diagnostics::GpuTimestampPool* gpu_timestamp_pool) -> ChainResult;
    void record_postchain(ChainResources& resources, vk::CommandBuffer cmd,
                          vk::ImageView source_view, vk::Extent2D source_extent,
                          vk::ImageView target_view, vk::Extent2D target_extent,
                          uint32_t frame_index, ScaleMode scale_mode, uint32_t integer_scale);
    void record_final_composition(ChainResources& resources, vk::CommandBuffer cmd,
                                  vk::ImageView source_view, vk::Extent2D source_extent,
                                  vk::ImageView target_view, vk::Extent2D target_extent,
                                  uint32_t frame_index, ScaleMode scale_mode,
                                  uint32_t integer_scale, diagnostics::DiagnosticSession* session,
                                  diagnostics::GpuTimestampPool* gpu_timestamp_pool);

    auto bind_pass_textures(ChainResources& resources, FilterPass& pass, size_t pass_index,
                            vk::ImageView original_view, vk::Extent2D original_extent,
                            vk::ImageView source_view,
                            diagnostics::DiagnosticSession* session = nullptr) -> BindPassResult;
    void copy_feedback_framebuffers(ChainResources& resources, vk::CommandBuffer cmd);
};

} // namespace goggles::render
