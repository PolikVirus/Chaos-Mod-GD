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
    GJBaseGameLayer* owner = nullptr;

    // Last-known *physical* inputs while active
    bool jump1 = false, left1 = false, right1 = false;
    bool jump2 = false, left2 = false, right2 = false;
};

static ReverseControlsState gRev;

static bool getJumpHeld(PlayerObject* p) {
    if (!p) return false;

    // m_holdingButtons is a map<int,bool>. Jump is PlayerButton::Jump == 1.
    auto const key = static_cast<int>(PlayerButton::Jump);

    auto it = p->m_holdingButtons.find(key);
    if (it == p->m_holdingButtons.end()) return false;
    return it->second;
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
        // - Jump becomes NOT(physical jump)
        // - Left/Right are swapped
        //
        // gameJump = !physJump
        // gameLeft = physRight
        // gameRight = physLeft

        pl->handleButton(!gRev.jump1,  static_cast<int>(PlayerButton::Jump),  true);
        pl->handleButton(gRev.right1,  static_cast<int>(PlayerButton::Left),  true);
        pl->handleButton(gRev.left1,   static_cast<int>(PlayerButton::Right), true);

        if (p2) {
            pl->handleButton(!gRev.jump2, static_cast<int>(PlayerButton::Jump),  false);
            pl->handleButton(gRev.right2, static_cast<int>(PlayerButton::Left),  false);
            pl->handleButton(gRev.left2,  static_cast<int>(PlayerButton::Right), false);
        }

        gRev.owner = static_cast<GJBaseGameLayer*>(pl);
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
        if (!gRev.active || gRev.owner != this) {
            return GJBaseGameLayer::handleButton(down, button, isPlayer1);
        }

        // Track *physical* state (before mapping)
        auto setStates = [&](bool isP1) {
            if (button == static_cast<int>(PlayerButton::Jump)) {
                if (isP1) gRev.jump1 = down;
                else      gRev.jump2 = down;
            }
            else if (button == static_cast<int>(PlayerButton::Left)) {
                if (isP1) gRev.left1 = down;
                else      gRev.left2 = down;
            }
            else if (button == static_cast<int>(PlayerButton::Right)) {
                if (isP1) gRev.right1 = down;
                else      gRev.right2 = down;
            }
        };
        setStates(isPlayer1);

        // Map to reversed controls
        int outButton = button;
        bool outDown = down;

        if (button == static_cast<int>(PlayerButton::Left)) {
            outButton = static_cast<int>(PlayerButton::Right);
        }
        else if (button == static_cast<int>(PlayerButton::Right)) {
            outButton = static_cast<int>(PlayerButton::Left);
        }

        if (button == static_cast<int>(PlayerButton::Jump)) {
            outDown = !down; // invert jump press/release
        }

        return GJBaseGameLayer::handleButton(outDown, outButton, isPlayer1);
    }
};

class ReverseControlsController : public cocos2d::CCNode {
public:
    PlayLayer* m_pl = nullptr;
    bool m_stopped = false;

    static ReverseControlsController* create(PlayLayer* pl) {
        auto ret = new ReverseControlsController();
        ret->m_pl = pl;
        ret->autorelease();
        return ret;
    }

    void start(float durationSeconds) {
        if (!m_pl) return;

        // Activate now (includes immediate state sync)
        setActive(m_pl, true);

        this->stopAllActions();
        this->runAction(cocos2d::CCSequence::create(
            cocos2d::CCDelayTime::create(durationSeconds),
            cocos2d::CCCallFunc::create(this, callfunc_selector(ReverseControlsController::stopAndRemove)),
            nullptr
        ));
    }

    void stopAndRemove() {
        if (m_stopped) return;
        m_stopped = true;

        if (m_pl) {
            setActive(m_pl, false);
        }

        this->removeFromParentAndCleanup(true);
    }

    void onExit() override {
        // Fail-safe: if the PlayLayer exits early, restore controls.
        if (!m_stopped && m_pl) {
            setActive(m_pl, false);
        }
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
                if (auto ctrl = dynamic_cast<ReverseControlsController*>(existing)) {
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
