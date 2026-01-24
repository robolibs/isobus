#pragma once

#include "event.hpp"

namespace isobus {
    namespace util {

        // ─── Generic state machine template ─────────────────────────────────────────
        template <typename StateEnum> class StateMachine {
            StateEnum state_;

          public:
            explicit StateMachine(StateEnum initial) : state_(initial) {}

            StateEnum state() const noexcept { return state_; }

            void transition(StateEnum new_state) {
                if (new_state != state_) {
                    StateEnum old = state_;
                    state_ = new_state;
                    on_transition.emit(old, new_state);
                }
            }

            bool is(StateEnum s) const noexcept { return state_ == s; }

            Event<StateEnum, StateEnum> on_transition; // (from, to)
        };

    } // namespace util
    using namespace util;
} // namespace isobus
