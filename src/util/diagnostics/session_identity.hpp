#pragma once

#include <cstdint>
#include <string>

namespace goggles::diagnostics {

struct SessionIdentity {
    std::string preset_hash;
    std::string expanded_source_hash;
    std::string compiled_contract_hash;
    uint64_t generation_id = 0;
    uint32_t frame_start = 0;
    uint32_t frame_end = 0;
    std::string capture_mode;
    std::string environment_fingerprint;
};

} // namespace goggles::diagnostics
