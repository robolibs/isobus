# wisobus

Modern C++20 ISO 11783 (ISOBUS) + J1939 + NMEA2000 networking stack with a fluent, application-first API.

Without it, this literally would not have happened. A big part of this library is best described as "a nicer API on top of them" - because the hard protocol truths, edge cases, and real-world behavior are learned from them first. See `ACKNOWLEDGMENTS.md`.

## Development Status

This repository uses `PLAN.md` as the master implementation roadmap.

## Overview

`wisobus` is a header-first CAN protocol stack focused on ISO 11783 (ISOBUS) and related ecosystems used on agricultural and marine equipment.
It provides a high level message/transport/network abstraction on top of a raw CAN endpoint, while keeping the API close to the underlying
standards (PGNs, source/destination addressing, address-claiming, TP/ETP, etc.).

The core idea is: application code should read like intent ("send this PGN", "subscribe to that PGN", "start address claiming") while the
library handles the repetitive protocol machinery: frame encoding, multi-packet reassembly, session tracking, and common timing rules.

The project is designed as a set of composable modules:
- A CAN frame layer that encodes/decodes 29-bit extended identifiers
- A network manager that routes frames, performs address claiming, and dispatches messages by PGN
- Transport protocols (TP / ETP) for multi-packet transfers and an optional NMEA2000 fast-packet implementation
- Protocol implementations for frequently used ISOBUS/J1939 services (diagnostics, file transfer, VT, TC, TIM, etc.)

### Architecture Diagrams

High level data flow (receive path):

```
┌───────────────────────────────────────────────────────────────────────────┐
│                                 Application                               │
│  - register_pgn_callback(PGN, fn)                                          │
│  - protocol modules (VT/TC/TIM/FS/Diagnostics/...)                         │
└───────────────────────────────┬───────────────────────────────────────────┘
                                │ on_message / callbacks
                                ▼
┌───────────────────────────────────────────────────────────────────────────┐
│                        isobus::network::NetworkManager                     │
│  - endpoint polling / send_frame                                           │
│  - address claiming + CF tracking                                          │
│  - routes TP/ETP/FastPacket frames                                         │
│  - dispatches Message{pgn,data,src,dst,prio,ts}                             │
└───────────────┬───────────────────────┬───────────────────────┬───────────┘
                │                       │                       │
                ▼                       ▼                       ▼
      ┌────────────────┐      ┌──────────────────┐     ┌──────────────────┐
      │ AddressClaimer │      │ TransportProtocol │     │ FastPacketProtocol│
      │ (ISO11783-5)   │      │ + ETP            │     │ (NMEA2000)        │
      └───────┬────────┘      └───────┬──────────┘     └─────────┬────────┘
              │                       │                            │
              └───────────────┬───────┴───────────────┬────────────┘
                              ▼                       ▼
                   ┌──────────────────┐    ┌─────────────────────────┐
                   │   Frame / ID     │    │ wirebit::CanEndpoint     │
                   │ (29-bit CAN ID)  │    │ (driver abstraction)     │
                   └──────────────────┘    └─────────────────────────┘
```

Library surface (include layout):

```
include/isobus.hpp
└── include/isobus/
    ├── core/        (types, PGN/NAME/ID, Frame, Message, errors)
    ├── util/        (event, scheduler, timer, bitfield, state machine)
    ├── network/     (NetworkManager, InternalCF/PartnerCF, address claiming)
    ├── transport/   (TP, ETP, session model, NMEA2000 fast packet)
    ├── protocol/    (common ISOBUS/J1939 protocols)
    ├── implement/   (tractor/implement messages and helpers)
    ├── vt/          (Virtual Terminal client/server + object pool helpers)
    ├── tc/          (Task Controller client/server, DDOP/DDI, geo, peer control)
    ├── sc/          (Sequence Control master/client)
    ├── niu/         (Network Interconnect Unit pieces)
    ├── nmea/        (NMEA2000 definitions + parser/generator + GNSS)
    └── fs/          (file server connection/properties helpers)
```

## Installation

Repository: https://github.com/robolibs/isobus

### Quick Start (CMake FetchContent)

```cmake
include(FetchContent)
FetchContent_Declare(
  wisobus
  GIT_REPOSITORY https://github.com/robolibs/isobus
  GIT_TAG main
)
FetchContent_MakeAvailable(wisobus)

target_link_libraries(your_target PRIVATE wisobus::wisobus)
```

Notes:
- The project is C++20.
- Dependencies are pulled via `FetchContent` as declared in `PROJECT`.

### Complete Development Environment (Nix + Direnv + Devbox)

This repository includes `devbox.json` and is compatible with direnv.

1) Install Nix:

```bash
curl --proto '=https' --tlsv1.2 -sSf -L https://install.determinate.systems/nix | sh -s -- install
```

2) Install direnv:

```bash
sudo apt install direnv
eval "$(direnv hook bash)"
```

3) Install Devbox:

```bash
curl -fsSL https://get.jetpack.io/devbox | bash
```

4) Enter the environment:

```bash
direnv allow
```

## Usage

The central entrypoint is `isobus::network::NetworkManager`. You attach one or more CAN endpoints (ports), create one or more internal control
functions, start address claiming, then send/receive PGNs.

### Basic Usage

```cpp
#include <isobus.hpp>

using namespace isobus;

int main() {
    network::NetworkManager net(
        network::NetworkConfig{}
            .ports(1)
            .bus_load(true)
            .fast_packet(false));

    // Provide a wirebit::CanEndpoint for port 0.
    // net.set_endpoint(0, &endpoint);

    // Create an internal control function (ECU) and claim an address.
    auto icf = net.create_internal(Name{}, 0).value();
    net.start_address_claiming();

    // Subscribe to a PGN.
    net.register_pgn_callback(PGN_ADDRESS_CLAIMED, [](const Message &msg) {
        (void)msg;
    });

    // Send a message (single frame / TP / ETP chosen automatically).
    dp::Vector<u8> payload;
    payload.push_back(0xFF);
    net.send(0x00EA00 /* Request PGN */, payload, icf);

    while (true) {
        net.update(10);
    }
}
```

### Advanced Usage

Transport selection rules are built-in:
- <= 8 bytes: single CAN frame
- 9..1785 bytes: ISO 11783 / J1939 TP
- > 1785 bytes: ISO 11783 / J1939 ETP (connection-mode only)
- Optional: NMEA2000 fast packet for registered PGNs

You can register NMEA2000 fast-packet PGNs:

```cpp
net.register_fast_packet_pgn(129029); // Example N2K GNSS position PGN
```

And you can access the protocol engines directly for custom integrations:

```cpp
auto &tp = net.transport_protocol();
auto &etp = net.extended_transport_protocol();
auto &fp = net.fast_packet_protocol();
(void)tp;
(void)etp;
(void)fp;
```

## Features

- **Network manager** - Port-based CAN endpoint integration, address claiming, CF tracking, and PGN-based dispatch.
- **Transport (TP/ETP)** - Automatic segmentation/reassembly for multi-packet ISO 11783 / J1939 messages.
- **NMEA2000 fast packet** - Optional fast-packet support for registered PGNs.
- **Protocol modules** - Building blocks for common services: diagnostics, file transfer, group function, request handling, heartbeat, time/date.
- **Virtual Terminal (VT)** - Client/server support with object pool utilities.
- **Task Controller (TC)** - Client/server, DDOP helpers, DDI database, geo helpers, and peer control.
- **Sequence Control (SC)** - Master/client components and types.
- **Integration tests + examples** - Large set of real usage examples under `examples/` and protocol tests under `test/`.

## Building and Testing

This repo ships a `Makefile` wrapper that selects a build system (CMake by default).

Common targets:

```bash
make config
make build
make test
```

Notes:
- `make build` runs `clang-format` over `./include` and `./src` before compiling.
- CMake options are driven by `PROJECT` and exposed as `WISOBUS_BUILD_EXAMPLES`, `WISOBUS_ENABLE_TESTS`, and `WISOBUS_BIG_TRANSFER`.

## Dependency Graph

`wisobus` is split into internal modules, but also composes a small set of external libraries.
The `PROJECT` file is the source of truth for versions.

External dependencies (library):
- `echo` - logging and categories used throughout the stack
- `datapod` - containers and utilities (`dp::Vector`, `dp::Map`, `dp::Array`, `dp::Optional`)
- `optinum` - optional helpers and small utilities (varies by module)
- `wirebit` - CAN driver abstraction (`wirebit::CanEndpoint`) and CAN primitives
- `concord` - small support library used by some components

Test dependency:
- `doctest` - unit tests in `test/`

Internal dependency map (conceptual):

```
core
  ├── util
  ├── network
  │     ├── transport
  │     └── protocol
  ├── vt
  ├── tc
  ├── sc
  ├── niu
  ├── nmea
  └── fs
```

## Core Concepts

This section gives names to the key building blocks, so the rest of the codebase is easier to navigate.

### CAN Frame vs Message

- `isobus::Frame` is a single CAN frame: a 29-bit identifier and up to 8 data bytes.
- `isobus::Message` is an arbitrary length payload tagged with routing metadata (PGN, src, dst, priority).
- The NetworkManager turns incoming frames into messages either directly (single frame) or after reassembly (TP/ETP/FastPacket).

### PGN / Priority / Addressing

- A PGN is the high level message type identifier in J1939 / ISO 11783.
- Messages are either broadcast or destination-specific.
- For destination-specific messages the destination address is encoded in the 29-bit identifier.

### Control Functions (CF)

`wisobus` models the bus participants as Control Functions:
- `InternalCF` represents an ECU owned by your application.
- `PartnerCF` represents a remote ECU that you care about and optionally filter by NAME.

Control functions have a NAME (64-bit) and an address (source address, SA).

### Address Claiming

On an ISOBUS/J1939 network, devices claim an address using their NAME and a priority comparison.
This repo includes an address claimer that participates in that process and updates control function state.

From the app perspective:
- create one or more `InternalCF`
- attach a CAN endpoint to a port
- call `start_address_claiming()`
- poll `update()`

### Ports and Endpoints

The library supports multiple ports so you can represent:
- multi-channel gateways
- multi-bus simulation
- NIU style bridging

Each port is attached to a `wirebit::CanEndpoint` which provides:
- `send_can(can_frame)`
- `recv_can(can_frame)`

### Transport Protocols

J1939 / ISO 11783 defines multi-packet transport on top of CAN:
- TP (up to 1785 bytes)
- ETP (extended, for larger payloads; connection-mode only)

`NetworkManager::send()` automatically picks:
- single frame for <= 8 bytes
- TP for 9..1785
- ETP for > 1785 when destination-specific

The receive side reassembles the payload and emits a single `Message`.

### NMEA2000 Fast Packet

NMEA2000 uses a different segmentation scheme called fast packet.
`wisobus` includes a fast packet engine that can be enabled and tied to a set of PGNs you register.

### Events and Callbacks

There are two complementary ways to consume messages:
- `NetworkManager::on_message` - stream of all decoded messages
- `NetworkManager::register_pgn_callback(pgn, fn)` - PGN specific callbacks

Both are synchronous callbacks fired in `NetworkManager::update()`.

## Protocol Coverage

This codebase intentionally spans multiple parts of ISO 11783 plus a subset of J1939 and NMEA2000.
The library is not claiming full compliance in every part yet; see `PLAN.md` for gaps and ordering.

### Implemented Areas (Highlights)

- **Core J1939/ISOBUS routing**: identifier encode/decode, frame/message model
- **Network layer**: address claiming, CF tracking, PGN dispatch, bus load tracking
- **Transport**: TP + ETP session handling, plus optional NMEA2000 fast packet
- **Virtual Terminal (VT)**: object pool modeling, client/server utilities, state tracking
- **Task Controller (TC)**: client/server, DDOP helpers, DDI database, geo helpers, peer control
- **Diagnostics**: DM1/DTC handling and other diagnostic protocol helpers
- **File Server**: file transfer protocol helpers and FS connection/properties types
- **Sequence Control (SC)**: master/client types and state machine scaffolding
- **NMEA2000**: definitions, interface parsing/generation, GNSS helpers

### Relationship Between Standards

This diagram shows how the standards are layered from a software perspective.

```
         ┌──────────────────────────────────────────┐
         │              Application                 │
         └─────────────────────┬────────────────────┘
                               │
                               ▼
┌──────────────────────────────────────────────────────────────┐
│                 ISO 11783 Application Profiles               │
│     VT (Part 6)   TC (Part 10)   TIM (Part 12)   SC (Part 14) │
└──────────────────────────────┬───────────────────────────────┘
                               │
                               ▼
┌──────────────────────────────────────────────────────────────┐
│          ISO 11783 Network/Transport + J1939 Services         │
│      Address Claiming, TP/ETP, Diagnostics, Request, etc.     │
└──────────────────────────────┬───────────────────────────────┘
                               │
                               ▼
┌──────────────────────────────────────────────────────────────┐
│                         CAN 2.0B                             │
│                29-bit identifier + 8 byte payload            │
└──────────────────────────────────────────────────────────────┘
```

## Module Tour

If you are reading the code, these are the most useful entrypoints.

### `include/isobus.hpp`

The umbrella include that exposes the full stack in one header.

### `include/isobus/core/*`

- `types.hpp` - fixed width integer aliases and common scalar types
- `pgn.hpp` and `pgn_defs.hpp` - PGN types and common definitions
- `name.hpp` - J1939 NAME packing/unpacking and helpers
- `identifier.hpp` - 29-bit identifier encode/decode (priority, PGN, src, dst)
- `frame.hpp` - CAN frame wrapper
- `message.hpp` - decoded message container for arbitrary-length payloads
- `error.hpp` - error codes and `Result<T>` wrapper

### `include/isobus/network/*`

- `network_manager.hpp` - the central orchestrator, owns transport engines, claimers, callbacks
- `address_claimer.hpp` - address claiming state machine and timing
- `control_function.hpp` - common CF types and state
- `internal_cf.hpp` - internal ECU representation
- `partner_cf.hpp` - partner discovery by NAME filtering
- `working_set.hpp` - helpers for ISOBUS working set modeling
- `eth_can.hpp` - Ethernet-CAN bridge integration point

### `include/isobus/transport/*`

- `tp.hpp` - TP connection management (RTS/CTS, BAM, DT)
- `etp.hpp` - extended transport protocol
- `session.hpp` - shared session state model
- `fast_packet.hpp` - NMEA2000 fast packet segmentation/reassembly

### `include/isobus/protocol/*`

This folder contains protocol helpers and message types. Examples include:
- `acknowledgment.hpp` - ACK/NACK handling
- `diagnostic.hpp` and `dm_memory.hpp` - diagnostic protocol helpers
- `file_transfer.hpp` - file server protocol messages
- `group_function.hpp` - group function protocol
- `heartbeat.hpp` - periodic heartbeat
- `request2.hpp` and `pgn_request.hpp` - request handling
- `language.hpp` and `time_date.hpp` - localization and time/date messages

### `include/isobus/vt/*`

Virtual Terminal support:
- object definitions and pool management
- client/server roles
- update helpers and state trackers

### `include/isobus/tc/*`

Task Controller support:
- client/server roles
- DDOP modeling and helpers
- DDI database
- geo helpers and peer control

### `include/isobus/nmea/*`

NMEA2000 support:
- definitions and PGN lists
- parser/generator utilities
- serial GNSS helpers

## Safety and Correctness

Industrial networks are harsh: you will see missing packets, reboots, and bad actors.
The library contains a mix of defensive decoding and explicit state tracking.

Key behaviors:
- Default values for missing fields in message decoders (0xFF/0xFFFF patterns where appropriate)
- Strict size bounds for TP/ETP payloads
- Address violation detection in `NetworkManager` (re-assert address claim when another device uses our SA)

## Performance Notes

This project is focused on low overhead message routing.
Some choices you will see:
- stack-friendly fixed arrays for CAN frame payloads
- vector-based payloads only after multi-packet reassembly
- minimal allocations on the single-frame path

SIMD flags can be enabled/disabled via `WISOBUS_ENABLE_SIMD` when building with CMake.

## Testing

Tests live under `test/` and use doctest.
They cover:
- core encoding/decoding (PGN/ID/frame/message)
- TP/ETP session behavior and timers
- VT pool validation and end-to-end flows
- TC DDOP correctness and end-to-end flows
- NMEA2000 parsing and batch utilities

To run everything:

```bash
make config
make test
```

## Project Layout

Top-level highlights:
- `include/` - public headers
- `src/` - optional compiled sources (this repo is mostly header-first)
- `examples/` - example executables (picked up automatically by CMake)
- `test/` - doctest test executables (picked up automatically by CMake)
- `xtra/` - reference and experimental code, including a vendored AgIsoStack++
- `PROJECT` - name/version/dependency manifest consumed by CMake
- `PLAN.md` - long-form implementation roadmap

## Contributing

Contributions are welcome, especially:
- protocol conformance fixes vs ISO 11783 / J1939 specs
- interoperability testing reports with real ECUs
- additional tests covering edge cases and timing

Practical guidance:
- keep public APIs in `include/`
- prefer small, testable message encoders/decoders
- add a focused example when adding a large feature

## Troubleshooting

### Build fails fetching dependencies

Dependencies are pulled via CMake FetchContent.
If you are behind a proxy or have restricted outbound access:
- set `CMAKE_TLS_VERIFY=ON` and configure proxy env vars
- prefer a local dependency mirror

### No messages received

Check these common integration issues:
- your `wirebit::CanEndpoint` returns frames in extended format (29-bit IDs)
- you are calling `NetworkManager::update()` frequently enough
- your internal CF has successfully claimed an address (not NULL)

### TP/ETP sessions time out

Multi-packet transfers require periodic updates to progress timers.
Ensure `update(elapsed_ms)` is called with realistic timing, and that your endpoint send/recv is non-blocking.

## Notes

- This README is intentionally long to serve as an orientation map for a large codebase.
- For detailed protocol implementation tasks and missing pieces, follow `PLAN.md`.

## Examples

Examples are built when examples are enabled in your build system configuration.
The `examples/` directory contains both small demos and more involved scenarios.

Selected entrypoints (non-exhaustive):
- `examples/network_basic.cpp` - minimal network manager usage
- `examples/socketcan_demo.cpp` - SocketCAN style integration
- `examples/virtual_can_demo.cpp` - virtual CAN bus demo
- `examples/transport_demo.cpp` and `examples/complex/transport_protocol.cpp` - TP/ETP usage
- `examples/vt_client_demo.cpp` / `examples/vt_server_demo.cpp` - VT client/server demos
- `examples/tc_client_demo.cpp` / `examples/tc_server_demo.cpp` - Task Controller demos
- `examples/tim_demo.cpp` - TIM demo
- `examples/serial_gnss.cpp` and NMEA2K examples under `examples/complex/`

## Design Notes

- **Message model** - `isobus::Message` represents an arbitrary-length payload after reassembly, tagged with PGN/source/destination.
- **Events** - `isobus::Event<T...>` is used for callbacks like `NetworkManager::on_message`.
- **Port abstraction** - one `NetworkManager` instance can manage multiple CAN ports, each bound to a `wirebit::CanEndpoint`.
- **Reference implementation** - a vendored copy of AgIsoStack++ is kept under `xtra/` for protocol behavior reference.

## Versioning

Project version is defined in `PROJECT`.

## License

MIT License - see `LICENSE` for details.

## Acknowledgments

Made possible thanks to the projects and references listed in `ACKNOWLEDGMENTS.md`.
