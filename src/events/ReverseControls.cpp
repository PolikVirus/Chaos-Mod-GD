#include "Event.hpp"

#include <Geode/Geode.hpp>
#include <Geode/Enums.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/binding/PlayerObject.hpp>

using namespace geode::prelude;

namespace chaosmod {

static constexpr float kDurationSeconds = 20.f;
static constexpr int kControllerTag = 0x52564354; // 'RVCT'

// Shared runtime state used by the input hook
struct ReverseControlsState {
    bool active = false;
    PlayLayer* owner = nullptr;

    // Last-known *physical* inputs while active
    bool jump1 = false, left1 = false, right1 = false;
    bool jump2 = false, left2 = false, right2 = false;
};

static ReverseControlsState gRev;

static bool getJumpHeld(PlayerObject* p) {
    if (!p) return false;

    auto const key = static_cast<int>(PlayerButton::Jump);
    auto it = p->m_holdingButtons.find(key);
    if (it == p->m_holdingButtons.end()) return false;
    return it->second;
}

// If player2 doesn't exist, treat ANY input as player1.
// This fixes the "A/D doesn't work but arrows do" symptom in platformer/single-player.
static bool normalizeIsPlayer1(PlayLayer* pl, bool isPlayer1) {
    if (!pl) return isPlayer1;
    if (!pl->m_player2) return true;
    return isPlayer1;
}

// IMPORTANT: We only resync LEFT/RIGHT after death/reset.
// We do NOT force-sync JUMP continuously, because reverse-jump is edge-sensitive
// (jump-on-release) and forcing it can swallow the edge event.
static void syncMovementToPhysical(PlayLayer* pl) {
    if (!pl || !gRev.active || gRev.owner != pl) return;

    auto p1 = pl->m_player1;
    auto p2 = pl->m_player2;

    bool wantLeft1  = gRev.right1;
    bool wantRight1 = gRev.left1;

    bool need1 = false;
    if (p1) {
        need1 = (p1->m_holdingLeft != wantLeft1) || (p1->m_holdingRight != wantRight1);
    }

    bool wantLeft2  = gRev.right2;
    bool wantRight2 = gRev.left2;

    bool need2 = false;
    if (p2) {
        need2 = (p2->m_holdingLeft != wantLeft2) || (p2->m_holdingRight != wantRight2);
    }

    if (!need1 && !need2) return;

    // Temporarily disable mapping while we push desired GAME-held movement state.
    bool wasActive = gRev.active;
    gRev.active = false;

    if (p1) {
        pl->handleButton(wantLeft1,  static_cast<int>(PlayerButton::Left),  true);
        pl->handleButton(wantRight1, static_cast<int>(PlayerButton::Right), true);
    }
    if (p2) {
        pl->handleButton(wantLeft2,  static_cast<int>(PlayerButton::Left),  false);
        pl->handleButton(wantRight2, static_cast<int>(PlayerButton::Right), false);
    }

    gRev.active = wasActive;
}

static void setActive(PlayLayer* pl, bool enable) {
    if (!pl) return;

    // Disable mapping while we resync button states via handleButton calls.
    gRev.active = false;

    auto p1 = pl->m_player1;
    auto p2 = pl->m_player2;

    if (enable) {
        // Capture current (pre-event) game-held states as "physical" states
        // since controls are normal before activation.
        gRev.jump1  = getJumpHeld(p1);
        gRev.left1  = p1 ? p1->m_holdingLeft  : false;
        gRev.right1 = p1 ? p1->m_holdingRight : false;

        gRev.jump2  = getJumpHeld(p2);
        gRev.left2  = p2 ? p2->m_holdingLeft  : false;
        gRev.right2 = p2 ? p2->m_holdingRight : false;

        // Immediately put the GAME into the inverted-control state:
        // - Jump becomes NOT(physical jump)  (reverse-jump style this mod uses)
        // - Left/Right are swapped
        pl->handleButton(!gRev.jump1,  static_cast<int>(PlayerButton::Jump),  true);
        pl->handleButton(gRev.right1,  static_cast<int>(PlayerButton::Left),  true);
        pl->handleButton(gRev.left1,   static_cast<int>(PlayerButton::Right), true);

        if (p2) {
            pl->handleButton(!gRev.jump2, static_cast<int>(PlayerButton::Jump),  false);
            pl->handleButton(gRev.right2, static_cast<int>(PlayerButton::Left),  false);
            pl->handleButton(gRev.left2,  static_cast<int>(PlayerButton::Right), false);
        }

        gRev.owner = pl;
        gRev.active = true;
    }
    else {
        // Restore GAME controls to match last-known physical states
        pl->handleButton(gRev.jump1,  static_cast<int>(PlayerButton::Jump),  true);
        pl->handleButton(gRev.left1,  static_cast<int>(PlayerButton::Left),  true);
        pl->handleButton(gRev.right1, static_cast<int>(PlayerButton::Right), true);

        if (p2) {
            pl->handleButton(gRev.jump2,  static_cast<int>(PlayerButton::Jump),  false);
            pl->handleButton(gRev.left2,  static_cast<int>(PlayerButton::Left),  false);
            pl->handleButton(gRev.right2, static_cast<int>(PlayerButton::Right), false);
        }

        gRev.owner = nullptr;
        gRev.active = false;
    }
}

// Input hook: swaps L/R and inverts jump while event is active on the owning PlayLayer
class $modify(ReverseControlsInputHook, GJBaseGameLayer) {
    void handleButton(bool down, int button, bool isPlayer1) {
        auto pl = typeinfo_cast<PlayLayer*>(this);

        if (!pl || !gRev.active || gRev.owner != pl) {
            return GJBaseGameLayer::handleButton(down, button, isPlayer1);
        }

        // Fix A/D vs arrows by normalizing to player1 when not in dual
        bool effP1 = normalizeIsPlayer1(pl, isPlayer1);

        // Track *physical* state (before mapping)
        if (button == static_cast<int>(PlayerButton::Jump)) {
            if (effP1) gRev.jump1 = down;
            else       gRev.jump2 = down;
        }
        else if (button == static_cast<int>(PlayerButton::Left)) {
            if (effP1) gRev.left1 = down;
            else       gRev.left2 = down;
        }
        else if (button == static_cast<int>(PlayerButton::Right)) {
            if (effP1) gRev.right1 = down;
            else       gRev.right2 = down;
        }

        // Map to reversed controls
        int outButton = button;
        bool outDown = down;

        if (button == static_cast<int>(PlayerButton::Left)) {
            outButton = static_cast<int>(PlayerButton::Right);
        }
        else if (button == static_cast<int>(PlayerButton::Right)) {
            outButton = static_cast<int>(PlayerButton::Left);
        }

        // Keep the mod's original "reverse jump" behavior (press/release inverted)
        if (button == static_cast<int>(PlayerButton::Jump)) {
            outDown = !down;
        }

        return GJBaseGameLayer::handleButton(outDown, outButton, effP1);
    }
};

class ReverseControlsController : public cocos2d::CCNode {
public:
    PlayLayer* m_pl = nullptr;
    bool m_stopped = false;
    bool m_started = false;

    static ReverseControlsController* create(PlayLayer* pl) {
        auto ret = new ReverseControlsController();
        ret->m_pl = pl;
        ret->autorelease();
        return ret;
    }

    void start(float durationSeconds) {
        if (!m_pl) return;

        if (!m_started) {
            // Activate now (includes immediate state sync)
            setActive(m_pl, true);
            m_started = true;
        } else {
            // If event retriggers while active, just re-sync movement (safe) and refresh timer
            syncMovementToPhysical(m_pl);
        }

        // Keep resyncing movement so it persists across death/new attempt,
        // without touching jump (which can break reverse-jump edges).
        this->scheduleUpdate();

        this->stopAllActions();
        this->runAction(cocos2d::CCSequence::create(
            cocos2d::CCDelayTime::create(durationSeconds),
            cocos2d::CCCallFunc::create(this, callfunc_selector(ReverseControlsController::stopAndRemove)),
            nullptr
        ));
    }

    void update(float) override {
        if (!m_pl || !gRev.active || gRev.owner != m_pl) {
            stopAndRemove();
            return;
        }
        syncMovementToPhysical(m_pl);
    }

    void stopAndRemove() {
        if (m_stopped) return;
        m_stopped = true;

        if (m_pl) {
            setActive(m_pl, false);
        }

        this->unscheduleUpdate();
        this->removeFromParentAndCleanup(true);
    }

    void onExit() override {
        // Fail-safe: if the PlayLayer exits early, restore controls.
        if (!m_stopped && m_pl) {
            setActive(m_pl, false);
        }
        this->unscheduleUpdate();
        cocos2d::CCNode::onExit();
    }
};

void registerReverseControls(EventRegistry& reg) {
    reg.add(EventDef(
        "reverse-controls",
        "Reverse Controls",
        kDurationSeconds,
        [](PlayLayer* pl) {
            if (!pl) return;

            // If already active, just refresh the timer.
            if (auto existing = pl->getChildByTag(kControllerTag)) {
                if (auto ctrl = typeinfo_cast<ReverseControlsController*>(existing)) {
                    ctrl->start(kDurationSeconds);
                    return;
                }
                existing->removeFromParentAndCleanup(true);
            }

            auto ctrl = ReverseControlsController::create(pl);
            if (!ctrl) return;

            ctrl->setTag(kControllerTag);
            pl->addChild(ctrl, 999999);
            ctrl->start(kDurationSeconds);
        }
    ));
}

} // namespace chaosmod
