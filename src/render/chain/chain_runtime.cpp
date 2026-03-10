#include "chain_runtime.hpp"

#include "chain_builder.hpp"

#include <render/shader/shader_runtime.hpp>
#include <util/logging.hpp>
#include <util/profiling.hpp>

namespace goggles::render {

auto ChainRuntime::create(const VulkanContext& vk_ctx, vk::Format swapchain_format,
                          uint32_t num_sync_indices, const FilterChainPaths& paths,
                          vk::Extent2D source_resolution) -> ResultPtr<ChainRuntime> {
    auto chain = std::unique_ptr<ChainRuntime>(new ChainRuntime());

    chain->m_shader_runtime = GOGGLES_TRY(ShaderRuntime::create(paths.cache_dir));
    chain->m_resources = GOGGLES_TRY(
        ChainResources::create(vk_ctx, swapchain_format, num_sync_indices, *chain->m_shader_runtime,
                               paths.shader_dir, source_resolution));
    chain->set_stage_policy(true, true);

    return make_result_ptr(std::move(chain));
}

ChainRuntime::~ChainRuntime() {
    shutdown();
}

void ChainRuntime::shutdown() {
    if (m_resources) {
        m_resources->shutdown();
        m_resources.reset();
    }
    if (m_shader_runtime) {
        m_shader_runtime->shutdown();
        m_shader_runtime.reset();
    }
}

auto ChainRuntime::load_preset(const std::filesystem::path& preset_path) -> Result<void> {
    if (!m_resources) {
        return make_error<void>(ErrorCode::vulkan_init_failed, "Filter chain not initialized");
    }
    if (preset_path.empty()) {
        return {};
    }

    auto compiled = GOGGLES_TRY(ChainBuilder::build(
        m_resources->m_vk_ctx, *m_resources->m_shader_runtime, m_resources->m_num_sync_indices,
        *m_resources->m_texture_loader, preset_path));
    m_resources->install(std::move(compiled));
    m_controls.replay_values(*m_resources);
    return {};
}

auto ChainRuntime::handle_resize(vk::Extent2D new_viewport_extent) -> Result<void> {
    if (!m_resources) {
        return {};
    }
    return m_resources->handle_resize(new_viewport_extent);
}

void ChainRuntime::record(vk::CommandBuffer cmd, vk::Image original_image,
                          vk::ImageView original_view, vk::Extent2D original_extent,
                          vk::ImageView target_view, vk::Extent2D viewport_extent,
                          uint32_t frame_index, ScaleMode scale_mode, uint32_t integer_scale) {
    if (!m_resources) {
        return;
    }
    m_executor.record(*m_resources, cmd, original_image, original_view, original_extent,
                      target_view, viewport_extent, frame_index, scale_mode, integer_scale);
}

void ChainRuntime::set_stage_policy(bool prechain_enabled, bool effect_stage_enabled) {
    m_prechain_policy_enabled = prechain_enabled;
    m_effect_stage_policy_enabled = effect_stage_enabled;

    if (!m_resources) {
        return;
    }

    m_resources->set_prechain_enabled(m_prechain_policy_enabled);
    m_resources->set_bypass(!m_effect_stage_policy_enabled);
}

void ChainRuntime::set_prechain_resolution(vk::Extent2D resolution) {
    if (!m_resources) {
        return;
    }
    m_resources->set_prechain_resolution(resolution.width, resolution.height);
}

auto ChainRuntime::get_prechain_resolution() const -> vk::Extent2D {
    if (!m_resources) {
        return vk::Extent2D{};
    }
    return m_resources->get_prechain_resolution();
}

auto ChainRuntime::list_controls() const -> std::vector<FilterControlDescriptor> {
    if (!m_resources) {
        return {};
    }
    return m_controls.list_controls(*m_resources);
}

auto ChainRuntime::list_controls(FilterControlStage stage) const
    -> std::vector<FilterControlDescriptor> {
    if (!m_resources) {
        return {};
    }
    return m_controls.list_controls(*m_resources, stage);
}

auto ChainRuntime::set_control_value(FilterControlId control_id, float value) -> bool {
    if (!m_resources) {
        return false;
    }
    return m_controls.set_control_value(*m_resources, control_id, value);
}

auto ChainRuntime::reset_control_value(FilterControlId control_id) -> bool {
    if (!m_resources) {
        return false;
    }
    return m_controls.reset_control_value(*m_resources, control_id);
}

void ChainRuntime::reset_controls() {
    if (!m_resources) {
        return;
    }
    m_controls.reset_controls(*m_resources);
}

} // namespace goggles::render
