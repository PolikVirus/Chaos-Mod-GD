#include "BottomTimerBar.hpp"
#include <algorithm>

using namespace geode::prelude;

namespace chaosui {

static constexpr float kResetSeconds = 0.3f;

static constexpr float kBarScale = 1.10f;
static constexpr float kBarY     = 24.f;
static constexpr float kInsetPx  = 6.f;

void BottomTimerBar::init(cocos2d::CCNode* parent) {
    // Check the setting; if disabled, do nothing (no timer bar)
    if (!Mod::get()->getSettingValue<bool>("mod-enabled")) return;

    if (m_timerBar.root) m_timerBar.destroy();

    m_timerBar = createSegmentBar(parent, kBarScale, kInsetPx, 0, 1);
    if (!m_timerBar.root) return;

    auto win = cocos2d::CCDirector::sharedDirector()->getWinSize();
    m_timerBar.root->setPosition({win.width / 2.f, kBarY});
    m_timerBar.root->setZOrder(10000);

    m_timeElapsed = 0.f;
    m_isResetting = false;
    m_resetTime = 0.f;
    m_cycleDuration = static_cast<float>(Mod::get()->getSettingValue<int>("timer-length"));

    if (m_timerBar.valid()) m_timerBar.setProgress(1.f);
}

bool BottomTimerBar::update(float dt) {
    // Check the setting; if disabled, do nothing (no timer updates)
    if (!Mod::get()->getSettingValue<bool>("mod-enabled")) return false;

    if (!m_timerBar.valid()) return false;

    // keep pinned if resolution changes
    auto win = cocos2d::CCDirector::sharedDirector()->getWinSize();
    m_timerBar.root->setPosition({win.width / 2.f, kBarY});

    if (!m_isResetting) {
        m_timeElapsed += dt;
        float progress = 1.f - (m_timeElapsed / m_cycleDuration);

        if (progress <= 0.f) {
            m_timerBar.setProgress(0.f);
            m_isResetting = true;
            m_resetTime = 0.f;
            return true; // fire event now
        } else {
            m_timerBar.setProgress(std::clamp(progress, 0.f, 1.f));
        }
    } else {
        m_resetTime += dt;
        float resetProgress = m_resetTime / kResetSeconds;

        if (resetProgress >= 1.f) {
            resetProgress = 1.f;
            m_isResetting = false;
            m_timeElapsed = 0.f;
        }

        m_timerBar.setProgress(std::clamp(resetProgress, 0.f, 1.f));
    }

    return false;
}

} // namespace chaosui