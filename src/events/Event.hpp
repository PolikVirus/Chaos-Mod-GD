#pragma once
#include <functional>
#include <string>
#include <utility>
#include <vector>

class PlayLayer;

namespace chaosmod {
    struct EventDef {
        std::string id;
        std::string name;
        float duration;
        std::function<void(PlayLayer*)> run;

        EventDef(std::string id_, std::string name_, std::function<void(PlayLayer*)> run_)
            : id(std::move(id_)), name(std::move(name_)), duration(0.f), run(std::move(run_)) {}

        EventDef(std::string id_, std::string name_, float duration_, std::function<void(PlayLayer*)> run_)
            : id(std::move(id_)), name(std::move(name_)), duration(duration_), run(std::move(run_)) {}
    };

    struct EventRegistry {
        std::vector<EventDef> events;
        void add(EventDef e) { events.push_back(std::move(e)); }
    };

    void registerAllEvents(EventRegistry& reg);
} // namespace chaosmod
