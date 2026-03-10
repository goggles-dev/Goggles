#pragma once

#include "chain_resources.hpp"

#include <vulkan/vulkan.hpp>

namespace goggles::render {

/// @brief Records filter-chain commands into a Vulkan command buffer.
class ChainExecutor {
public:
    void record(ChainResources& resources, vk::CommandBuffer cmd, vk::Image original_image,
                vk::ImageView original_view, vk::Extent2D original_extent,
                vk::ImageView swapchain_view, vk::Extent2D viewport_extent, uint32_t frame_index,
                ScaleMode scale_mode = ScaleMode::stretch, uint32_t integer_scale = 0);

private:
    struct ChainResult {
        vk::ImageView view;
        vk::Extent2D extent;
    };

    auto record_prechain(ChainResources& resources, vk::CommandBuffer cmd,
                         vk::ImageView original_view, vk::Extent2D original_extent,
                         uint32_t frame_index) -> ChainResult;
    void record_postchain(ChainResources& resources, vk::CommandBuffer cmd,
                          vk::ImageView source_view, vk::Extent2D source_extent,
                          vk::ImageView target_view, vk::Extent2D target_extent,
                          uint32_t frame_index, ScaleMode scale_mode, uint32_t integer_scale);

    void bind_pass_textures(ChainResources& resources, FilterPass& pass, size_t pass_index,
                            vk::ImageView original_view, vk::Extent2D original_extent,
                            vk::ImageView source_view);
    void copy_feedback_framebuffers(ChainResources& resources, vk::CommandBuffer cmd);
};

} // namespace goggles::render
