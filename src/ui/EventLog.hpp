// kinda vibecoded :(
#ifndef CHAOSUI_EVENTLOG_HPP
#define CHAOSUI_EVENTLOG_HPP

#include <deque>
#include <string>
#include <vector>

namespace cocos2d {
    class CCNode;
    class CCLabelBMFont;
}

namespace chaosui {

// durationSeconds:
//   > 0  => countdown shown, expires at 0
//   == 0 => countdown shown, uses 10s fallback (legacy)
//   < 0  => NO countdown shown, still expires after 10s fallback
class EventLog {
public:
    void init(cocos2d::CCNode* parent);

    void add(std::string const& name, float durationSeconds);

    void update(float dt);

private:
    struct Line {
        std::string name;
        float timeLeft = 0.f;   // counts down to 0
        bool showTimer = true;  // if false, render name only
    };

    struct Slot {
        cocos2d::CCNode* node = nullptr;
        cocos2d::CCLabelBMFont* label = nullptr;
        bool active = false;
    };

    void layout();
    void refreshText();

    cocos2d::CCNode* m_root = nullptr;
    std::deque<Line> m_lines;   // newest first
    std::vector<Slot> m_slots;
};

} // namespace chaosui

#endif // CHAOSUI_EVENTLOG_HPP
