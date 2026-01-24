#include <doctest/doctest.h>
#include <isobus.hpp>
#include <isobus/vt/server.hpp>

using namespace isobus;
using namespace isobus::vt;

TEST_CASE("VTServer - initialization and state") {
    NetworkManager nm;
    auto* cf = nm.create_internal(Name::build().set_identity_number(1), 0, 0x10).value();

    VTServer server(nm, cf);

    SUBCASE("Initial state is Disconnected") {
        CHECK(server.state() == VTServerState::Disconnected);
    }

    SUBCASE("Start transitions to WaitForClientStatus") {
        server.start();
        CHECK(server.state() == VTServerState::WaitForClientStatus);
    }

    SUBCASE("Stop transitions to Disconnected") {
        server.start();
        server.stop();
        CHECK(server.state() == VTServerState::Disconnected);
    }
}

TEST_CASE("VTServer - screen configuration") {
    NetworkManager nm;
    auto* cf = nm.create_internal(Name::build().set_identity_number(1), 0, 0x10).value();

    VTServer server(nm, cf, VTServerConfig{}.screen(320, 240).version(4));
    CHECK(server.screen_width() == 320);
    CHECK(server.screen_height() == 240);
}

TEST_CASE("VTServer - client tracking") {
    NetworkManager nm;
    auto* cf = nm.create_internal(Name::build().set_identity_number(1), 0, 0x10).value();

    VTServer server(nm, cf);
    server.start();

    CHECK(server.clients().empty());
}

TEST_CASE("VTServer - update sends status periodically") {
    NetworkManager nm;
    auto* cf = nm.create_internal(Name::build().set_identity_number(1), 0, 0x10).value();

    VTServer server(nm, cf);
    server.start();

    // Should not crash
    for (i32 i = 0; i < 20; ++i) {
        server.update(100);
    }
}
