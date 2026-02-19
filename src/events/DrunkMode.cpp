#include "Event.hpp"

#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>

#if __has_include(<Geode/binding/PauseLayer.hpp>)
    #include <Geode/binding/PauseLayer.hpp>
    #define CHAOS_HAS_PAUSE_LAYER 1
#else
    #define CHAOS_HAS_PAUSE_LAYER 0
#endif

#include <cmath>
#include <limits>

using namespace geode::prelude;

namespace chaosmod {

static constexpr float kEventDuration = 20.f;
static constexpr int kEffectNodeTag = 0x4452554E; // 'DRUN'
static constexpr int kOverlayNodeTag = 0x44524F56; // 'DROV'
static constexpr int kOverlayZ = 1000000000;

static float clampf(float v, float lo, float hi) {
    return (v < lo) ? lo : (v > hi) ? hi : v;
}

static float lerpf(float a, float b, float t) {
    return a + (b - a) * t;
}

static bool isDescendant(cocos2d::CCNode* root, cocos2d::CCNode* node) {
    if (!root || !node) return false;
    for (auto cur = node; cur; cur = cur->getParent()) {
        if (cur == root) return true;
    }
    return false;
}

#if CHAOS_HAS_PAUSE_LAYER
static bool containsPauseLayer(cocos2d::CCNode* n) {
    if (!n) return false;
    auto children = n->getChildren();
    if (!children) return false;

    for (auto obj : CCArrayExt(children)) {
        if (auto child = typeinfo_cast<cocos2d::CCNode*>(obj)) {
            if (typeinfo_cast<PauseLayer*>(child)) return true;
        }
    }
    return false;
}
#endif

static bool isPausedLike(PlayLayer* pl) {
    if (!pl) return false;

#if CHAOS_HAS_PAUSE_LAYER
    if (pl->m_uiLayer && containsPauseLayer(pl->m_uiLayer)) return true;
    if (containsPauseLayer(pl)) return true;

    if (auto scene = cocos2d::CCDirector::sharedDirector()->getRunningScene()) {
        if (containsPauseLayer(scene)) return true;
    }
#endif

    return !pl->isGameplayActive();
}

static cocos2d::ccColor3B hsv_to_rgb(float h, float s, float v) {
    h = std::fmod(h, 360.f);
    if (h < 0.f) h += 360.f;

    float c = v * s;
    float x = c * (1.f - std::fabs(std::fmod(h / 60.f, 2.f) - 1.f));
    float m = v - c;

    float r = 0.f, g = 0.f, b = 0.f;
    if (h < 60.f)       { r = c; g = x; b = 0.f; }
    else if (h < 120.f) { r = x; g = c; b = 0.f; }
    else if (h < 180.f) { r = 0.f; g = c; b = x; }
    else if (h < 240.f) { r = 0.f; g = x; b = c; }
    else if (h < 300.f) { r = x; g = 0.f; b = c; }
    else                { r = c; g = 0.f; b = x; }

    auto to255 = [](float f) -> unsigned char {
        int vv = static_cast<int>(std::round(f * 255.f));
        if (vv < 0) vv = 0;
        if (vv > 255) vv = 255;
        return static_cast<unsigned char>(vv);
    };

    return cocos2d::ccc3(to255(r + m), to255(g + m), to255(b + m));
}

class DrunkEffectNode : public cocos2d::CCNode {
public:
    PlayLayer* m_playLayer = nullptr;

    float m_duration = kEventDuration;
    float m_timeLeft = kEventDuration;

    float m_elapsed = 0.f;

    float m_noiseTimer = 0.f;
    cocos2d::CCPoint m_noiseTarget = cocos2d::CCPoint(0.f, 0.f);
    cocos2d::CCPoint m_noisePos = cocos2d::CCPoint(0.f, 0.f);

    cocos2d::CCPoint m_lastPositionOffset = cocos2d::CCPoint(0.f, 0.f);
    float m_lastRotationOffset = 0.f;
    float m_lastSkewXOffset = 0.f;
    float m_lastSkewYOffset = 0.f;

    float m_lastScaleXFactor = 1.f;
    float m_lastScaleYFactor = 1.f;

    bool m_finished = false;
    bool m_wasPaused = false;

    static DrunkEffectNode* create(PlayLayer* pl) {
        if (!pl) return nullptr;
        auto ret = new DrunkEffectNode();
        ret->m_playLayer = pl;
        ret->autorelease();
        return ret;
    }

    cocos2d::CCScene* getScene() {
        return cocos2d::CCDirector::sharedDirector()->getRunningScene();
    }

    cocos2d::CCPoint getBasePosition() const {
        auto cur = m_playLayer->getPosition();
        return cocos2d::CCPoint(cur.x - m_lastPositionOffset.x,
                                cur.y - m_lastPositionOffset.y);
    }
    float getBaseRotation() const { return m_playLayer->getRotation() - m_lastRotationOffset; }
    float getBaseSkewX() const { return m_playLayer->getSkewX() - m_lastSkewXOffset; }
    float getBaseSkewY() const { return m_playLayer->getSkewY() - m_lastSkewYOffset; }
    float getBaseScaleX() const {
        float cur = m_playLayer->getScaleX();
        return (m_lastScaleXFactor == 0.f) ? cur : (cur / m_lastScaleXFactor);
    }
    float getBaseScaleY() const {
        float cur = m_playLayer->getScaleY();
        return (m_lastScaleYFactor == 0.f) ? cur : (cur / m_lastScaleYFactor);
    }

    void resetToBase() {
        auto p = getBasePosition();
        float r = getBaseRotation();
        float sx = getBaseScaleX();
        float sy = getBaseScaleY();
        float skx = getBaseSkewX();
        float sky = getBaseSkewY();

        m_playLayer->setPosition(p);
        m_playLayer->setRotation(r);
        m_playLayer->setScaleX(sx);
        m_playLayer->setScaleY(sy);
        m_playLayer->setSkewX(skx);
        m_playLayer->setSkewY(sky);

        m_lastPositionOffset = cocos2d::CCPoint(0.f, 0.f);
        m_lastRotationOffset = 0.f;
        m_lastSkewXOffset = 0.f;
        m_lastSkewYOffset = 0.f;
        m_lastScaleXFactor = 1.f;
        m_lastScaleYFactor = 1.f;
    }

    cocos2d::CCLayerColor* overlay() {
        if (m_finished) return nullptr;

        auto sc = getScene();
        if (!sc) return nullptr;

        auto ov = typeinfo_cast<cocos2d::CCLayerColor*>(sc->getChildByTag(kOverlayNodeTag));
        if (!ov) {
            ov = cocos2d::CCLayerColor::create(cocos2d::ccc4(255, 255, 255, 120));
            if (!ov) return nullptr;

            ov->setTag(kOverlayNodeTag);
            ov->setAnchorPoint({0.f, 0.f});
            ov->setPosition({0.f, 0.f});
            ov->setBlendFunc(ccBlendFunc{GL_SRC_ALPHA, GL_ONE});

            sc->addChild(ov, kOverlayZ);
        }

        auto ws = cocos2d::CCDirector::sharedDirector()->getWinSize();
        ov->setContentSize(ws);
        ov->setZOrder(kOverlayZ);
        return ov;
    }

    void updateNoise(float dt) {
        m_noiseTimer -= dt;
        if (m_noiseTimer <= 0.f) {
            m_noiseTimer = 0.55f + CCRANDOM_0_1() * 0.25f;
            float nx = CCRANDOM_MINUS1_1();
            float ny = CCRANDOM_MINUS1_1();
            float amp = 14.f;
            m_noiseTarget = cocos2d::CCPoint(nx * amp, ny * amp);
        }

        float follow = clampf(dt * 3.8f, 0.f, 1.f);
        m_noisePos = cocos2d::CCPoint(
            lerpf(m_noisePos.x, m_noiseTarget.x, follow),
            lerpf(m_noisePos.y, m_noiseTarget.y, follow)
        );
    }

    void updateTransform(float dt) {
        (void)dt;

        float posAmp = 20.f;
        float rotAmp = 5.5f;
        float skewAmp = 7.0f;
        float zoomAmp = 0.085f;

        float s1 = std::sinf(m_elapsed * 1.15f);
        float s2 = std::sinf(m_elapsed * 0.47f + 1.7f);
        float s3 = std::sinf(m_elapsed * 2.05f + 0.2f);

        float c1 = std::cosf(m_elapsed * 0.92f);
        float c2 = std::cosf(m_elapsed * 1.62f + 0.4f);

        float ox = (s1 * 0.55f + s2 * 0.25f + s3 * 0.20f) * posAmp;
        float oy = (c1 * 0.55f + c2 * 0.45f) * posAmp;

        ox += m_noisePos.x;
        oy += m_noisePos.y;

        float rot =
            (std::sinf(m_elapsed * 0.70f) * 0.55f +
             std::sinf(m_elapsed * 1.55f + 0.6f) * 0.30f +
             std::sinf(m_elapsed * 2.40f + 1.2f) * 0.15f) * rotAmp;

        float skx = (std::sinf(m_elapsed * 0.88f + 0.9f) * 0.6f +
                     std::sinf(m_elapsed * 1.90f) * 0.4f) * skewAmp;
        float sky = (std::cosf(m_elapsed * 0.73f + 1.3f) * 0.6f +
                     std::cosf(m_elapsed * 2.10f) * 0.4f) * skewAmp;

        float zoom = 1.f + (std::sinf(m_elapsed * 0.62f) * 0.65f +
                             std::sinf(m_elapsed * 1.85f + 0.7f) * 0.35f) * zoomAmp;

        float stretchX = 1.f + std::sinf(m_elapsed * 1.10f + 0.2f) * 0.045f;
        float stretchY = 1.f + std::cosf(m_elapsed * 0.95f + 1.1f) * 0.040f;

        float mulX = zoom * stretchX;
        float mulY = zoom * stretchY;

        auto bp = getBasePosition();
        float br = getBaseRotation();
        float bskx = getBaseSkewX();
        float bsky = getBaseSkewY();
        float bsx = getBaseScaleX();
        float bsy = getBaseScaleY();

        m_playLayer->setPosition({bp.x + ox, bp.y + oy});
        m_playLayer->setRotation(br + rot);
        m_playLayer->setSkewX(bskx + skx);
        m_playLayer->setSkewY(bsky + sky);
        m_playLayer->setScaleX(bsx * mulX);
        m_playLayer->setScaleY(bsy * mulY);

        m_lastPositionOffset = cocos2d::CCPoint(ox, oy);
        m_lastRotationOffset = rot;
        m_lastSkewXOffset = skx;
        m_lastSkewYOffset = sky;
        m_lastScaleXFactor = mulX;
        m_lastScaleYFactor = mulY;
    }

    void updateOverlay(cocos2d::CCLayerColor* ov) {
        if (!ov) return;
        float hue = std::fmod(m_elapsed * 160.f, 360.f);
        auto col = hsv_to_rgb(hue, 1.0f, 1.0f);
        float pulse = 0.5f + 0.5f * std::sinf(m_elapsed * 1.6f);
        unsigned char alpha = static_cast<unsigned char>(90 + pulse * 110);
        ov->setColor(col);
        ov->setOpacity(alpha);
    }

    void start(float seconds) {
        if (!m_playLayer) return;
        m_duration = seconds;
        m_timeLeft = seconds;
        m_elapsed = 0.f;
        m_noiseTimer = 0.f;
        m_noiseTarget = cocos2d::CCPoint(0.f, 0.f);
        m_noisePos = cocos2d::CCPoint(0.f, 0.f);
        m_lastPositionOffset = cocos2d::CCPoint(0.f, 0.f);
        m_lastRotationOffset = 0.f;
        m_lastSkewXOffset = 0.f;
        m_lastSkewYOffset = 0.f;
        m_lastScaleXFactor = 1.f;
        m_lastScaleYFactor = 1.f;
        (void)overlay();
        this->scheduleUpdate();
    }

    void update(float dt) override {
        if (m_finished) {
            this->unscheduleUpdate();
            return;
        }
        auto sc = getScene();
        if (!sc || !m_playLayer) {
            finishAndCleanup();
            return;
        }
        if (!isDescendant(sc, m_playLayer)) {
            finishAndCleanup();
            return;
        }
        auto ov = overlay();
        bool paused = isPausedLike(m_playLayer);
        if (paused) {
            if (!m_wasPaused) {
                resetToBase();
                m_wasPaused = true;
            }
            updateOverlay(ov);
            return;
        }
        if (m_wasPaused) {
            resetToBase();
            m_wasPaused = false;
        }
        if (dt < 0.f) dt = 0.f;
        if (dt > 0.05f) dt = 0.05f;
        m_timeLeft -= dt;
        if (m_timeLeft <= 0.f) {
            finishAndCleanup();
            return;
        }
        m_elapsed += dt;
        updateNoise(dt);
        updateTransform(dt);
        updateOverlay(ov);
    }

    void finishEffect() {
        if (m_finished) return;
        m_finished = true;
        this->unscheduleUpdate();
        if (m_playLayer) resetToBase();
        if (auto sc = getScene()) {
            if (auto ov = sc->getChildByTag(kOverlayNodeTag)) {
                ov->removeFromParentAndCleanup(true);
            }
        }
    }

    void finishAndCleanup() {
        finishEffect();
        this->removeFromParentAndCleanup(true);
    }

    void onExit() override {
        finishEffect();
        cocos2d::CCNode::onExit();
    }
};

static void startDrunk(PlayLayer* pl, float seconds) {
    if (!pl) return;
    auto sc = cocos2d::CCDirector::sharedDirector()->getRunningScene();
    if (!sc) return;
    if (auto existing = sc->getChildByTag(kEffectNodeTag)) {
        if (auto fx = typeinfo_cast<DrunkEffectNode*>(existing)) {
            fx->m_playLayer = pl;
            fx->start(seconds);
            return;
        }
        existing->removeFromParentAndCleanup(true);
    }
    auto fx = DrunkEffectNode::create(pl);
    if (!fx) return;
    fx->setTag(kEffectNodeTag);
    sc->addChild(fx, std::numeric_limits<int>::max());
    fx->start(seconds);
}

void registerDrunkMode(EventRegistry& reg) {
    reg.add(EventDef(
        "drunk-mode",
        "Drunk Mode",
        kEventDuration,
        [](PlayLayer* pl) {
            startDrunk(pl, kEventDuration);
        }
    ));
}

} // namespace chaosmod
