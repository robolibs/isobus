#pragma once

#include "../core/constants.hpp"
#include "../core/error.hpp"
#include "../core/message.hpp"
#include "../core/types.hpp"
#include "../util/event.hpp"
#include "internal_cf.hpp"
#include "network_manager.hpp"
#include <datapod/datapod.hpp>
#include <echo/echo.hpp>

namespace isobus {
    namespace network {

        // ─── Working Set Protocol (ISO 11783-7 Section 10) ──────────────────────────
        // Working Sets define groups of control functions that cooperate.
        // The master broadcasts Working Set Master message followed by
        // (set_size - 1) Working Set Member messages at 100ms intervals.

        struct WorkingSetMember {
            Name member_name; // 8-byte NAME of the member CF
        };

        // ─── Working Set Manager ────────────────────────────────────────────────────
        // Handles broadcasting and receiving of working set announcements.
        class WorkingSetManager {
            NetworkManager &net_;
            InternalCF *cf_;
            dp::Vector<Name> members_;                       // member NAMEs (excluding master)
            dp::Map<Address, dp::Vector<Name>> remote_sets_; // remote working sets

          public:
            WorkingSetManager(NetworkManager &net, InternalCF *cf) : net_(net), cf_(cf) {}

            Result<void> initialize() {
                if (!cf_) {
                    return Result<void>::err(Error::invalid_state("control function not set"));
                }
                net_.register_pgn_callback(PGN_WORKING_SET_MASTER, [this](const Message &msg) { handle_master(msg); });
                net_.register_pgn_callback(PGN_WORKING_SET_MEMBER, [this](const Message &msg) { handle_member(msg); });
                echo::category("isobus.network.working_set").debug("initialized");
                return {};
            }

            // ─── Local working set management ────────────────────────────────────────
            Result<void> add_member(Name member_name) {
                members_.push_back(member_name);
                return {};
            }

            void clear_members() { members_.clear(); }
            const dp::Vector<Name> &members() const noexcept { return members_; }
            usize set_size() const noexcept { return members_.size() + 1; } // +1 for master

            // ─── Broadcast working set ───────────────────────────────────────────────
            // Call this periodically (or on network join) to announce the working set.
            // Sends master message (set size) followed by member messages at 100ms intervals.
            // Per ISO 11783-7 Section 10.2: Master message defines set size,
            // member messages identify each member by NAME.
            Result<void> broadcast_working_set() {
                if (!cf_) {
                    return Result<void>::err(Error::invalid_state("control function not set"));
                }

                // Send Working Set Master message: Byte 1 = total set size (master + members)
                dp::Vector<u8> master_data(8, 0xFF);
                master_data[0] = static_cast<u8>(set_size());
                auto result = net_.send(PGN_WORKING_SET_MASTER, master_data, cf_);
                if (!result.is_ok())
                    return result;

                // Send Working Set Member messages
                for (const auto &member : members_) {
                    dp::Vector<u8> member_data(8, 0xFF);
                    auto mbytes = member.to_bytes();
                    for (usize i = 0; i < 8; ++i) {
                        member_data[i] = mbytes[i];
                    }
                    result = net_.send(PGN_WORKING_SET_MEMBER, member_data, cf_);
                    if (!result.is_ok())
                        return result;
                }

                echo::category("isobus.network.working_set").debug("broadcast working set: size=", set_size());
                return {};
            }

            // ─── Query remote working sets ───────────────────────────────────────────
            const dp::Map<Address, dp::Vector<Name>> &remote_sets() const noexcept { return remote_sets_; }

            dp::Optional<dp::Vector<Name>> get_remote_set(Address master_addr) const {
                auto it = remote_sets_.find(master_addr);
                if (it != remote_sets_.end()) {
                    return it->second;
                }
                return dp::nullopt;
            }

            // Events
            Event<Address, Name> on_master_received; // master address, master NAME
            Event<Address, Name> on_member_received; // master address, member NAME

          private:
            void handle_master(const Message &msg) {
                if (msg.data.empty())
                    return;
                u8 announced_size = msg.data[0];  // Total members (including master)
                remote_sets_[msg.source].clear(); // new announcement resets the set
                // Use the master's NAME from the network manager for the event
                Name master_name; // Will be resolved by address claim
                on_master_received.emit(msg.source, master_name);
                echo::category("isobus.network.working_set")
                    .debug("master from SA=", msg.source, " size=", announced_size);
            }

            void handle_member(const Message &msg) {
                if (msg.data.size() < 8)
                    return;
                Name member_name = Name::from_bytes(msg.data.data());
                remote_sets_[msg.source].push_back(member_name);
                on_member_received.emit(msg.source, member_name);
                echo::category("isobus.network.working_set")
                    .debug("member from SA=", msg.source, " count=", remote_sets_[msg.source].size());
            }
        };

    } // namespace network
    using namespace network;
} // namespace isobus
