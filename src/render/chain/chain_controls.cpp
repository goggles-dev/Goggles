#include "chain_controls.hpp"

#include <cmath>

namespace goggles::render {

namespace {

auto make_optional_description(const std::string& description) -> std::optional<std::string> {
    if (description.empty()) {
        return std::nullopt;
    }
    return description;
}

auto normalize_control_value(const FilterControlDescriptor& descriptor, float value) -> float {
    const float clamped = clamp_filter_control_value(descriptor, value);
    if (descriptor.stage == FilterControlStage::prechain && descriptor.name == "filter_type") {
        return std::round(clamped);
    }
    return clamped;
}

} // namespace

auto ChainControls::list_controls(ChainResources& resources) const
    -> std::vector<FilterControlDescriptor> {
    auto prechain_controls = collect_prechain_controls(resources);
    auto effect_controls = collect_effect_controls(resources);

    prechain_controls.insert(prechain_controls.end(), effect_controls.begin(),
                             effect_controls.end());
    return prechain_controls;
}

auto ChainControls::list_controls(ChainResources& resources, FilterControlStage stage) const
    -> std::vector<FilterControlDescriptor> {
    if (stage == FilterControlStage::prechain) {
        return collect_prechain_controls(resources);
    }
    return collect_effect_controls(resources);
}

auto ChainControls::set_control_value(ChainResources& resources, FilterControlId control_id,
                                      float value) -> bool {
    for (const auto& descriptor : list_controls(resources)) {
        if (descriptor.control_id != control_id) {
            continue;
        }
        const float normalized = normalize_control_value(descriptor, value);
        remember_control_value(descriptor, normalized);
        if (descriptor.stage == FilterControlStage::prechain) {
            resources.set_prechain_parameter(descriptor.name, normalized);
        } else {
            resources.set_parameter(descriptor.name, normalized);
        }
        return true;
    }

    return false;
}

auto ChainControls::reset_control_value(ChainResources& resources, FilterControlId control_id)
    -> bool {
    for (const auto& descriptor : list_controls(resources)) {
        if (descriptor.control_id != control_id) {
            continue;
        }
        forget_control_value(descriptor);
        if (descriptor.stage == FilterControlStage::prechain) {
            resources.set_prechain_parameter(descriptor.name, descriptor.default_value);
        } else {
            resources.set_parameter(descriptor.name, descriptor.default_value);
        }
        return true;
    }

    return false;
}

void ChainControls::reset_controls(ChainResources& resources) {
    m_prechain_overrides.clear();
    m_effect_overrides.clear();
    resources.clear_parameter_overrides();
    for (const auto& descriptor : collect_prechain_controls(resources)) {
        resources.set_prechain_parameter(descriptor.name, descriptor.default_value);
    }
}

void ChainControls::replay_values(ChainResources& resources) {
    for (const auto& descriptor : list_controls(resources)) {
        auto replay_value = replay_value_for(descriptor);
        if (!replay_value.has_value()) {
            continue;
        }

        if (descriptor.stage == FilterControlStage::prechain) {
            resources.set_prechain_parameter(descriptor.name, *replay_value);
        } else {
            resources.set_parameter(descriptor.name, *replay_value);
        }
    }
}

void ChainControls::remember_control_value(const FilterControlDescriptor& descriptor, float value) {
    auto& overrides = descriptor.stage == FilterControlStage::prechain ? m_prechain_overrides
                                                                       : m_effect_overrides;
    overrides[descriptor.name] = normalize_control_value(descriptor, value);
}

void ChainControls::forget_control_value(const FilterControlDescriptor& descriptor) {
    auto& overrides = descriptor.stage == FilterControlStage::prechain ? m_prechain_overrides
                                                                       : m_effect_overrides;
    overrides.erase(descriptor.name);
}

auto ChainControls::replay_value_for(const FilterControlDescriptor& descriptor) const
    -> std::optional<float> {
    const auto& overrides = descriptor.stage == FilterControlStage::prechain ? m_prechain_overrides
                                                                             : m_effect_overrides;
    if (auto it = overrides.find(descriptor.name); it != overrides.end()) {
        return it->second;
    }
    return std::nullopt;
}

auto ChainControls::collect_prechain_controls(ChainResources& resources) const
    -> std::vector<FilterControlDescriptor> {
    auto prechain_parameters = resources.get_prechain_parameters();
    std::vector<FilterControlDescriptor> descriptors;
    descriptors.reserve(prechain_parameters.size());

    for (const auto& parameter : prechain_parameters) {
        descriptors.push_back({
            .control_id = make_filter_control_id(FilterControlStage::prechain, parameter.name),
            .stage = FilterControlStage::prechain,
            .name = parameter.name,
            .description = make_optional_description(parameter.description),
            .current_value = parameter.current_value,
            .default_value = parameter.default_value,
            .min_value = parameter.min_value,
            .max_value = parameter.max_value,
            .step = parameter.step,
        });
    }

    return descriptors;
}

auto ChainControls::collect_effect_controls(ChainResources& resources) const
    -> std::vector<FilterControlDescriptor> {
    auto parameters = resources.get_all_parameters();
    std::vector<FilterControlDescriptor> descriptors;
    descriptors.reserve(parameters.size());

    for (const auto& parameter : parameters) {
        descriptors.push_back({
            .control_id = make_filter_control_id(FilterControlStage::effect, parameter.name),
            .stage = FilterControlStage::effect,
            .name = parameter.name,
            .description = make_optional_description(parameter.description),
            .current_value = parameter.current_value,
            .default_value = parameter.default_value,
            .min_value = parameter.min_value,
            .max_value = parameter.max_value,
            .step = parameter.step,
        });
    }

    return descriptors;
}

} // namespace goggles::render
