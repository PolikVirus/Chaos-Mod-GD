#include "Event.hpp"

#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>

using namespace geode::prelude;

namespace chaosmod {

static constexpr char const* evId = "pause-game";
static constexpr char const* evName = "Pause game";
static constexpr float logDur = -1.f;

static void runPauseGame(PlayLayer* pl) {
    if (!pl) return;
    if (!pl->isGameplayActive()) return;
    pl->pauseGame(false);
}

void registerPauseGame(EventRegistry& reg) {
    reg.add(EventDef(
        evId,
        evName,
        logDur,
        &runPauseGame
    ));
}

} // namespace chaosmod