#pragma once

class PlayLayer;

namespace chaosmod {
    // Internal bypass used to avoid re-consuming when we replay delayed inputs.
    bool isJumpDelayBypassing();

    // Called from PlayLayer::handleButton hook.
    // Returns true if the input was consumed (delayed), false if it should pass through normally.
    bool consumeJumpDelayButton(PlayLayer* pl, bool down, int button, bool isPlayer1);

    // Called by the Jump Delay event.
    void enableJumpDelay(PlayLayer* pl, float delaySeconds, float durationSeconds);
}
