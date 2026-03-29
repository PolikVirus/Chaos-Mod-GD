#include "Event.hpp"

#include <Geode/Geode.hpp>

#include <chrono>
#include <cmath>

using namespace geode::prelude;

namespace chaosmod {

static constexpr float kEventDuration = 10.f;
static constexpr int kSpeedControllerTag = 0x53504544; // 'SPED'

static PlayLayer* findPlayLayerRecursive(cocos2d::CCNode* node) {
    if (!node) return nullptr;
    if (auto pl = typeinfo_cast<PlayLayer*>(node)) return pl;

    auto children = node->getChildren();
    if (!children) return nullptr;

    for (auto obj : CCArrayExt(children)) {
        if (auto child = typeinfo_cast<cocos2d::CCNode*>(obj)) {
            if (auto pl = findPlayLayerRecursive(child)) return pl;
        }
    }
    return nullptr;
}

static PlayLayer* findCurrentPlayLayer() {
    return findPlayLayerRecursive(cocos2d::CCDirector::sharedDirector()->getRunningScene());
}

static bool isPausedLike(PlayLayer* pl) {
    return pl && !pl->isGameplayActive();
}

static bool nearlyEqual(float a, float b) {
    return std::fabs(a - b) <= 0.0001f;
}

class SpeedController : public cocos2d::CCNode {
public:
    PlayLayer* m_playLayer = nullptr;
    float m_factor = 1.f;
    float m_elapsed = 0.f;
    float m_duration = 0.f;
    float m_originalTimeScale = 1.f;
    bool m_hasOriginalTimeScale = false;
    bool m_restored = false;
    bool m_wasPaused = false;
    std::chrono::steady_clock::time_point m_lastTick;

    static SpeedController* create(PlayLayer* pl) {
        auto ctrl = new SpeedController();
        if (!ctrl) return nullptr;
        ctrl->m_playLayer = pl;
        ctrl->autorelease();
        return ctrl;
    }

    void refreshPlayLayer() {
        if (auto current = findCurrentPlayLayer()) {
            m_playLayer = current;
        }
    }

    void applyFactor() {
        auto sched = cocos2d::CCDirector::sharedDirector()->getScheduler();
        if (!sched || !m_hasOriginalTimeScale) return;
        sched->setTimeScale(m_originalTimeScale * m_factor);
    }

    void start(PlayLayer* pl, float factor, float duration) {
        m_playLayer = pl ? pl : findCurrentPlayLayer();
        m_factor = factor;
        m_duration = duration;
        m_elapsed = 0.f;
        m_restored = false;
        m_wasPaused = false;
        m_lastTick = std::chrono::steady_clock::now();

        if (!m_hasOriginalTimeScale) {
            if (auto sched = cocos2d::CCDirector::sharedDirector()->getScheduler()) {
                m_originalTimeScale = sched->getTimeScale();
                m_hasOriginalTimeScale = true;
            }
        }

        applyFactor();
        scheduleUpdate();
    }

    void update(float) override {
        if (m_restored) {
            unscheduleUpdate();
            return;
        }

        refreshPlayLayer();

        auto now = std::chrono::steady_clock::now();
        bool paused = !m_playLayer || isPausedLike(m_playLayer);

        if (m_wasPaused && !paused) {
            applyFactor();
        }

        auto sched = cocos2d::CCDirector::sharedDirector()->getScheduler();
        if (sched && m_hasOriginalTimeScale) {
            float wanted = m_originalTimeScale * m_factor;
            if (!nearlyEqual(sched->getTimeScale(), wanted)) {
                sched->setTimeScale(wanted);
            }
        }

        if (paused) {
            m_lastTick = now;
            m_wasPaused = true;
            return;
        }

        m_elapsed += std::chrono::duration_cast<std::chrono::duration<float>>(now - m_lastTick).count();
        m_lastTick = now;
        m_wasPaused = false;

        if (m_elapsed >= m_duration) {
            restoreAndDelete();
        }
    }

    void restore() {
        if (m_restored) return;
        m_restored = true;

        if (auto sched = cocos2d::CCDirector::sharedDirector()->getScheduler(); sched && m_hasOriginalTimeScale) {
            sched->setTimeScale(m_originalTimeScale);
        }
    }

    void restoreAndDelete() {
        restore();
        unscheduleUpdate();
        removeFromParentAndCleanup(true);
    }

    void onExit() override {
        restore();
        cocos2d::CCNode::onExit();
    }
};

static void changePlaySpeed(PlayLayer* pl, float factor, float duration) {
    auto scene = cocos2d::CCDirector::sharedDirector()->getRunningScene();
    if (!scene) return;

    if (auto existing = scene->getChildByTag(kSpeedControllerTag)) {
        if (auto ctrl = typeinfo_cast<SpeedController*>(existing)) {
            ctrl->start(pl, factor, duration);
            return;
        }
        existing->removeFromParentAndCleanup(true);
    }

    auto ctrl = SpeedController::create(pl);
    if (!ctrl) return;

    ctrl->setTag(kSpeedControllerTag);
    scene->addChild(ctrl, 999999);
    ctrl->start(pl, factor, duration);
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