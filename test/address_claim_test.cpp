#include <doctest/doctest.h>
#include <isobus/network/address_claimer.hpp>
#include <isobus/network/internal_cf.hpp>
#include <isobus/core/constants.hpp>

using namespace isobus;

TEST_CASE("Address claim state machine") {
    Name name;
    name.set_identity_number(1234);
    name.set_manufacturer_code(100);
    name.set_self_configurable(true);

    InternalCF cf(name, 0, 0x28);
    AddressClaimer claimer(&cf);

    SUBCASE("initial state is None") {
        CHECK(cf.claim_state() == ClaimState::None);
    }

    SUBCASE("start sends request and claim") {
        auto frames = claimer.start();
        CHECK(frames.size() >= 2); // request + claim
        CHECK(cf.claim_state() == ClaimState::WaitForContest);
    }

    SUBCASE("claim succeeds after timeout") {
        claimer.start();
        auto frames = claimer.update(ADDRESS_CLAIM_TIMEOUT_MS);
        CHECK(cf.claim_state() == ClaimState::Claimed);
        CHECK(cf.address() == 0x28);
    }

    SUBCASE("claim not complete before timeout") {
        claimer.start();
        claimer.update(100); // Less than 250ms
        CHECK(cf.claim_state() == ClaimState::WaitForContest);
    }
}

TEST_CASE("Address claim conflict resolution") {
    Name our_name;
    our_name.set_identity_number(100); // Lower = higher priority
    our_name.set_manufacturer_code(50);
    our_name.set_self_configurable(true);

    InternalCF cf(our_name, 0, 0x28);
    AddressClaimer claimer(&cf);
    claimer.start();

    SUBCASE("we win contest (lower NAME)") {
        Name other_name;
        other_name.set_identity_number(200); // Higher = lower priority
        other_name.set_manufacturer_code(50);

        auto frames = claimer.handle_claim(0x28, other_name);
        CHECK(!frames.empty()); // Re-send our claim
        CHECK(cf.claim_state() != ClaimState::Failed);
    }

    SUBCASE("we lose contest (higher NAME)") {
        Name other_name;
        other_name.set_identity_number(50); // Lower = higher priority than us
        other_name.set_manufacturer_code(50);

        auto frames = claimer.handle_claim(0x28, other_name);
        // Self-configurable: should try next address
        CHECK(!frames.empty());
    }

    SUBCASE("different address ignored") {
        Name other_name;
        other_name.set_identity_number(50);
        auto frames = claimer.handle_claim(0x30, other_name);
        CHECK(frames.empty()); // Not our address
    }
}

TEST_CASE("Address claimed event") {
    Name name;
    name.set_identity_number(1);
    name.set_self_configurable(false);

    InternalCF cf(name, 0, 0x10);
    AddressClaimer claimer(&cf);

    Address claimed_addr = NULL_ADDRESS;
    cf.on_address_claimed.subscribe([&](Address a) { claimed_addr = a; });

    claimer.start();
    claimer.update(ADDRESS_CLAIM_TIMEOUT_MS);
    CHECK(claimed_addr == 0x10);
}
