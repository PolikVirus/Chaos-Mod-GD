#include "Event.hpp"

#include <Geode/Geode.hpp>

#include <cmath>

using namespace geode::prelude;

namespace chaosmod {

static constexpr float dur = 30.f;
static constexpr int fpsTag = 0x46503230;

static PlayLayer* findPL(cocos2d::CCNode* node) {
    if (!node) return nullptr;
    if (auto pl = typeinfo_cast<PlayLayer*>(node)) return pl;

    auto children = node->getChildren();
    if (!children) return nullptr;

    for (auto obj : CCArrayExt(children)) {
        if (auto child = typeinfo_cast<cocos2d::CCNode*>(obj)) {
            if (auto pl = findPL(child)) return pl;
        }
    }
    return nullptr;
}

static PlayLayer* curPL() {
    return findPL(cocos2d::CCDirector::sharedDirector()->getRunningScene());
}

static bool sameish(double a, double b) {
    return std::fabs(a - b) <= 0.0001;
}

class FpsCtrl : public cocos2d::CCNode {
public:
    PlayLayer* m_playLayer = nullptr;
    float m_timeLeft = dur;
    bool m_restored = false;
    double m_originalInterval = 0.0;
    bool m_hasOriginalInterval = false;

    static FpsCtrl* create(PlayLayer* pl) {
        auto c = new FpsCtrl();
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
        m_playLayer = pl ? pl : curPL();
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

        if (auto current = curPL()) {
            m_playLayer = current;
        }

        auto director = cocos2d::CCDirector::sharedDirector();
        if (!director) {
            restoreAndDelete();
            return;
        }

        if (!sameish(director->getAnimationInterval(), 1.0 / 20.0)) {
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

static void run20Fps(PlayLayer* pl) {
    auto scene = cocos2d::CCDirector::sharedDirector()->getRunningScene();
    if (!scene) return;

    if (auto existing = scene->getChildByTag(fpsTag)) {
        if (auto ctrl = typeinfo_cast<FpsCtrl*>(existing)) {
            ctrl->start(pl, dur);
            return;
        }
        existing->removeFromParentAndCleanup(true);
    }

    auto ctrl = FpsCtrl::create(pl);
    if (!ctrl) return;
    ctrl->setTag(fpsTag);
    scene->addChild(ctrl, 999999);
    ctrl->start(pl, dur);
}

void registerFPS20Event(EventRegistry& reg) {
    reg.add(EventDef(
        "fps-20",
        "20 FPS",
        dur,
        [](PlayLayer* pl) {
            run20Fps(pl);
        }
    ));
}

} // namespace chaosmod
