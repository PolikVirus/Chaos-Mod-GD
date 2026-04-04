#include "SegmentBar.hpp"
#include <cmath>
#include <algorithm>

using namespace geode::prelude;

namespace chaosui {

bool SegmentBarWidget::valid() const {
    return root && groove && fillNode && (solidFill || !segments.empty());
}

void SegmentBarWidget::destroy() {
    if (root) root->removeFromParentAndCleanup(true);
    root = nullptr;
    groove = nullptr;
    fillNode = nullptr;
    segments.clear();
    solidFill = nullptr;
    width = fillW = fillH = segmentWidth = fullSegmentWidth = 0.f;
}

void SegmentBarWidget::setVisible(bool v) {
    if (root) root->setVisible(v);
}

bool SegmentBarWidget::isVisible() const {
    return root && root->isVisible();
}

void SegmentBarWidget::setProgress(float progress) {
    if (!valid()) return;

    progress = std::clamp(progress, 0.f, 1.f);

    if (solidFill) {
        float w = fillW * progress;
        if (w < 0.f) w = 0.f;
        solidFill->setContentSize({ w, fillH });
        return;
    }

    float fillLength = fillW * progress;

    int fullSegments = (segmentWidth > 0.f) ? (int)std::floor(fillLength / segmentWidth) : 0;
    float remainingLength = (segmentWidth > 0.f) ? (fillLength - fullSegments * segmentWidth) : 0.f;

    for (int i = 0; i < (int)segments.size(); i++) {
        auto seg = segments[i];
        if (!seg) continue;

        if (i < fullSegments) {
            seg->setVisible(true);
            seg->setTextureRect(segFullRect);
        }
        else if (i == fullSegments && remainingLength > 0.001f) {
            seg->setVisible(true);

            float ratio = remainingLength / segmentWidth;
            float newW  = fullSegmentWidth * ratio;
            newW = std::clamp(newW, 0.1f, fullSegmentWidth);

            auto r = segFullRect;
            r.size.width = newW;
            seg->setTextureRect(r);
        }
        else {
            seg->setVisible(false);
        }
    }
}

cocos2d::CCSprite* loadFileSprite(char const* base) {
    if (!base) return nullptr;

    if (auto s = cocos2d::CCSprite::create(base)) return s;

    std::string hd(base), uhd(base);
    auto dot = hd.rfind(".png");
    if (dot != std::string::npos) {
        hd.insert(dot, "-hd");
        uhd.insert(dot, "-uhd");
    } else {

        hd += "-hd.png";

        uhd += "-uhd.png";
    }

    if (auto s = cocos2d::CCSprite::create(hd.c_str())) return s;
    if (auto s = cocos2d::CCSprite::create(uhd.c_str())) return s;

    auto cache = cocos2d::CCSpriteFrameCache::sharedSpriteFrameCache();
    if (auto fr = cache->spriteFrameByName(base)) return cocos2d::CCSprite::createWithSpriteFrame(fr);
    if (auto fr = cache->spriteFrameByName(hd.c_str())) return cocos2d::CCSprite::createWithSpriteFrame(fr);
    if (auto fr = cache->spriteFrameByName(uhd.c_str())) return cocos2d::CCSprite::createWithSpriteFrame(fr);

    return nullptr;
}

static void useSolidFill(SegmentBarWidget& out) {
    auto col = cocos2d::ccc4(255, 220, 0, 255);

    out.solidFill = cocos2d::CCLayerColor::create(col, out.fillW, out.fillH);
    if (!out.solidFill) return;
    out.solidFill->setPosition({0.f, -out.fillH / 2.f});
    out.fillNode->addChild(out.solidFill, 0);

    geode::log::warn("[Chaos Mod] Using solid fallback fill (no clipping, always visible)");
}

SegmentBarWidget createSegmentBar(cocos2d::CCNode* parent, float scale, float insetPx, int zFill, int zFrame) {
    SegmentBarWidget out;

    out.root = cocos2d::CCNode::create();
    parent->addChild(out.root);

    out.groove = loadFileSprite("slidergroove.png");
    if (!out.groove) {
        geode::log::warn("[Chaos Mod] slidergroove.png missing -> bar frame may not show");
        return out;
    }

    out.groove->setAnchorPoint({0.5f, 0.5f});
    out.groove->setScale(scale);
    out.groove->setPosition({0.f, 0.f});

    float grooveW = out.groove->getContentSize().width * scale;
    float grooveH = out.groove->getContentSize().height * scale;
    out.width = grooveW;

    float inset = insetPx * scale;
    out.fillW = std::max(10.f, grooveW - inset);
    out.fillH = std::max(6.f,  grooveH - inset);

    out.fillNode = cocos2d::CCNode::create();
    out.fillNode->setPosition({-out.fillW / 2.f, 0.f});
    out.root->addChild(out.fillNode, zFill);

    out.root->addChild(out.groove, zFrame);

    auto segProto = loadFileSprite("sliderBar.png");
    if (!segProto || !segProto->getTexture() ||
        segProto->getContentSize().width <= 1.f ||
        segProto->getContentSize().height <= 1.f) {
        geode::log::warn("[Chaos Mod] sliderBar.png missing/unusable -> fallback");
        useSolidFill(out);
        out.setProgress(1.f);
        return out;
    }

    out.segFullRect = segProto->getTextureRect();
    out.fullSegmentWidth = out.segFullRect.size.width;
    out.segmentWidth = segProto->getContentSize().width * scale;

    int count = (out.segmentWidth > 0.f) ? (int)std::ceil(out.fillW / out.segmentWidth) + 2 : 0;
    out.segments.reserve(std::max(0, count));

    for (int i = 0; i < count; i++) {
        auto seg = loadFileSprite("sliderBar.png");
        if (!seg) continue;

        seg->setAnchorPoint({0.f, 0.5f});
        seg->setScale(scale);
        seg->setPosition({i * out.segmentWidth, 0.f});
        out.fillNode->addChild(seg, 0);

        out.segments.push_back(seg);
    }

    if (out.segments.empty()) {
        geode::log::warn("[Chaos Mod] sliderBar.png loaded but segments creation failed -> fallback");
        useSolidFill(out);
    }

    out.setProgress(1.f);
    return out;
}

} // namespace chaosui
