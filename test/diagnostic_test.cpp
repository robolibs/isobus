#include <doctest/doctest.h>
#include <isobus/protocol/diagnostic.hpp>

using namespace isobus;

TEST_CASE("DTC encode/decode") {
    SUBCASE("basic encode") {
        DTC dtc;
        dtc.spn = 1234;
        dtc.fmi = FMI::VoltageHigh;
        dtc.occurrence_count = 5;

        auto bytes = dtc.encode();
        DTC decoded = DTC::decode(bytes.data());

        CHECK(decoded.spn == 1234);
        CHECK(decoded.fmi == FMI::VoltageHigh);
        CHECK(decoded.occurrence_count == 5);
    }

    SUBCASE("max SPN value (19 bits)") {
        DTC dtc;
        dtc.spn = 0x7FFFF; // max 19-bit value
        dtc.fmi = FMI::AboveNormal;
        dtc.occurrence_count = 126;

        auto bytes = dtc.encode();
        DTC decoded = DTC::decode(bytes.data());

        CHECK(decoded.spn == 0x7FFFF);
        CHECK(decoded.occurrence_count == 126);
    }

    SUBCASE("equality comparison") {
        DTC a{100, FMI::VoltageLow, 1};
        DTC b{100, FMI::VoltageLow, 5};
        DTC c{100, FMI::VoltageHigh, 1};
        CHECK(a == b); // Same SPN+FMI, different OC
        CHECK(!(a == c)); // Different FMI
    }
}

TEST_CASE("DiagnosticLamps encode/decode") {
    DiagnosticLamps lamps;
    lamps.malfunction = LampStatus::On;
    lamps.red_stop = LampStatus::Off;
    lamps.amber_warning = LampStatus::On;
    lamps.engine_protect = LampStatus::Off;
    lamps.malfunction_flash = LampFlash::FastFlash;
    lamps.amber_warning_flash = LampFlash::SlowFlash;

    auto bytes = lamps.encode();
    DiagnosticLamps decoded = DiagnosticLamps::decode(bytes.data());

    CHECK(decoded.malfunction == LampStatus::On);
    CHECK(decoded.red_stop == LampStatus::Off);
    CHECK(decoded.amber_warning == LampStatus::On);
    CHECK(decoded.engine_protect == LampStatus::Off);
    CHECK(decoded.malfunction_flash == LampFlash::FastFlash);
    CHECK(decoded.amber_warning_flash == LampFlash::SlowFlash);
}

TEST_CASE("DiagnosticProtocol DTC management") {
    NetworkManager nm;
    Name name;
    auto cf_result = nm.create_internal(name, 0, 0x28);
    auto* cf = cf_result.value();

    DiagnosticProtocol diag(nm, cf);

    SUBCASE("set active DTC") {
        DTC dtc{500, FMI::AboveNormal, 0};
        auto result = diag.set_active(dtc);
        CHECK(result.is_ok());
        CHECK(diag.active_dtcs().size() == 1);
        CHECK(diag.active_dtcs()[0].spn == 500);
        CHECK(diag.active_dtcs()[0].occurrence_count == 1);
    }

    SUBCASE("duplicate DTC increments count") {
        DTC dtc{500, FMI::AboveNormal, 0};
        diag.set_active(dtc);
        diag.set_active(dtc);
        CHECK(diag.active_dtcs().size() == 1);
        CHECK(diag.active_dtcs()[0].occurrence_count == 2);
    }

    SUBCASE("clear active moves to previous") {
        DTC dtc{500, FMI::AboveNormal, 0};
        diag.set_active(dtc);
        diag.clear_active(500, FMI::AboveNormal);
        CHECK(diag.active_dtcs().empty());
        CHECK(diag.previous_dtcs().size() == 1);
    }

    SUBCASE("clear all active") {
        diag.set_active({100, FMI::VoltageLow, 0});
        diag.set_active({200, FMI::VoltageHigh, 0});
        diag.clear_all_active();
        CHECK(diag.active_dtcs().empty());
        CHECK(diag.previous_dtcs().size() == 2);
    }

    SUBCASE("clear previous") {
        diag.set_active({100, FMI::VoltageLow, 0});
        diag.clear_all_active();
        diag.clear_previous();
        CHECK(diag.previous_dtcs().empty());
    }
}

TEST_CASE("DM5 DiagnosticProtocolID") {
    SUBCASE("encode/decode") {
        DiagnosticProtocolID id;
        id.protocols = DiagProtocol::J1939_73 | DiagProtocol::ISO_14229_3;

        auto data = id.encode();
        CHECK(data.size() == 8);
        CHECK(data[0] == 0x05); // J1939_73 (0x01) | ISO_14229_3 (0x04) = 0x05
        CHECK(data[1] == 0xFF); // reserved

        auto decoded = DiagnosticProtocolID::decode(data);
        CHECK(decoded.protocols == 0x05);
    }

    SUBCASE("default is J1939") {
        DiagnosticProtocolID id;
        CHECK(id.protocols == static_cast<u8>(DiagProtocol::J1939_73));
    }

    SUBCASE("set_diag_protocol and accessor") {
        NetworkManager nm;
        Name name;
        auto cf_result = nm.create_internal(name, 0, 0x28);
        auto *cf = cf_result.value();
        DiagnosticProtocol diag(nm, cf);

        DiagnosticProtocolID id;
        id.protocols = DiagProtocol::J1939_73 | DiagProtocol::ISO_14230;
        diag.set_diag_protocol(id);
        CHECK(diag.diag_protocol_id().protocols == 0x03);
    }
}
