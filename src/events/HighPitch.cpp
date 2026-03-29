#include "Event.hpp"

#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>

using namespace geode::prelude;

namespace chaosmod {

static constexpr float kEventDuration = 30.f;
static constexpr int kHighPitchControllerTag = 0x48494750; // 'HIGP'
static constexpr float kTargetPitch = 1.5f;
static constexpr float kDefaultPitch = 1.0f;

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

class HighPitchController : public cocos2d::CCNode {
public:
    PlayLayer* m_playLayer = nullptr;
    float m_timeRemaining = 0.f;
    bool m_isRestored = false;

    static HighPitchController* create(PlayLayer* pl) {
        auto ctrl = new HighPitchController();
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
        m_playLayer = pl ? pl : findCurrentPlayLayer();
        m_timeRemaining = duration;
        m_isRestored = false;
        applyPitch(kTargetPitch);
        scheduleUpdate();
    }

    void update(float dt) override {
        if (m_isRestored) {
            unscheduleUpdate();
            return;
        }

        if (auto current = findCurrentPlayLayer()) {
            m_playLayer = current;
        }

        applyPitch(kTargetPitch);

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
        applyPitch(kDefaultPitch);
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

static void applyHighPitch(PlayLayer* pl, float duration) {
    auto sc = cocos2d::CCDirector::sharedDirector()->getRunningScene();
    if (!sc) return;

    if (auto existing = sc->getChildByTag(kHighPitchControllerTag)) {
        if (auto ctrl = typeinfo_cast<HighPitchController*>(existing)) {
            ctrl->start(pl, duration);
            return;
        }
        existing->removeFromParentAndCleanup(true);
    }

    auto ctrl = HighPitchController::create(pl);
    if (!ctrl) return;
    ctrl->setTag(kHighPitchControllerTag);
    sc->addChild(ctrl, 999999);
    ctrl->start(pl, duration);
}

void registerHighPitch(EventRegistry& reg) {
    reg.add(EventDef(
        "high-pitch",
        "High Pitch",
        kEventDuration,
        [](PlayLayer* pl) {
            applyHighPitch(pl, kEventDuration);
        }
    ));
}

} // namespace chaosmod