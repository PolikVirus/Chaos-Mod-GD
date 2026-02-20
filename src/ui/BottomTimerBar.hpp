#pragma once
#include <Geode/Geode.hpp>
#include "SegmentBar.hpp"

namespace chaosui {

class BottomTimerBar {
public:
    void init(cocos2d::CCNode* parent);
    bool update(float dt);

private:
    SegmentBarWidget m_timerBar;

    float m_timeElapsed = 0.f;
    bool  m_isResetting = false;
    float m_resetTime = 0.f;
    float m_cycleDuration = 30.f;
};

} // namespace chaosui
