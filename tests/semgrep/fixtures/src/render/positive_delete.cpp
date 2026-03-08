namespace goggles::render {

class Framebuffer final {};

struct FramebufferOwner final {
    Framebuffer* framebuffer;
};

void destroy_framebuffer() {
    FramebufferOwner owner{.framebuffer = nullptr};
    const bool owns_framebuffer = true;
    if (owns_framebuffer)
        delete owner.framebuffer;
}

} // namespace goggles::render
