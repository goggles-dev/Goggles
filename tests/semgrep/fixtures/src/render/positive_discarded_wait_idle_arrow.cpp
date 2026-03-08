#include <vulkan/vulkan.hpp>

namespace goggles::render {

struct DeviceHolder final {
    vk::Device* device;
};

void shutdown_device(DeviceHolder holder) {
    static_cast<void>(holder.device->waitIdle());
}

} // namespace goggles::render
