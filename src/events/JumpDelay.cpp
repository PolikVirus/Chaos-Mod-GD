#include "Event.hpp"

#include <Geode/Geode.hpp>
#include <Geode/Enums.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>

#include <deque>

using namespace geode::prelude;

namespace chaosmod {

static constexpr char const* evId = "jump-delay";
static constexpr char const* evName = "Jump Delay";

static constexpr float delaySec = 0.3f;
static constexpr float dur = 20.f;

static constexpr int delayTag = 0x4A44454C;

struct JumpBuf {
    float timeLeft = 0.f;
    bool down = false;
    int button = 0;
    bool isP1 = true;
};

struct DelayState {
    bool active = false;
    bool bypass = false;
    PlayLayer* owner = nullptr;
} static gJumpDelay;

static bool pausedNow(PlayLayer* pl) {
    return pl && !pl->isGameplayActive();
}

class DelayCtrl : public cocos2d::CCNode {
public:
    PlayLayer* m_playLayer = nullptr;
    float m_delay = delaySec;
    float m_timeRemaining = dur;
    std::deque<JumpBuf> m_queue;

    static DelayCtrl* create(PlayLayer* pl) {
        auto ctrl = new DelayCtrl();
        if (!ctrl) return nullptr;
        ctrl->m_playLayer = pl;
        ctrl->autorelease();
        return ctrl;
    }

    void bindToPlayLayer(PlayLayer* pl, bool clearQueue) {
        if (!pl) return;
        m_playLayer = pl;
        if (clearQueue) {
            m_queue.clear();
        }
        gJumpDelay.active = true;
        gJumpDelay.owner = pl;
        gJumpDelay.bypass = false;
    }

    void start(PlayLayer* pl, float delaySeconds, float duration) {
        m_delay = delaySeconds;
        m_timeRemaining = duration;
        bindToPlayLayer(pl ? pl : PlayLayer::get(), false);
        scheduleUpdate();
    }

    void enqueue(bool down, int button, bool isP1) {
        m_queue.push_back(JumpBuf{m_delay, down, button, isP1});
    }

    void dispatchNow(bool down, int button, bool isP1) {
        if (!m_playLayer) return;
        gJumpDelay.bypass = true;
        m_playLayer->handleButton(down, button, isP1);
        gJumpDelay.bypass = false;
    }

    void flushQueue() {
        while (!m_queue.empty()) {
            auto jump = m_queue.front();
            m_queue.pop_front();
            dispatchNow(jump.down, jump.button, jump.isP1);
        }
    }

    void clearGlobalState() {
        gJumpDelay.active = false;
        gJumpDelay.owner = nullptr;
        gJumpDelay.bypass = false;
    }

    void stopAndDelete() {
        auto current = PlayLayer::get();
        if (current && current == m_playLayer) {
            flushQueue();
        } else {
            m_queue.clear();
        }
        clearGlobalState();
        unscheduleUpdate();
        removeFromParentAndCleanup(true);
    }

    void update(float dt) override {
        auto current = PlayLayer::get();
        if (current && current != m_playLayer) {
            bindToPlayLayer(current, true);
        }

        if (!m_playLayer || gJumpDelay.owner != m_playLayer || !gJumpDelay.active) {
            stopAndDelete();
            return;
        }

        if (pausedNow(m_playLayer)) {
            return;
        }

        m_timeRemaining -= dt;
        for (auto& q : m_queue) {
            q.timeLeft -= dt;
        }

        while (!m_queue.empty() && m_queue.front().timeLeft <= 0.f) {
            auto jump = m_queue.front();
            m_queue.pop_front();
            dispatchNow(jump.down, jump.button, jump.isP1);
        }

        if (m_timeRemaining <= 0.f) {
            stopAndDelete();
        }
    }

    void onExit() override {
        clearGlobalState();
        cocos2d::CCNode::onExit();
    }
};

class $modify(DelayInputHook, GJBaseGameLayer) {
    void handleButton(bool down, int button, bool isPlayer1) {
        if (gJumpDelay.bypass) {
            return GJBaseGameLayer::handleButton(down, button, isPlayer1);
        }

        auto pl = typeinfo_cast<PlayLayer*>(this);
        if (!pl || !gJumpDelay.active || gJumpDelay.owner != pl) {
            return GJBaseGameLayer::handleButton(down, button, isPlayer1);
        }

        if (button != static_cast<int>(PlayerButton::Jump)) {
            return GJBaseGameLayer::handleButton(down, button, isPlayer1);
        }

        auto scene = cocos2d::CCDirector::sharedDirector()->getRunningScene();
        auto ctrlNode = scene ? scene->getChildByTag(delayTag) : nullptr;
        auto ctrl = ctrlNode ? typeinfo_cast<DelayCtrl*>(ctrlNode) : nullptr;
        if (!ctrl) {
            return GJBaseGameLayer::handleButton(down, button, isPlayer1);
        }

        ctrl->enqueue(down, button, isPlayer1);
    }
};

static void runDelay(PlayLayer* pl, float delaySeconds, float duration) {
    auto scene = cocos2d::CCDirector::sharedDirector()->getRunningScene();
    if (!scene) return;

    DelayCtrl* ctrl = nullptr;
    if (auto existing = scene->getChildByTag(delayTag)) {
        ctrl = typeinfo_cast<DelayCtrl*>(existing);
        if (!ctrl) {
            existing->removeFromParentAndCleanup(true);
            ctrl = nullptr;
        }
    }

    if (!ctrl) {
        ctrl = DelayCtrl::create(pl);
        if (!ctrl) return;
        ctrl->setTag(delayTag);
        scene->addChild(ctrl);
    }

    ctrl->start(pl, delaySeconds, duration);
}

void registerJumpDelay(EventRegistry& reg) {
    reg.add(EventDef(
        evId,
        evName,
        dur,
        [](PlayLayer* pl) {
            runDelay(pl, delaySec, dur);
        }
    ));
}

} // namespace chaosmod