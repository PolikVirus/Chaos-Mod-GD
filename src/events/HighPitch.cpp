#include "Event.hpp"

#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>

using namespace geode::prelude;

namespace chaosmod {

static constexpr float dur = 30.f;
static constexpr int pitchTag = 0x48494750;
static constexpr float pitchUp = 1.5f;
static constexpr float basePitch = 1.0f;

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

class PitchUpCtrl : public cocos2d::CCNode {
public:
    PlayLayer* m_playLayer = nullptr;
    float m_timeRemaining = 0.f;
    bool m_isRestored = false;

    static PitchUpCtrl* create(PlayLayer* pl) {
        auto ctrl = new PitchUpCtrl();
        if (!ctrl) return nullptr;
        ctrl->m_playLayer = pl;
        ctrl->autorelease();
        return ctrl;
    }

    void applyPitch(float pitch) {
        if (auto engine = FMODAudioEngine::sharedEngine()) {
            engine->setChannelPitch(0, AudioTargetType::MusicChannel, pitch);
        }
    }

    void start(PlayLayer* pl, float duration) {
        m_playLayer = pl ? pl : curPL();
        m_timeRemaining = duration;
        m_isRestored = false;
        applyPitch(pitchUp);
        scheduleUpdate();
    }

    void update(float dt) override {
        if (m_isRestored) {
            unscheduleUpdate();
            return;
        }

        if (auto current = curPL()) {
            m_playLayer = current;
        }

        applyPitch(pitchUp);

        if (!m_playLayer || !m_playLayer->isGameplayActive()) {
            return;
        }

        m_timeRemaining -= dt;
        if (m_timeRemaining <= 0.f) {
            restoreAndDelete();
        }
    }

    void restore() {
        if (m_isRestored) return;
        m_isRestored = true;
        unscheduleUpdate();
        applyPitch(basePitch);
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

static void doHighPitch(PlayLayer* pl, float duration) {
    auto sc = cocos2d::CCDirector::sharedDirector()->getRunningScene();
    if (!sc) return;

    if (auto existing = sc->getChildByTag(pitchTag)) {
        if (auto ctrl = typeinfo_cast<PitchUpCtrl*>(existing)) {
            ctrl->start(pl, duration);
            return;
        }
        existing->removeFromParentAndCleanup(true);
    }

    auto ctrl = PitchUpCtrl::create(pl);
    if (!ctrl) return;
    ctrl->setTag(pitchTag);
    sc->addChild(ctrl, 999999);
    ctrl->start(pl, duration);
}

void registerHighPitch(EventRegistry& reg) {
    reg.add(EventDef(
        "high-pitch",
        "High Pitch",
        dur,
        [](PlayLayer* pl) {
            doHighPitch(pl, dur);
        }
    ));
}

} // namespace chaosmod