#include "Event.hpp"

#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/binding/PlayerObject.hpp>

#include <cmath>

using namespace geode::prelude;

namespace chaosmod {

static constexpr float dur = 20.f;
static constexpr float lowMul = 0.5f;
static constexpr float highMul = 2.0f;
static constexpr int gravTag = 0x47524156;

static PlayLayer* findPL(cocos2d::CCNode* node) {
    if (!node) return nullptr;
    if (auto pl = typeinfo_cast<PlayLayer*>(node)) return pl;

    auto children = node->getChildren();
    if (!children) return nullptr;

    for (auto obj : CCArrayExt(children)) {
        if (auto child = typeinfo_cast<cocos2d::CCNode*>(obj)) {
            if (auto pl = findPL(child)) return pl;
        }
    }
    return nullptr;
}

static PlayLayer* curPL() {
    return findPL(cocos2d::CCDirector::sharedDirector()->getRunningScene());
}

static bool pausedNow(PlayLayer* pl) {
    return pl && !pl->isGameplayActive();
}

static bool sameish(float a, float b) {
    float diff = std::fabs(a - b);
    float scale = std::fmax(1.f, std::fmax(std::fabs(a), std::fabs(b)));
    return diff <= 1e-4f * scale;
}

class GravThing : public cocos2d::CCNode {
public:
    PlayLayer* m_playLayer = nullptr;
    PlayerObject* m_player1 = nullptr;
    PlayerObject* m_player2 = nullptr;
    float m_player1Base = 1.f;
    float m_player2Base = 1.f;
    bool m_hasPlayer2 = false;
    float m_multiplier = 1.f;
    float m_timeRemaining = 0.f;
    bool m_finished = false;
    bool m_initialized = false;

    static GravThing* create(PlayLayer* pl) {
        auto node = new GravThing();
        if (!node) return nullptr;
        node->m_playLayer = pl;
        node->autorelease();
        return node;
    }

    void captureCurrentPlayers() {
        if (!m_playLayer) {
            m_player1 = nullptr;
            m_player2 = nullptr;
            m_hasPlayer2 = false;
            return;
        }

        m_player1 = m_playLayer->m_player1;
        m_player2 = m_playLayer->m_player2;
        m_hasPlayer2 = m_player2 != nullptr;

        if (m_player1) m_player1Base = m_player1->m_gravityMod;
        if (m_player2) m_player2Base = m_player2->m_gravityMod;
    }

    void bindToPlayLayer(PlayLayer* pl, bool recaptureBase) {
        m_playLayer = pl;
        if (recaptureBase || !m_initialized) {
            captureCurrentPlayers();
            m_initialized = true;
            return;
        }

        if (m_playLayer) {
            if (m_playLayer->m_player1 && m_playLayer->m_player1 != m_player1) {
                m_player1 = m_playLayer->m_player1;
                m_player1Base = m_player1->m_gravityMod;
            }
            if (m_playLayer->m_player2 && m_playLayer->m_player2 != m_player2) {
                m_player2 = m_playLayer->m_player2;
                m_player2Base = m_player2->m_gravityMod;
                m_hasPlayer2 = true;
            } else if (!m_playLayer->m_player2) {
                m_player2 = nullptr;
                m_hasPlayer2 = false;
            }
        }
    }

    void start(PlayLayer* pl, float mul, float seconds) {
        bool playLayerChanged = pl && pl != m_playLayer;
        bindToPlayLayer(pl ? pl : curPL(), playLayerChanged || !m_initialized);
        m_multiplier = mul;
        m_timeRemaining = seconds;
        m_finished = false;
        applyCurrentGravity();
        scheduleUpdate();
    }

    void applyCurrentGravity() {
        if (!m_playLayer || m_finished) return;

        auto p1 = m_playLayer->m_player1;
        if (p1) {
            if (p1 != m_player1) {
                m_player1 = p1;
                m_player1Base = p1->m_gravityMod;
            }
            p1->m_gravityMod = m_player1Base * m_multiplier;
        }

        auto p2 = m_playLayer->m_player2;
        if (p2) {
            if (p2 != m_player2) {
                m_player2 = p2;
                m_player2Base = p2->m_gravityMod;
            }
            m_hasPlayer2 = true;
            p2->m_gravityMod = m_player2Base * m_multiplier;
        } else {
            m_player2 = nullptr;
            m_hasPlayer2 = false;
        }
    }

    void restoreCurrentGravity() {
        if (!m_playLayer) return;

        if (auto p1 = m_playLayer->m_player1) {
            if (m_player1 && p1 == m_player1) {
                p1->m_gravityMod = m_player1Base;
            } else if (sameish(p1->m_gravityMod, m_player1Base * m_multiplier)) {
                p1->m_gravityMod = m_player1Base;
            }
        }

        if (auto p2 = m_playLayer->m_player2) {
            if (m_player2 && p2 == m_player2) {
                p2->m_gravityMod = m_player2Base;
            } else if (m_hasPlayer2 && sameish(p2->m_gravityMod, m_player2Base * m_multiplier)) {
                p2->m_gravityMod = m_player2Base;
            }
        }
    }

    void update(float dt) override {
        auto current = curPL();
        if (current && current != m_playLayer) {
            bindToPlayLayer(current, true);
        }

        if (!m_playLayer || m_finished) {
            unscheduleUpdate();
            return;
        }

        applyCurrentGravity();

        if (pausedNow(m_playLayer)) {
            return;
        }

        m_timeRemaining -= dt;
        if (m_timeRemaining <= 0.f) {
            stopAndCleanup();
        }
    }

    void stop() {
        if (m_finished) return;
        m_finished = true;
        restoreCurrentGravity();
        unscheduleUpdate();
    }

    void stopAndCleanup() {
        stop();
        removeFromParentAndCleanup(true);
    }

    void onExit() override {
        m_finished = true;
        unscheduleUpdate();
        cocos2d::CCNode::onExit();
    }
};

static void doGravity(PlayLayer* pl, float mul, float seconds) {
    auto scene = cocos2d::CCDirector::sharedDirector()->getRunningScene();
    if (!scene) return;

    if (auto existing = scene->getChildByTag(gravTag)) {
        if (auto node = typeinfo_cast<GravThing*>(existing)) {
            node->start(pl, mul, seconds);
            return;
        }
        existing->removeFromParentAndCleanup(true);
    }

    auto node = GravThing::create(pl);
    if (!node) return;
    node->setTag(gravTag);
    scene->addChild(node, 999999);
    node->start(pl, mul, seconds);
}

void registerLowGravity(EventRegistry& reg) {
    reg.add(EventDef(
        "gravity-low",
        "Low Gravity",
        dur,
        [](PlayLayer* pl) {
            doGravity(pl, lowMul, dur);
        }
    ));
}

void registerHighGravity(EventRegistry& reg) {
    reg.add(EventDef(
        "gravity-high",
        "High Gravity",
        dur,
        [](PlayLayer* pl) {
            doGravity(pl, highMul, dur);
        }
    ));
}

} // namespace chaosmod
