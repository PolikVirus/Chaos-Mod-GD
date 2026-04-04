
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

class EventLog {
public:
    void init(cocos2d::CCNode* parent);

    void add(std::string const& name, float durationSeconds);

    void update(float dt);

private:
    struct Line {
        std::string name;
        float timeLeft = 0.f;
        bool showTimer = true;
    };

    struct Slot {
        cocos2d::CCNode* node = nullptr;
        cocos2d::CCLabelBMFont* label = nullptr;
        bool active = false;
    };

    void layout();
    void refreshText();

    cocos2d::CCNode* m_root = nullptr;
    std::deque<Line> m_lines;
    std::vector<Slot> m_slots;
};

} // namespace chaosui

#endif
