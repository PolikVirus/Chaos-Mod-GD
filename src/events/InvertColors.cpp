#include "Event.hpp"

#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>

#include <limits>

using namespace geode::prelude;

namespace chaosmod {

static constexpr float kEventDuration = 20.f;
static constexpr int kInvertControllerTag = 0x494E5654; // 'INVT'
static constexpr int kOverlayTag = 0x494E564F; // 'INVO'

static bool isDescendant(cocos2d::CCNode* root, cocos2d::CCNode* node) {
    if (!root || !node) return false;
    for (auto cur = node; cur; cur = cur->getParent()) {
        if (cur == root) return true;
    }
    return false;
}

class InvertColorsController : public cocos2d::CCNode {
public:
    PlayLayer* m_playLayer = nullptr;
    cocos2d::CCLayerColor* m_overlayLayer = nullptr;
    float m_timeRemaining = 0.f;
    bool m_isRestored = false;

    static InvertColorsController* create(PlayLayer* pl) {
        auto ctrl = new InvertColorsController();
        ctrl->m_playLayer = pl;
        ctrl->autorelease();
        return ctrl;
    }

    cocos2d::CCScene* scene() {
        return cocos2d::CCDirector::sharedDirector()->getRunningScene();
    }

    void ensureOverlay() {
        if (m_isRestored) return;
        auto sc = scene();
        if (!sc) return;
        if (!m_overlayLayer) {
            if (auto existing = sc->getChildByTag(kOverlayTag)) {
                if (auto layer = typeinfo_cast<cocos2d::CCLayerColor*>(existing)) {
                    m_overlayLayer = layer;
                    m_overlayLayer->retain();
                }
            }
        }
        if (!m_overlayLayer) {
            m_overlayLayer = cocos2d::CCLayerColor::create(cocos2d::ccc4(255,255,255,255));
            if (!m_overlayLayer) return;
            m_overlayLayer->retain();
            m_overlayLayer->setTag(kOverlayTag);
            m_overlayLayer->setAnchorPoint({0.f,0.f});
            m_overlayLayer->setPosition({0.f,0.f});
            m_overlayLayer->setBlendFunc(ccBlendFunc{GL_ONE_MINUS_DST_COLOR, GL_ZERO});
            sc->addChild(m_overlayLayer, std::numeric_limits<int>::max());
        }
        auto ws = cocos2d::CCDirector::sharedDirector()->getWinSize();
        m_overlayLayer->setContentSize(ws);
        if (m_overlayLayer->getParent() != sc) {
            if (m_overlayLayer->getParent()) {
                m_overlayLayer->removeFromParentAndCleanup(false);
            }
            sc->addChild(m_overlayLayer, std::numeric_limits<int>::max());
        }
        m_overlayLayer->setZOrder(std::numeric_limits<int>::max());
        auto children = sc->getChildren();
        if (children && children->count() > 0) {
            if (children->lastObject() != m_overlayLayer) {
                m_overlayLayer->removeFromParentAndCleanup(false);
                sc->addChild(m_overlayLayer, std::numeric_limits<int>::max());
            }
        }
    }

    void start(float duration) {
        if (m_isRestored) return;
        m_timeRemaining = duration;
        ensureOverlay();
        scheduleUpdate();
    }

    void update(float dt) override {
        if (m_isRestored) {
            unscheduleUpdate();
            return;
        }
        auto sc = scene();
        if (!sc) {
            restoreAndDelete();
            return;
        }
        if (m_playLayer && !isDescendant(sc, m_playLayer)) {
            restoreAndDelete();
            return;
        }
        ensureOverlay();
        m_timeRemaining -= dt;
        if (m_timeRemaining <= 0.f) {
            restoreAndDelete();
        }
    }

    void restore() {
        if (m_isRestored) return;
        m_isRestored = true;
        unscheduleUpdate();
        if (m_overlayLayer) {
            if (m_overlayLayer->getParent()) {
                m_overlayLayer->removeFromParentAndCleanup(true);
            }
            m_overlayLayer->release();
            m_overlayLayer = nullptr;
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

static void applyInvert(PlayLayer* pl, float duration) {
    if (!pl) return;
    auto sc = cocos2d::CCDirector::sharedDirector()->getRunningScene();
    if (!sc) return;
    if (auto existing = sc->getChildByTag(kInvertControllerTag)) {
        if (auto ctrl = typeinfo_cast<InvertColorsController*>(existing)) {
            ctrl->m_playLayer = pl;
            ctrl->start(duration);
            return;
        }
        existing->removeFromParentAndCleanup(true);
    }
    auto ctrl = InvertColorsController::create(pl);
    if (!ctrl) return;
    ctrl->setTag(kInvertControllerTag);
    sc->addChild(ctrl, std::numeric_limits<int>::max());
    ctrl->start(duration);
}

void registerInvertColors(EventRegistry& reg) {
    reg.add(EventDef(
        "invert-colors",
        "Invert Colors",
        kEventDuration,
        [](PlayLayer* pl) {
            applyInvert(pl, kEventDuration);
        }
    ));
}

} // namespace chaosmod
