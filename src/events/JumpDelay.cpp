#include "Event.hpp"

#include <Geode/Geode.hpp>
#include <Geode/Enums.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>

#include <deque>

using namespace geode::prelude;

namespace chaosmod {

static constexpr char const* kId   = "jump-delay";
static constexpr char const* kName = "Jump Delay";

static constexpr float kDelaySeconds    = 0.3f;
static constexpr float kDurationSeconds = 20.f;

static constexpr int kControllerTag = 0x4A44454C; // 'JDEL'

struct PendingJump {
    float t = 0.f;
    bool down = false;
    int button = 0;
    bool isPlayer1 = true;
};

struct JumpDelayState {
    bool active = false;
    bool bypass = false;      // prevents re-queuing when we replay delayed input
    PlayLayer* owner = nullptr;
} static gJD;

class JumpDelayController : public cocos2d::CCNode {
public:
    PlayLayer* m_pl = nullptr;
    float m_delay = kDelaySeconds;
    float m_remaining = kDurationSeconds;
    std::deque<PendingJump> m_queue;

    static JumpDelayController* create(PlayLayer* pl) {
        auto ret = new JumpDelayController();
        ret->m_pl = pl;
        ret->autorelease();
        return ret;
    }

    void start(float delaySeconds, float durationSeconds) {
        m_delay = delaySeconds;
        m_remaining = durationSeconds;

        this->stopAllActions();
        this->scheduleUpdate();
    }

    void queue(bool down, int button, bool isPlayer1) {
        m_queue.push_back(PendingJump{ m_delay, down, button, isPlayer1 });
    }

    void sendNow(bool down, int button, bool isPlayer1) {
        if (!m_pl) return;
        gJD.bypass = true;
        m_pl->handleButton(down, button, isPlayer1); // goes through hook, but bypass makes it pass-through
        gJD.bypass = false;
    }

    void flushAll() {
        while (!m_queue.empty()) {
            auto p = m_queue.front();
            m_queue.pop_front();
            sendNow(p.down, p.button, p.isPlayer1);
        }
    }

    void stopAndRemove() {
        flushAll();

        if (gJD.owner == m_pl) {
            gJD.active = false;
            gJD.owner = nullptr;
            gJD.bypass = false;
        }

        this->removeFromParentAndCleanup(true);
    }

    void update(float dt) override {
        if (!m_pl || gJD.owner != m_pl || !gJD.active) {
            stopAndRemove();
            return;
        }

        m_remaining -= dt;

        for (auto& p : m_queue) p.t -= dt;
        while (!m_queue.empty() && m_queue.front().t <= 0.f) {
            auto p = m_queue.front();
            m_queue.pop_front();
            sendNow(p.down, p.button, p.isPlayer1);
        }

        if (m_remaining <= 0.f) {
            stopAndRemove();
        }
    }

    void onExit() override {
        // fail-safe: don't leave global state on scene changes
        if (gJD.owner == m_pl) {
            gJD.active = false;
            gJD.owner = nullptr;
            gJD.bypass = false;
        }
        cocos2d::CCNode::onExit();
    }
};

// This is the IMPORTANT hook: input comes through here.
class $modify(JumpDelayInputHook, GJBaseGameLayer) {
    void handleButton(bool down, int button, bool isPlayer1) {
        if (gJD.bypass) {
            return GJBaseGameLayer::handleButton(down, button, isPlayer1);
        }

        // Only when active for the current PlayLayer
        auto pl = typeinfo_cast<PlayLayer*>(this);
        if (!pl || !gJD.active || gJD.owner != pl) {
            return GJBaseGameLayer::handleButton(down, button, isPlayer1);
        }

        // Only delay Jump button
        if (button != static_cast<int>(PlayerButton::Jump)) {
            return GJBaseGameLayer::handleButton(down, button, isPlayer1);
        }

        auto ctrlNode = pl->getChildByTag(kControllerTag);
        auto ctrl = ctrlNode ? typeinfo_cast<JumpDelayController*>(ctrlNode) : nullptr;
        if (!ctrl) {
            // If controller missing for some reason, don't break input
            return GJBaseGameLayer::handleButton(down, button, isPlayer1);
        }

        // Consume now; replay after delay
        ctrl->queue(down, button, isPlayer1);
        return;
    }
};

static void enableJumpDelay(PlayLayer* pl, float delaySeconds, float durationSeconds) {
    if (!pl) return;

    JumpDelayController* ctrl = nullptr;

    if (auto existing = pl->getChildByTag(kControllerTag)) {
        ctrl = typeinfo_cast<JumpDelayController*>(existing);
        if (!ctrl) {
            existing->removeFromParentAndCleanup(true);
            ctrl = nullptr;
        }
    }

    if (!ctrl) {
        ctrl = JumpDelayController::create(pl);
        if (!ctrl) return;
        ctrl->setTag(kControllerTag);
        pl->addChild(ctrl, 999999);
    }

    gJD.active = true;
    gJD.owner = pl;

    ctrl->start(delaySeconds, durationSeconds);
}

void registerJumpDelay(EventRegistry& reg) {
    reg.add(EventDef(
        kId,
        kName,
        kDurationSeconds,
        [](PlayLayer* pl) {
            enableJumpDelay(pl, kDelaySeconds, kDurationSeconds);
        }
    ));
}

} // namespace chaosmod
