#include "wl_helpers.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string_view>

namespace goggles::test {

constexpr int MAIN_WIDTH = 640;
constexpr int MAIN_HEIGHT = 480;
constexpr int CHILD_WIDTH = 200;
constexpr int CHILD_HEIGHT = 200;
constexpr int CHILD_X = 100;
constexpr int CHILD_Y = 100;
constexpr int FRAME_COUNT = 30;

struct GlobalState {
    UniqueCompositor compositor{};
    UniqueShm shm{};
    UniqueXdgWmBase wm_base{};
    UniqueSubcompositor subcompositor{};
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
    } else if (std::string_view(interface) == wl_subcompositor_interface.name) {
        state->subcompositor.reset(static_cast<wl_subcompositor*>(
            wl_registry_bind(registry, name, &wl_subcompositor_interface, 1)));
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

} // namespace goggles::test

int main() {
    const char* wayland_display = std::getenv("WAYLAND_DISPLAY");
    if (wayland_display == nullptr || *wayland_display == '\0') {
        std::fprintf(stderr, "WAYLAND_DISPLAY is not set\n");
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

    if (!globals.compositor || !globals.shm || !globals.wm_base || !globals.subcompositor) {
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

    goggles::test::UniqueSurface main_surface{
        wl_compositor_create_surface(globals.compositor.get())};
    goggles::test::UniqueSurface child_surface{
        wl_compositor_create_surface(globals.compositor.get())};
    if (!main_surface || !child_surface) {
        std::fprintf(stderr, "Failed to create Wayland surfaces\n");
        return 1;
    }

    goggles::test::UniqueXdgSurface xdg_surface{
        xdg_wm_base_get_xdg_surface(globals.wm_base.get(), main_surface.get())};
    if (!xdg_surface) {
        std::fprintf(stderr, "Failed to create main xdg_surface\n");
        return 1;
    }

    goggles::test::UniqueXdgToplevel toplevel{xdg_surface_get_toplevel(xdg_surface.get())};
    if (!toplevel) {
        std::fprintf(stderr, "Failed to create main xdg_toplevel\n");
        return 1;
    }

    xdg_toplevel_set_title(toplevel.get(), "multi_surface_client");

    goggles::test::UniqueSubsurface subsurface{wl_subcompositor_get_subsurface(
        globals.subcompositor.get(), child_surface.get(), main_surface.get())};
    if (!subsurface) {
        std::fprintf(stderr, "Failed to create subsurface\n");
        return 1;
    }

    wl_subsurface_set_position(subsurface.get(), goggles::test::CHILD_X, goggles::test::CHILD_Y);

    bool configured = false;
    static constexpr xdg_surface_listener XDG_SURFACE_LISTENER = {
        .configure = goggles::test::xdg_surface_configure,
    };

    if (xdg_surface_add_listener(xdg_surface.get(), &XDG_SURFACE_LISTENER, &configured) != 0) {
        std::fprintf(stderr, "Failed to add xdg_surface listener\n");
        return 1;
    }

    wl_surface_commit(main_surface.get());

    while (!configured) {
        if (wl_display_dispatch(display.get()) < 0) {
            std::fprintf(stderr, "Failed while waiting for xdg_surface configure\n");
            return 1;
        }
    }

    auto main_buffer = goggles::test::create_shm_buffer(
        globals.shm.get(), goggles::test::MAIN_WIDTH, goggles::test::MAIN_HEIGHT);
    auto child_buffer = goggles::test::create_shm_buffer(
        globals.shm.get(), goggles::test::CHILD_WIDTH, goggles::test::CHILD_HEIGHT);
    if (!main_buffer || !child_buffer) {
        std::fprintf(stderr, "Failed to create shm buffers\n");
        return 1;
    }

    const uint32_t blue = goggles::test::pack_argb8888(0, 0, 255, 255);
    const uint32_t red = goggles::test::pack_argb8888(255, 0, 0, 255);

    for (int y = 0; y < main_buffer->height; ++y) {
        uint32_t* row = main_buffer->data + (static_cast<std::size_t>(y) *
                                             static_cast<std::size_t>(main_buffer->width));
        std::fill(row, row + main_buffer->width, blue);
    }

    for (int y = 0; y < child_buffer->height; ++y) {
        uint32_t* row = child_buffer->data + (static_cast<std::size_t>(y) *
                                              static_cast<std::size_t>(child_buffer->width));
        std::fill(row, row + child_buffer->width, red);
    }

    static constexpr wl_callback_listener FRAME_LISTENER = {
        .done = goggles::test::frame_done,
    };

    for (int frame = 0; frame < goggles::test::FRAME_COUNT; ++frame) {
        bool frame_ready = false;
        goggles::test::UniqueCallback frame_callback{wl_surface_frame(main_surface.get())};
        if (!frame_callback) {
            std::fprintf(stderr, "Failed to create frame callback\n");
            return 1;
        }

        if (wl_callback_add_listener(frame_callback.get(), &FRAME_LISTENER, &frame_ready) != 0) {
            std::fprintf(stderr, "Failed to add frame callback listener\n");
            return 1;
        }

        wl_surface_attach(main_surface.get(), main_buffer->buffer.get(), 0, 0);
        wl_surface_damage_buffer(main_surface.get(), 0, 0, main_buffer->width, main_buffer->height);
        wl_surface_commit(main_surface.get());

        wl_surface_attach(child_surface.get(), child_buffer->buffer.get(), 0, 0);
        wl_surface_damage_buffer(child_surface.get(), 0, 0, child_buffer->width,
                                 child_buffer->height);
        wl_surface_commit(child_surface.get());

        while (!frame_ready) {
            if (wl_display_dispatch(display.get()) < 0) {
                std::fprintf(stderr, "Failed while waiting for frame callback\n");
                return 1;
            }
        }
    }

    return 0;
}
