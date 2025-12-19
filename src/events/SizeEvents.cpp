#include "Event.hpp"

#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/binding/PlayerObject.hpp>

using namespace geode::prelude;

namespace chaosmod {

static void applyMiniOnce(PlayLayer* pl, bool mini) {
    if (!pl) return;

    // Correct method: updates scale + hitbox/collision properly (same as size portals)
    if (pl->m_player1) pl->m_player1->togglePlayerScale(mini, false);
    if (pl->m_player2) pl->m_player2->togglePlayerScale(mini, false);
}

// ---- registrations ----
// One-time only (NOT enforced). Size portals can override normally.
// duration < 0 => NO timer shown in EventLog (with your patched EventLog).

void registerForceMiniMode(EventRegistry& reg) {
    reg.add(EventDef(
        "size-mini",
        "Mini Mode",
        -1.f, // NO countdown in log
        [](PlayLayer* pl) {
            applyMiniOnce(pl, true);
        }
    ));
}

void registerForceNormalSize(EventRegistry& reg) {
    reg.add(EventDef(
        "size-normal",
        "Normal Size",
        -1.f, // NO countdown in log
        [](PlayLayer* pl) {
            applyMiniOnce(pl, false);
        }
    ));
}

} // namespace chaosmod
