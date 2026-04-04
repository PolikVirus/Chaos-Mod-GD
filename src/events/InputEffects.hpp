#pragma once

class PlayLayer;

namespace chaosmod {

    bool isJumpDelayBypassing();

    bool consumeJumpDelayButton(PlayLayer* pl, bool down, int button, bool isPlayer1);

    void enableJumpDelay(PlayLayer* pl, float delaySeconds, float durationSeconds);
} // namespace chaosmod
