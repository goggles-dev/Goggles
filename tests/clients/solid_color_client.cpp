#include "wl_helpers.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <optional>
#include <string_view>

namespace goggles::test {

constexpr int WIDTH = 640;
constexpr int HEIGHT = 480;
constexpr int FRAME_COUNT = 30;

struct GlobalState {
    UniqueCompositor compositor{};
    UniqueShm shm{};
    UniqueXdgWmBase wm_base{};
};

void registry_global(void* data, wl_registry* registry, uint32_t name, const char* interface,
                     uint32_t version) {
    auto* state = static_cast<GlobalState*>(data);

    if (std::string_view(interface) == wl_compositor_interface.name) {
        state->compositor.reset(static_cast<wl_compositor*>(
            wl_registry_bind(registry, name, &wl_compositor_interface, std::min(version, 4U))));
    } else if (std::string_view(interface) == wl_shm_interface.name) {
        state->shm.reset(
            static_cast<wl_shm*>(wl_registry_bind(registry, name, &wl_shm_interface, 1)));
    } else if (std::string_view(interface) == xdg_wm_base_interface.name) {
        state->wm_base.reset(
            static_cast<xdg_wm_base*>(wl_registry_bind(registry, name, &xdg_wm_base_interface, 1)));
    }
}

void registry_global_remove(void* data, wl_registry* registry, uint32_t name) {
    (void)data;
    (void)registry;
    (void)name;
}

void xdg_wm_base_ping(void* data, xdg_wm_base* wm_base, uint32_t serial) {
    (void)data;
    xdg_wm_base_pong(wm_base, serial);
}

void xdg_surface_configure(void* data, xdg_surface* surface, uint32_t serial) {
    auto* configured = static_cast<bool*>(data);
    xdg_surface_ack_configure(surface, serial);
    *configured = true;
}

void frame_done(void* data, wl_callback* callback, uint32_t callback_data) {
    (void)callback;
    (void)callback_data;
    auto* frame_ready = static_cast<bool*>(data);
    *frame_ready = true;
}

std::optional<std::array<uint8_t, 4>> parse_color() {
    const char* env = std::getenv("TEST_COLOR");
    if (env == nullptr || *env == '\0') {
        return std::array<uint8_t, 4>{255, 0, 0, 255};
    }

    int r = 0;
    int g = 0;
    int b = 0;
    int a = 0;
    if (std::sscanf(env, "%d,%d,%d,%d", &r, &g, &b, &a) != 4) {
        return std::nullopt;
    }

    if (r < 0 || r > 255 || g < 0 || g > 255 || b < 0 || b > 255 || a < 0 || a > 255) {
        return std::nullopt;
    }

    return std::array<uint8_t, 4>{
        static_cast<uint8_t>(r),
        static_cast<uint8_t>(g),
        static_cast<uint8_t>(b),
        static_cast<uint8_t>(a),
    };
}

} // namespace goggles::test

int main() {
    const char* wayland_display = std::getenv("WAYLAND_DISPLAY");
    if (wayland_display == nullptr || *wayland_display == '\0') {
        std::fprintf(stderr, "WAYLAND_DISPLAY is not set\n");
        return 1;
    }

    const auto color = goggles::test::parse_color();
    if (!color) {
        std::fprintf(stderr, "TEST_COLOR must be in R,G,B,A format with 0-255 channels\n");
        return 1;
    }

    goggles::test::UniqueDisplay display{wl_display_connect(nullptr)};
    if (!display) {
        std::fprintf(stderr, "Failed to connect to Wayland display\n");
        return 1;
    }

    goggles::test::GlobalState globals{};
    goggles::test::UniqueRegistry registry{wl_display_get_registry(display.get())};
    if (!registry) {
        std::fprintf(stderr, "Failed to get Wayland registry\n");
        return 1;
    }

    static constexpr wl_registry_listener REGISTRY_LISTENER = {
        .global = goggles::test::registry_global,
        .global_remove = goggles::test::registry_global_remove,
    };

    if (wl_registry_add_listener(registry.get(), &REGISTRY_LISTENER, &globals) != 0) {
        std::fprintf(stderr, "Failed to add registry listener\n");
        return 1;
    }

    if (wl_display_roundtrip(display.get()) < 0) {
        std::fprintf(stderr, "Failed to roundtrip Wayland registry\n");
        return 1;
    }

    if (!globals.compositor || !globals.shm || !globals.wm_base) {
        std::fprintf(stderr, "Missing required Wayland globals\n");
        return 1;
    }

    static constexpr xdg_wm_base_listener WM_BASE_LISTENER = {
        .ping = goggles::test::xdg_wm_base_ping,
    };

    if (xdg_wm_base_add_listener(globals.wm_base.get(), &WM_BASE_LISTENER, nullptr) != 0) {
        std::fprintf(stderr, "Failed to add xdg_wm_base listener\n");
        return 1;
    }

    goggles::test::UniqueSurface surface{wl_compositor_create_surface(globals.compositor.get())};
    if (!surface) {
        std::fprintf(stderr, "Failed to create Wayland surface\n");
        return 1;
    }

    goggles::test::UniqueXdgSurface xdg_surface{
        xdg_wm_base_get_xdg_surface(globals.wm_base.get(), surface.get())};
    if (!xdg_surface) {
        std::fprintf(stderr, "Failed to create xdg_surface\n");
        return 1;
    }

    goggles::test::UniqueXdgToplevel toplevel{xdg_surface_get_toplevel(xdg_surface.get())};
    if (!toplevel) {
        std::fprintf(stderr, "Failed to create xdg_toplevel\n");
        return 1;
    }

    xdg_toplevel_set_title(toplevel.get(), "solid_color_client");

    bool configured = false;
    static constexpr xdg_surface_listener XDG_SURFACE_LISTENER = {
        .configure = goggles::test::xdg_surface_configure,
    };

    if (xdg_surface_add_listener(xdg_surface.get(), &XDG_SURFACE_LISTENER, &configured) != 0) {
        std::fprintf(stderr, "Failed to add xdg_surface listener\n");
        return 1;
    }

    wl_surface_commit(surface.get());

    while (!configured) {
        if (wl_display_dispatch(display.get()) < 0) {
            std::fprintf(stderr, "Failed while waiting for xdg_surface configure\n");
            return 1;
        }
    }

    auto shm_buffer = goggles::test::create_shm_buffer(globals.shm.get(), goggles::test::WIDTH,
                                                       goggles::test::HEIGHT);
    if (!shm_buffer) {
        std::fprintf(stderr, "Failed to create shm buffer\n");
        return 1;
    }

    const uint32_t packed_color =
        goggles::test::pack_argb8888((*color)[0], (*color)[1], (*color)[2], (*color)[3]);
    for (int y = 0; y < shm_buffer->height; ++y) {
        uint32_t* row = shm_buffer->data +
                        (static_cast<std::size_t>(y) * static_cast<std::size_t>(shm_buffer->width));
        std::fill(row, row + shm_buffer->width, packed_color);
    }

    static constexpr wl_callback_listener FRAME_LISTENER = {
        .done = goggles::test::frame_done,
    };

    for (int frame = 0; frame < goggles::test::FRAME_COUNT; ++frame) {
        bool frame_ready = false;
        goggles::test::UniqueCallback frame_callback{wl_surface_frame(surface.get())};
        if (!frame_callback) {
            std::fprintf(stderr, "Failed to create frame callback\n");
            return 1;
        }

        if (wl_callback_add_listener(frame_callback.get(), &FRAME_LISTENER, &frame_ready) != 0) {
            std::fprintf(stderr, "Failed to add frame callback listener\n");
            return 1;
        }

        wl_surface_attach(surface.get(), shm_buffer->buffer.get(), 0, 0);
        wl_surface_damage_buffer(surface.get(), 0, 0, shm_buffer->width, shm_buffer->height);
        wl_surface_commit(surface.get());

        while (!frame_ready) {
            if (wl_display_dispatch(display.get()) < 0) {
                std::fprintf(stderr, "Failed while waiting for frame callback\n");
                return 1;
            }
        }
    }

    return 0;
}
