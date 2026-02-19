#include "BottomTimerBar.hpp"
#include <algorithm>

using namespace geode::prelude;

namespace chaosui {

static constexpr float kResetSeconds = 0.3f;

static constexpr float kBarScale = 1.10f;
static constexpr float kBarY     = 24.f;
static constexpr float kInsetPx  = 6.f;

void BottomTimerBar::init(cocos2d::CCNode* parent) {
    if (m_bar.root) m_bar.destroy();

    m_bar = createSegmentBar(parent, kBarScale, kInsetPx, 0, 1);
    if (!m_bar.root) return;

    auto win = cocos2d::CCDirector::sharedDirector()->getWinSize();
    m_bar.root->setPosition({win.width / 2.f, kBarY});
    m_bar.root->setZOrder(10000);

    m_elapsed = 0.f;
    m_resetting = false;
    m_resetTimer = 0.f;
    m_cycleSeconds = static_cast<float>(Mod::get()->getSettingValue<int>("timer-length"));

    if (m_bar.valid()) m_bar.setProgress(1.f);
}

bool BottomTimerBar::update(float dt) {
    if (!m_bar.valid()) return false;

    // keep pinned if resolution changes
    auto win = cocos2d::CCDirector::sharedDirector()->getWinSize();
    m_bar.root->setPosition({win.width / 2.f, kBarY});

    if (!m_resetting) {
        m_elapsed += dt;
        float t = 1.f - (m_elapsed / m_cycleSeconds);

        if (t <= 0.f) {
            m_bar.setProgress(0.f);
            m_resetting = true;
            m_resetTimer = 0.f;
            return true; // fire event now
        } else {
            m_bar.setProgress(std::clamp(t, 0.f, 1.f));
        }
    } else {
        m_resetTimer += dt;
        float r = m_resetTimer / kResetSeconds;

        if (r >= 1.f) {
            r = 1.f;
            m_resetting = false;
            m_elapsed = 0.f;
        }

        m_bar.setProgress(std::clamp(r, 0.f, 1.f));
    }

    return false;
}

} // namespace chaosui
