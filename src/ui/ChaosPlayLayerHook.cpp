#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>

#include "../events/Event.hpp"
#include "BottomTimerBar.hpp"
#include "EventLog.hpp"

#include <random>
#include <chrono>

using namespace geode::prelude;

static chaosmod::EventRegistry gEvents;
static std::mt19937 gRng;

static void ensureEventsRegistered() {
    static bool done = false;
    if (!done) {
        chaosmod::registerAllEvents(gEvents);
        done = true;
    }
}

static chaosmod::EventDef const* findEventById(std::string const& id) {
    for (auto const& ev : gEvents.events) {
        if (ev.id == id) return &ev;
    }
    return nullptr;
}

// debug forcing (kept)
static bool isForceEnabled() {
    auto mod = Mod::get();
    if (!mod || !mod->hasSetting("force-event")) return false;
    return mod->getSettingValue<bool>("force-event");
}

static std::string getForcedEventId() {
    auto mod = Mod::get();
    if (!mod || !mod->hasSetting("forced-event-id")) return "random";
    return mod->getSettingValue<std::string>("forced-event-id");
}

static chaosmod::EventDef const* pickEvent() {
    ensureEventsRegistered();
    if (gEvents.events.empty()) return nullptr;

    if (isForceEnabled()) {
        auto forced = getForcedEventId();
        if (!forced.empty() && forced != "random") {
            if (auto ev = findEventById(forced)) return ev;
        }
    }

    std::uniform_int_distribution<size_t> dist(0, gEvents.events.size() - 1);
    return &gEvents.events[dist(gRng)];
}

$execute {
    auto seed = (uint32_t)std::chrono::high_resolution_clock::now().time_since_epoch().count();
    gRng.seed(seed);
}

class $modify(ChaosBarPlayLayer, PlayLayer) {
    struct Fields {
        chaosui::BottomTimerBar bottom;
        chaosui::EventLog log;
        bool uiInited = false;
    };

    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects))
            return false;

        ensureEventsRegistered();

        auto parent = this->m_uiLayer
            ? static_cast<cocos2d::CCNode*>(this->m_uiLayer)
            : static_cast<cocos2d::CCNode*>(this);

        m_fields->bottom.init(parent);
        m_fields->log.init(parent);
        m_fields->uiInited = true;

        return true;
    }

    void postUpdate(float dt) {
        PlayLayer::postUpdate(dt);
        if (!this->isGameplayActive()) return;
        if (!m_fields->uiInited) return;

        // Keep log positioned correctly (text-only log)
        m_fields->log.update(dt);

        // Bottom bar ticks; when true -> fire event NOW
        if (m_fields->bottom.update(dt)) {
            if (auto ev = pickEvent()) {
                m_fields->log.add(ev->name, ev->duration);
                if (ev->run) ev->run(this);
            }
        }

    }
};
