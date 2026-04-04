#include "Event.hpp"

#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/binding/PlayerObject.hpp>

using namespace geode::prelude;

namespace chaosmod {

static void setPlayerSizeOnce(PlayLayer* pl, bool mini) {
    if (!pl) return;
    if (pl->m_player1) pl->m_player1->togglePlayerScale(mini, false);
    if (pl->m_player2) pl->m_player2->togglePlayerScale(mini, false);
}

void registerMini(EventRegistry& reg) {
    reg.add(EventDef(
        "size-mini",
        "Mini Mode",
        -1.f,
        [](PlayLayer* pl) {
            setPlayerSizeOnce(pl, true);
        }
    ));
}

} // namespace chaosmod