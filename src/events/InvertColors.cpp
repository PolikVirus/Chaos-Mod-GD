#include "Event.hpp"

#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>

#include <limits>

using namespace geode::prelude;

namespace chaosmod {

static constexpr float kDurationSeconds = 20.f;

// Controller is on the running scene (so it ticks + timers work).
static constexpr int kInvertControllerTag = 0x494E5654; // 'INVT'

// Overlay is on director notification node (so it survives pause/unpause).
static constexpr int kInvertOverlayTag    = 0x494E564F; // 'INVO'

static cocos2d::CCNode* getOrMakeNotificationNode() {
    auto director = cocos2d::CCDirector::sharedDirector();
    if (!director) return nullptr;

    auto notif = director->getNotificationNode();
    if (!notif) {
        notif = cocos2d::CCNode::create();
        if (!notif) return nullptr;
        director->setNotificationNode(notif);
    }
    return notif;
}

static bool isDescendantOf(cocos2d::CCNode* root, cocos2d::CCNode* node) {
    if (!root || !node) return false;
    cocos2d::CCNode* cur = node;
    while (cur) {
        if (cur == root) return true;
        cur = cur->getParent();
    }
    return false;
}

class InvertColorsController : public cocos2d::CCNode {
public:
    PlayLayer* m_pl = nullptr;

    // We retain the overlay so even if some layer temporarily removes it, we can re-add it.
    cocos2d::CCLayerColor* m_overlay = nullptr;

    bool m_restored = false;

    static InvertColorsController* create(PlayLayer* pl) {
        auto ret = new InvertColorsController();
        ret->m_pl = pl;
        ret->autorelease();
        return ret;
    }

    void ensureOverlay() {
        if (m_restored) return;

        auto notif = getOrMakeNotificationNode();
        if (!notif) return;

        // Try recover if overlay already exists (e.g., refreshed event)
        if (!m_overlay) {
            if (auto existing = notif->getChildByTag(kInvertOverlayTag)) {
                if (auto layer = typeinfo_cast<cocos2d::CCLayerColor*>(existing)) {
                    m_overlay = layer;
                    m_overlay->retain();
                }
            }
        }

        // Create if needed
        if (!m_overlay) {
            m_overlay = cocos2d::CCLayerColor::create(cocos2d::ccc4(255, 255, 255, 255));
            if (!m_overlay) return;

            m_overlay->retain();
            m_overlay->setTag(kInvertOverlayTag);
            m_overlay->setAnchorPoint({0.f, 0.f});
            m_overlay->setPosition({0.f, 0.f});

            // True invert: output = 1 - dstColor (white src with ONE_MINUS_DST_COLOR blend)
            m_overlay->setBlendFunc(ccBlendFunc{GL_ONE_MINUS_DST_COLOR, GL_ZERO});
        }

        // Make sure it's attached to the *current* notification node and drawn last
        if (m_overlay->getParent() != notif) {
            if (m_overlay->getParent()) {
                m_overlay->removeFromParentAndCleanup(false);
            }
            notif->addChild(m_overlay, std::numeric_limits<int>::max());
        } else {
            m_overlay->setZOrder(std::numeric_limits<int>::max());
        }

        // Fullscreen
        auto ws = cocos2d::CCDirector::sharedDirector()->getWinSize();
        m_overlay->setContentSize(ws);
    }

    void start(float durationSeconds) {
        if (m_restored) return;

        ensureOverlay();
        this->scheduleUpdate();

        // Reliable timer (works because controller is on the running scene)
        this->stopAllActions();
        this->runAction(cocos2d::CCSequence::create(
            cocos2d::CCDelayTime::create(durationSeconds),
            cocos2d::CCCallFunc::create(this, callfunc_selector(InvertColorsController::restoreAndRemove)),
            nullptr
        ));
    }

    void update(float) override {
        if (m_restored) {
            this->unscheduleUpdate();
            return;
        }

        // If we left the level, clean up so it doesn't leak into menus.
        auto scene = cocos2d::CCDirector::sharedDirector()->getRunningScene();
        if (!scene || !isDescendantOf(scene, m_pl)) {
            restoreAndRemove();
            return;
        }

        // Pause/unpause can mess with nodes; keep it enforced.
        ensureOverlay();
    }

    void restore() {
        if (m_restored) return;
        m_restored = true;

        this->unscheduleUpdate();
        this->stopAllActions();

        if (m_overlay) {
            if (m_overlay->getParent()) {
                m_overlay->removeFromParentAndCleanup(true);
            }
            m_overlay->release();
            m_overlay = nullptr;
        }
    }

    void restoreAndRemove() {
        restore();
        this->removeFromParentAndCleanup(true);
    }

    void onExit() override {
        // Fail-safe.
        restore();
        cocos2d::CCNode::onExit();
    }
};

static void applyInvertColors(PlayLayer* pl, float durationSeconds) {
    if (!pl) return;

    auto scene = cocos2d::CCDirector::sharedDirector()->getRunningScene();
    if (!scene) return;

    // Controller lives on the scene so it always ticks + timer always ends.
    if (auto existing = scene->getChildByTag(kInvertControllerTag)) {
        if (auto ctrl = typeinfo_cast<InvertColorsController*>(existing)) {
            ctrl->m_pl = pl;
            ctrl->start(durationSeconds);
            return;
        }
        existing->removeFromParentAndCleanup(true);
    }

    auto ctrl = InvertColorsController::create(pl);
    if (!ctrl) return;

    ctrl->setTag(kInvertControllerTag);
    scene->addChild(ctrl, std::numeric_limits<int>::max());
    ctrl->start(durationSeconds);
}

void registerInvertColors(EventRegistry& reg) {
    reg.add(EventDef(
        "invert-colors",
        "Invert Colors",
        kDurationSeconds,
        [](PlayLayer* pl) {
            applyInvertColors(pl, kDurationSeconds);
        }
    ));
}

} // namespace chaosmod
