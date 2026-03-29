#pragma once
#include <Geode/Geode.hpp>
#include <vector>

namespace chaosui {

struct SegmentBarWidget {
    cocos2d::CCNode* root = nullptr;
    cocos2d::CCSprite* groove = nullptr;
    cocos2d::CCNode* fillNode = nullptr;
    std::vector<cocos2d::CCSprite*> segments;

    cocos2d::CCLayerColor* solidFill = nullptr;

    float width = 0.f;
    float fillW = 0.f;
    float fillH = 0.f;
    float segmentWidth  = 0.f;

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
