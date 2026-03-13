#include "render/backend/filter_chain_controller.hpp"
#include "render/chain/api/cpp/goggles_filter_chain.hpp"

#include <atomic>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <thread>
#include <vector>
#include <vulkan/vulkan.hpp>

namespace {

constexpr uint32_t TEST_SYNC_INDICES = 2u;

auto make_cache_dir() -> std::filesystem::path {
    static std::atomic<uint64_t> counter{0u};
    auto path = std::filesystem::temp_directory_path() /
                ("goggles_filter_chain_retarget_" + std::to_string(counter.fetch_add(1u)));
    std::filesystem::create_directories(path);
    return path;
}

auto read_text_file(const std::filesystem::path& path) -> std::optional<std::string> {
    std::ifstream file(path);
    if (!file.is_open()) {
        return std::nullopt;
    }
    return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}

struct VulkanRuntimeFixture {
    VulkanRuntimeFixture() {
        VkApplicationInfo app_info{};
        app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        app_info.pApplicationName = "goggles_filter_chain_retarget_tests";
        app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        app_info.apiVersion = VK_API_VERSION_1_3;

        VkInstanceCreateInfo instance_info{};
        instance_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        instance_info.pApplicationInfo = &app_info;
        if (vkCreateInstance(&instance_info, nullptr, &m_instance) != VK_SUCCESS) {
            return;
        }
        VULKAN_HPP_DEFAULT_DISPATCHER.init(vkGetInstanceProcAddr);
        VULKAN_HPP_DEFAULT_DISPATCHER.init(vk::Instance{m_instance});

        uint32_t device_count = 0u;
        if (vkEnumeratePhysicalDevices(m_instance, &device_count, nullptr) != VK_SUCCESS ||
            device_count == 0u) {
            return;
        }

        std::vector<VkPhysicalDevice> devices(device_count);
        if (vkEnumeratePhysicalDevices(m_instance, &device_count, devices.data()) != VK_SUCCESS) {
            return;
        }

        for (const auto candidate : devices) {
            uint32_t family_count = 0u;
            vkGetPhysicalDeviceQueueFamilyProperties(candidate, &family_count, nullptr);
            if (family_count == 0u) {
                continue;
            }

            std::vector<VkQueueFamilyProperties> families(family_count);
            vkGetPhysicalDeviceQueueFamilyProperties(candidate, &family_count, families.data());
            for (uint32_t family = 0u; family < family_count; ++family) {
                if ((families[family].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0u) {
                    continue;
                }

                m_physical_device = candidate;
                m_queue_family_index = family;
                goto found_queue;
            }
        }

    found_queue:
        if (m_physical_device == VK_NULL_HANDLE) {
            return;
        }

        float queue_priority = 1.0f;
        VkDeviceQueueCreateInfo queue_info{};
        queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_info.queueFamilyIndex = m_queue_family_index;
        queue_info.queueCount = 1u;
        queue_info.pQueuePriorities = &queue_priority;

        VkPhysicalDeviceVulkan13Features features13{};
        features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;

        VkPhysicalDeviceFeatures2 features2{};
        features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        features2.pNext = &features13;
        vkGetPhysicalDeviceFeatures2(m_physical_device, &features2);
        if (features13.dynamicRendering != VK_TRUE) {
            return;
        }
        features13.dynamicRendering = VK_TRUE;

        VkDeviceCreateInfo device_info{};
        device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        device_info.queueCreateInfoCount = 1u;
        device_info.pQueueCreateInfos = &queue_info;
        device_info.pNext = &features13;
        if (vkCreateDevice(m_physical_device, &device_info, nullptr, &m_device) != VK_SUCCESS) {
            return;
        }

        vkGetDeviceQueue(m_device, m_queue_family_index, 0u, &m_queue);
        VULKAN_HPP_DEFAULT_DISPATCHER.init(vk::Device{m_device});
    }

    ~VulkanRuntimeFixture() {
        if (m_device != VK_NULL_HANDLE) {
            vkDeviceWaitIdle(m_device);
            vkDestroyDevice(m_device, nullptr);
        }
        if (m_instance != VK_NULL_HANDLE) {
            vkDestroyInstance(m_instance, nullptr);
        }
    }

    [[nodiscard]] auto available() const -> bool { return m_device != VK_NULL_HANDLE; }

    [[nodiscard]] auto create_info() const -> goggles::render::ChainCreateInfo {
        return goggles::render::ChainCreateInfo{
            .device = vk::Device{m_device},
            .physical_device = vk::PhysicalDevice{m_physical_device},
            .graphics_queue = vk::Queue{m_queue},
            .graphics_queue_family_index = m_queue_family_index,
            .target_format = vk::Format::eB8G8R8A8Unorm,
            .num_sync_indices = TEST_SYNC_INDICES,
            .shader_dir = {},
            .cache_dir = {},
            .initial_prechain_resolution = {},
        };
    }

private:
    VkInstance m_instance = VK_NULL_HANDLE;
    VkPhysicalDevice m_physical_device = VK_NULL_HANDLE;
    VkDevice m_device = VK_NULL_HANDLE;
    VkQueue m_queue = VK_NULL_HANDLE;
    uint32_t m_queue_family_index = UINT32_MAX;
};

struct CacheDirGuard {
    explicit CacheDirGuard(std::filesystem::path path) : dir(std::move(path)) {}
    ~CacheDirGuard() {
        if (!dir.empty()) {
            std::filesystem::remove_all(dir);
        }
    }

    std::filesystem::path dir;
};

auto find_control(const std::vector<goggles::render::ChainControlDescriptor>& controls,
                  std::string_view name) -> const goggles::render::ChainControlDescriptor* {
    for (const auto& control : controls) {
        if (control.name == name) {
            return &control;
        }
    }
    return nullptr;
}

auto find_filter_control(const std::vector<goggles::render::FilterControlDescriptor>& controls,
                         std::string_view name) -> const goggles::render::FilterControlDescriptor* {
    for (const auto& control : controls) {
        if (control.name == name) {
            return &control;
        }
    }
    return nullptr;
}

auto make_runtime_build_config(const VulkanRuntimeFixture& fixture,
                               const std::filesystem::path& shader_root,
                               const std::filesystem::path& cache_dir,
                               vk::Format target_format = vk::Format::eB8G8R8A8Unorm,
                               vk::Extent2D target_extent = {1u, 1u})
    -> goggles::render::backend_internal::FilterChainController::RuntimeBuildConfig {
    return {
        .vulkan_context = {.device = vk::Device{fixture.create_info().device},
                           .physical_device =
                               vk::PhysicalDevice{fixture.create_info().physical_device},
                           .command_pool = vk::CommandPool{},
                           .graphics_queue = vk::Queue{fixture.create_info().graphics_queue}},
        .graphics_queue_family_index = fixture.create_info().graphics_queue_family_index,
        .target_format = target_format,
        .target_extent = target_extent,
        .num_sync_indices = TEST_SYNC_INDICES,
        .shader_dir = shader_root,
        .cache_dir = cache_dir,
        .initial_prechain_resolution = {1u, 1u},
        .diagnostics_config = std::nullopt,
    };
}

void configure_controller_runtime(
    goggles::render::backend_internal::FilterChainController& controller,
    const std::filesystem::path& preset_path) {
    controller.preset_path = preset_path;
    controller.set_stage_policy({.prechain_enabled = true, .effect_stage_enabled = false});
    controller.set_prechain_resolution({.requested_resolution = {2u, 3u}});

    const auto controls =
        controller.list_filter_controls(goggles::render::FilterControlStage::prechain);
    const auto* filter_type = find_filter_control(controls, "filter_type");
    REQUIRE(filter_type != nullptr);
    REQUIRE(controller.set_filter_control_value(filter_type->control_id, 2.0F));
}

void require_controller_state(goggles::render::backend_internal::FilterChainController& controller,
                              const std::filesystem::path& preset_path) {
    REQUIRE(controller.current_preset_path() == preset_path);
    REQUIRE(controller.current_prechain_resolution() == vk::Extent2D{2u, 3u});

    const auto policy_result = controller.filter_chain.get_stage_policy();
    REQUIRE(policy_result.has_value());
    REQUIRE(policy_result->prechain_enabled);
    REQUIRE(!policy_result->effect_stage_enabled);

    const auto controls =
        controller.list_filter_controls(goggles::render::FilterControlStage::prechain);
    const auto* filter_type = find_filter_control(controls, "filter_type");
    REQUIRE(filter_type != nullptr);
    REQUIRE(filter_type->current_value == Catch::Approx(2.0F));
}

void wait_for_reload_start(
    const goggles::render::backend_internal::FilterChainController& controller) {
    using namespace std::chrono_literals;

    for (int attempt = 0; attempt < 200; ++attempt) {
        if (controller.pending_chain_ready.load(std::memory_order_acquire)) {
            return;
        }
        if (controller.pending_load_future.valid() &&
            controller.pending_load_future.wait_for(0ms) == std::future_status::ready) {
            break;
        }
        std::this_thread::sleep_for(10ms);
    }

    FAIL("Timed out waiting for pending shader reload to become observable");
}

} // namespace

TEST_CASE("Filter chain output retarget preserves runtime state", "[filter_chain][retarget]") {
    VulkanRuntimeFixture fixture;
    if (!fixture.available()) {
        SKIP("Skipping Vulkan-backed retarget test because no Vulkan graphics device is available");
    }

    const auto shader_root = std::filesystem::path(GOGGLES_SOURCE_DIR) / "shaders";
    const auto preset_path =
        std::filesystem::path(GOGGLES_SOURCE_DIR) / "shaders/retroarch/test/format.slangp";
    REQUIRE(std::filesystem::exists(shader_root));
    REQUIRE(std::filesystem::exists(preset_path));

    CacheDirGuard cache_dir_guard(make_cache_dir());
    auto create_info = fixture.create_info();
    create_info.shader_dir = shader_root;
    create_info.cache_dir = cache_dir_guard.dir;
    create_info.initial_prechain_resolution = vk::Extent2D{1u, 1u};

    auto runtime_result = goggles::render::FilterChainRuntime::create(create_info);
    REQUIRE(runtime_result.has_value());
    auto runtime = std::move(runtime_result.value());

    REQUIRE(runtime.load_preset(preset_path).has_value());
    REQUIRE(runtime.set_stage_policy({.prechain_enabled = true, .effect_stage_enabled = false})
                .has_value());
    REQUIRE(runtime.set_prechain_resolution(vk::Extent2D{2u, 3u}).has_value());

    auto controls_result = runtime.list_controls(goggles::render::ChainControlStage::prechain);
    REQUIRE(controls_result.has_value());
    const auto* filter_type = find_control(*controls_result, "filter_type");
    REQUIRE(filter_type != nullptr);
    REQUIRE(runtime.set_control_value(filter_type->control_id, 2.0F).has_value());
    REQUIRE(runtime.set_control_value(filter_type->control_id, 2.0F).value());

    REQUIRE(runtime.retarget_output(vk::Format::eB8G8R8A8Srgb).has_value());

    auto policy_result = runtime.get_stage_policy();
    REQUIRE(policy_result.has_value());
    REQUIRE(policy_result->prechain_enabled);
    REQUIRE(!policy_result->effect_stage_enabled);

    auto resolution_result = runtime.get_prechain_resolution();
    REQUIRE(resolution_result.has_value());
    REQUIRE(*resolution_result == vk::Extent2D{2u, 3u});

    controls_result = runtime.list_controls(goggles::render::ChainControlStage::prechain);
    REQUIRE(controls_result.has_value());
    filter_type = find_control(*controls_result, "filter_type");
    REQUIRE(filter_type != nullptr);
    REQUIRE(filter_type->current_value == Catch::Approx(2.0F));
}

TEST_CASE("Controller retarget preserves active runtime without swap signaling",
          "[filter_chain][retarget][runtime]") {
    VulkanRuntimeFixture fixture;
    if (!fixture.available()) {
        SKIP("Skipping Vulkan-backed controller retarget test because no Vulkan graphics device is "
             "available");
    }

    const auto shader_root = std::filesystem::path(GOGGLES_SOURCE_DIR) / "shaders";
    const auto preset_path =
        std::filesystem::path(GOGGLES_SOURCE_DIR) / "shaders/retroarch/test/format.slangp";
    REQUIRE(std::filesystem::exists(shader_root));
    REQUIRE(std::filesystem::exists(preset_path));

    CacheDirGuard cache_dir_guard(make_cache_dir());
    auto controller = goggles::render::backend_internal::FilterChainController{};
    auto build_config = make_runtime_build_config(fixture, shader_root, cache_dir_guard.dir);

    REQUIRE(controller.recreate_filter_chain(build_config).has_value());
    configure_controller_runtime(controller, preset_path);

    REQUIRE_FALSE(controller.consume_chain_swapped());
    REQUIRE(controller
                .retarget_filter_chain(
                    {.format = vk::Format::eB8G8R8A8Srgb, .extent = vk::Extent2D{4u, 5u}})
                .has_value());
    REQUIRE_FALSE(controller.consume_chain_swapped());
    require_controller_state(controller, preset_path);

    controller.shutdown([device = fixture.create_info().device]() {
        REQUIRE(device.waitIdle() == vk::Result::eSuccess);
    });
}

TEST_CASE("Controller retarget failure keeps the previous runtime usable",
          "[filter_chain][retarget][runtime]") {
    VulkanRuntimeFixture fixture;
    if (!fixture.available()) {
        SKIP("Skipping Vulkan-backed controller retarget failure test because no Vulkan graphics "
             "device is available");
    }

    const auto shader_root = std::filesystem::path(GOGGLES_SOURCE_DIR) / "shaders";
    const auto preset_path =
        std::filesystem::path(GOGGLES_SOURCE_DIR) / "shaders/retroarch/test/format.slangp";
    REQUIRE(std::filesystem::exists(shader_root));
    REQUIRE(std::filesystem::exists(preset_path));

    CacheDirGuard cache_dir_guard(make_cache_dir());
    auto controller = goggles::render::backend_internal::FilterChainController{};
    auto build_config = make_runtime_build_config(fixture, shader_root, cache_dir_guard.dir);

    REQUIRE(controller.recreate_filter_chain(build_config).has_value());
    configure_controller_runtime(controller, preset_path);

    const auto invalid_retarget = controller.retarget_filter_chain(
        {.format = vk::Format::eUndefined, .extent = vk::Extent2D{4u, 5u}});
    REQUIRE_FALSE(invalid_retarget.has_value());
    REQUIRE_FALSE(controller.consume_chain_swapped());
    require_controller_state(controller, preset_path);
    REQUIRE(controller
                .retarget_filter_chain(
                    {.format = vk::Format::eB8G8R8A8Srgb, .extent = vk::Extent2D{4u, 5u}})
                .has_value());

    controller.shutdown([device = fixture.create_info().device]() {
        REQUIRE(device.waitIdle() == vk::Result::eSuccess);
    });
}

TEST_CASE("Pending reload swaps only after activation and preserves authoritative state",
          "[filter_chain][retarget][runtime]") {
    VulkanRuntimeFixture fixture;
    if (!fixture.available()) {
        SKIP(
            "Skipping Vulkan-backed pending reload retarget test because no Vulkan graphics device "
            "is available");
    }

    const auto shader_root = std::filesystem::path(GOGGLES_SOURCE_DIR) / "shaders";
    const auto preset_path =
        std::filesystem::path(GOGGLES_SOURCE_DIR) / "shaders/retroarch/test/format.slangp";
    REQUIRE(std::filesystem::exists(shader_root));
    REQUIRE(std::filesystem::exists(preset_path));

    CacheDirGuard cache_dir_guard(make_cache_dir());
    auto controller = goggles::render::backend_internal::FilterChainController{};
    auto build_config = make_runtime_build_config(fixture, shader_root, cache_dir_guard.dir);

    REQUIRE(controller.recreate_filter_chain(build_config).has_value());
    configure_controller_runtime(controller, preset_path);

    REQUIRE(controller.reload_shader_preset(preset_path, build_config).has_value());
    wait_for_reload_start(controller);
    REQUIRE(controller.pending_chain_ready.load(std::memory_order_acquire));
    REQUIRE_FALSE(controller.consume_chain_swapped());

    REQUIRE(controller
                .retarget_filter_chain(
                    {.format = vk::Format::eB8G8R8A8Srgb, .extent = vk::Extent2D{4u, 5u}})
                .has_value());
    REQUIRE_FALSE(controller.consume_chain_swapped());

    controller.check_pending_chain_swap([] {});
    REQUIRE(controller.consume_chain_swapped());
    REQUIRE_FALSE(controller.pending_chain_ready.load(std::memory_order_acquire));
    require_controller_state(controller, preset_path);

    controller.shutdown([device = fixture.create_info().device]() {
        REQUIRE(device.waitIdle() == vk::Result::eSuccess);
    });
}

TEST_CASE("Explicit reload failure preserves the previous runtime",
          "[filter_chain][retarget][runtime]") {
    VulkanRuntimeFixture fixture;
    if (!fixture.available()) {
        SKIP("Skipping Vulkan-backed reload failure test because no Vulkan graphics device is "
             "available");
    }

    const auto shader_root = std::filesystem::path(GOGGLES_SOURCE_DIR) / "shaders";
    const auto preset_path =
        std::filesystem::path(GOGGLES_SOURCE_DIR) / "shaders/retroarch/test/format.slangp";
    const auto missing_preset_path =
        std::filesystem::path(GOGGLES_SOURCE_DIR) / "shaders/retroarch/test/missing.slangp";
    REQUIRE(std::filesystem::exists(shader_root));
    REQUIRE(std::filesystem::exists(preset_path));
    REQUIRE_FALSE(std::filesystem::exists(missing_preset_path));

    CacheDirGuard cache_dir_guard(make_cache_dir());
    auto controller = goggles::render::backend_internal::FilterChainController{};
    auto build_config = make_runtime_build_config(fixture, shader_root, cache_dir_guard.dir);

    REQUIRE(controller.recreate_filter_chain(build_config).has_value());
    configure_controller_runtime(controller, preset_path);

    REQUIRE(controller.reload_shader_preset(missing_preset_path, build_config).has_value());

    using namespace std::chrono_literals;
    REQUIRE(controller.pending_load_future.valid());
    REQUIRE(controller.pending_load_future.wait_for(5s) == std::future_status::ready);
    const auto reload_result = controller.pending_load_future.get();
    REQUIRE_FALSE(reload_result.has_value());
    REQUIRE_FALSE(controller.pending_chain_ready.load(std::memory_order_acquire));
    REQUIRE_FALSE(controller.consume_chain_swapped());
    require_controller_state(controller, preset_path);
    REQUIRE(controller
                .retarget_filter_chain(
                    {.format = vk::Format::eB8G8R8A8Srgb, .extent = vk::Extent2D{4u, 5u}})
                .has_value());

    controller.shutdown([device = fixture.create_info().device]() {
        REQUIRE(device.waitIdle() == vk::Result::eSuccess);
    });
}

TEST_CASE("Controller and backend retarget path stays distinct from reload",
          "[filter_chain][retarget_contract]") {
    const auto controller_cpp = std::filesystem::path(GOGGLES_SOURCE_DIR) /
                                "src/render/backend/filter_chain_controller.cpp";
    const auto backend_cpp =
        std::filesystem::path(GOGGLES_SOURCE_DIR) / "src/render/backend/vulkan_backend.cpp";

    auto controller_text = read_text_file(controller_cpp);
    auto backend_text = read_text_file(backend_cpp);
    REQUIRE(controller_text.has_value());
    REQUIRE(backend_text.has_value());

    REQUIRE(controller_text->find("authoritative_output_target = OutputTarget{") !=
            std::string::npos);
    REQUIRE(controller_text->find("align_runtime_output_target(") != std::string::npos);
    REQUIRE(controller_text->find("pending_chain, requested_output_target,") != std::string::npos);
    REQUIRE(controller_text->find("pending_filter_chain, authoritative_output_target,") !=
            std::string::npos);

    const auto recreate_swapchain_pos =
        backend_text->find("auto VulkanBackend::recreate_swapchain(");
    const auto retarget_call_pos = backend_text->find(
        "m_filter_chain_controller.retarget_filter_chain(", recreate_swapchain_pos);
    const auto reload_call_pos =
        backend_text->find("m_filter_chain_controller.reload_shader_preset(");
    REQUIRE(recreate_swapchain_pos != std::string::npos);
    REQUIRE(retarget_call_pos != std::string::npos);
    REQUIRE(reload_call_pos != std::string::npos);
    REQUIRE(recreate_swapchain_pos < retarget_call_pos);
}

TEST_CASE("Retarget failure path stays staged and non-destructive",
          "[filter_chain][retarget_contract]") {
    const auto resources_cpp =
        std::filesystem::path(GOGGLES_SOURCE_DIR) / "src/render/chain/chain_resources.cpp";
    const auto controller_cpp = std::filesystem::path(GOGGLES_SOURCE_DIR) /
                                "src/render/backend/filter_chain_controller.cpp";

    auto resources_text = read_text_file(resources_cpp);
    auto controller_text = read_text_file(controller_cpp);
    REQUIRE(resources_text.has_value());
    REQUIRE(controller_text.has_value());

    const auto retarget_pos = resources_text->find("auto ChainResources::retarget_output(");
    const auto build_candidate_pos =
        resources_text->find("auto candidate = GOGGLES_TRY(build_output_state(", retarget_pos);
    const auto shutdown_old_pos =
        resources_text->find("shutdown_output_state(m_output_state);", build_candidate_pos);
    const auto swap_candidate_pos =
        resources_text->find("m_output_state = std::move(candidate);", shutdown_old_pos);

    REQUIRE(retarget_pos != std::string::npos);
    REQUIRE(build_candidate_pos != std::string::npos);
    REQUIRE(shutdown_old_pos != std::string::npos);
    REQUIRE(swap_candidate_pos != std::string::npos);
    REQUIRE(build_candidate_pos < shutdown_old_pos);
    REQUIRE(shutdown_old_pos < swap_candidate_pos);

    const auto controller_retarget_pos =
        controller_text->find("auto FilterChainController::retarget_filter_chain(");
    const auto controller_retarget_end =
        controller_text->find("void FilterChainController::shutdown(", controller_retarget_pos);
    const auto align_active_pos = controller_text->find(
        "align_runtime_output_target(filter_chain, authoritative_output_target,",
        controller_retarget_pos);
    const auto destroy_active_pos =
        controller_text->find("destroy_filter_chain(filter_chain", controller_retarget_pos);

    REQUIRE(controller_retarget_pos != std::string::npos);
    REQUIRE(controller_retarget_end != std::string::npos);
    REQUIRE(align_active_pos != std::string::npos);
    REQUIRE(
        (destroy_active_pos == std::string::npos || destroy_active_pos >= controller_retarget_end));
}
