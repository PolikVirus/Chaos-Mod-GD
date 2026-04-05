#include "Event.hpp"

#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>

using namespace geode::prelude;

namespace chaosmod {

static constexpr char const* evId   = "doggie-jumpscare";
static constexpr char const* evName = "Doggie Jumpscare";
static constexpr float logDur = -1.f;
static constexpr float showSecs    = 1.0f;
static constexpr float fadeSecs = 1.0f;

static void startDoggie(PlayLayer* pl) {
    if (!pl) return;
    auto win = cocos2d::CCDirector::sharedDirector()->getWinSize();
    cocos2d::CCNode* parent = pl->m_uiLayer ? static_cast<cocos2d::CCNode*>(pl->m_uiLayer)
                                            : static_cast<cocos2d::CCNode*>(pl);
    auto path = (Mod::get()->getResourcesDir() / "doggie.png").string();
    auto spr = cocos2d::CCSprite::create(path.c_str());
    if (!spr) {
        log::warn("[Chaos Mod] Failed to load doggie.png from {}", path);
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
    parent->addChild(spr);
    spr->runAction(cocos2d::CCSequence::create(
        cocos2d::CCDelayTime::create(showSecs),
        cocos2d::CCFadeOut::create(fadeSecs),
        cocos2d::CCRemoveSelf::create(),
        nullptr
    ));
}

void registerDoggieJumpscare(EventRegistry& reg) {
    reg.add(EventDef(
        evId,
        evName,
        logDur,
        &startDoggie
    ));
}

} // namespace chaosmod