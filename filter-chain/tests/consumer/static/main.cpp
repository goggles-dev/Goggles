#include <goggles/filter_chain/filter_controls.hpp>
#include <goggles_filter_chain.hpp>

int main() {
    goggles::render::FilterControlDescriptor descriptor{};
    descriptor.stage = goggles::render::FilterControlStage::prechain;

    goggles::render::ChainStagePolicy policy{};
    return (descriptor.stage == goggles::render::FilterControlStage::prechain &&
            policy.prechain_enabled)
               ? 0
               : 1;
}
