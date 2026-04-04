#include "Event.hpp"

#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>

#if __has_include(<Geode/binding/PauseLayer.hpp>)
    #include <Geode/binding/PauseLayer.hpp>
    #define CHAOS_HAS_PAUSE_LAYER 1
#else
    #define CHAOS_HAS_PAUSE_LAYER 0
#endif

#include <limits>

using namespace geode::prelude;

namespace chaosmod {

static constexpr float dur = 10.f;
static constexpr int flipTag = 0x464C5056;

static bool isDescendant(cocos2d::CCNode* root, cocos2d::CCNode* node) {
    if (!root || !node) return false;
    for (auto cur = node; cur; cur = cur->getParent()) {
        if (cur == root) return true;
    }
    return false;
}

#if CHAOS_HAS_PAUSE_LAYER
static bool hasPauseLayerIn(cocos2d::CCNode* n) {
    if (!n) return false;
    auto children = n->getChildren();
    if (!children) return false;
    for (auto obj : CCArrayExt(children)) {
        if (auto child = typeinfo_cast<cocos2d::CCNode*>(obj)) {
            if (typeinfo_cast<PauseLayer*>(child)) return true;
        }
    }
    return false;
}
#endif

static bool isPauseMenuOpen(PlayLayer* pl) {
    if (!pl) return false;
#if CHAOS_HAS_PAUSE_LAYER
    if (pl->m_uiLayer && hasPauseLayerIn(pl->m_uiLayer)) return true;
    if (hasPauseLayerIn(pl)) return true;
    if (auto scene = cocos2d::CCDirector::sharedDirector()->getRunningScene()) {
        if (hasPauseLayerIn(scene)) return true;
    }
#endif
    return !pl->isGameplayActive();
}

class FlipVCtrl : public cocos2d::CCNode {
public:
    PlayLayer* m_playLayer = nullptr;
    float m_timeLeft = dur;
    cocos2d::CCPoint m_originalPosition;
    float m_originalScaleX;
    float m_originalScaleY;
    bool m_restored = false;
    bool m_wasPaused = false;

    static FlipVCtrl* create(PlayLayer* pl) {
        if (!pl) return nullptr;
        auto c = new FlipVCtrl();
        c->m_playLayer = pl;
        c->autorelease();
        return c;
    }

    cocos2d::CCScene* scene() {
        return cocos2d::CCDirector::sharedDirector()->getRunningScene();
    }

    void start(float duration) {
        if (!m_playLayer) return;
        m_timeLeft = duration;
        m_originalPosition = m_playLayer->getPosition();
        m_originalScaleX = m_playLayer->getScaleX();
        m_originalScaleY = m_playLayer->getScaleY();

        m_playLayer->setAnchorPoint({0.5f, 0.5f});

        m_playLayer->setScaleY(-m_originalScaleY);

        scheduleUpdate();
    }

    void update(float dt) override {
        if (m_restored) { unscheduleUpdate(); return; }
        auto sc = scene();
        if (!sc || !m_playLayer) { restoreAndDelete(); return; }
        if (!isDescendant(sc, m_playLayer)) { restoreAndDelete(); return; }
        bool paused = isPauseMenuOpen(m_playLayer);
        if (paused) {
            if (!m_wasPaused) { m_wasPaused = true; }
            return;
        }
        if (m_wasPaused) { m_wasPaused = false; }
        dt = std::clamp(dt, 0.f, 0.05f);
        m_timeLeft -= dt;
        if (m_timeLeft <= 0.f) { restoreAndDelete(); return; }
    }

    void restore() {
        if (m_restored) return;
        m_restored = true;
        unscheduleUpdate();
        if (m_playLayer) {
            m_playLayer->setScaleX(m_originalScaleX);
            m_playLayer->setScaleY(m_originalScaleY);
            m_playLayer->setPosition(m_originalPosition);

            m_playLayer->setAnchorPoint({0.5f, 0.5f});
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

static void runFlipVerticalEffect(PlayLayer* pl, float duration) {
    if (!pl) return;
    auto scene = cocos2d::CCDirector::sharedDirector()->getRunningScene();
    if (!scene) return;
    if (auto existing = scene->getChildByTag(flipTag)) {
        if (auto ctrl = typeinfo_cast<FlipVCtrl*>(existing)) {
            ctrl->m_playLayer = pl;
            ctrl->start(duration);
            return;
        }
        existing->removeFromParentAndCleanup(true);
    }
    auto ctrl = FlipVCtrl::create(pl);
    if (!ctrl) return;
    ctrl->setTag(flipTag);
    scene->addChild(ctrl, std::numeric_limits<int>::max());
    ctrl->start(duration);
}

void registerFlipVertical(EventRegistry& reg) {
    reg.add(EventDef(
        "flip-vertical",
        "Flip Screen Vertically",
        dur,
        [](PlayLayer* pl) {
            runFlipVerticalEffect(pl, dur);
        }
    ));
}

} // namespace chaosmod
