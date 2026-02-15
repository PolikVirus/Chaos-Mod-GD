#include "Event.hpp"

#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>

using namespace geode::prelude;

namespace chaosmod {

static constexpr char const* kId   = "soggy-jumpscare";
static constexpr char const* kName = "Soggy Jumpscare";
static constexpr float kLogDuration = -1.f;
static constexpr float kShowSeconds    = 1.0f;
static constexpr float kFadeOutSeconds = 1.0f;

static void startSoggy(PlayLayer* pl) {
    if (!pl) return;
    auto win = cocos2d::CCDirector::sharedDirector()->getWinSize();
    cocos2d::CCNode* parent = pl->m_uiLayer ? static_cast<cocos2d::CCNode*>(pl->m_uiLayer)
                                            : static_cast<cocos2d::CCNode*>(pl);
    auto path = (Mod::get()->getResourcesDir() / "Soggy.jpg").string();
    auto spr = cocos2d::CCSprite::create(path.c_str());
    if (!spr) {
        log::warn("[Chaos Mod] Failed to load Soggy.jpg from {}", path);
        return;
    }
    spr->setAnchorPoint({0.5f, 0.5f});
    spr->setPosition({win.width / 2.f, win.height / 2.f});
    spr->setOpacity(255);
    auto sz = spr->getContentSize();
    if (sz.width > 0.f && sz.height > 0.f) {
        spr->setScaleX(win.width / sz.width);
        spr->setScaleY(win.height / sz.height);
    }
    parent->addChild(spr, 999999);
    spr->runAction(cocos2d::CCSequence::create(
        cocos2d::CCDelayTime::create(kShowSeconds),
        cocos2d::CCFadeOut::create(kFadeOutSeconds),
        cocos2d::CCRemoveSelf::create(),
        nullptr
    ));
}

void registerSoggyJumpscare(EventRegistry& reg) {
    reg.add(EventDef(
        kId,
        kName,
        kLogDuration,
        &startSoggy
    ));
}

} // namespace chaosmod
