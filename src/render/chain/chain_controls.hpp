#pragma once

#include "chain_resources.hpp"

#include <goggles/filter_chain/filter_controls.hpp>
#include <optional>
#include <unordered_map>
#include <vector>

namespace goggles::render {

/// @brief Manages filter control descriptors, normalization, and parameter forwarding.
class ChainControls {
public:
    [[nodiscard]] auto list_controls(ChainResources& resources) const
        -> std::vector<FilterControlDescriptor>;
    [[nodiscard]] auto list_controls(ChainResources& resources, FilterControlStage stage) const
        -> std::vector<FilterControlDescriptor>;
    [[nodiscard]] auto set_control_value(ChainResources& resources, FilterControlId control_id,
                                         float value) -> bool;
    [[nodiscard]] auto reset_control_value(ChainResources& resources, FilterControlId control_id)
        -> bool;
    void reset_controls(ChainResources& resources);
    void replay_values(ChainResources& resources);

    void remember_control_value(const FilterControlDescriptor& descriptor, float value);
    void forget_control_value(const FilterControlDescriptor& descriptor);
    [[nodiscard]] auto replay_value_for(const FilterControlDescriptor& descriptor) const
        -> std::optional<float>;

private:
    [[nodiscard]] auto collect_prechain_controls(ChainResources& resources) const
        -> std::vector<FilterControlDescriptor>;
    [[nodiscard]] auto collect_effect_controls(ChainResources& resources) const
        -> std::vector<FilterControlDescriptor>;

    std::unordered_map<std::string, float> m_prechain_overrides;
    std::unordered_map<std::string, float> m_effect_overrides;
};

} // namespace goggles::render
