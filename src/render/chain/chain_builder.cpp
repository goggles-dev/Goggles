#include "chain_builder.hpp"

#include "vulkan_result.hpp"

#include <charconv>
#include <render/shader/retroarch_preprocessor.hpp>
#include <unordered_set>
#include <util/logging.hpp>
#include <util/profiling.hpp>

namespace goggles::render {

namespace {

auto parse_original_history_index(std::string_view name) -> std::optional<uint32_t> {
    constexpr std::string_view PREFIX = "OriginalHistory";
    if (!name.starts_with(PREFIX)) {
        return std::nullopt;
    }
    auto suffix = name.substr(PREFIX.size());
    if (suffix.empty()) {
        return std::nullopt;
    }
    uint32_t index = 0;
    const auto* end = suffix.data() + suffix.size();
    auto [ptr, ec] = std::from_chars(suffix.data(), end, index);
    if (ptr != end) {
        return std::nullopt;
    }
    return index;
}

constexpr std::string_view FEEDBACK_SUFFIX = "Feedback";

auto parse_feedback_alias(std::string_view name) -> std::optional<std::string> {
    if (!name.ends_with(FEEDBACK_SUFFIX)) {
        return std::nullopt;
    }
    auto alias = name.substr(0, name.size() - FEEDBACK_SUFFIX.size());
    if (alias.empty()) {
        return std::nullopt;
    }
    return std::string(alias);
}

auto parse_pass_feedback_index(std::string_view name) -> std::optional<size_t> {
    constexpr std::string_view PREFIX = "PassFeedback";
    if (!name.starts_with(PREFIX)) {
        return std::nullopt;
    }
    auto suffix = name.substr(PREFIX.size());
    if (suffix.empty()) {
        return std::nullopt;
    }
    size_t index = 0;
    const auto* end = suffix.data() + suffix.size();
    auto [ptr, ec] = std::from_chars(suffix.data(), end, index);
    if (ptr != end) {
        return std::nullopt;
    }
    return index;
}

} // namespace

auto ChainBuilder::build(const VulkanContext& vk_ctx, ShaderRuntime& shader_runtime,
                         uint32_t num_sync_indices, TextureLoader& texture_loader,
                         const std::filesystem::path& preset_path) -> Result<CompiledChain> {
    GOGGLES_PROFILE_FUNCTION();

    PresetParser parser;
    auto preset_result = parser.load(preset_path);
    if (!preset_result) {
        return make_error<CompiledChain>(preset_result.error().code, preset_result.error().message);
    }

    std::vector<std::unique_ptr<FilterPass>> new_passes;
    std::unordered_map<std::string, size_t> new_alias_map;
    RetroArchPreprocessor preprocessor;

    for (size_t i = 0; i < preset_result->passes.size(); ++i) {
        const auto& pass_config = preset_result->passes[i];
        auto preprocessed = preprocessor.preprocess(pass_config.shader_path);
        if (!preprocessed) {
            return make_error<CompiledChain>(preprocessed.error().code,
                                             preprocessed.error().message);
        }

        FilterPassConfig config{
            .target_format = pass_config.framebuffer_format,
            .num_sync_indices = num_sync_indices,
            .vertex_source = preprocessed->vertex_source,
            .fragment_source = preprocessed->fragment_source,
            .shader_name = pass_config.shader_path.stem().string(),
            .filter_mode = pass_config.filter_mode,
            .mipmap = pass_config.mipmap,
            .wrap_mode = pass_config.wrap_mode,
            .parameters = preprocessed->parameters,
        };
        auto pass = GOGGLES_TRY(FilterPass::create(vk_ctx, shader_runtime, config));

        for (const auto& override : preset_result->parameters) {
            pass->set_parameter_override(override.name, override.value);
        }
        auto ubo_result = pass->update_ubo_parameters();
        if (!ubo_result) {
            return make_error<CompiledChain>(ubo_result.error().code, ubo_result.error().message);
        }

        new_passes.push_back(std::move(pass));

        if (pass_config.alias.has_value()) {
            new_alias_map[*pass_config.alias] = i;
        }
    }

    uint32_t required_history_depth = 0;
    std::unordered_set<size_t> feedback_pass_indices;
    for (const auto& pass : new_passes) {
        for (const auto& tex : pass->texture_bindings()) {
            if (auto idx = parse_original_history_index(tex.name)) {
                required_history_depth = std::max(required_history_depth, *idx + 1);
            }
            if (auto alias = parse_feedback_alias(tex.name)) {
                if (auto it = new_alias_map.find(*alias); it != new_alias_map.end()) {
                    feedback_pass_indices.insert(it->second);
                    GOGGLES_LOG_DEBUG("Detected feedback texture '{}' -> pass {} (alias '{}')",
                                      tex.name, it->second, *alias);
                }
            }
            if (auto fb_idx = parse_pass_feedback_index(tex.name)) {
                if (*fb_idx < new_passes.size()) {
                    feedback_pass_indices.insert(*fb_idx);
                    GOGGLES_LOG_DEBUG("Detected PassFeedback{} texture", *fb_idx);
                }
            }
        }
    }
    if (required_history_depth > 0) {
        required_history_depth = std::min(required_history_depth, FrameHistory::MAX_HISTORY);
        GOGGLES_LOG_DEBUG("Detected OriginalHistory usage, depth={}", required_history_depth);
    }

    auto texture_registry =
        GOGGLES_TRY(load_preset_textures(vk_ctx, texture_loader, *preset_result));

    GOGGLES_LOG_INFO(
        "FilterChain loaded preset: {} ({} passes, {} textures, {} aliases, {} params)",
        preset_path.filename().string(), new_passes.size(), texture_registry.size(),
        new_alias_map.size(), preset_result->parameters.size());
    for (const auto& [alias, pass_idx] : new_alias_map) {
        GOGGLES_LOG_DEBUG("  Alias '{}' -> pass {}", alias, pass_idx);
    }

    return CompiledChain{
        .preset = std::move(*preset_result),
        .passes = std::move(new_passes),
        .alias_to_pass_index = std::move(new_alias_map),
        .required_history_depth = required_history_depth,
        .texture_registry = std::move(texture_registry),
        .feedback_pass_indices = std::move(feedback_pass_indices),
    };
}

auto ChainBuilder::load_preset_textures(const VulkanContext& vk_ctx, TextureLoader& texture_loader,
                                        const PresetConfig& preset)
    -> Result<std::unordered_map<std::string, LoadedTexture>> {
    GOGGLES_PROFILE_SCOPE("LoadPresetTextures");

    std::unordered_map<std::string, LoadedTexture> registry;

    for (const auto& tex_config : preset.textures) {
        TextureLoadConfig load_cfg{.generate_mipmaps = tex_config.mipmap,
                                   .linear = tex_config.linear};

        auto tex_data_result = texture_loader.load_from_file(tex_config.path, load_cfg);
        if (!tex_data_result) {
            return nonstd::make_unexpected(tex_data_result.error());
        }

        auto sampler_result = create_texture_sampler(vk_ctx, tex_config);
        if (!sampler_result) {
            auto& loaded = tex_data_result.value();
            if (loaded.view) {
                vk_ctx.device.destroyImageView(loaded.view);
            }
            if (loaded.memory) {
                vk_ctx.device.freeMemory(loaded.memory);
            }
            if (loaded.image) {
                vk_ctx.device.destroyImage(loaded.image);
            }
            return nonstd::make_unexpected(sampler_result.error());
        }
        auto sampler = sampler_result.value();

        auto texture_data = tex_data_result.value();
        registry[tex_config.name] = LoadedTexture{.data = texture_data, .sampler = sampler};

        GOGGLES_LOG_DEBUG("Loaded texture '{}' from {}", tex_config.name,
                          tex_config.path.filename().string());
    }
    return registry;
}

auto ChainBuilder::create_texture_sampler(const VulkanContext& vk_ctx, const TextureConfig& config)
    -> Result<vk::Sampler> {
    vk::Filter filter =
        (config.filter_mode == FilterMode::linear) ? vk::Filter::eLinear : vk::Filter::eNearest;

    vk::SamplerAddressMode address_mode;
    switch (config.wrap_mode) {
    case WrapMode::clamp_to_edge:
        address_mode = vk::SamplerAddressMode::eClampToEdge;
        break;
    case WrapMode::repeat:
        address_mode = vk::SamplerAddressMode::eRepeat;
        break;
    case WrapMode::mirrored_repeat:
        address_mode = vk::SamplerAddressMode::eMirroredRepeat;
        break;
    case WrapMode::clamp_to_border:
    default:
        address_mode = vk::SamplerAddressMode::eClampToBorder;
        break;
    }

    vk::SamplerMipmapMode mipmap_mode = (config.filter_mode == FilterMode::linear)
                                            ? vk::SamplerMipmapMode::eLinear
                                            : vk::SamplerMipmapMode::eNearest;

    vk::SamplerCreateInfo sampler_info{};
    sampler_info.magFilter = filter;
    sampler_info.minFilter = filter;
    sampler_info.addressModeU = address_mode;
    sampler_info.addressModeV = address_mode;
    sampler_info.addressModeW = address_mode;
    sampler_info.anisotropyEnable = VK_FALSE;
    sampler_info.maxAnisotropy = 1.0f;
    sampler_info.borderColor = vk::BorderColor::eFloatTransparentBlack;
    sampler_info.unnormalizedCoordinates = VK_FALSE;
    sampler_info.compareEnable = VK_FALSE;
    sampler_info.compareOp = vk::CompareOp::eAlways;
    sampler_info.mipmapMode = mipmap_mode;
    sampler_info.mipLodBias = 0.0f;
    sampler_info.minLod = 0.0f;
    sampler_info.maxLod = config.mipmap ? VK_LOD_CLAMP_NONE : 0.0f;

    auto [result, sampler] = vk_ctx.device.createSampler(sampler_info);
    if (result != vk::Result::eSuccess) {
        return make_error<vk::Sampler>(ErrorCode::vulkan_init_failed,
                                       "Failed to create texture sampler: " +
                                           vk::to_string(result));
    }
    return Result<vk::Sampler>{sampler};
}

} // namespace goggles::render
