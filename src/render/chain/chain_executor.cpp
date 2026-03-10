#include "chain_executor.hpp"

#include <format>
#include <util/logging.hpp>
#include <util/profiling.hpp>

namespace goggles::render {

namespace {

constexpr std::string_view FEEDBACK_SUFFIX = "Feedback";

struct LayoutTransition {
    vk::ImageLayout from;
    vk::ImageLayout to;
};

void transition_image_layout(vk::CommandBuffer cmd, vk::Image image, LayoutTransition transition) {
    vk::ImageMemoryBarrier barrier{};
    barrier.oldLayout = transition.from;
    barrier.newLayout = transition.to;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1};

    vk::PipelineStageFlags src_stage;
    vk::PipelineStageFlags dst_stage;
    if (transition.to == vk::ImageLayout::eColorAttachmentOptimal) {
        barrier.srcAccessMask = vk::AccessFlagBits::eShaderRead;
        barrier.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
        src_stage = vk::PipelineStageFlagBits::eFragmentShader;
        dst_stage = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    } else {
        barrier.srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
        barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
        src_stage = vk::PipelineStageFlagBits::eColorAttachmentOutput;
        dst_stage = vk::PipelineStageFlagBits::eFragmentShader;
    }

    cmd.pipelineBarrier(src_stage, dst_stage, {}, {}, {}, barrier);
}

} // namespace

auto ChainExecutor::record_prechain(ChainResources& resources, vk::CommandBuffer cmd,
                                    vk::ImageView original_view, vk::Extent2D original_extent,
                                    uint32_t frame_index) -> ChainResult {
    if (resources.m_prechain_passes.empty() || resources.m_prechain_framebuffers.empty()) {
        return {.view = original_view, .extent = original_extent};
    }

    vk::ImageView current_view = original_view;
    vk::Extent2D current_extent = original_extent;

    for (size_t i = 0; i < resources.m_prechain_passes.size(); ++i) {
        auto& pass = resources.m_prechain_passes[i];
        auto& framebuffer = resources.m_prechain_framebuffers[i];
        auto output_extent = framebuffer->extent();

        transition_image_layout(
            cmd, framebuffer->image(),
            {.from = vk::ImageLayout::eUndefined, .to = vk::ImageLayout::eColorAttachmentOptimal});

        PassContext ctx{};
        ctx.frame_index = frame_index;
        ctx.source_extent = current_extent;
        ctx.output_extent = output_extent;
        ctx.target_image_view = framebuffer->view();
        ctx.target_format = framebuffer->format();
        ctx.source_texture = current_view;
        ctx.original_texture = original_view;
        ctx.scale_mode = ScaleMode::stretch;
        ctx.integer_scale = 0;

        pass->record(cmd, ctx);

        transition_image_layout(cmd, framebuffer->image(),
                                {.from = vk::ImageLayout::eColorAttachmentOptimal,
                                 .to = vk::ImageLayout::eShaderReadOnlyOptimal});

        GOGGLES_LOG_TRACE("Pre-chain pass {}: {}x{} -> {}x{}", i, current_extent.width,
                          current_extent.height, output_extent.width, output_extent.height);

        current_view = framebuffer->view();
        current_extent = output_extent;
    }

    return {.view = current_view, .extent = current_extent};
}

void ChainExecutor::record_postchain(ChainResources& resources, vk::CommandBuffer cmd,
                                     vk::ImageView source_view, vk::Extent2D source_extent,
                                     vk::ImageView target_view, vk::Extent2D target_extent,
                                     uint32_t frame_index, ScaleMode scale_mode,
                                     uint32_t integer_scale) {
    if (resources.m_postchain_passes.empty()) {
        return;
    }

    vk::ImageView current_view = source_view;
    vk::Extent2D current_extent = source_extent;

    for (size_t i = 0; i < resources.m_postchain_passes.size(); ++i) {
        auto& pass = resources.m_postchain_passes[i];
        bool is_final = (i == resources.m_postchain_passes.size() - 1);

        vk::ImageView pass_target;
        vk::Extent2D pass_output_extent;
        vk::Format pass_format;

        if (is_final) {
            pass_target = target_view;
            pass_output_extent = target_extent;
            pass_format = resources.m_swapchain_format;
        } else {
            auto& framebuffer = resources.m_postchain_framebuffers[i];
            pass_target = framebuffer->view();
            pass_output_extent = framebuffer->extent();
            pass_format = framebuffer->format();

            transition_image_layout(cmd, framebuffer->image(),
                                    {.from = vk::ImageLayout::eUndefined,
                                     .to = vk::ImageLayout::eColorAttachmentOptimal});
        }

        PassContext ctx{};
        ctx.frame_index = frame_index;
        ctx.source_extent = current_extent;
        ctx.output_extent = pass_output_extent;
        ctx.target_image_view = pass_target;
        ctx.target_format = pass_format;
        ctx.source_texture = current_view;
        ctx.original_texture = source_view;
        ctx.scale_mode = scale_mode;
        ctx.integer_scale = integer_scale;

        pass->record(cmd, ctx);

        if (!is_final) {
            auto& framebuffer = resources.m_postchain_framebuffers[i];

            transition_image_layout(cmd, framebuffer->image(),
                                    {.from = vk::ImageLayout::eColorAttachmentOptimal,
                                     .to = vk::ImageLayout::eShaderReadOnlyOptimal});

            current_view = framebuffer->view();
            current_extent = pass_output_extent;
        }
    }
}

void ChainExecutor::record(ChainResources& resources, vk::CommandBuffer cmd,
                           vk::Image original_image, vk::ImageView original_view,
                           vk::Extent2D original_extent, vk::ImageView swapchain_view,
                           vk::Extent2D viewport_extent, uint32_t frame_index, ScaleMode scale_mode,
                           uint32_t integer_scale) {
    GOGGLES_PROFILE_FUNCTION();

    resources.m_last_scale_mode = scale_mode;
    resources.m_last_integer_scale = integer_scale;
    resources.m_last_source_extent = original_extent;

    vk::Image effective_original_image = original_image;
    vk::ImageView effective_original_view = original_view;
    vk::Extent2D effective_original_extent = original_extent;
    if (resources.m_prechain_enabled.load(std::memory_order_relaxed)) {
        GOGGLES_MUST(resources.ensure_prechain_passes(original_extent));
        auto prechain_result =
            record_prechain(resources, cmd, original_view, original_extent, frame_index);
        if (!resources.m_prechain_framebuffers.empty()) {
            effective_original_image = resources.m_prechain_framebuffers.back()->image();
        }
        effective_original_view = prechain_result.view;
        effective_original_extent = prechain_result.extent;
    }

    GOGGLES_MUST(resources.ensure_frame_history(effective_original_extent));

    if (resources.m_passes.empty() || resources.m_bypass_enabled.load(std::memory_order_relaxed)) {
        record_postchain(resources, cmd, effective_original_view, effective_original_extent,
                         swapchain_view, viewport_extent, frame_index, scale_mode, integer_scale);
        resources.m_frame_count++;
        return;
    }

    auto vp = calculate_viewport(effective_original_extent.width, effective_original_extent.height,
                                 viewport_extent.width, viewport_extent.height, scale_mode,
                                 integer_scale);
    GOGGLES_MUST(resources.ensure_framebuffers(
        {.viewport = viewport_extent, .source = effective_original_extent}, {vp.width, vp.height}));

    for (auto& [pass_idx, feedback_fb] : resources.m_feedback_framebuffers) {
        if (!feedback_fb) {
            continue;
        }

        bool initialized = false;
        if (auto it = resources.m_feedback_initialized.find(pass_idx);
            it != resources.m_feedback_initialized.end()) {
            initialized = it->second;
        }
        if (initialized) {
            continue;
        }

        vk::ImageMemoryBarrier init_barrier{};
        init_barrier.srcAccessMask = {};
        init_barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
        init_barrier.oldLayout = vk::ImageLayout::eUndefined;
        init_barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        init_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        init_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        init_barrier.image = feedback_fb->image();
        init_barrier.subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1};
        cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe,
                            vk::PipelineStageFlagBits::eFragmentShader, {}, {}, {}, init_barrier);
        resources.m_feedback_initialized[pass_idx] = true;
    }

    vk::ImageView source_view = effective_original_view;
    vk::Extent2D source_extent = effective_original_extent;

    for (size_t i = 0; i < resources.m_passes.size(); ++i) {
        auto& pass = resources.m_passes[i];

        vk::ImageView target_view = resources.m_framebuffers[i]->view();
        vk::Extent2D target_extent = resources.m_framebuffers[i]->extent();
        vk::Format target_format = resources.m_framebuffers[i]->format();

        transition_image_layout(
            cmd, resources.m_framebuffers[i]->image(),
            {.from = vk::ImageLayout::eUndefined, .to = vk::ImageLayout::eColorAttachmentOptimal});

        pass->set_source_size(source_extent.width, source_extent.height);
        pass->set_output_size(target_extent.width, target_extent.height);
        pass->set_original_size(effective_original_extent.width, effective_original_extent.height);
        pass->set_frame_count(resources.m_frame_count,
                              resources.m_preset.passes[i].frame_count_mod);
        pass->set_final_viewport_size(vp.width, vp.height);
        pass->set_rotation(0);

        bind_pass_textures(resources, *pass, i, effective_original_view, effective_original_extent,
                           source_view);

        PassContext ctx{};
        ctx.frame_index = frame_index;
        ctx.output_extent = target_extent;
        ctx.source_extent = source_extent;
        ctx.target_image_view = target_view;
        ctx.target_format = target_format;
        ctx.source_texture = source_view;
        ctx.original_texture = effective_original_view;
        ctx.scale_mode = scale_mode;
        ctx.integer_scale = integer_scale;

        pass->record(cmd, ctx);

        transition_image_layout(cmd, resources.m_framebuffers[i]->image(),
                                {.from = vk::ImageLayout::eColorAttachmentOptimal,
                                 .to = vk::ImageLayout::eShaderReadOnlyOptimal});

        source_view = resources.m_framebuffers[i]->view();
        source_extent = resources.m_framebuffers[i]->extent();
    }

    record_postchain(resources, cmd, source_view, source_extent, swapchain_view, viewport_extent,
                     frame_index, scale_mode, integer_scale);

    if (resources.m_frame_history.is_initialized()) {
        resources.m_frame_history.push(cmd, effective_original_image, effective_original_extent);
    }

    copy_feedback_framebuffers(resources, cmd);
    resources.m_frame_count++;
}

void ChainExecutor::bind_pass_textures(ChainResources& resources, FilterPass& pass,
                                       size_t pass_index, vk::ImageView original_view,
                                       vk::Extent2D original_extent, vk::ImageView source_view) {
    pass.clear_alias_sizes();
    pass.clear_texture_bindings();

    pass.set_texture_binding("Source", source_view, nullptr);
    pass.set_texture_binding("Original", original_view, nullptr);

    pass.set_texture_binding("OriginalHistory0", original_view, nullptr);
    pass.set_alias_size("OriginalHistory0", original_extent.width, original_extent.height);

    for (uint32_t h = 0; h < resources.m_required_history_depth; ++h) {
        auto name = std::format("OriginalHistory{}", h + 1);
        if (auto hist_view = resources.m_frame_history.get(h)) {
            pass.set_texture_binding(name, hist_view, nullptr);
            auto ext = resources.m_frame_history.get_extent(h);
            pass.set_alias_size(name, ext.width, ext.height);
        } else {
            pass.set_texture_binding(name, original_view, nullptr);
            pass.set_alias_size(name, original_extent.width, original_extent.height);
        }
    }

    for (size_t p = 0; p < pass_index; ++p) {
        if (resources.m_framebuffers[p]) {
            auto pass_name = std::format("PassOutput{}", p);
            auto pass_extent = resources.m_framebuffers[p]->extent();
            pass.set_texture_binding(pass_name, resources.m_framebuffers[p]->view(), nullptr);
            pass.set_alias_size(pass_name, pass_extent.width, pass_extent.height);
        }
    }

    for (const auto& [fb_idx, feedback_fb] : resources.m_feedback_framebuffers) {
        auto feedback_name = std::format("PassFeedback{}", fb_idx);
        if (feedback_fb) {
            pass.set_texture_binding(feedback_name, feedback_fb->view(), nullptr);
            auto fb_extent = feedback_fb->extent();
            pass.set_alias_size(feedback_name, fb_extent.width, fb_extent.height);
        } else {
            pass.set_texture_binding(feedback_name, source_view, nullptr);
        }
    }

    for (const auto& [alias, idx] : resources.m_alias_to_pass_index) {
        if (idx < pass_index && resources.m_framebuffers[idx]) {
            pass.set_texture_binding(alias, resources.m_framebuffers[idx]->view(), nullptr);
            auto alias_extent = resources.m_framebuffers[idx]->extent();
            pass.set_alias_size(alias, alias_extent.width, alias_extent.height);
        }
        if (auto fb_it = resources.m_feedback_framebuffers.find(idx);
            fb_it != resources.m_feedback_framebuffers.end() && fb_it->second) {
            auto feedback_name = alias + std::string(FEEDBACK_SUFFIX);
            pass.set_texture_binding(feedback_name, fb_it->second->view(), nullptr);
            auto fb_extent = fb_it->second->extent();
            pass.set_alias_size(feedback_name, fb_extent.width, fb_extent.height);
        }
    }

    for (const auto& [name, tex] : resources.m_texture_registry) {
        pass.set_texture_binding(name, tex.data.view, tex.sampler);
    }
}

void ChainExecutor::copy_feedback_framebuffers(ChainResources& resources, vk::CommandBuffer cmd) {
    for (auto& [pass_idx, feedback_fb] : resources.m_feedback_framebuffers) {
        if (!feedback_fb || !resources.m_framebuffers[pass_idx]) {
            continue;
        }
        auto extent = resources.m_framebuffers[pass_idx]->extent();
        vk::ImageSubresourceLayers layers{vk::ImageAspectFlagBits::eColor, 0, 0, 1};
        vk::ImageCopy region{
            layers, {0, 0, 0}, layers, {0, 0, 0}, {extent.width, extent.height, 1}};

        std::array<vk::ImageMemoryBarrier, 2> pre{};
        pre[0].srcAccessMask = vk::AccessFlagBits::eShaderRead;
        pre[0].dstAccessMask = vk::AccessFlagBits::eTransferRead;
        pre[0].oldLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        pre[0].newLayout = vk::ImageLayout::eTransferSrcOptimal;
        pre[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        pre[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        pre[0].image = resources.m_framebuffers[pass_idx]->image();
        pre[0].subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1};

        pre[1].srcAccessMask = vk::AccessFlagBits::eShaderRead;
        pre[1].dstAccessMask = vk::AccessFlagBits::eTransferWrite;
        pre[1].oldLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        pre[1].newLayout = vk::ImageLayout::eTransferDstOptimal;
        pre[1].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        pre[1].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        pre[1].image = feedback_fb->image();
        pre[1].subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1};

        cmd.pipelineBarrier(vk::PipelineStageFlagBits::eFragmentShader,
                            vk::PipelineStageFlagBits::eTransfer, {}, {}, {}, pre);

        cmd.copyImage(resources.m_framebuffers[pass_idx]->image(),
                      vk::ImageLayout::eTransferSrcOptimal, feedback_fb->image(),
                      vk::ImageLayout::eTransferDstOptimal, region);

        std::array<vk::ImageMemoryBarrier, 2> post{};
        post[0].srcAccessMask = vk::AccessFlagBits::eTransferRead;
        post[0].dstAccessMask = vk::AccessFlagBits::eShaderRead;
        post[0].oldLayout = vk::ImageLayout::eTransferSrcOptimal;
        post[0].newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        post[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        post[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        post[0].image = resources.m_framebuffers[pass_idx]->image();
        post[0].subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1};

        post[1].srcAccessMask = vk::AccessFlagBits::eTransferWrite;
        post[1].dstAccessMask = vk::AccessFlagBits::eShaderRead;
        post[1].oldLayout = vk::ImageLayout::eTransferDstOptimal;
        post[1].newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        post[1].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        post[1].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        post[1].image = feedback_fb->image();
        post[1].subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1};

        cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
                            vk::PipelineStageFlagBits::eFragmentShader, {}, {}, {}, post);
    }
}

} // namespace goggles::render
