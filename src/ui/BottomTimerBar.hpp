#pragma once
#include <Geode/Geode.hpp>
#include "SegmentBar.hpp"

namespace chaosui {

class BottomTimerBar {
public:
    void init(cocos2d::CCNode* parent);
    // returns true exactly when the timer hits 0 and an event should fire
    bool update(float dt);

private:
    SegmentBarWidget m_bar;

    float m_elapsed = 0.f;
    bool  m_resetting = false;
    float m_resetTimer = 0.f;
};

} // namespace chaosui
