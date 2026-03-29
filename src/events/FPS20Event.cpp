#include "Event.hpp"

#include <Geode/Geode.hpp>

#include <cmath>

using namespace geode::prelude;

namespace chaosmod {

static constexpr float kEventDuration = 30.f;
static constexpr int kFPS20ControllerTag = 0x46503230; // 'FP20'

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

static bool nearlyEqual(double a, double b) {
    return std::fabs(a - b) <= 0.0001;
}

class FPS20Controller : public cocos2d::CCNode {
public:
    PlayLayer* m_playLayer = nullptr;
    float m_timeLeft = kEventDuration;
    bool m_restored = false;
    double m_originalInterval = 0.0;
    bool m_hasOriginalInterval = false;

    static FPS20Controller* create(PlayLayer* pl) {
        auto c = new FPS20Controller();
        if (!c) return nullptr;
        c->m_playLayer = pl;
        c->autorelease();
        return c;
    }

    void applyFPS() {
        if (auto director = cocos2d::CCDirector::sharedDirector()) {
            director->setAnimationInterval(1.f / 20.f);
        }
    }

    void start(PlayLayer* pl, float duration) {
        m_playLayer = pl ? pl : findCurrentPlayLayer();
        m_timeLeft = duration;
        m_restored = false;

        if (!m_hasOriginalInterval) {
            if (auto director = cocos2d::CCDirector::sharedDirector()) {
                m_originalInterval = director->getAnimationInterval();
                m_hasOriginalInterval = true;
            }
        }

        applyFPS();
        scheduleUpdate();
    }

    void update(float dt) override {
        if (m_restored) {
            unscheduleUpdate();
            return;
        }

        if (auto current = findCurrentPlayLayer()) {
            m_playLayer = current;
        }

        auto director = cocos2d::CCDirector::sharedDirector();
        if (!director) {
            restoreAndDelete();
            return;
        }

        if (!nearlyEqual(director->getAnimationInterval(), 1.0 / 20.0)) {
            applyFPS();
        }

        if (!m_playLayer || !m_playLayer->isGameplayActive()) {
            return;
        }

        m_timeLeft -= dt;
        if (m_timeLeft <= 0.f) {
            restoreAndDelete();
        }
    }

    void restore() {
        if (m_restored) return;
        m_restored = true;
        unscheduleUpdate();

        if (auto director = cocos2d::CCDirector::sharedDirector(); director && m_hasOriginalInterval) {
            director->setAnimationInterval(m_originalInterval);
        }
    }

    void restoreAndDelete() {
        restore();
        removeFromParentAndCleanup(true);
    }

    void onExit() override {
        restore();
        cocos2d::CCNode::onExit();
    }
};

static void runFPS20Effect(PlayLayer* pl) {
    auto scene = cocos2d::CCDirector::sharedDirector()->getRunningScene();
    if (!scene) return;

    if (auto existing = scene->getChildByTag(kFPS20ControllerTag)) {
        if (auto ctrl = typeinfo_cast<FPS20Controller*>(existing)) {
            ctrl->start(pl, kEventDuration);
            return;
        }
        existing->removeFromParentAndCleanup(true);
    }

    auto ctrl = FPS20Controller::create(pl);
    if (!ctrl) return;
    ctrl->setTag(kFPS20ControllerTag);
    scene->addChild(ctrl, 999999);
    ctrl->start(pl, kEventDuration);
}

void registerFPS20Event(EventRegistry& reg) {
    reg.add(EventDef(
        "fps-20",
        "20 FPS",
        kEventDuration,
        [](PlayLayer* pl) {
            runFPS20Effect(pl);
        }
    ));
}

} // namespace chaosmod