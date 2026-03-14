#include <atomic>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <goggles_filter_chain.hpp>
#include <string>
#include <string_view>
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

} // namespace

TEST_CASE("Filter chain output retarget preserves runtime state",
          "[filter_chain][retarget][contract]") {
    VulkanRuntimeFixture fixture;
    if (!fixture.available()) {
        SKIP("Skipping Vulkan-backed retarget test because no Vulkan graphics device is available");
    }

    const auto shader_root = std::filesystem::path(FILTER_CHAIN_ASSET_DIR) / "shaders";
    const auto preset_path =
        std::filesystem::path(FILTER_CHAIN_ASSET_DIR) / "shaders/test/format.slangp";
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

TEST_CASE("Retarget failure preserves runtime state for continued use",
          "[filter_chain][retarget][contract]") {
    VulkanRuntimeFixture fixture;
    if (!fixture.available()) {
        SKIP("Skipping Vulkan-backed retarget test because no Vulkan graphics device is available");
    }

    const auto shader_root = std::filesystem::path(FILTER_CHAIN_ASSET_DIR) / "shaders";
    const auto preset_path =
        std::filesystem::path(FILTER_CHAIN_ASSET_DIR) / "shaders/test/format.slangp";
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
    REQUIRE(runtime.set_prechain_resolution(vk::Extent2D{4u, 5u}).has_value());

    auto controls_result = runtime.list_controls(goggles::render::ChainControlStage::prechain);
    REQUIRE(controls_result.has_value());
    const auto* filter_type = find_control(*controls_result, "filter_type");
    REQUIRE(filter_type != nullptr);
    REQUIRE(runtime.set_control_value(filter_type->control_id, 1.0F).has_value());
    REQUIRE(runtime.set_control_value(filter_type->control_id, 1.0F).value());

    // Attempt retarget with eUndefined — expected to fail on conformant drivers.
    auto retarget_result = runtime.retarget_output(vk::Format::eUndefined);
    if (!retarget_result.has_value()) {
        // Failure path: verify the runtime remains fully usable.
        auto policy_result = runtime.get_stage_policy();
        REQUIRE(policy_result.has_value());
        REQUIRE(policy_result->prechain_enabled);
        REQUIRE(!policy_result->effect_stage_enabled);

        auto resolution_result = runtime.get_prechain_resolution();
        REQUIRE(resolution_result.has_value());
        REQUIRE(*resolution_result == vk::Extent2D{4u, 5u});

        controls_result = runtime.list_controls(goggles::render::ChainControlStage::prechain);
        REQUIRE(controls_result.has_value());
        filter_type = find_control(*controls_result, "filter_type");
        REQUIRE(filter_type != nullptr);
        REQUIRE(filter_type->current_value == Catch::Approx(1.0F));

        // Verify a valid retarget still works after a failed one.
        REQUIRE(runtime.retarget_output(vk::Format::eB8G8R8A8Srgb).has_value());

        policy_result = runtime.get_stage_policy();
        REQUIRE(policy_result.has_value());
        REQUIRE(policy_result->prechain_enabled);
    } else {
        // Some drivers accept eUndefined; still verify state is coherent.
        auto policy_result = runtime.get_stage_policy();
        REQUIRE(policy_result.has_value());
        REQUIRE(policy_result->prechain_enabled);
        REQUIRE(!policy_result->effect_stage_enabled);
    }
}
