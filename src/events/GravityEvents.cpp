#include "Event.hpp"

#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/binding/PlayerObject.hpp>

#include <cmath>

using namespace geode::prelude;

namespace chaosmod {

static constexpr float kDurationSeconds = 20.f;

// Factors:
// - Low gravity:  0.5x
// - High gravity: 2.0x
static constexpr float kLowFactor  = 0.5f;
static constexpr float kHighFactor = 2.0f;

static constexpr int kGravityControllerTag = 0x47524156; // 'GRAV'

static bool nearlyEqual(float a, float b) {
    // Relative-ish tolerance to avoid oscillations from float noise.
    float diff = std::fabs(a - b);
    float scale = std::fmax(1.f, std::fmax(std::fabs(a), std::fabs(b)));
    return diff <= 1e-4f * scale;
}

class GravityEventController : public cocos2d::CCNode {
public:
    PlayLayer* m_pl = nullptr;

    // "Baseline" gravity that the game would have WITHOUT our effect.
    // This is updated if the game changes gravity while the effect is running
    // (portals, respawn/reset, etc.).
    float m_baseP1 = 1.f;
    float m_baseP2 = 1.f;
    bool  m_hasP2  = false;

    float m_factor = 1.f;

    PlayerObject* m_lastP1 = nullptr;
    PlayerObject* m_lastP2 = nullptr;

    bool  m_restored = false;
    bool  m_started  = false;

    static GravityEventController* create(PlayLayer* pl) {
        if (!pl) return nullptr;

        auto ret = new GravityEventController();
        ret->m_pl = pl;
        ret->autorelease();
        return ret;
    }

    void start(float factor, float durationSeconds) {
        if (!m_pl) return;

        // IMPORTANT: don't recapture baseline when refreshing an already-running event,
        // otherwise we would treat our already-multiplied gravity as the new baseline
        // and the effect would stack (e.g. 0.5x -> 0.25x on refresh).
        if (!m_started) {
            // Capture current baseline from the *current* player objects.
            // If the player object is recreated on respawn, update() will re-capture.
            if (m_pl->m_player1) {
                m_baseP1 = m_pl->m_player1->m_gravityMod;
                m_lastP1 = m_pl->m_player1;
            }
            if (m_pl->m_player2) {
                m_baseP2 = m_pl->m_player2->m_gravityMod;
                m_lastP2 = m_pl->m_player2;
                m_hasP2 = true;
            } else {
                m_hasP2 = false;
                m_lastP2 = nullptr;
            }
            m_started = true;
        } else {
            // Refresh player pointers / p2 presence for cases like toggling dual mode.
            if (m_pl->m_player1 && m_pl->m_player1 != m_lastP1) {
                m_lastP1 = m_pl->m_player1;
                m_baseP1 = m_pl->m_player1->m_gravityMod;
            }
            if (m_pl->m_player2) {
                m_hasP2 = true;
                if (m_pl->m_player2 != m_lastP2) {
                    m_lastP2 = m_pl->m_player2;
                    m_baseP2 = m_pl->m_player2->m_gravityMod;
                }
            } else {
                m_hasP2 = false;
                m_lastP2 = nullptr;
            }
        }

        m_factor = factor;

        // Enforce immediately (and keep enforcing via update()).
        applyNow();
        this->scheduleUpdate();

        startTimer(durationSeconds);
    }

    void applyNow() {
        if (!m_pl || m_restored) return;

        if (m_pl->m_player1) {
            m_pl->m_player1->m_gravityMod = m_baseP1 * m_factor;
        }
        if (m_hasP2 && m_pl->m_player2) {
            m_pl->m_player2->m_gravityMod = m_baseP2 * m_factor;
        }
    }

    void enforceOne(PlayerObject*& lastPtr, float& base, PlayerObject* p) {
        if (!p) return;

        // If GD recreates the player on a new attempt, re-capture baseline.
        if (p != lastPtr) {
            lastPtr = p;
            base = p->m_gravityMod;
        }

        float desired = base * m_factor;
        float current = p->m_gravityMod;

        if (nearlyEqual(current, desired)) return;

        // If the game changed gravity while we were active (portal, respawn reset, etc.),
        // treat the current value as the new baseline and re-apply our factor.
        // But if we're just slightly off from float noise, snap back to desired.
        if (std::fabs(current - desired) < std::fabs(current - base)) {
            p->m_gravityMod = desired;
        } else {
            base = current;
            p->m_gravityMod = base * m_factor;
        }
    }

    void update(float) override {
        if (!m_pl || m_restored) {
            this->unscheduleUpdate();
            return;
        }

        // Keep enforcing every frame so the effect survives death/new attempt.
        enforceOne(m_lastP1, m_baseP1, m_pl->m_player1);
        if (m_hasP2) {
            enforceOne(m_lastP2, m_baseP2, m_pl->m_player2);
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

        // Restore to the most recently observed baseline.
        if (m_pl->m_player1) {
            m_pl->m_player1->m_gravityMod = m_baseP1;
        }
        if (m_hasP2 && m_pl->m_player2) {
            m_pl->m_player2->m_gravityMod = m_baseP2;
        }

        this->unscheduleUpdate();
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
            ctrl->start(factor, durationSeconds);
            return;
        }
        // Tag collision (unlikely) - remove and recreate.
        existing->removeFromParentAndCleanup(true);
    }

    auto ctrl = GravityEventController::create(pl);
    if (!ctrl) return;

    ctrl->setTag(kGravityControllerTag);
    pl->addChild(ctrl, 999999);

    ctrl->start(factor, durationSeconds);
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
