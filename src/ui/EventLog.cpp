#include "EventLog.hpp"

#include <Geode/Geode.hpp>
#include <fmt/format.h>
#include <algorithm>

using namespace geode::prelude;

namespace chaosui {

static constexpr int   kMaxLines = 4;

static constexpr float kRightPad = 10.f;
static constexpr float kTopPad   = 80.f;
static constexpr float kLineGap  = 22.f;

static constexpr float kTextScale = 0.33f;

static constexpr float kDefaultLifetime = 10.f;

void EventLog::init(cocos2d::CCNode* parent) {
    if (m_root) m_root->removeFromParent();

    m_root = cocos2d::CCNode::create();
    parent->addChild(m_root, 20001);

    m_lines.clear();
    m_slots.clear();
    m_slots.resize(kMaxLines);

    for (int i = 0; i < kMaxLines; i++) {
        auto& s = m_slots[i];

        s.node = cocos2d::CCNode::create();
        m_root->addChild(s.node, 1);

        s.label = cocos2d::CCLabelBMFont::create("", "bigFont.fnt");
        if (s.label) {
            s.label->setScale(kTextScale);
            s.label->setOpacity(230);
            s.label->setAnchorPoint({1.f, 0.5f}); // right aligned
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
        // legacy behavior: default 10s with countdown
        line.timeLeft = kDefaultLifetime;
        line.showTimer = true;
    }
    else {
        // NEW: no countdown shown, but still expires after default 10s
        line.timeLeft = kDefaultLifetime;
        line.showTimer = false;
    }

    // newest first
    m_lines.push_front(std::move(line));
    while ((int)m_lines.size() > kMaxLines) {
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

    // remove expired
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

    for (int i = 0; i < kMaxLines; i++) {
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
    float rightX = win.width - kRightPad;
    float topY   = win.height - kTopPad;

    for (int i = 0; i < kMaxLines; i++) {
        auto& slot = m_slots[i];
        if (!slot.node) continue;
        slot.node->setPosition({rightX, topY - i * kLineGap});
    }
}

} // namespace chaosui
