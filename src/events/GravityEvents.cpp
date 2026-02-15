#include "Event.hpp"

#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/binding/PlayerObject.hpp>

#include <cmath>

using namespace geode::prelude;

namespace chaosmod {

static constexpr float kEventDuration = 20.f;
static constexpr float kLowGravityMultiplier = 0.5f;
static constexpr float kHighGravityMultiplier = 2.0f;
static constexpr int kGravityTag = 0x47524156; // 'GRAV'

static bool nearlyEqual(float a, float b) {
    float diff = std::fabs(a - b);
    float scale = std::fmax(1.f, std::fmax(std::fabs(a), std::fabs(b)));
    return diff <= 1e-4f * scale;
}

class GravityNode : public cocos2d::CCNode {
public:
    PlayLayer* m_playLayer = nullptr;
    float m_player1Base = 1.f;
    float m_player2Base = 1.f;
    bool m_hasPlayer2 = false;
    float m_multiplier = 1.f;
    PlayerObject* m_player1 = nullptr;
    PlayerObject* m_player2 = nullptr;
    bool m_finished = false;
    bool m_initialized = false;

    static GravityNode* create(PlayLayer* pl) {
        if (!pl) return nullptr;
        auto node = new GravityNode();
        node->m_playLayer = pl;
        node->autorelease();
        return node;
    }

    void start(float mul, float seconds) {
        if (!m_playLayer) return;

        if (!m_initialized) {
            if (m_playLayer->m_player1) {
                m_player1Base = m_playLayer->m_player1->m_gravityMod;
                m_player1 = m_playLayer->m_player1;
            }
            if (m_playLayer->m_player2) {
                m_player2Base = m_playLayer->m_player2->m_gravityMod;
                m_player2 = m_playLayer->m_player2;
                m_hasPlayer2 = true;
            } else {
                m_hasPlayer2 = false;
                m_player2 = nullptr;
            }
            m_initialized = true;
        } else {
            if (m_playLayer->m_player1 && m_playLayer->m_player1 != m_player1) {
                m_player1 = m_playLayer->m_player1;
                m_player1Base = m_player1->m_gravityMod;
            }
            if (m_playLayer->m_player2) {
                m_hasPlayer2 = true;
                if (m_playLayer->m_player2 != m_player2) {
                    m_player2 = m_playLayer->m_player2;
                    m_player2Base = m_player2->m_gravityMod;
                }
            } else {
                m_hasPlayer2 = false;
                m_player2 = nullptr;
            }
        }
        m_multiplier = mul;
        applyImmediately();
        scheduleUpdate();
        armTimer(seconds);
    }

    void applyImmediately() {
        if (!m_playLayer || m_finished) return;
        if (m_playLayer->m_player1) {
            m_playLayer->m_player1->m_gravityMod = m_player1Base * m_multiplier;
        }
        if (m_hasPlayer2 && m_playLayer->m_player2) {
            m_playLayer->m_player2->m_gravityMod = m_player2Base * m_multiplier;
        }
    }

    void syncPlayer(PlayerObject*& lastPtr, float& base, PlayerObject* current) {
        if (!current) return;
        if (current != lastPtr) {
            lastPtr = current;
            base = current->m_gravityMod;
        }
        float wanted = base * m_multiplier;
        float cur = current->m_gravityMod;
        if (nearlyEqual(cur, wanted)) return;
        if (std::fabs(cur - wanted) < std::fabs(cur - base)) {
            current->m_gravityMod = wanted;
        } else {
            base = cur;
            current->m_gravityMod = base * m_multiplier;
        }
    }

    void update(float) override {
        if (!m_playLayer || m_finished) {
            unscheduleUpdate();
            return;
        }
        syncPlayer(m_player1, m_player1Base, m_playLayer->m_player1);
        if (m_hasPlayer2) {
            syncPlayer(m_player2, m_player2Base, m_playLayer->m_player2);
        }
    }

    void armTimer(float seconds) {
        stopAllActions();
        runAction(cocos2d::CCSequence::create(
            cocos2d::CCDelayTime::create(seconds),
            cocos2d::CCCallFunc::create(this, callfunc_selector(GravityNode::stopAndCleanup)),
            nullptr
        ));
    }

    void stop() {
        if (m_finished) return;
        m_finished = true;
        if (!m_playLayer) return;
        if (m_playLayer->m_player1) {
            m_playLayer->m_player1->m_gravityMod = m_player1Base;
        }
        if (m_hasPlayer2 && m_playLayer->m_player2) {
            m_playLayer->m_player2->m_gravityMod = m_player2Base;
        }
        unscheduleUpdate();
    }

    void stopAndCleanup() {
        stop();
        removeFromParentAndCleanup(true);
    }

    void onExit() override {
        stop();
        cocos2d::CCNode::onExit();
    }
};

static void runGravityEffect(PlayLayer* pl, float mul, float seconds) {
    if (!pl) return;
    if (auto existing = pl->getChildByTag(kGravityTag)) {
        if (auto node = dynamic_cast<GravityNode*>(existing)) {
            node->start(mul, seconds);
            return;
        }
        existing->removeFromParentAndCleanup(true);
    }
    auto node = GravityNode::create(pl);
    if (!node) return;
    node->setTag(kGravityTag);
    pl->addChild(node, 999999);
    node->start(mul, seconds);
}

void registerLowGravity(EventRegistry& reg) {
    reg.add(EventDef(
        "gravity-low",
        "Low Gravity",
        kEventDuration,
        [](PlayLayer* pl) {
            runGravityEffect(pl, kLowGravityMultiplier, kEventDuration);
        }
    ));
}

void registerHighGravity(EventRegistry& reg) {
    reg.add(EventDef(
        "gravity-high",
        "High Gravity",
        kEventDuration,
        [](PlayLayer* pl) {
            runGravityEffect(pl, kHighGravityMultiplier, kEventDuration);
        }
    ));
}

} // namespace chaosmod
