#include "Event.hpp"

namespace chaosmod {
    void registerSpeedX2(EventRegistry&);
    void registerSpeedX1_5(EventRegistry&);
    void registerSpeedX0_5(EventRegistry&);

    void registerLowGravity(EventRegistry&);
    void registerHighGravity(EventRegistry&);

    void registerReverseControls(EventRegistry&);

    void registerForceMiniMode(EventRegistry&);
    void registerForceNormalSize(EventRegistry&);

    void registerJumpDelay(EventRegistry&);

    void registerSoggyJumpscare(EventRegistry&);

    void registerInvertColors(EventRegistry&);

    void registerAllEvents(EventRegistry& reg) {
        registerSpeedX2(reg);
        registerSpeedX1_5(reg);
        registerSpeedX0_5(reg);

        registerLowGravity(reg);
        registerHighGravity(reg);

        registerReverseControls(reg);

        registerForceMiniMode(reg);
        registerForceNormalSize(reg);

        registerJumpDelay(reg);

        registerSoggyJumpscare(reg);

        registerInvertColors(reg);
    }
}
