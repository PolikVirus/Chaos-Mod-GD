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

static constexpr float kEventDuration = 10.f;
static constexpr int kControllerTag = 0x5348414B; // 'SHAK'

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
    cocos2d::CCObject* obj = nullptr;
    CCARRAY_FOREACH(children, obj) {
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

class ShakeController : public cocos2d::CCNode {
public:
    PlayLayer* m_playLayer = nullptr;
    float m_totalTime = kEventDuration;
    float m_timeLeft = kEventDuration;
    float m_strength = 14.f;
    cocos2d::CCPoint m_previousOffset = cocos2d::CCPoint(0.f, 0.f);
    bool m_restored = false;
    bool m_wasPaused = false;

    static ShakeController* create(PlayLayer* pl) {
        if (!pl) return nullptr;
        auto c = new ShakeController();
        c->m_playLayer = pl;
        c->autorelease();
        return c;
    }

    cocos2d::CCScene* scene() {
        return cocos2d::CCDirector::sharedDirector()->getRunningScene();
    }

    cocos2d::CCPoint basePosition() const {
        auto cur = m_playLayer->getPosition();
        return {cur.x - m_previousOffset.x, cur.y - m_previousOffset.y};
    }

    void resetPosition() {
        auto base = basePosition();
        m_playLayer->setPosition(base);
        m_previousOffset = cocos2d::CCPoint(0.f, 0.f);
    }

    void start(float duration) {
        if (!m_playLayer) return;
        m_totalTime = duration;
        m_timeLeft = duration;
        m_previousOffset = cocos2d::CCPoint(0.f, 0.f);
        scheduleUpdate();
    }

    void applyShake(float strength) {
        auto base = basePosition();
        float ox = CCRANDOM_MINUS1_1() * strength;
        float oy = CCRANDOM_MINUS1_1() * strength;
        m_playLayer->setPosition({base.x + ox, base.y + oy});
        m_previousOffset = cocos2d::CCPoint(ox, oy);
    }

    void update(float dt) override {
        if (m_restored) { unscheduleUpdate(); return; }
        auto sc = scene();
        if (!sc || !m_playLayer) { restoreAndDelete(); return; }
        if (!isDescendant(sc, m_playLayer)) { restoreAndDelete(); return; }
        bool paused = isPauseMenuOpen(m_playLayer);
        if (paused) {
            if (!m_wasPaused) { resetPosition(); m_wasPaused = true; }
            return;
        }
        if (m_wasPaused) { resetPosition(); m_wasPaused = false; }
        dt = std::clamp(dt, 0.f, 0.05f);
        m_timeLeft -= dt;
        if (m_timeLeft <= 0.f) { restoreAndDelete(); return; }
        float t = (m_totalTime > 0.f) ? (m_timeLeft / m_totalTime) : 0.f;
        t = std::clamp(t, 0.f, 1.f);
        applyShake(m_strength * t);
    }

    void restore() {
        if (m_restored) return;
        m_restored = true;
        unscheduleUpdate();
        if (m_playLayer) resetPosition();
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

static void runShakeEffect(PlayLayer* pl, float duration) {
    if (!pl) return;
    auto scene = cocos2d::CCDirector::sharedDirector()->getRunningScene();
    if (!scene) return;
    if (auto existing = scene->getChildByTag(kControllerTag)) {
        if (auto ctrl = typeinfo_cast<ShakeController*>(existing)) {
            ctrl->m_playLayer = pl;
            ctrl->start(duration);
            return;
        }
        existing->removeFromParentAndCleanup(true);
    }
    auto ctrl = ShakeController::create(pl);
    if (!ctrl) return;
    ctrl->setTag(kControllerTag);
    scene->addChild(ctrl, std::numeric_limits<int>::max());
    ctrl->start(duration);
}

void registerShakeScreen(EventRegistry& reg) {
    reg.add(EventDef(
        "shake-screen",
        "Shake Screen",
        kEventDuration,
        [](PlayLayer* pl) {
            runShakeEffect(pl, kEventDuration);
        }
    ));
}

} // namespace chaosmod
