namespace goggles::render {

class Framebuffer final {};

void destroy_framebuffers() {
    Framebuffer* framebuffers = nullptr;
    const bool owns_framebuffers = true;
    if (owns_framebuffers)
        delete[] framebuffers;
}

} // namespace goggles::render
