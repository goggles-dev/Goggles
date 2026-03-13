#include "filter_chain_controller.hpp"

#include <chrono>
#include <limits>
#include <util/job_system.hpp>
#include <util/logging.hpp>
#include <util/profiling.hpp>

namespace goggles::render::backend_internal {

namespace {

auto to_reporting_mode(std::string_view mode) -> ChainDiagnosticReportingMode {
    if (mode == "minimal") {
        return ChainDiagnosticReportingMode::minimal;
    }
    if (mode == "investigate") {
        return ChainDiagnosticReportingMode::investigate;
    }
    if (mode == "forensic") {
        return ChainDiagnosticReportingMode::forensic;
    }
    return ChainDiagnosticReportingMode::standard;
}

auto create_diagnostics_session(FilterChainRuntime& runtime, const Config::Diagnostics& diagnostics)
    -> Result<void> {
    return runtime.create_diagnostics_session(ChainDiagnosticsCreateInfo{
        .reporting_mode = to_reporting_mode(diagnostics.mode),
        .policy_mode = diagnostics.strict ? ChainDiagnosticPolicyMode::strict
                                          : ChainDiagnosticPolicyMode::compatibility,
        .activation_tier = diagnostics.tier,
        .capture_frame_limit = diagnostics.capture_frame_limit,
        .retention_bytes = diagnostics.retention_bytes,
    });
}

auto to_chain_stage(FilterControlStage stage) -> ChainControlStage {
    switch (stage) {
    case FilterControlStage::prechain:
        return ChainControlStage::prechain;
    case FilterControlStage::effect:
    default:
        return ChainControlStage::effect;
    }
}

auto to_filter_stage(ChainControlStage stage) -> FilterControlStage {
    switch (stage) {
    case ChainControlStage::prechain:
        return FilterControlStage::prechain;
    case ChainControlStage::effect:
    case ChainControlStage::postchain:
    default:
        return FilterControlStage::effect;
    }
}

auto to_filter_descriptor(const ChainControlDescriptor& descriptor) -> FilterControlDescriptor {
    return FilterControlDescriptor{
        .control_id = descriptor.control_id,
        .stage = to_filter_stage(descriptor.stage),
        .name = descriptor.name,
        .description = descriptor.description,
        .current_value = descriptor.current_value,
        .default_value = descriptor.default_value,
        .min_value = descriptor.min_value,
        .max_value = descriptor.max_value,
        .step = descriptor.step,
    };
}

void destroy_filter_chain(FilterChainRuntime& chain, const char* failure_message) {
    if (!chain) {
        return;
    }

    auto destroy_result = chain.destroy();
    if (!destroy_result) {
        GOGGLES_LOG_WARN("{}: {}", failure_message, destroy_result.error().message);
    }
}

auto create_filter_chain_runtime(const FilterChainController::RuntimeBuildConfig& config)
    -> Result<FilterChainRuntime> {
    auto filter_chain_result = FilterChainRuntime::create(ChainCreateInfo{
        .device = config.vulkan_context.device,
        .physical_device = config.vulkan_context.physical_device,
        .graphics_queue = config.vulkan_context.graphics_queue,
        .graphics_queue_family_index = config.graphics_queue_family_index,
        .target_format = config.target_format,
        .num_sync_indices = config.num_sync_indices,
        .shader_dir = config.shader_dir,
        .cache_dir = config.cache_dir,
        .initial_prechain_resolution = config.initial_prechain_resolution,
    });
    if (!filter_chain_result) {
        return nonstd::make_unexpected(filter_chain_result.error());
    }

    if (config.diagnostics_config.has_value()) {
        GOGGLES_TRY(
            create_diagnostics_session(filter_chain_result.value(), *config.diagnostics_config));
    }

    return std::move(filter_chain_result.value());
}

auto snapshot_runtime_controls(const FilterChainRuntime& chain)
    -> std::vector<FilterChainController::ControlOverride> {
    if (!chain) {
        return {};
    }

    auto controls_result = chain.list_controls();
    if (!controls_result) {
        GOGGLES_LOG_WARN("Failed to snapshot filter controls: {}", controls_result.error().message);
        return {};
    }

    std::vector<FilterChainController::ControlOverride> controls;
    controls.reserve(controls_result->size());
    for (const auto& descriptor : controls_result.value()) {
        controls.push_back(FilterChainController::ControlOverride{
            .control_id = descriptor.control_id,
            .value = descriptor.current_value,
        });
    }
    return controls;
}

auto apply_runtime_state(FilterChainRuntime& chain, const ChainStagePolicy& policy,
                         vk::Extent2D prechain_resolution,
                         const std::vector<FilterChainController::ControlOverride>& controls,
                         const char* control_failure_prefix) -> Result<void> {
    auto policy_result = chain.set_stage_policy(policy);
    if (!policy_result) {
        return make_error<void>(policy_result.error().code,
                                "Failed to apply filter-chain stage policy: " +
                                    policy_result.error().message);
    }

    auto resolution_result = chain.set_prechain_resolution(prechain_resolution);
    if (!resolution_result) {
        return make_error<void>(resolution_result.error().code,
                                "Failed to set prechain resolution: " +
                                    resolution_result.error().message);
    }

    for (const auto& control : controls) {
        auto set_result = chain.set_control_value(control.control_id, control.value);
        if (!set_result) {
            GOGGLES_LOG_WARN("{}: {}", control_failure_prefix, set_result.error().message);
            continue;
        }
        if (!set_result.value()) {
            GOGGLES_LOG_WARN("{}: control {} not found", control_failure_prefix,
                             control.control_id);
        }
    }

    return {};
}

auto align_runtime_output_target(FilterChainRuntime& chain,
                                 const FilterChainController::OutputTarget& output_target,
                                 std::string_view operation_context) -> Result<void> {
    if (output_target.format == vk::Format::eUndefined) {
        return make_error<void>(ErrorCode::invalid_data,
                                "Authoritative output format is undefined");
    }
    if (output_target.extent.width == 0 || output_target.extent.height == 0) {
        return make_error<void>(ErrorCode::invalid_data, "Authoritative output extent is zero");
    }

    auto retarget_result = chain.retarget_output(output_target.format);
    if (!retarget_result) {
        return make_error<void>(retarget_result.error().code, std::string("Failed to retarget ") +
                                                                  std::string(operation_context) +
                                                                  ": " +
                                                                  retarget_result.error().message);
    }

    auto resize_result = chain.handle_resize(output_target.extent);
    if (!resize_result) {
        return make_error<void>(resize_result.error().code,
                                std::string("Failed to resize ") + std::string(operation_context) +
                                    ": " + resize_result.error().message);
    }

    return {};
}

auto fallback_retire_after_frame(uint64_t frame_count) -> uint64_t {
    constexpr uint64_t MAX_FRAME = std::numeric_limits<uint64_t>::max();
    constexpr uint64_t RETIRE_DELAY =
        FilterChainController::RetiredRuntimeTracker::FALLBACK_RETIRE_DELAY_FRAMES;
    return frame_count > (MAX_FRAME - RETIRE_DELAY) ? MAX_FRAME : frame_count + RETIRE_DELAY;
}

void retire_runtime_with_bounded_fallback(
    FilterChainController::RetiredRuntimeTracker& retired_runtimes,
    FilterChainRuntime retired_runtime, uint64_t frame_count,
    const std::function<void()>& wait_all_frames) {
    if (!retired_runtime) {
        return;
    }

    if (retired_runtimes.retired_count <
        FilterChainController::RetiredRuntimeTracker::MAX_RETIRED_RUNTIMES) {
        auto& retired = retired_runtimes.retired_runtimes[retired_runtimes.retired_count++];
        retired.filter_chain = std::move(retired_runtime);
        retired.destroy_after_frame = fallback_retire_after_frame(frame_count);
        return;
    }

    GOGGLES_LOG_WARN("Retired runtime queue full, forcing immediate retirement");
    wait_all_frames();
    destroy_filter_chain(retired_runtime, "Failed to destroy retired filter chain");
}

void cleanup_retired_runtime_tracker(FilterChainController::RetiredRuntimeTracker& retired_runtimes,
                                     uint64_t frame_count) {
    size_t write_idx = 0;
    for (size_t i = 0; i < retired_runtimes.retired_count; ++i) {
        if (frame_count >= retired_runtimes.retired_runtimes[i].destroy_after_frame) {
            GOGGLES_LOG_DEBUG("Destroying retired filter chain");
            destroy_filter_chain(retired_runtimes.retired_runtimes[i].filter_chain,
                                 "Failed to destroy retired filter chain");
            retired_runtimes.retired_runtimes[i].destroy_after_frame = 0;
            continue;
        }

        if (write_idx != i) {
            retired_runtimes.retired_runtimes[write_idx].filter_chain =
                std::move(retired_runtimes.retired_runtimes[i].filter_chain);
            retired_runtimes.retired_runtimes[write_idx].destroy_after_frame =
                retired_runtimes.retired_runtimes[i].destroy_after_frame;
        }
        ++write_idx;
    }
    retired_runtimes.retired_count = write_idx;
}

void shutdown_retired_runtime_tracker(
    FilterChainController::RetiredRuntimeTracker& retired_runtimes) {
    for (size_t i = 0; i < retired_runtimes.retired_count; ++i) {
        destroy_filter_chain(retired_runtimes.retired_runtimes[i].filter_chain,
                             "Retired filter chain destroy during shutdown failed");
        retired_runtimes.retired_runtimes[i].destroy_after_frame = 0;
    }
    retired_runtimes.retired_count = 0;
}

} // namespace

auto FilterChainController::recreate_filter_chain(const RuntimeBuildConfig& config)
    -> Result<void> {
    GOGGLES_PROFILE_FUNCTION();

    authoritative_output_target = OutputTarget{
        .format = config.target_format,
        .extent = config.target_extent,
    };

    const auto control_state = authoritative_control_overrides.empty()
                                   ? snapshot_runtime_controls(filter_chain)
                                   : authoritative_control_overrides;

    if (filter_chain) {
        auto destroy_result = filter_chain.destroy();
        if (!destroy_result) {
            return make_error<void>(ErrorCode::unknown_error,
                                    "Failed to destroy existing filter chain: " +
                                        destroy_result.error().message);
        }
    }

    auto filter_chain_result = create_filter_chain_runtime(config);
    if (!filter_chain_result) {
        return nonstd::make_unexpected(filter_chain_result.error());
    }
    filter_chain = std::move(filter_chain_result.value());

    const ChainStagePolicy policy{
        .prechain_enabled = prechain_policy_enabled,
        .effect_stage_enabled = effect_stage_policy_enabled,
    };

    auto load_result = filter_chain.load_preset(preset_path);
    if (!load_result) {
        GOGGLES_LOG_WARN("Failed to reload shader preset after chain rebuild: {}",
                         load_result.error().message);
    }

    auto restore_result =
        apply_runtime_state(filter_chain, policy, source_resolution, control_state,
                            "Failed to restore filter control value after chain rebuild");
    if (!restore_result) {
        destroy_filter_chain(filter_chain, "Failed to destroy filter chain after restore failure");
        filter_chain = {};
        return nonstd::make_unexpected(restore_result.error());
    }

    auto output_result = align_runtime_output_target(filter_chain, authoritative_output_target,
                                                     "filter chain after chain rebuild");
    if (!output_result) {
        destroy_filter_chain(filter_chain, "Failed to destroy filter chain after output failure");
        filter_chain = {};
        return nonstd::make_unexpected(output_result.error());
    }

    authoritative_control_overrides = snapshot_runtime_controls(filter_chain);

    return {};
}

auto FilterChainController::retarget_filter_chain(const OutputTarget& output_target)
    -> Result<void> {
    authoritative_output_target = output_target;

    if (!filter_chain) {
        return {};
    }

    auto retarget_result = align_runtime_output_target(filter_chain, authoritative_output_target,
                                                       "active filter chain after retarget");
    if (!retarget_result) {
        return nonstd::make_unexpected(retarget_result.error());
    }

    authoritative_control_overrides = snapshot_runtime_controls(filter_chain);
    return {};
}

void FilterChainController::shutdown(const std::function<void()>& wait_for_gpu_idle) {
    if (pending_load_future.valid()) {
        auto status = pending_load_future.wait_for(std::chrono::seconds(3));
        if (status == std::future_status::timeout) {
            GOGGLES_LOG_WARN(
                "Shader load task still running during shutdown, waiting for completion");
        }

        try {
            auto pending_result = pending_load_future.get();
            if (!pending_result) {
                GOGGLES_LOG_WARN("Pending shader load finished with error during shutdown: {}",
                                 pending_result.error().message);
            }
        } catch (const std::exception& ex) {
            GOGGLES_LOG_WARN("Pending shader load threw exception during shutdown: {}", ex.what());
        } catch (...) {
            GOGGLES_LOG_WARN("Pending shader load threw unknown exception during shutdown");
        }
    }

    pending_chain_ready.store(false, std::memory_order_release);

    wait_for_gpu_idle();

    destroy_filter_chain(filter_chain, "Filter chain destroy during shutdown failed");
    destroy_filter_chain(pending_filter_chain,
                         "Pending filter chain destroy during shutdown failed");

    shutdown_retired_runtime_tracker(retired_runtimes);
}

void FilterChainController::load_shader_preset(const std::filesystem::path& new_preset_path) {
    GOGGLES_PROFILE_FUNCTION();

    preset_path = new_preset_path;

    if (!filter_chain) {
        GOGGLES_LOG_WARN("Filter chain not initialized; preset load skipped");
        return;
    }

    auto load_result = filter_chain.load_preset(new_preset_path);
    if (!load_result) {
        GOGGLES_LOG_WARN("Failed to load shader preset '{}': {} - falling back to passthrough",
                         new_preset_path.string(), load_result.error().message);
    } else if (new_preset_path.empty()) {
        GOGGLES_LOG_DEBUG("No shader preset specified, using passthrough mode");
    }

    authoritative_control_overrides = snapshot_runtime_controls(filter_chain);
}

auto FilterChainController::reload_shader_preset(std::filesystem::path new_preset_path,
                                                 RuntimeBuildConfig config) -> Result<void> {
    GOGGLES_PROFILE_FUNCTION();

    if (pending_chain_ready.load(std::memory_order_acquire)) {
        GOGGLES_LOG_WARN("Shader reload already pending, ignoring request");
        return {};
    }

    if (pending_load_future.valid() &&
        pending_load_future.wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
        GOGGLES_LOG_WARN("Shader compilation in progress, ignoring request");
        return {};
    }

    pending_preset_path = new_preset_path;
    const ChainStagePolicy requested_policy{
        .prechain_enabled = prechain_policy_enabled,
        .effect_stage_enabled = effect_stage_policy_enabled,
    };
    const auto requested_output_target = authoritative_output_target;
    const auto requested_prechain_resolution = source_resolution;
    auto requested_controls = authoritative_control_overrides.empty()
                                  ? snapshot_runtime_controls(filter_chain)
                                  : authoritative_control_overrides;

    pending_load_future = util::JobSystem::submit(
        [this, build_config = std::move(config), requested_preset_path = std::move(new_preset_path),
         requested_policy, requested_output_target, requested_prechain_resolution,
         requested_controls = std::move(requested_controls)]() -> Result<void> {
            GOGGLES_PROFILE_SCOPE("AsyncShaderLoad");

            auto pending_chain_result = create_filter_chain_runtime(build_config);
            if (!pending_chain_result) {
                GOGGLES_LOG_ERROR("Failed to create filter chain");
                return nonstd::make_unexpected(pending_chain_result.error());
            }
            auto pending_chain = std::move(pending_chain_result.value());

            auto load_result = pending_chain.load_preset(requested_preset_path);
            if (!load_result) {
                GOGGLES_LOG_ERROR("Failed to load preset '{}'", requested_preset_path.string());
                return load_result;
            }

            GOGGLES_TRY(apply_runtime_state(pending_chain, requested_policy,
                                            requested_prechain_resolution, requested_controls,
                                            "Failed to restore filter control value before swap"));
            GOGGLES_TRY(align_runtime_output_target(pending_chain, requested_output_target,
                                                    "pending filter chain before swap"));

            pending_filter_chain = std::move(pending_chain);
            pending_chain_ready.store(true, std::memory_order_release);

            GOGGLES_LOG_INFO("Shader preset compiled: {}", requested_preset_path.empty()
                                                               ? "(passthrough)"
                                                               : requested_preset_path.string());
            return {};
        });

    return {};
}

void FilterChainController::advance_frame() {
    ++frame_count;
}

void FilterChainController::check_pending_chain_swap(const std::function<void()>& wait_all_frames) {
    if (!pending_chain_ready.load(std::memory_order_acquire)) {
        return;
    }

    if (pending_load_future.valid()) {
        Result<void> result;
        try {
            result = pending_load_future.get();
        } catch (const std::exception& ex) {
            GOGGLES_LOG_ERROR("Async shader load threw exception: {}", ex.what());
            destroy_filter_chain(pending_filter_chain, "Failed to destroy pending filter chain");
            pending_chain_ready.store(false, std::memory_order_release);
            return;
        } catch (...) {
            GOGGLES_LOG_ERROR("Async shader load threw unknown exception");
            destroy_filter_chain(pending_filter_chain, "Failed to destroy pending filter chain");
            pending_chain_ready.store(false, std::memory_order_release);
            return;
        }

        if (!result) {
            GOGGLES_LOG_ERROR("Async shader load failed: {}", result.error().message);
            destroy_filter_chain(pending_filter_chain, "Failed to destroy pending filter chain");
            pending_chain_ready.store(false, std::memory_order_release);
            return;
        }
    }

    auto restore_result =
        apply_runtime_state(pending_filter_chain,
                            ChainStagePolicy{
                                .prechain_enabled = prechain_policy_enabled,
                                .effect_stage_enabled = effect_stage_policy_enabled,
                            },
                            source_resolution, authoritative_control_overrides,
                            "Failed to restore filter control value before swap");
    if (!restore_result) {
        GOGGLES_LOG_ERROR("Failed to restore filter-chain runtime state before swap: {}",
                          restore_result.error().message);
        destroy_filter_chain(pending_filter_chain, "Failed to destroy pending filter chain");
        pending_chain_ready.store(false, std::memory_order_release);
        return;
    }

    auto output_result =
        align_runtime_output_target(pending_filter_chain, authoritative_output_target,
                                    "pending filter chain before activation");
    if (!output_result) {
        GOGGLES_LOG_ERROR("Failed to align pending filter-chain output before swap: {}",
                          output_result.error().message);
        destroy_filter_chain(pending_filter_chain, "Failed to destroy pending filter chain");
        pending_chain_ready.store(false, std::memory_order_release);
        return;
    }

    retire_runtime_with_bounded_fallback(retired_runtimes, std::move(filter_chain), frame_count,
                                         wait_all_frames);

    filter_chain = std::move(pending_filter_chain);
    authoritative_control_overrides = snapshot_runtime_controls(filter_chain);
    preset_path = pending_preset_path;
    pending_chain_ready.store(false, std::memory_order_release);
    chain_swapped.store(true, std::memory_order_release);

    GOGGLES_LOG_INFO("Shader chain swapped: {}",
                     preset_path.empty() ? "(passthrough)" : preset_path.string());
}

void FilterChainController::cleanup_retired_runtimes() {
    cleanup_retired_runtime_tracker(retired_runtimes, frame_count);
}

void FilterChainController::set_stage_policy(const ChainStagePolicy& policy) {
    prechain_policy_enabled = policy.prechain_enabled;
    effect_stage_policy_enabled = policy.effect_stage_enabled;

    if (!filter_chain) {
        return;
    }

    auto policy_result = filter_chain.set_stage_policy(policy);
    if (!policy_result) {
        GOGGLES_LOG_WARN("Failed to apply filter-chain stage policy: {}",
                         policy_result.error().message);
    }
}

void FilterChainController::set_prechain_resolution(const PrechainResolutionConfig& config) {
    source_resolution = config.requested_resolution;
    if (filter_chain) {
        auto set_result = filter_chain.set_prechain_resolution(config.requested_resolution);
        if (!set_result) {
            GOGGLES_LOG_WARN("Failed to set prechain resolution: {}", set_result.error().message);
        }
    }
}

auto FilterChainController::handle_resize(vk::Extent2D target_extent) -> Result<void> {
    if (!filter_chain) {
        return {};
    }

    return filter_chain.handle_resize(target_extent);
}

auto FilterChainController::record(const ChainRecordInfo& record_info) -> Result<void> {
    if (!filter_chain) {
        return make_error<void>(ErrorCode::vulkan_init_failed, "Filter chain not initialized");
    }

    return filter_chain.record(record_info);
}

auto FilterChainController::current_prechain_resolution() const -> vk::Extent2D {
    if (filter_chain) {
        auto resolution_result = filter_chain.get_prechain_resolution();
        if (resolution_result) {
            return resolution_result.value();
        }
    }
    return source_resolution;
}

auto FilterChainController::list_filter_controls() const -> std::vector<FilterControlDescriptor> {
    if (!filter_chain) {
        return {};
    }

    auto controls_result = filter_chain.list_controls();
    if (!controls_result) {
        GOGGLES_LOG_WARN("Failed to list filter controls: {}", controls_result.error().message);
        return {};
    }

    std::vector<FilterControlDescriptor> controls;
    controls.reserve(controls_result->size());
    for (const auto& descriptor : controls_result.value()) {
        controls.push_back(to_filter_descriptor(descriptor));
    }
    return controls;
}

auto FilterChainController::list_filter_controls(FilterControlStage stage) const
    -> std::vector<FilterControlDescriptor> {
    if (!filter_chain) {
        return {};
    }

    auto controls_result = filter_chain.list_controls(to_chain_stage(stage));
    if (!controls_result) {
        GOGGLES_LOG_WARN("Failed to list stage filter controls: {}",
                         controls_result.error().message);
        return {};
    }

    std::vector<FilterControlDescriptor> controls;
    controls.reserve(controls_result->size());
    for (const auto& descriptor : controls_result.value()) {
        controls.push_back(to_filter_descriptor(descriptor));
    }
    return controls;
}

auto FilterChainController::set_filter_control_value(FilterControlId control_id, float value)
    -> bool {
    if (!filter_chain) {
        return false;
    }

    auto set_result = filter_chain.set_control_value(control_id, value);
    if (!set_result) {
        GOGGLES_LOG_WARN("Failed to set filter control value: {}", set_result.error().message);
        return false;
    }
    if (set_result.value()) {
        authoritative_control_overrides = snapshot_runtime_controls(filter_chain);
    }
    return set_result.value();
}

auto FilterChainController::reset_filter_control_value(FilterControlId control_id) -> bool {
    if (!filter_chain) {
        return false;
    }

    auto reset_result = filter_chain.reset_control_value(control_id);
    if (!reset_result) {
        GOGGLES_LOG_WARN("Failed to reset filter control value: {}", reset_result.error().message);
        return false;
    }
    if (reset_result.value()) {
        authoritative_control_overrides = snapshot_runtime_controls(filter_chain);
    }
    return reset_result.value();
}

void FilterChainController::reset_filter_controls() {
    if (!filter_chain) {
        return;
    }

    auto reset_result = filter_chain.reset_all_controls();
    if (!reset_result) {
        GOGGLES_LOG_WARN("Failed to reset filter controls: {}", reset_result.error().message);
        return;
    }

    authoritative_control_overrides = snapshot_runtime_controls(filter_chain);
}

} // namespace goggles::render::backend_internal
