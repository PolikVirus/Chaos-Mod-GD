#include "Event.hpp"

#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>

#include <limits>

using namespace geode::prelude;

namespace chaosmod {

static constexpr float kDurationSeconds = 20.f;

static constexpr int kInvertControllerTag = 0x494E5654; // 'INVT'
static constexpr int kInvertOverlayTag    = 0x494E564F; // 'INVO'

// Helper: is `node` a descendant of `root`?
static bool isDescendantOf(cocos2d::CCNode* root, cocos2d::CCNode* node) {
    if (!root || !node) return false;
    for (auto cur = node; cur; cur = cur->getParent()) {
        if (cur == root) return true;
    }
    return false;
}

class InvertColorsController : public cocos2d::CCNode {
public:
    PlayLayer* m_pl = nullptr;

    cocos2d::CCLayerColor* m_overlay = nullptr; // retained
    float m_remaining = 0.f;
    bool m_restored = false;

    static InvertColorsController* create(PlayLayer* pl) {
        auto ret = new InvertColorsController();
        ret->m_pl = pl;
        ret->autorelease();
        return ret;
    }

    cocos2d::CCScene* getScene() {
        return cocos2d::CCDirector::sharedDirector()->getRunningScene();
    }

    void ensureOverlay() {
        if (m_restored) return;

        auto scene = getScene();
        if (!scene) return;

        // Recover existing overlay if present (e.g. refreshed event)
        if (!m_overlay) {
            if (auto existing = scene->getChildByTag(kInvertOverlayTag)) {
                if (auto layer = typeinfo_cast<cocos2d::CCLayerColor*>(existing)) {
                    m_overlay = layer;
                    m_overlay->retain();
                }
            }
        }

        // Create overlay if needed
        if (!m_overlay) {
            m_overlay = cocos2d::CCLayerColor::create(cocos2d::ccc4(255, 255, 255, 255));
            if (!m_overlay) return;

            m_overlay->retain();
            m_overlay->setTag(kInvertOverlayTag);
            m_overlay->setAnchorPoint({0.f, 0.f});
            m_overlay->setPosition({0.f, 0.f});

            // True invert: output = 1 - dstColor (white src)
            m_overlay->setBlendFunc(ccBlendFunc{GL_ONE_MINUS_DST_COLOR, GL_ZERO});

            scene->addChild(m_overlay, std::numeric_limits<int>::max());
        }

        // Fullscreen size
        auto ws = cocos2d::CCDirector::sharedDirector()->getWinSize();
        m_overlay->setContentSize(ws);

        // Re-attach if something moved/removed it during pause/unpause
        if (m_overlay->getParent() != scene) {
            if (m_overlay->getParent()) {
                m_overlay->removeFromParentAndCleanup(false);
            }
            scene->addChild(m_overlay, std::numeric_limits<int>::max());
        }

        // Make sure it draws AFTER everything else (pause layer often adds itself late).
        // If it isn't the last child, remove+readd to become last.
        m_overlay->setZOrder(std::numeric_limits<int>::max());
        auto children = scene->getChildren();
        if (children && children->count() > 0) {
            auto last = children->lastObject();
            if (last != m_overlay) {
                m_overlay->removeFromParentAndCleanup(false);
                scene->addChild(m_overlay, std::numeric_limits<int>::max());
            }
        }
    }

    void start(float durationSeconds) {
        if (m_restored) return;

        m_remaining = durationSeconds;

        ensureOverlay();
        this->scheduleUpdate();
    }

    void update(float dt) override {
        if (m_restored) {
            this->unscheduleUpdate();
            return;
        }

        auto scene = getScene();
        if (!scene) {
            restoreAndRemove();
            return;
        }

        // If we actually left the level, clean up so it doesn't leak into menus.
        // (During pause, PlayLayer is still in the scene, so we keep running.)
        if (m_pl && !isDescendantOf(scene, m_pl)) {
            restoreAndRemove();
            return;
        }

        // Keep overlay enforced through pause/unpause.
        ensureOverlay();

        // Tick down while we're alive. If dt is 0 while paused, it simply won't tick (fine).
        m_remaining -= dt;
        if (m_remaining <= 0.f) {
            restoreAndRemove();
        }
    }

    void restore() {
        if (m_restored) return;
        m_restored = true;

        this->unscheduleUpdate();

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

    // Controller lives on the scene so it keeps running even if PlayLayer is paused.
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
