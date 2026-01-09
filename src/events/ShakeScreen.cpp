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

static constexpr float kDurationSeconds = 10.f;

// Controller lives on the running scene so it resumes correctly after pause.
static constexpr int kShakeControllerTag = 0x5348414B; // 'SHAK'

static bool isDescendantOf(cocos2d::CCNode* root, cocos2d::CCNode* node) {
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
    // PauseLayer is typically added to PlayLayer and/or its UI layer.
    if (pl->m_uiLayer && hasPauseLayerIn(pl->m_uiLayer)) return true;
    if (hasPauseLayerIn(pl)) return true;

    if (auto scene = cocos2d::CCDirector::sharedDirector()->getRunningScene()) {
        if (hasPauseLayerIn(scene)) return true;
    }
#endif

    // Fallback: better than nothing (but not reliable on all builds)
    return !pl->isGameplayActive();
}

class ShakeScreenController : public cocos2d::CCNode {
public:
    PlayLayer* m_pl = nullptr;

    // Countdown that should NOT tick while paused.
    float m_total = kDurationSeconds;
    float m_remaining = kDurationSeconds;

    float m_strengthPx = 14.f; // intensity

    // Track last applied offset so we can recover the "true" base position without breaking camera movement.
    cocos2d::CCPoint m_lastOffset = cocos2d::CCPoint(0.f, 0.f);

    bool m_restored = false;
    bool m_wasPaused = false;

    static ShakeScreenController* create(PlayLayer* pl) {
        if (!pl) return nullptr;
        auto ret = new ShakeScreenController();
        ret->m_pl = pl;
        ret->autorelease();
        return ret;
    }

    cocos2d::CCScene* getScene() {
        return cocos2d::CCDirector::sharedDirector()->getRunningScene();
    }

    cocos2d::CCPoint getBasePos() const {
        // Current pos includes last offset; subtract it to get the real base.
        auto cur = m_pl->getPosition();
        return cocos2d::CCPoint(cur.x - m_lastOffset.x, cur.y - m_lastOffset.y);
    }

    void setToBasePos() {
        auto base = getBasePos();
        m_pl->setPosition(base);
        m_lastOffset = cocos2d::CCPoint(0.f, 0.f);
    }

    void begin(float durationSeconds) {
        if (!m_pl) return;

        m_total = durationSeconds;
        m_remaining = durationSeconds;

        // Start from "base" (no offset)
        m_lastOffset = cocos2d::CCPoint(0.f, 0.f);

        this->scheduleUpdate();
    }

    void applyShake(float strength) {
        auto base = getBasePos();

        float ox = CCRANDOM_MINUS1_1() * strength;
        float oy = CCRANDOM_MINUS1_1() * strength;

        m_pl->setPosition(cocos2d::CCPoint(base.x + ox, base.y + oy));
        m_lastOffset = cocos2d::CCPoint(ox, oy);
    }

    void update(float dt) override {
        if (m_restored) {
            this->unscheduleUpdate();
            return;
        }

        auto scene = getScene();
        if (!scene || !m_pl) {
            restoreAndRemove();
            return;
        }

        // If we left the level, clean up so it doesn't leak elsewhere.
        if (!isDescendantOf(scene, m_pl)) {
            restoreAndRemove();
            return;
        }

        bool paused = isPauseMenuOpen(m_pl);

        // While paused: remove any current shake offset and DO NOT consume time.
        if (paused) {
            if (!m_wasPaused) {
                setToBasePos();
                m_wasPaused = true;
            }
            return;
        }

        // Just resumed
        if (m_wasPaused) {
            // Ensure we're starting from base (no offset) after unpause.
            setToBasePos();
            m_wasPaused = false;
        }

        // Clamp dt so the first frame after unpausing can't skip the whole effect.
        if (dt < 0.f) dt = 0.f;
        if (dt > 0.05f) dt = 0.05f;

        // Tick only while not paused
        m_remaining -= dt;
        if (m_remaining <= 0.f) {
            restoreAndRemove();
            return;
        }

        // Fade out
        float t = (m_total > 0.f) ? (m_remaining / m_total) : 0.f;
        if (t < 0.f) t = 0.f;
        if (t > 1.f) t = 1.f;

        applyShake(m_strengthPx * t);
    }

    void restore() {
        if (m_restored) return;
        m_restored = true;

        this->unscheduleUpdate();

        if (m_pl) {
            setToBasePos();
        }
    }

    void restoreAndRemove() {
        restore();
        this->removeFromParentAndCleanup(true);
    }

    void onExit() override {
        restore();
        cocos2d::CCNode::onExit();
    }
};

static void applyShake(PlayLayer* pl, float durationSeconds) {
    if (!pl) return;

    auto scene = cocos2d::CCDirector::sharedDirector()->getRunningScene();
    if (!scene) return;

    // Reuse controller if already active
    if (auto existing = scene->getChildByTag(kShakeControllerTag)) {
        if (auto ctrl = typeinfo_cast<ShakeScreenController*>(existing)) {
            ctrl->m_pl = pl;
            ctrl->begin(durationSeconds);
            return;
        }
        existing->removeFromParentAndCleanup(true);
    }

    auto ctrl = ShakeScreenController::create(pl);
    if (!ctrl) return;

    ctrl->setTag(kShakeControllerTag);
    scene->addChild(ctrl, std::numeric_limits<int>::max());
    ctrl->begin(durationSeconds);
}

void registerShakeScreen(EventRegistry& reg) {
    reg.add(EventDef(
        "shake-screen",
        "Shake Screen",
        kDurationSeconds,
        [](PlayLayer* pl) {
            applyShake(pl, kDurationSeconds);
        }
    ));
}

} // namespace chaosmod
