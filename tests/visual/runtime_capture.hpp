#pragma once

#include "diagnostics/diagnostic_session.hpp"
#include "diagnostics/test_harness_sink.hpp"

#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <util/error.hpp>
#include <utility>
#include <vector>
#include <vulkan/vulkan.hpp>

namespace goggles::test {

struct TempDir {
    std::filesystem::path path;

    TempDir();
    ~TempDir();

    TempDir(const TempDir&) = delete;
    auto operator=(const TempDir&) -> TempDir& = delete;
};

struct RuntimeCapturePlan {
    std::filesystem::path preset_path;
    std::string preset_name;
    std::vector<uint32_t> frame_indices;
    std::vector<uint32_t> intermediate_pass_ordinals;
    std::vector<std::pair<std::string, float>> control_overrides;
    vk::Extent2D source_extent{64U, 64U};
    vk::Extent2D target_extent{64U, 64U};
};

struct RuntimeCaptureResult {
    std::unique_ptr<TempDir> temp_dir;
    std::unordered_map<uint32_t, std::filesystem::path> final_frames;
    std::unordered_map<std::string, std::filesystem::path> intermediate_frames;
    std::unique_ptr<diagnostics::DiagnosticSession> session;
    diagnostics::TestHarnessSink* sink = nullptr;
};

[[nodiscard]] auto pass_frame_key(uint32_t pass_ordinal, uint32_t frame_index) -> std::string;

[[nodiscard]] auto capture_runtime_outputs(const RuntimeCapturePlan& plan)
    -> Result<RuntimeCaptureResult>;

} // namespace goggles::test
