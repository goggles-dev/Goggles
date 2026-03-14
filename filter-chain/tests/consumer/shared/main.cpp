#include <goggles/filter_chain/filter_controls.hpp>
#include <goggles_filter_chain.hpp>

int main() {
    const goggles::render::ChainStagePolicy policy{};
    const auto stage_name = goggles::render::to_string(goggles::render::FilterControlStage::effect);
    return (policy.effect_stage_enabled && stage_name[0] == 'e') ? 0 : 1;
}
