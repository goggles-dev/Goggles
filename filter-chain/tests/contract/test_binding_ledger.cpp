#include "diagnostics/binding_ledger.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace goggles::diagnostics;

TEST_CASE("BindingLedger records and queries entries", "[diagnostics][ledger]") {
    BindingLedger ledger;
    REQUIRE(ledger.all_entries().empty());

    ledger.record(BindingEntry{.pass_ordinal = 0,
                               .binding_slot = 0,
                               .status = BindingStatus::resolved,
                               .resource_identity = "source",
                               .width = 640,
                               .height = 480,
                               .format = 37,
                               .producer_pass_ordinal = UINT32_MAX,
                               .alias_name = {}});

    ledger.record(BindingEntry{.pass_ordinal = 0,
                               .binding_slot = 1,
                               .status = BindingStatus::substituted,
                               .resource_identity = "fallback",
                               .width = 0,
                               .height = 0,
                               .format = 0,
                               .producer_pass_ordinal = UINT32_MAX,
                               .alias_name = {}});

    ledger.record(BindingEntry{.pass_ordinal = 1,
                               .binding_slot = 0,
                               .status = BindingStatus::resolved,
                               .resource_identity = "pass0_output",
                               .width = 0,
                               .height = 0,
                               .format = 0,
                               .producer_pass_ordinal = 0,
                               .alias_name = {}});

    REQUIRE(ledger.all_entries().size() == 3);

    auto pass0 = ledger.entries_for_pass(0);
    REQUIRE(pass0.size() == 2);
    REQUIRE(pass0[0].resource_identity == "source");
    REQUIRE(pass0[1].status == BindingStatus::substituted);

    auto pass1 = ledger.entries_for_pass(1);
    REQUIRE(pass1.size() == 1);
    REQUIRE(pass1[0].producer_pass_ordinal == 0);
}

TEST_CASE("BindingLedger status classification", "[diagnostics][ledger]") {
    REQUIRE(static_cast<uint8_t>(BindingStatus::resolved) == 0);
    REQUIRE(static_cast<uint8_t>(BindingStatus::substituted) == 1);
    REQUIRE(static_cast<uint8_t>(BindingStatus::unresolved) == 2);
}

TEST_CASE("BindingLedger records extents", "[diagnostics][ledger]") {
    BindingLedger ledger;
    ledger.record(BindingEntry{.pass_ordinal = 0,
                               .binding_slot = 0,
                               .status = BindingStatus::resolved,
                               .resource_identity = {},
                               .width = 1920,
                               .height = 1080,
                               .format = 44,
                               .producer_pass_ordinal = UINT32_MAX,
                               .alias_name = {}});

    auto entries = ledger.entries_for_pass(0);
    REQUIRE(entries.size() == 1);
    REQUIRE(entries[0].width == 1920);
    REQUIRE(entries[0].height == 1080);
    REQUIRE(entries[0].format == 44);
}

TEST_CASE("BindingLedger clear", "[diagnostics][ledger]") {
    BindingLedger ledger;
    ledger.record(BindingEntry{.pass_ordinal = 0,
                               .binding_slot = 0,
                               .status = BindingStatus::resolved,
                               .resource_identity = {},
                               .width = 0,
                               .height = 0,
                               .format = 0,
                               .producer_pass_ordinal = UINT32_MAX,
                               .alias_name = {}});
    REQUIRE(ledger.all_entries().size() == 1);

    ledger.clear();
    REQUIRE(ledger.all_entries().empty());
}

TEST_CASE("BindingLedger empty pass query", "[diagnostics][ledger]") {
    BindingLedger ledger;
    ledger.record(BindingEntry{.pass_ordinal = 5,
                               .binding_slot = 0,
                               .status = BindingStatus::resolved,
                               .resource_identity = {},
                               .width = 0,
                               .height = 0,
                               .format = 0,
                               .producer_pass_ordinal = UINT32_MAX,
                               .alias_name = {}});

    auto pass0 = ledger.entries_for_pass(0);
    REQUIRE(pass0.empty());
}
