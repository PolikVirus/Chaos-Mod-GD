#include "Event.hpp"

#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>

using namespace geode::prelude;

namespace chaosmod {

static constexpr float kEventDuration = 30.f;
static constexpr int kHighPitchControllerTag = 0x48494750; // 'HIGP'

class HighPitchController : public cocos2d::CCNode {
public:
    PlayLayer* m_playLayer = nullptr;
    float m_timeRemaining = 0.f;
    bool m_isRestored = false;
    float m_originalPitch = 1.0f;

    static HighPitchController* create(PlayLayer* pl) {
        auto ctrl = new HighPitchController();
        ctrl->m_playLayer = pl;
        ctrl->autorelease();
        return ctrl;
    }

    void start(float duration) {
        if (m_isRestored) return;
        m_timeRemaining = duration;

        // Store original pitch and set high pitch
        auto engine = FMODAudioEngine::sharedEngine();
        if (engine) {
            // Assuming music channel 0, get current pitch if possible
            // For now, assume it starts at 1.0 and set to 1.5
            m_originalPitch = 1.0f;
            engine->setChannelPitch(0, AudioTargetType::MusicChannel, 1.5f);
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

static void applyHighPitch(PlayLayer* pl, float duration) {
    if (!pl) return;
    auto sc = cocos2d::CCDirector::sharedDirector()->getRunningScene();
    if (!sc) return;
    if (auto existing = sc->getChildByTag(kHighPitchControllerTag)) {
        if (auto ctrl = typeinfo_cast<HighPitchController*>(existing)) {
            ctrl->m_playLayer = pl;
            ctrl->start(duration);
            return;
        }
        existing->removeFromParentAndCleanup(true);
    }
    auto ctrl = HighPitchController::create(pl);
    if (!ctrl) return;
    ctrl->setTag(kHighPitchControllerTag);
    sc->addChild(ctrl);
    ctrl->start(duration);
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