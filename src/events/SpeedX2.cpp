#include "Event.hpp"

#include <Geode/Geode.hpp>

using namespace geode::prelude;

namespace chaosmod {

static constexpr float kDurationSeconds = 10.f;
static constexpr int kSpeedControllerTag = 0x53504544; // 'SPED'

class SpeedEventController : public cocos2d::CCNode {
public:
    float m_baseScale = 1.f;
    bool m_restored = false;

    static SpeedEventController* create(float baseScale) {
        auto ret = new SpeedEventController();
        if (ret) {
            ret->m_baseScale = baseScale;
            ret->autorelease();
            return ret;
        }
        return nullptr;
    }

    void applyFactor(float factor) {
        auto sched = cocos2d::CCDirector::sharedDirector()->getScheduler();
        if (!sched) return;
        sched->setTimeScale(m_baseScale * factor);
    }

    void startTimer(float durationSeconds) {
        this->stopAllActions();
        this->runAction(cocos2d::CCSequence::create(
            cocos2d::CCDelayTime::create(durationSeconds),
            cocos2d::CCCallFunc::create(this, callfunc_selector(SpeedEventController::restoreAndRemove)),
            nullptr
        ));
    }

    void restore() {
        if (m_restored) return;
        m_restored = true;

        auto sched = cocos2d::CCDirector::sharedDirector()->getScheduler();
        if (!sched) return;
        sched->setTimeScale(m_baseScale);
    }

    void restoreAndRemove() {
        this->restore();
        this->removeFromParentAndCleanup(true);
    }

    void onExit() override {
        // Fail-safe: restore if the PlayLayer exits before our timer ends.
        this->restore();
        cocos2d::CCNode::onExit();
    }
};

static void applySpeedFactor(PlayLayer* pl, float factor, float durationSeconds) {
    if (!pl) return;

    auto sched = cocos2d::CCDirector::sharedDirector()->getScheduler();
    if (!sched) return;

    // If already active, reuse the controller: switch factor + refresh timer.
    if (auto existing = pl->getChildByTag(kSpeedControllerTag)) {
        if (auto ctrl = dynamic_cast<SpeedEventController*>(existing)) {
            ctrl->applyFactor(factor);
            ctrl->startTimer(durationSeconds);
            return;
        }
        // Tag collision (unlikely) — remove and recreate cleanly.
        existing->removeFromParentAndCleanup(true);
    }

    float base = sched->getTimeScale();
    auto ctrl = SpeedEventController::create(base);
    if (!ctrl) return;

    ctrl->setTag(kSpeedControllerTag);
    pl->addChild(ctrl, 999999);

    ctrl->applyFactor(factor);
    ctrl->startTimer(durationSeconds);
}

// ---- Event registrations ----

void registerSpeedX2(EventRegistry& reg) {
    reg.add(EventDef(
        "speed-x2",
        "Speed x2",
        kDurationSeconds,
        [](PlayLayer* pl) {
            applySpeedFactor(pl, 2.f, kDurationSeconds);
        }
    ));
}

void registerSpeedX1_5(EventRegistry& reg) {
    reg.add(EventDef(
        "speed-x1-5",
        "Speed x1.5",
        kDurationSeconds,
        [](PlayLayer* pl) {
            applySpeedFactor(pl, 1.5f, kDurationSeconds);
        }
    ));
}

void registerSpeedX0_5(EventRegistry& reg) {
    reg.add(EventDef(
        "speed-x0-5",
        "Speed x0.5",
        kDurationSeconds,
        [](PlayLayer* pl) {
            applySpeedFactor(pl, 0.5f, kDurationSeconds);
        }
    ));
}

} // namespace chaosmod
