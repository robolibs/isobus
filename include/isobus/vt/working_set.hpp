#pragma once

#include "../core/types.hpp"
#include "objects.hpp"
#include <datapod/datapod.hpp>

namespace isobus::vt {

    // ─── Working set management ──────────────────────────────────────────────────
    class WorkingSet {
        ObjectID active_mask_ = 0;
        dp::Vector<ObjectID> masks_;
        dp::Vector<ObjectID> soft_key_masks_;

      public:
        void set_active_mask(ObjectID mask_id) noexcept { active_mask_ = mask_id; }

        ObjectID active_mask() const noexcept { return active_mask_; }

        void add_data_mask(ObjectID mask_id) { masks_.push_back(mask_id); }

        void add_soft_key_mask(ObjectID mask_id) { soft_key_masks_.push_back(mask_id); }

        const dp::Vector<ObjectID> &data_masks() const noexcept { return masks_; }
        const dp::Vector<ObjectID> &soft_key_masks() const noexcept { return soft_key_masks_; }
    };

} // namespace isobus::vt
namespace isobus {
    using namespace vt;
}
