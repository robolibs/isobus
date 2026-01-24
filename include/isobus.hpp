#pragma once

// ─── Core ────────────────────────────────────────────────────────────────────
#include "isobus/core/can_bus_config.hpp"
#include "isobus/core/constants.hpp"
#include "isobus/core/error.hpp"
#include "isobus/core/frame.hpp"
#include "isobus/core/identifier.hpp"
#include "isobus/core/message.hpp"
#include "isobus/core/name.hpp"
#include "isobus/core/pgn.hpp"
#include "isobus/core/types.hpp"
#include "isobus/pgn_defs.hpp"

// ─── Utilities ───────────────────────────────────────────────────────────────
#include "isobus/util/bitfield.hpp"
#include "isobus/util/data_span.hpp"
#include "isobus/util/event.hpp"
#include "isobus/util/iop_parser.hpp"
#include "isobus/util/scheduler.hpp"
#include "isobus/util/state_machine.hpp"
#include "isobus/util/timer.hpp"

// ─── Network ─────────────────────────────────────────────────────────────────
#include "isobus/network/address_claimer.hpp"
#include "isobus/network/bus_load.hpp"
#include "isobus/network/control_function.hpp"
#include "isobus/network/internal_cf.hpp"
#include "isobus/network/name_manager.hpp"
#include "isobus/network/network_manager.hpp"
#include "isobus/network/partner_cf.hpp"
#include "isobus/network/working_set.hpp"

// ─── Transport ───────────────────────────────────────────────────────────────
#include "isobus/transport/etp.hpp"
#include "isobus/transport/fast_packet.hpp"
#include "isobus/transport/session.hpp"
#include "isobus/transport/tp.hpp"

// ─── Protocols ───────────────────────────────────────────────────────────────
#include "isobus/protocol/acknowledgment.hpp"
#include "isobus/protocol/auxiliary.hpp"
#include "isobus/protocol/diagnostic.hpp"
#include "isobus/protocol/dm_memory.hpp"
#include "isobus/protocol/file_transfer.hpp"
#include "isobus/protocol/functionalities.hpp"
#include "isobus/protocol/group_function.hpp"
#include "isobus/protocol/guidance.hpp"
#include "isobus/protocol/heartbeat.hpp"
#include "isobus/protocol/language.hpp"
#include "isobus/protocol/maintain_power.hpp"
#include "isobus/protocol/pgn_request.hpp"
#include "isobus/protocol/proprietary.hpp"
#include "isobus/protocol/request2.hpp"
#include "isobus/protocol/shortcut_button.hpp"
#include "isobus/protocol/speed_distance.hpp"
#include "isobus/protocol/tim.hpp"
#include "isobus/protocol/time_date.hpp"

// ─── Implement Messages (ISO 11783-7/9) ─────────────────────────────────────
#include "isobus/implement/aux_valve_status.hpp"
#include "isobus/implement/drive_strategy.hpp"
#include "isobus/implement/guidance.hpp"
#include "isobus/implement/lighting.hpp"
#include "isobus/implement/machine_speed_cmd.hpp"
#include "isobus/implement/speed_distance.hpp"
#include "isobus/implement/tractor_commands.hpp"
#include "isobus/implement/tractor_facilities.hpp"

// ─── Virtual Terminal ────────────────────────────────────────────────────────
#include "isobus/vt/client.hpp"
#include "isobus/vt/objects.hpp"
#include "isobus/vt/server.hpp"
#include "isobus/vt/server_working_set.hpp"
#include "isobus/vt/state_tracker.hpp"
#include "isobus/vt/update_helper.hpp"
#include "isobus/vt/working_set.hpp"

// ─── Task Controller ─────────────────────────────────────────────────────────
#include "isobus/tc/client.hpp"
#include "isobus/tc/ddi_database.hpp"
#include "isobus/tc/ddop.hpp"
#include "isobus/tc/ddop_helpers.hpp"
#include "isobus/tc/geo.hpp"
#include "isobus/tc/objects.hpp"
#include "isobus/tc/peer_control.hpp"
#include "isobus/tc/server.hpp"
#include "isobus/tc/server_options.hpp"

// ─── J1939 Engine / Powertrain ───────────────────────────────────────────────
#include "isobus/j1939/engine.hpp"
#include "isobus/j1939/transmission.hpp"

// ─── Network Interconnect Unit (ISO 11783-4) ────────────────────────────────
#include "isobus/niu/niu.hpp"

// ─── Sequence Control (ISO 11783-14) ─────────────────────────────────────────
#include "isobus/sc/client.hpp"
#include "isobus/sc/master.hpp"
#include "isobus/sc/types.hpp"

// ─── NMEA2000 ────────────────────────────────────────────────────────────────
#include "isobus/nmea/definitions.hpp"
#include "isobus/nmea/interface.hpp"
#include "isobus/nmea/n2k_management.hpp"
#include "isobus/nmea/position.hpp"

// ─── File Server (ISO 11783-13 extensions) ──────────────────────────────────
#include "isobus/fs/connection.hpp"
#include "isobus/fs/properties.hpp"

// ─── Network (Ethernet CAN bridge) ──────────────────────────────────────────
#include "isobus/network/eth_can.hpp"

// ─── NMEA Serial GNSS ───────────────────────────────────────────────────────
#include "isobus/nmea/serial_gnss.hpp"
