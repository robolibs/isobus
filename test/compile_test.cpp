#include <doctest/doctest.h>

// Include the master header to verify all headers compile together
#include <isobus.hpp>

TEST_CASE("All headers compile") {
    // If this compiles, all headers are syntactically valid
    CHECK(true);
}

TEST_CASE("Core types available") {
    isobus::Address addr = 0x28;
    isobus::PGN pgn = isobus::PGN_DM1;
    isobus::Priority prio = isobus::Priority::Default;
    CHECK(addr == 0x28);
    CHECK(pgn == 0xFECA);
    CHECK(prio == isobus::Priority::Default);
}

TEST_CASE("Name constructible") {
    isobus::Name name;
    name.set_identity_number(42);
    name.set_manufacturer_code(100);
    CHECK(name.identity_number() == 42);
}

TEST_CASE("Frame constructible") {
    isobus::Frame f;
    CHECK(f.length == 8);
}

TEST_CASE("Message constructible") {
    isobus::Message msg;
    msg.pgn = isobus::PGN_VEHICLE_SPEED;
    msg.data = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
    CHECK(msg.get_u8(0) == 0x01);
}

TEST_CASE("NetworkManager constructible") {
    isobus::NetworkManager nm;
    CHECK(nm.internal_cfs().empty());
}

TEST_CASE("TransportProtocol constructible") {
    isobus::TransportProtocol tp;
    CHECK(tp.active_sessions().empty());
}

TEST_CASE("DiagnosticProtocol constructible") {
    isobus::NetworkManager nm;
    isobus::Name name;
    auto cf = nm.create_internal(name, 0);
    isobus::DiagnosticProtocol diag(nm, cf.value());
    CHECK(diag.active_dtcs().empty());
}

TEST_CASE("NMEA position constructible") {
    isobus::nmea::GNSSPosition pos;
    pos.wgs = concord::earth::WGS(48.0, 11.0, 500.0);
    CHECK(pos.wgs.latitude == doctest::Approx(48.0));
}
