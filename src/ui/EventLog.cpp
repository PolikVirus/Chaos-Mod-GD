#include "EventLog.hpp"

#include <Geode/Geode.hpp>
#include <fmt/format.h>
#include <algorithm>

using namespace geode::prelude;

namespace chaosui {

static constexpr int   maxLines = 4;

static constexpr float rightPad = 10.f;
static constexpr float topPad   = 80.f;
static constexpr float lineGap  = 22.f;

static constexpr float textScale = 0.33f;

static constexpr float keepFor = 10.f;

void EventLog::init(cocos2d::CCNode* parent) {
    if (m_root) m_root->removeFromParent();

    m_root = cocos2d::CCNode::create();
    parent->addChild(m_root);

    m_lines.clear();
    m_slots.clear();
    m_slots.resize(maxLines);

    for (int i = 0; i < maxLines; i++) {
        auto& s = m_slots[i];

        s.node = cocos2d::CCNode::create();
        m_root->addChild(s.node, 1);

        s.label = cocos2d::CCLabelBMFont::create("", "bigFont.fnt");
        if (s.label) {
            s.label->setScale(textScale);
            s.label->setOpacity(230);
            s.label->setAnchorPoint({1.f, 0.5f});
            s.node->addChild(s.label, 1);
        }

        s.active = false;
        s.node->setVisible(false);
    }

    layout();
    refreshText();
}

void EventLog::add(std::string const& name, float durationSeconds) {
    if (!m_root) return;

    Line line;
    line.name = name;

    if (durationSeconds > 0.f) {
        line.timeLeft = durationSeconds;
        line.showTimer = true;
    }
    else if (durationSeconds == 0.f) {

        line.timeLeft = keepFor;
        line.showTimer = true;
    }
    else {

        line.timeLeft = keepFor;
        line.showTimer = false;
    }

    m_lines.push_front(std::move(line));
    while ((int)m_lines.size() > maxLines) {
        m_lines.pop_back();
    }

    refreshText();
    layout();
}

void EventLog::update(float dt) {
    if (!m_root) return;

    for (auto& ln : m_lines) {
        ln.timeLeft -= dt;
    }

    bool removed = false;
    for (auto it = m_lines.begin(); it != m_lines.end(); ) {
        if (it->timeLeft <= 0.f) {
            it = m_lines.erase(it);
            removed = true;
        } else {
            ++it;
        }
    }

    refreshText();
    layout();
}

void EventLog::refreshText() {
    int numLines = (int)m_lines.size();

    for (int i = 0; i < maxLines; i++) {
        auto& slot = m_slots[i];

        if (i >= numLines) {
            slot.active = false;
            if (slot.label) slot.label->setString("");
            if (slot.node) slot.node->setVisible(false);
            continue;
        }

        slot.active = true;
        if (slot.node) slot.node->setVisible(true);

        auto const& line = m_lines[i];

        if (!line.showTimer) {
            if (slot.label) slot.label->setString(line.name.c_str());
            continue;
        }

        float timeLeft = std::max(0.f, line.timeLeft);
        auto text = fmt::format("{} {:.1f}s", line.name, timeLeft);
        if (slot.label) slot.label->setString(text.c_str());
    }
}

void EventLog::layout() {
    if (!m_root) return;

    auto win = cocos2d::CCDirector::sharedDirector()->getWinSize();
    float rightX = win.width - rightPad;
    float topY   = win.height - topPad;

    for (int i = 0; i < maxLines; i++) {
        auto& slot = m_slots[i];
        if (!slot.node) continue;
        slot.node->setPosition({rightX, topY - i * lineGap});
    }
}

} // namespace chaosui
