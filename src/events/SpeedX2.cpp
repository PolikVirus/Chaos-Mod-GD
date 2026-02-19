#include "Event.hpp"
#include <Geode/Geode.hpp>

using namespace geode::prelude;

namespace chaosmod {

static constexpr float kEventDuration = 10.f;
static constexpr int kSpeedControllerTag = 0x53504544; // 'SPED'

// helper copied from DrunkMode for pause detection
static bool isPausedLike(PlayLayer* pl) {
    return pl && !pl->isGameplayActive();
}

class SpeedController : public cocos2d::CCNode {
public:
    PlayLayer* m_playLayer = nullptr;
    float m_baseTimeScale = 1.f;
    float m_factor = 1.f;
    bool m_restored = false;
    bool m_wasPaused = false;

    static SpeedController* create(PlayLayer* pl, float baseScale) {
        auto ctrl = new SpeedController();
        if (ctrl) {
            ctrl->m_playLayer = pl;
            ctrl->m_baseTimeScale = baseScale;
            ctrl->autorelease();
            return ctrl;
        }
        return nullptr;
    }

    void setFactor(float factor) {
        m_factor = factor;
        if (auto sched = cocos2d::CCDirector::sharedDirector()->getScheduler()) {
            sched->setTimeScale(m_baseTimeScale * factor);
        }
    }

    void update(float) override {
        // reapply when resuming from pause
        bool paused = !m_playLayer || isPausedLike(m_playLayer);
        if (m_wasPaused && !paused) {
            setFactor(m_factor);
        }
        m_wasPaused = paused;
    }

    void scheduleEnd(float duration) {
        this->stopAllActions();
        scheduleUpdate();
        this->runAction(cocos2d::CCSequence::create(
            cocos2d::CCDelayTime::create(duration),
            cocos2d::CCCallFunc::create(this, callfunc_selector(SpeedController::restoreAndDelete)),
            nullptr
        ));
    }

    void restore() {
        if (m_restored) return;
        m_restored = true;
        if (auto sched = cocos2d::CCDirector::sharedDirector()->getScheduler()) {
            sched->setTimeScale(m_baseTimeScale);
        }
    }

    void restoreAndDelete() {
        restore();
        this->removeFromParentAndCleanup(true);
    }

    void onExit() override {
        if (!m_playLayer || !isPausedLike(m_playLayer)) {
            restore();
        }
        cocos2d::CCNode::onExit();
    }
};

static void changePlaySpeed(PlayLayer* pl, float factor, float duration) {
    if (!pl) return;
    auto sched = cocos2d::CCDirector::sharedDirector()->getScheduler();
    if (!sched) return;

    if (auto existing = pl->getChildByTag(kSpeedControllerTag)) {
        if (auto ctrl = dynamic_cast<SpeedController*>(existing)) {
            ctrl->setFactor(factor);
            ctrl->scheduleEnd(duration);
            return;
        }
        existing->removeFromParentAndCleanup(true);
    }

    float base = sched->getTimeScale();
    auto ctrl = SpeedController::create(pl, base);
    if (!ctrl) return;
    ctrl->setTag(kSpeedControllerTag);
    pl->addChild(ctrl, 999999);
    ctrl->setFactor(factor);
    ctrl->scheduleEnd(duration);
}

void registerSpeedX2(EventRegistry& reg) {
    reg.add(EventDef(
        "speed-x2",
        "Speed x2",
        kEventDuration,
        [](PlayLayer* pl) {
            changePlaySpeed(pl, 2.f, kEventDuration);
        }
    ));
}

void registerSpeedX1_5(EventRegistry& reg) {
    reg.add(EventDef(
        "speed-x1-5",
        "Speed x1.5",
        kEventDuration,
        [](PlayLayer* pl) {
            changePlaySpeed(pl, 1.5f, kEventDuration);
        }
    ));
}

void registerSpeedX0_5(EventRegistry& reg) {
    reg.add(EventDef(
        "speed-x0-5",
        "Speed x0.5",
        kEventDuration,
        [](PlayLayer* pl) {
            changePlaySpeed(pl, 0.5f, kEventDuration);
        }
    ));
}

} // namespace chaosmod
