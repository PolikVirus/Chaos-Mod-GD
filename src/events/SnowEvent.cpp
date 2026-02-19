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

static constexpr float kEventDuration = 20.f;
static constexpr int kSnowControllerTag = 0x534E4F57; // 'SNOW'

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

class SnowController : public cocos2d::CCNode {
public:
    PlayLayer* m_playLayer = nullptr;
    cocos2d::CCParticleSystemQuad* m_snowParticles = nullptr;
    float m_timeLeft = kEventDuration;
    bool m_restored = false;
    bool m_wasPaused = false;
    float m_originalEmissionRate = 80.f;

    static SnowController* create(PlayLayer* pl) {
        if (!pl) return nullptr;
        auto c = new SnowController();
        c->m_playLayer = pl;
        c->autorelease();
        return c;
    }

    cocos2d::CCScene* scene() {
        return cocos2d::CCDirector::sharedDirector()->getRunningScene();
    }

    void start() {
        if (!m_playLayer) return;
        m_timeLeft = kEventDuration;
        auto sc = scene();
        if (!sc) return;

        // Create snow particle system
        m_snowParticles = cocos2d::CCParticleSnow::create();
        if (m_snowParticles) {
            // Center the particle system on screen
            auto winSize = cocos2d::CCDirector::sharedDirector()->getWinSize();
            m_snowParticles->setPosition(cocos2d::CCPoint(winSize.width / 2, winSize.height / 2));
            // Increase variance to cover entire screen evenly
            m_snowParticles->setPosVar(cocos2d::CCPoint(winSize.width / 2 + 50, winSize.height / 2 + 50));
            m_snowParticles->setLife(8.f);
            m_snowParticles->setLifeVar(4.f);
            m_snowParticles->setSpeed(80.f);
            m_snowParticles->setSpeedVar(30.f);
            m_snowParticles->setEmissionRate(m_originalEmissionRate); // More particles for better coverage
            sc->addChild(m_snowParticles, std::numeric_limits<int>::max());
        }

        scheduleUpdate();
    }

    void update(float dt) override {
        if (m_restored) { unscheduleUpdate(); return; }
        auto sc = scene();
        if (!sc || !m_playLayer) { restoreAndDelete(); return; }
        if (!isDescendant(sc, m_playLayer)) { restoreAndDelete(); return; }
        
        bool paused = isPauseMenuOpen(m_playLayer);
        if (paused) {
            if (!m_wasPaused && m_snowParticles) { 
                m_snowParticles->setEmissionRate(0.f); 
                m_wasPaused = true; 
            }
            return;
        }
        if (m_wasPaused && m_snowParticles) { 
            m_snowParticles->setEmissionRate(m_originalEmissionRate); 
            m_wasPaused = false; 
        }
        
        m_timeLeft -= dt;
        if (m_timeLeft <= 0.f) { restoreAndDelete(); return; }
    }

    void restore() {
        if (m_restored) return;
        m_restored = true;
        unscheduleUpdate();
        if (m_snowParticles) {
            m_snowParticles->stopSystem();
            m_snowParticles->removeFromParentAndCleanup(true);
            m_snowParticles = nullptr;
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

static void runSnowEffect(PlayLayer* pl) {
    if (!pl) return;
    auto scene = cocos2d::CCDirector::sharedDirector()->getRunningScene();
    if (!scene) return;
    if (auto existing = scene->getChildByTag(kSnowControllerTag)) {
        if (auto ctrl = typeinfo_cast<SnowController*>(existing)) {
            ctrl->m_playLayer = pl;
            ctrl->start();
            return;
        }
        existing->removeFromParentAndCleanup(true);
    }
    auto ctrl = SnowController::create(pl);
    if (!ctrl) return;
    ctrl->setTag(kSnowControllerTag);
    scene->addChild(ctrl, std::numeric_limits<int>::max());
    ctrl->start();
}

void registerSnowEvent(EventRegistry& reg) {
    reg.add(EventDef(
        "snow-screen",
        "Snow Screen",
        kEventDuration,
        [](PlayLayer* pl) {
            runSnowEffect(pl);
        }
    ));
}

} // namespace chaosmod