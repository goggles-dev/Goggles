#pragma once

#include <cstdint>
#include <util/unique_fd.hpp>
#include <vulkan/vulkan.hpp>

namespace goggles::util {

struct ExternalImage {
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t stride = 0;
    uint32_t offset = 0;
    vk::Format format = vk::Format::eUndefined;
    uint64_t modifier = 0;
    util::UniqueFd handle;
};

struct ExternalImageFrame {
    ExternalImage image;
    uint64_t frame_number = 0;
    util::UniqueFd sync_fd;
};

} // namespace goggles::util
