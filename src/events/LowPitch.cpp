#include "Event.hpp"

#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>

using namespace geode::prelude;

namespace chaosmod {

static constexpr float kEventDuration = 30.f;
static constexpr int kLowPitchControllerTag = 0x4c4f5750; // 'LOWP'

class LowPitchController : public cocos2d::CCNode {
public:
    PlayLayer* m_playLayer = nullptr;
    float m_timeRemaining = 0.f;
    bool m_isRestored = false;
    float m_originalPitch = 1.0f;

    static LowPitchController* create(PlayLayer* pl) {
        auto ctrl = new LowPitchController();
        ctrl->m_playLayer = pl;
        ctrl->autorelease();
        return ctrl;
    }

    void start(float duration) {
        if (m_isRestored) return;
        m_timeRemaining = duration;

        // Store original pitch and set low pitch
        auto engine = FMODAudioEngine::sharedEngine();
        if (engine) {
            // Assuming music channel 0, get current pitch if possible
            // For now, assume it starts at 1.0 and set to 0.7
            m_originalPitch = 1.0f;
            engine->setChannelPitch(0, AudioTargetType::MusicChannel, 0.7f);
        }

        scheduleUpdate();
    }

    void update(float dt) override {
        if (m_isRestored) {
            unscheduleUpdate();
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

        // Restore original pitch
        auto engine = FMODAudioEngine::sharedEngine();
        if (engine) {
            engine->setChannelPitch(0, AudioTargetType::MusicChannel, m_originalPitch);
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

static void applyLowPitch(PlayLayer* pl, float duration) {
    if (!pl) return;
    auto sc = cocos2d::CCDirector::sharedDirector()->getRunningScene();
    if (!sc) return;
    if (auto existing = sc->getChildByTag(kLowPitchControllerTag)) {
        if (auto ctrl = typeinfo_cast<LowPitchController*>(existing)) {
            ctrl->m_playLayer = pl;
            ctrl->start(duration);
            return;
        }
        existing->removeFromParentAndCleanup(true);
    }
    auto ctrl = LowPitchController::create(pl);
    if (!ctrl) return;
    ctrl->setTag(kLowPitchControllerTag);
    sc->addChild(ctrl);
    ctrl->start(duration);
}

void registerLowPitch(EventRegistry& reg) {
    reg.add(EventDef(
        "low-pitch",
        "Low Pitch",
        kEventDuration,
        [](PlayLayer* pl) {
            applyLowPitch(pl, kEventDuration);
        }
    ));
}

} // namespace chaosmod