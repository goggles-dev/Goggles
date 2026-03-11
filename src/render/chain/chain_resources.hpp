#pragma once

#include "chain_builder.hpp"
#include "downsample_pass.hpp"
#include "filter_pass.hpp"
#include "frame_history.hpp"
#include "framebuffer.hpp"
#include "output_pass.hpp"
#include "pass.hpp"
#include "preset_parser.hpp"
#include "vulkan_context.hpp"

#include <atomic>
#include <memory>
#include <render/texture/texture_loader.hpp>
#include <unordered_map>
#include <util/diagnostics/diagnostic_session.hpp>
#include <vector>

namespace goggles::render {

/// @brief User-facing parameter state for UI and overrides.
struct ParameterInfo {
    std::string name;
    std::string description;
    float current_value;
    float default_value;
    float min_value;
    float max_value;
    float step;
};

/// @brief Viewport and source extents used for framebuffer sizing.
struct FramebufferExtents {
    vk::Extent2D viewport;
    vk::Extent2D source;
};

/// @brief Owns all mutable GPU allocations for the filter chain.
class ChainResources {
public:
    [[nodiscard]] static auto create(const VulkanContext& vk_ctx, vk::Format swapchain_format,
                                     uint32_t num_sync_indices, ShaderRuntime& shader_runtime,
                                     const std::filesystem::path& shader_dir,
                                     vk::Extent2D source_resolution = {0, 0})
        -> ResultPtr<ChainResources>;

    ~ChainResources();

    ChainResources(const ChainResources&) = delete;
    ChainResources& operator=(const ChainResources&) = delete;
    ChainResources(ChainResources&&) = delete;
    ChainResources& operator=(ChainResources&&) = delete;

    void shutdown();
    void install(CompiledChain&& compiled, diagnostics::DiagnosticSession* session = nullptr);

    [[nodiscard]] auto ensure_framebuffers(const FramebufferExtents& extents,
                                           vk::Extent2D viewport_extent) -> Result<void>;
    [[nodiscard]] auto ensure_frame_history(vk::Extent2D extent) -> Result<void>;
    [[nodiscard]] auto ensure_prechain_passes(vk::Extent2D captured_extent) -> Result<void>;
    void apply_prechain_parameters();
    void cleanup_texture_registry();

    [[nodiscard]] auto handle_resize(vk::Extent2D new_viewport_extent) -> Result<void>;

    void set_prechain_resolution(uint32_t width, uint32_t height);
    [[nodiscard]] auto get_prechain_resolution() const -> vk::Extent2D;

    void set_bypass(bool enabled) { m_bypass_enabled.store(enabled, std::memory_order_relaxed); }
    [[nodiscard]] auto is_bypass() const -> bool {
        return m_bypass_enabled.load(std::memory_order_relaxed);
    }
    void set_prechain_enabled(bool enabled) {
        m_prechain_enabled.store(enabled, std::memory_order_relaxed);
    }
    [[nodiscard]] auto is_prechain_enabled() const -> bool {
        return m_prechain_enabled.load(std::memory_order_relaxed);
    }

    [[nodiscard]] auto pass_count() const -> size_t { return m_passes.size(); }

    [[nodiscard]] static auto calculate_pass_output_size(const ShaderPassConfig& pass_config,
                                                         vk::Extent2D source_extent,
                                                         vk::Extent2D viewport_extent)
        -> vk::Extent2D;

    // Parameter management
    [[nodiscard]] auto get_all_parameters() const -> std::vector<ParameterInfo>;
    void set_parameter(const std::string& name, float value);
    void reset_parameter(const std::string& name);
    void clear_parameter_overrides();
    [[nodiscard]] auto get_prechain_parameters() const -> std::vector<ShaderParameter>;
    void set_prechain_parameter(const std::string& name, float value);

    VulkanContext m_vk_ctx;
    vk::Format m_swapchain_format = vk::Format::eUndefined;
    uint32_t m_num_sync_indices = 0;
    ShaderRuntime* m_shader_runtime = nullptr;
    std::filesystem::path m_shader_dir;

    std::vector<std::unique_ptr<FilterPass>> m_passes;
    std::vector<std::unique_ptr<Framebuffer>> m_framebuffers;

    PresetConfig m_preset;
    uint32_t m_frame_count = 0;

    std::unique_ptr<TextureLoader> m_texture_loader;
    std::unordered_map<std::string, LoadedTexture> m_texture_registry;
    std::unordered_map<std::string, size_t> m_alias_to_pass_index;
    std::unordered_map<size_t, std::unique_ptr<Framebuffer>> m_feedback_framebuffers;
    std::unordered_map<size_t, bool> m_feedback_initialized;
    uint64_t m_generation_id = 0;

    ScaleMode m_last_scale_mode = ScaleMode::stretch;
    uint32_t m_last_integer_scale = 0;
    vk::Extent2D m_last_source_extent;

    FrameHistory m_frame_history;
    uint32_t m_required_history_depth = 0;
    std::atomic<bool> m_bypass_enabled{false};
    std::atomic<bool> m_prechain_enabled{true};

    // Pre-chain stage
    vk::Extent2D m_prechain_requested_resolution;
    vk::Extent2D m_prechain_resolved_resolution;
    vk::Extent2D m_prechain_last_captured_extent;
    std::vector<ShaderParameter> m_prechain_parameters;
    std::vector<std::unique_ptr<Pass>> m_prechain_passes;
    std::vector<std::unique_ptr<Framebuffer>> m_prechain_framebuffers;

    // Post-chain stage
    std::vector<std::unique_ptr<Pass>> m_postchain_passes;
    std::vector<std::unique_ptr<Framebuffer>> m_postchain_framebuffers;

private:
    ChainResources() = default;
};

} // namespace goggles::render
