#include "Event.hpp"

#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/binding/PlayerObject.hpp>

using namespace geode::prelude;

namespace chaosmod {

static constexpr float kDurationSeconds = 20.f;

// Factors:
// - Low gravity:  0.5x
// - High gravity: 2.0x
static constexpr float kLowFactor  = 0.5f;
static constexpr float kHighFactor = 2.0f;

static constexpr int kGravityControllerTag = 0x47524156; // 'GRAV'

class GravityEventController : public cocos2d::CCNode {
public:
    PlayLayer* m_pl = nullptr;

    float m_baseP1 = 1.f;
    float m_baseP2 = 1.f;
    bool  m_hasP2  = false;

    bool  m_restored = false;

    static GravityEventController* create(PlayLayer* pl) {
        if (!pl) return nullptr;

        auto ret = new GravityEventController();
        ret->m_pl = pl;

        if (pl->m_player1) ret->m_baseP1 = pl->m_player1->m_gravityMod;
        if (pl->m_player2) {
            ret->m_baseP2 = pl->m_player2->m_gravityMod;
            ret->m_hasP2 = true;
        }

        ret->autorelease();
        return ret;
    }

    void applyFactor(float factor) {
        if (!m_pl) return;

        if (m_pl->m_player1) {
            m_pl->m_player1->m_gravityMod = m_baseP1 * factor;
        }
        if (m_hasP2 && m_pl->m_player2) {
            m_pl->m_player2->m_gravityMod = m_baseP2 * factor;
        }
    }

    void startTimer(float durationSeconds) {
        this->stopAllActions();
        this->runAction(cocos2d::CCSequence::create(
            cocos2d::CCDelayTime::create(durationSeconds),
            cocos2d::CCCallFunc::create(this, callfunc_selector(GravityEventController::restoreAndRemove)),
            nullptr
        ));
    }

    void restore() {
        if (m_restored) return;
        m_restored = true;

        if (!m_pl) return;

        if (m_pl->m_player1) {
            m_pl->m_player1->m_gravityMod = m_baseP1;
        }
        if (m_hasP2 && m_pl->m_player2) {
            m_pl->m_player2->m_gravityMod = m_baseP2;
        }
    }

    void restoreAndRemove() {
        this->restore();
        this->removeFromParentAndCleanup(true);
    }

    void onExit() override {
        // Fail-safe: restore if the PlayLayer/scene exits early.
        this->restore();
        cocos2d::CCNode::onExit();
    }
};

static void applyGravityEvent(PlayLayer* pl, float factor, float durationSeconds) {
    if (!pl) return;

    // If already active, reuse controller: switch factor + refresh timer.
    if (auto existing = pl->getChildByTag(kGravityControllerTag)) {
        if (auto ctrl = dynamic_cast<GravityEventController*>(existing)) {
            ctrl->applyFactor(factor);
            ctrl->startTimer(durationSeconds);
            return;
        }
        // Tag collision (unlikely) - remove and recreate.
        existing->removeFromParentAndCleanup(true);
    }

    auto ctrl = GravityEventController::create(pl);
    if (!ctrl) return;

    ctrl->setTag(kGravityControllerTag);
    pl->addChild(ctrl, 999999);

    ctrl->applyFactor(factor);
    ctrl->startTimer(durationSeconds);
}

// ---- registrations ----

void registerLowGravity(EventRegistry& reg) {
    reg.add(EventDef(
        "gravity-low",
        "Low Gravity",
        kDurationSeconds,
        [](PlayLayer* pl) {
            applyGravityEvent(pl, kLowFactor, kDurationSeconds);
        }
    ));
}

void registerHighGravity(EventRegistry& reg) {
    reg.add(EventDef(
        "gravity-high",
        "High Gravity",
        kDurationSeconds,
        [](PlayLayer* pl) {
            applyGravityEvent(pl, kHighFactor, kDurationSeconds);
        }
    ));
}

} // namespace chaosmod
