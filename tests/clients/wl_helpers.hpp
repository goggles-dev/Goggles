#pragma once

extern "C" {
#include "xdg-shell-client-protocol.h"
}

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <fcntl.h>
#include <linux/memfd.h>
#include <memory>
#include <optional>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <utility>
#include <wayland-client.h>

namespace goggles::test {

struct WlDisplayDeleter {
    void operator()(wl_display* display) const noexcept {
        if (display != nullptr) {
            wl_display_disconnect(display);
        }
    }
};

struct WlRegistryDeleter {
    void operator()(wl_registry* registry) const noexcept {
        if (registry != nullptr) {
            wl_registry_destroy(registry);
        }
    }
};

struct WlCompositorDeleter {
    void operator()(wl_compositor* compositor) const noexcept {
        if (compositor != nullptr) {
            wl_compositor_destroy(compositor);
        }
    }
};

struct WlShmDeleter {
    void operator()(wl_shm* shm) const noexcept {
        if (shm != nullptr) {
            wl_shm_destroy(shm);
        }
    }
};

struct WlSurfaceDeleter {
    void operator()(wl_surface* surface) const noexcept {
        if (surface != nullptr) {
            wl_surface_destroy(surface);
        }
    }
};

struct WlBufferDeleter {
    void operator()(wl_buffer* buffer) const noexcept {
        if (buffer != nullptr) {
            wl_buffer_destroy(buffer);
        }
    }
};

struct WlShmPoolDeleter {
    void operator()(wl_shm_pool* pool) const noexcept {
        if (pool != nullptr) {
            wl_shm_pool_destroy(pool);
        }
    }
};

struct XdgWmBaseDeleter {
    void operator()(xdg_wm_base* wm_base) const noexcept {
        if (wm_base != nullptr) {
            xdg_wm_base_destroy(wm_base);
        }
    }
};

struct XdgSurfaceDeleter {
    void operator()(xdg_surface* surface) const noexcept {
        if (surface != nullptr) {
            xdg_surface_destroy(surface);
        }
    }
};

struct XdgToplevelDeleter {
    void operator()(xdg_toplevel* toplevel) const noexcept {
        if (toplevel != nullptr) {
            xdg_toplevel_destroy(toplevel);
        }
    }
};

struct WlSubcompositorDeleter {
    void operator()(wl_subcompositor* subcompositor) const noexcept {
        if (subcompositor != nullptr) {
            wl_subcompositor_destroy(subcompositor);
        }
    }
};

struct WlSubsurfaceDeleter {
    void operator()(wl_subsurface* subsurface) const noexcept {
        if (subsurface != nullptr) {
            wl_subsurface_destroy(subsurface);
        }
    }
};

struct WlCallbackDeleter {
    void operator()(wl_callback* callback) const noexcept {
        if (callback != nullptr) {
            wl_callback_destroy(callback);
        }
    }
};

using UniqueDisplay = std::unique_ptr<wl_display, WlDisplayDeleter>;
using UniqueRegistry = std::unique_ptr<wl_registry, WlRegistryDeleter>;
using UniqueCompositor = std::unique_ptr<wl_compositor, WlCompositorDeleter>;
using UniqueShm = std::unique_ptr<wl_shm, WlShmDeleter>;
using UniqueSurface = std::unique_ptr<wl_surface, WlSurfaceDeleter>;
using UniqueBuffer = std::unique_ptr<wl_buffer, WlBufferDeleter>;
using UniqueShmPool = std::unique_ptr<wl_shm_pool, WlShmPoolDeleter>;
using UniqueXdgWmBase = std::unique_ptr<xdg_wm_base, XdgWmBaseDeleter>;
using UniqueXdgSurface = std::unique_ptr<xdg_surface, XdgSurfaceDeleter>;
using UniqueXdgToplevel = std::unique_ptr<xdg_toplevel, XdgToplevelDeleter>;
using UniqueSubcompositor = std::unique_ptr<wl_subcompositor, WlSubcompositorDeleter>;
using UniqueSubsurface = std::unique_ptr<wl_subsurface, WlSubsurfaceDeleter>;
using UniqueCallback = std::unique_ptr<wl_callback, WlCallbackDeleter>;

inline uint32_t pack_argb8888(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    return (static_cast<uint32_t>(a) << 24U) | (static_cast<uint32_t>(r) << 16U) |
           (static_cast<uint32_t>(g) << 8U) | static_cast<uint32_t>(b);
}

struct ShmBuffer {
    UniqueBuffer buffer{};
    UniqueShmPool pool{};
    uint32_t* data{nullptr};
    int width{0};
    int height{0};
    int stride{0};
    int fd{-1};
    std::size_t size{0U};

    ShmBuffer() = default;

    ShmBuffer(UniqueBuffer in_buffer, UniqueShmPool in_pool, uint32_t* in_data, int in_width,
              int in_height, int in_stride, int in_fd, std::size_t in_size)
        : buffer(std::move(in_buffer)), pool(std::move(in_pool)), data(in_data), width(in_width),
          height(in_height), stride(in_stride), fd(in_fd), size(in_size) {}

    ShmBuffer(const ShmBuffer&) = delete;
    ShmBuffer& operator=(const ShmBuffer&) = delete;

    ShmBuffer(ShmBuffer&& other) noexcept
        : buffer(std::move(other.buffer)), pool(std::move(other.pool)), data(other.data),
          width(other.width), height(other.height), stride(other.stride), fd(other.fd),
          size(other.size) {
        other.data = nullptr;
        other.fd = -1;
        other.size = 0U;
    }

    ShmBuffer& operator=(ShmBuffer&& other) noexcept {
        if (this == &other) {
            return *this;
        }

        cleanup();

        buffer = std::move(other.buffer);
        pool = std::move(other.pool);
        data = other.data;
        width = other.width;
        height = other.height;
        stride = other.stride;
        fd = other.fd;
        size = other.size;

        other.data = nullptr;
        other.fd = -1;
        other.size = 0U;

        return *this;
    }

    ~ShmBuffer() { cleanup(); }

private:
    void cleanup() noexcept {
        if (data != nullptr && size > 0U) {
            static_cast<void>(munmap(data, size));
            data = nullptr;
            size = 0U;
        }

        if (fd >= 0) {
            static_cast<void>(close(fd));
            fd = -1;
        }
    }
};

inline int create_memfd(const char* name) {
#if defined(SYS_memfd_create)
    return static_cast<int>(syscall(SYS_memfd_create, name, MFD_CLOEXEC));
#else
    errno = ENOSYS;
    (void)name;
    return -1;
#endif
}

inline std::optional<ShmBuffer> create_shm_buffer(wl_shm* shm, int width, int height) {
    if (shm == nullptr || width <= 0 || height <= 0) {
        return std::nullopt;
    }

    const int stride = width * static_cast<int>(sizeof(uint32_t));
    const std::size_t size = static_cast<std::size_t>(stride) * static_cast<std::size_t>(height);

    const int fd = create_memfd("wl_shm");
    if (fd < 0) {
        return std::nullopt;
    }

    if (ftruncate(fd, static_cast<off_t>(size)) != 0) {
        static_cast<void>(close(fd));
        return std::nullopt;
    }

    void* mapped = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (mapped == MAP_FAILED) {
        static_cast<void>(close(fd));
        return std::nullopt;
    }

    UniqueShmPool pool{wl_shm_create_pool(shm, fd, static_cast<int32_t>(size))};
    if (!pool) {
        static_cast<void>(munmap(mapped, size));
        static_cast<void>(close(fd));
        return std::nullopt;
    }

    UniqueBuffer buffer{
        wl_shm_pool_create_buffer(pool.get(), 0, width, height, stride, WL_SHM_FORMAT_ARGB8888)};

    if (!buffer) {
        static_cast<void>(munmap(mapped, size));
        static_cast<void>(close(fd));
        return std::nullopt;
    }

    return ShmBuffer{
        std::move(buffer),
        std::move(pool),
        static_cast<uint32_t*>(mapped),
        width,
        height,
        stride,
        fd,
        size,
    };
}

} // namespace goggles::test
