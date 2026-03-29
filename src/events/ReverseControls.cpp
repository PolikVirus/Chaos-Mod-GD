#include "Event.hpp"

#include <Geode/Geode.hpp>
#include <Geode/Enums.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/binding/PlayerObject.hpp>

using namespace geode::prelude;

namespace chaosmod {

static constexpr float kEventDuration = 20.f;
static constexpr int kControllerTag = 0x52564354; // 'RVCT'

struct RevState {
    bool active = false;
    PlayLayer* owner = nullptr;
    bool jump1 = false, left1 = false, right1 = false;
    bool jump2 = false, left2 = false, right2 = false;
};
static RevState gRev;

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

static bool isJumpHeld(PlayerObject* p) {
    if (!p) return false;
    auto key = static_cast<int>(PlayerButton::Jump);
    auto it = p->m_holdingButtons.find(key);
    if (it == p->m_holdingButtons.end()) return false;
    return it->second;
}

static bool normalizePlayer1(PlayLayer* pl, bool isPlayer1) {
    if (!pl) return isPlayer1;
    if (!pl->m_player2) return true;
    return isPlayer1;
}

static void syncMovement(PlayLayer* pl) {
    if (!pl || !gRev.active || gRev.owner != pl) return;

    auto p1 = pl->m_player1;
    auto p2 = pl->m_player2;

    bool wantLeft1 = gRev.right1;
    bool wantRight1 = gRev.left1;
    bool need1 = p1 && ((p1->m_holdingLeft != wantLeft1) || (p1->m_holdingRight != wantRight1));

    bool wantLeft2 = gRev.right2;
    bool wantRight2 = gRev.left2;
    bool need2 = p2 && ((p2->m_holdingLeft != wantLeft2) || (p2->m_holdingRight != wantRight2));

    if (!need1 && !need2) return;

    bool wasActive = gRev.active;
    gRev.active = false;

    if (p1) {
        pl->handleButton(wantLeft1, static_cast<int>(PlayerButton::Left), true);
        pl->handleButton(wantRight1, static_cast<int>(PlayerButton::Right), true);
    }
    if (p2) {
        pl->handleButton(wantLeft2, static_cast<int>(PlayerButton::Left), false);
        pl->handleButton(wantRight2, static_cast<int>(PlayerButton::Right), false);
    }

    gRev.active = wasActive;
}

static void clearReverseState() {
    gRev.active = false;
    gRev.owner = nullptr;
    gRev.jump1 = gRev.left1 = gRev.right1 = false;
    gRev.jump2 = gRev.left2 = gRev.right2 = false;
}

static void setActive(PlayLayer* pl, bool enable) {
    if (!pl) return;

    auto p1 = pl->m_player1;
    auto p2 = pl->m_player2;

    if (enable) {
        gRev.active = false;
        gRev.owner = nullptr;

        gRev.jump1 = isJumpHeld(p1);
        gRev.left1 = p1 ? p1->m_holdingLeft : false;
        gRev.right1 = p1 ? p1->m_holdingRight : false;
        gRev.jump2 = isJumpHeld(p2);
        gRev.left2 = p2 ? p2->m_holdingLeft : false;
        gRev.right2 = p2 ? p2->m_holdingRight : false;

        pl->handleButton(!gRev.jump1, static_cast<int>(PlayerButton::Jump), true);
        pl->handleButton(gRev.right1, static_cast<int>(PlayerButton::Left), true);
        pl->handleButton(gRev.left1, static_cast<int>(PlayerButton::Right), true);
        if (p2) {
            pl->handleButton(!gRev.jump2, static_cast<int>(PlayerButton::Jump), false);
            pl->handleButton(gRev.right2, static_cast<int>(PlayerButton::Left), false);
            pl->handleButton(gRev.left2, static_cast<int>(PlayerButton::Right), false);
        }

        gRev.owner = pl;
        gRev.active = true;
        return;
    }

    bool hadOwner = gRev.owner == pl;
    bool jump1 = gRev.jump1;
    bool left1 = gRev.left1;
    bool right1 = gRev.right1;
    bool jump2 = gRev.jump2;
    bool left2 = gRev.left2;
    bool right2 = gRev.right2;

    clearReverseState();

    if (!hadOwner) return;

    pl->handleButton(jump1, static_cast<int>(PlayerButton::Jump), true);
    pl->handleButton(left1, static_cast<int>(PlayerButton::Left), true);
    pl->handleButton(right1, static_cast<int>(PlayerButton::Right), true);
    if (p2) {
        pl->handleButton(jump2, static_cast<int>(PlayerButton::Jump), false);
        pl->handleButton(left2, static_cast<int>(PlayerButton::Left), false);
        pl->handleButton(right2, static_cast<int>(PlayerButton::Right), false);
    }
}

class $modify(ReverseControlsInputHook, GJBaseGameLayer) {
    void handleButton(bool down, int button, bool isPlayer1) {
        auto pl = typeinfo_cast<PlayLayer*>(this);
        if (!pl || !gRev.active || gRev.owner != pl) {
            return GJBaseGameLayer::handleButton(down, button, isPlayer1);
        }

        bool effP1 = normalizePlayer1(pl, isPlayer1);
        if (button == static_cast<int>(PlayerButton::Jump)) {
            if (effP1) gRev.jump1 = down;
            else gRev.jump2 = down;
        } else if (button == static_cast<int>(PlayerButton::Left)) {
            if (effP1) gRev.left1 = down;
            else gRev.left2 = down;
        } else if (button == static_cast<int>(PlayerButton::Right)) {
            if (effP1) gRev.right1 = down;
            else gRev.right2 = down;
        }

        int outButton = button;
        bool outDown = down;
        if (button == static_cast<int>(PlayerButton::Left)) {
            outButton = static_cast<int>(PlayerButton::Right);
        } else if (button == static_cast<int>(PlayerButton::Right)) {
            outButton = static_cast<int>(PlayerButton::Left);
        }
        if (button == static_cast<int>(PlayerButton::Jump)) {
            outDown = !down;
        }

        return GJBaseGameLayer::handleButton(outDown, outButton, effP1);
    }
};

static bool isPausedLike(PlayLayer* pl) {
    return pl && !pl->isGameplayActive();
}

class ReverseControlsController : public cocos2d::CCNode {
public:
    PlayLayer* m_playLayer = nullptr;
    bool m_stopped = false;
    float m_timeRemaining = 0.f;

    static ReverseControlsController* create(PlayLayer* pl) {
        auto ctrl = new ReverseControlsController();
        if (!ctrl) return nullptr;
        ctrl->m_playLayer = pl;
        ctrl->autorelease();
        return ctrl;
    }

    void bindToPlayLayer(PlayLayer* pl) {
        if (!pl) return;
        if (pl != m_playLayer || gRev.owner != pl || !gRev.active) {
            m_playLayer = pl;
            setActive(m_playLayer, true);
        }
    }

    void start(PlayLayer* pl, float duration) {
        m_stopped = false;
        m_timeRemaining = duration;
        bindToPlayLayer(pl ? pl : findCurrentPlayLayer());
        syncMovement(m_playLayer);
        scheduleUpdate();
    }

    void update(float dt) override {
        auto current = findCurrentPlayLayer();
        if (current) {
            bindToPlayLayer(current);
        }

        if (!m_playLayer) {
            stopAndRemove();
            return;
        }

        syncMovement(m_playLayer);

        if (isPausedLike(m_playLayer)) {
            return;
        }

        m_timeRemaining -= dt;
        if (m_timeRemaining <= 0.f) {
            stopAndRemove();
        }
    }

    void stopAndRemove() {
        if (m_stopped) return;
        m_stopped = true;

        auto current = findCurrentPlayLayer();
        if (current && m_playLayer == current && gRev.owner == m_playLayer) {
            setActive(m_playLayer, false);
        } else {
            clearReverseState();
        }

        unscheduleUpdate();
        removeFromParentAndCleanup(true);
    }

    void onExit() override {
        if (!m_stopped) {
            clearReverseState();
        }
        unscheduleUpdate();
        cocos2d::CCNode::onExit();
    }
};

void registerReverseControls(EventRegistry& reg) {
    reg.add(EventDef(
        "reverse-controls",
        "Reverse Controls",
        kEventDuration,
        [](PlayLayer* pl) {
            auto scene = cocos2d::CCDirector::sharedDirector()->getRunningScene();
            if (!scene) return;

            if (auto existing = scene->getChildByTag(kControllerTag)) {
                if (auto ctrl = typeinfo_cast<ReverseControlsController*>(existing)) {
                    ctrl->start(pl, kEventDuration);
                    return;
                }
                existing->removeFromParentAndCleanup(true);
            }

            auto ctrl = ReverseControlsController::create(pl);
            if (!ctrl) return;
            ctrl->setTag(kControllerTag);
            scene->addChild(ctrl, 999999);
            ctrl->start(pl, kEventDuration);
        }
    ));
}

} // namespace chaosmod