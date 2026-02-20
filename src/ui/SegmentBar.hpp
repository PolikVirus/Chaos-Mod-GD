#pragma once
#include <Geode/Geode.hpp>
#include <vector>

namespace chaosui {

struct SegmentBarWidget {
    cocos2d::CCNode* root = nullptr;

    cocos2d::CCSprite* groove = nullptr;

    // fill container (left-anchored at x = -innerW/2)
    cocos2d::CCNode* fillNode = nullptr;

    // segmented fill sprites (preferred)
    std::vector<cocos2d::CCSprite*> segments;

    // fallback solid fill (used if sliderBar.png is missing / unusable)
    cocos2d::CCLayerColor* solidFill = nullptr;

    float width = 0.f;   // groove width (scaled)
    float fillW = 0.f;   // inner width (scaled)
    float fillH = 0.f;   // inner height (scaled)
    float segmentWidth  = 0.f;   // segment spacing (scaled)

    cocos2d::CCRect segFullRect{};
    float fullSegmentWidth = 0.f;

    bool valid() const;
    void destroy();

    void setVisible(bool v);
    bool isVisible() const;

    void setProgress(float t01);
};

cocos2d::CCSprite* loadFileSprite(char const* base);

SegmentBarWidget createSegmentBar(
    cocos2d::CCNode* parent,
    float scale,
    float insetPx,
    int zFill,
    int zFrame
);

} // namespace chaosui
