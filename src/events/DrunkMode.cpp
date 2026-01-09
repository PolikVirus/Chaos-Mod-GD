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

static constexpr float kDurationSeconds = 20.f;
static constexpr int kDrunkControllerTag = 0x4452554E; // 'DRUN'
static constexpr int kDrunkOverlayTag    = 0x44524F56; // 'DROV'

// Big Z that isn't INT_MAX (avoid edge cases with other mods using INT_MAX)
static constexpr int kOverlayZ = 1000000000;

static bool isDescendantOf(cocos2d::CCNode* root, cocos2d::CCNode* node) {
    if (!root || !node) return false;
    for (auto cur = node; cur; cur = cur->getParent()) {
        if (cur == root) return true;
    }
    return false;
}

#if CHAOS_HAS_PAUSE_LAYER
static bool hasPauseLayerIn(cocos2d::CCNode* n) {
    if (!n) return false;
    auto children = n->getChildren();
    if (!children) return false;

    cocos2d::CCObject* obj = nullptr;
    CCARRAY_FOREACH(children, obj) {
        if (auto child = typeinfo_cast<cocos2d::CCNode*>(obj)) {
            if (typeinfo_cast<PauseLayer*>(child)) return true;
        }
    }
    return false;
}
#endif

static bool isPauseMenuOpen(PlayLayer* pl) {
    if (!pl) return false;

#if CHAOS_HAS_PAUSE_LAYER
    if (pl->m_uiLayer && hasPauseLayerIn(pl->m_uiLayer)) return true;
    if (hasPauseLayerIn(pl)) return true;

    if (auto scene = cocos2d::CCDirector::sharedDirector()->getRunningScene()) {
        if (hasPauseLayerIn(scene)) return true;
    }
#endif

    return !pl->isGameplayActive();
}

static cocos2d::ccColor3B hsvToRgb(float h, float s, float v) {
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

static float clampf(float v, float lo, float hi) {
    return (v < lo) ? lo : (v > hi) ? hi : v;
}

static float lerpf(float a, float b, float t) {
    return a + (b - a) * t;
}

class DrunkModeController : public cocos2d::CCNode {
public:
    PlayLayer* m_pl = nullptr;

    float m_total = kDurationSeconds;
    float m_remaining = kDurationSeconds;

    float m_t = 0.f;

    // Low-frequency "drunk lurch" noise (smooth random targets)
    float m_noiseTimer = 0.f;
    cocos2d::CCPoint m_noiseTarget = cocos2d::CCPoint(0.f, 0.f);
    cocos2d::CCPoint m_noiseCurrent = cocos2d::CCPoint(0.f, 0.f);

    // Track last applied deltas so we can restore cleanly (non-drifting)
    cocos2d::CCPoint m_lastPosOffset = cocos2d::CCPoint(0.f, 0.f);
    float m_lastRotOffset = 0.f;
    float m_lastSkewXOffset = 0.f;
    float m_lastSkewYOffset = 0.f;

    float m_lastScaleXMult = 1.f;
    float m_lastScaleYMult = 1.f;

    bool m_restored = false;
    bool m_wasPaused = false;

    static DrunkModeController* create(PlayLayer* pl) {
        if (!pl) return nullptr;
        auto ret = new DrunkModeController();
        ret->m_pl = pl;
        ret->autorelease();
        return ret;
    }

    cocos2d::CCScene* getScene() {
        return cocos2d::CCDirector::sharedDirector()->getRunningScene();
    }

    cocos2d::CCPoint getBasePos() const {
        auto cur = m_pl->getPosition();
        return cocos2d::CCPoint(cur.x - m_lastPosOffset.x, cur.y - m_lastPosOffset.y);
    }
    float getBaseRot() const {
        return m_pl->getRotation() - m_lastRotOffset;
    }
    float getBaseSkewX() const {
        return m_pl->getSkewX() - m_lastSkewXOffset;
    }
    float getBaseSkewY() const {
        return m_pl->getSkewY() - m_lastSkewYOffset;
    }
    float getBaseScaleX() const {
        float cur = m_pl->getScaleX();
        if (m_lastScaleXMult == 0.f) return cur;
        return cur / m_lastScaleXMult;
    }
    float getBaseScaleY() const {
        float cur = m_pl->getScaleY();
        if (m_lastScaleYMult == 0.f) return cur;
        return cur / m_lastScaleYMult;
    }

    void resetToBase() {
        auto bp = getBasePos();
        float br = getBaseRot();
        float bsx = getBaseScaleX();
        float bsy = getBaseScaleY();
        float bskx = getBaseSkewX();
        float bsky = getBaseSkewY();

        m_pl->setPosition(bp);
        m_pl->setRotation(br);
        m_pl->setScaleX(bsx);
        m_pl->setScaleY(bsy);
        m_pl->setSkewX(bskx);
        m_pl->setSkewY(bsky);

        m_lastPosOffset = cocos2d::CCPoint(0.f, 0.f);
        m_lastRotOffset = 0.f;
        m_lastSkewXOffset = 0.f;
        m_lastSkewYOffset = 0.f;
        m_lastScaleXMult = 1.f;
        m_lastScaleYMult = 1.f;
    }

    cocos2d::CCLayerColor* ensureOverlay() {
        if (m_restored) return nullptr;

        auto scene = getScene();
        if (!scene) return nullptr;

        auto overlay = typeinfo_cast<cocos2d::CCLayerColor*>(scene->getChildByTag(kDrunkOverlayTag));
        if (!overlay) {
            // Stronger wash than before
            overlay = cocos2d::CCLayerColor::create(cocos2d::ccc4(255, 255, 255, 120));
            if (!overlay) return nullptr;

            overlay->setTag(kDrunkOverlayTag);
            overlay->setAnchorPoint(cocos2d::CCPoint(0.f, 0.f));
            overlay->setPosition(cocos2d::CCPoint(0.f, 0.f));

            // Additive "trippy" wash
            overlay->setBlendFunc(ccBlendFunc{GL_SRC_ALPHA, GL_ONE});

            scene->addChild(overlay, kOverlayZ);
        }

        auto ws = cocos2d::CCDirector::sharedDirector()->getWinSize();
        overlay->setContentSize(ws);
        overlay->setZOrder(kOverlayZ);

        return overlay;
    }

    void stepNoise(float dt) {
        // Every ~0.55s pick a new random target lurch
        m_noiseTimer -= dt;
        if (m_noiseTimer <= 0.f) {
            m_noiseTimer = 0.55f + CCRANDOM_0_1() * 0.25f; // 0.55..0.80

            float nx = CCRANDOM_MINUS1_1();
            float ny = CCRANDOM_MINUS1_1();

            // Lurch scale (px). This is the "RDR2-ish" unpredictable sway.
            float lurchAmp = 14.f;
            m_noiseTarget = cocos2d::CCPoint(nx * lurchAmp, ny * lurchAmp);
        }

        // Smoothly approach target (critical damping-ish)
        float follow = clampf(dt * 3.8f, 0.f, 1.f);
        m_noiseCurrent = cocos2d::CCPoint(
            lerpf(m_noiseCurrent.x, m_noiseTarget.x, follow),
            lerpf(m_noiseCurrent.y, m_noiseTarget.y, follow)
        );
    }

    void applyWobble(float dt) {
        // Stronger, messier, more "drunk"
        float posAmp = 20.f;     // px (was 10)
        float rotAmp = 5.5f;     // degrees (was ~2.2)
        float skewAmp = 7.0f;    // degrees of skew
        float zoomAmp = 0.085f;  // 8.5% zoom wobble

        // Multiple sine layers
        float s1 = std::sinf(m_t * 1.15f);
        float s2 = std::sinf(m_t * 0.47f + 1.7f);
        float s3 = std::sinf(m_t * 2.05f + 0.2f);

        float c1 = std::cosf(m_t * 0.92f);
        float c2 = std::cosf(m_t * 1.62f + 0.4f);

        // Base smooth wobble
        float ox = (s1 * 0.55f + s2 * 0.25f + s3 * 0.20f) * posAmp;
        float oy = (c1 * 0.55f + c2 * 0.45f) * posAmp;

        // Add lurch noise
        ox += m_noiseCurrent.x;
        oy += m_noiseCurrent.y;

        // Rotation: slow roll + faster wobble
        float r =
            (std::sinf(m_t * 0.70f) * 0.55f +
             std::sinf(m_t * 1.55f + 0.6f) * 0.30f +
             std::sinf(m_t * 2.40f + 1.2f) * 0.15f) * rotAmp;

        // Skew: makes it feel "warped"
        float skx = (std::sinf(m_t * 0.88f + 0.9f) * 0.6f + std::sinf(m_t * 1.90f) * 0.4f) * skewAmp;
        float sky = (std::cosf(m_t * 0.73f + 1.3f) * 0.6f + std::cosf(m_t * 2.10f) * 0.4f) * skewAmp;

        // Zoom + stretch: anisotropic scale like drunk lens breathing
        float zoom = 1.f + (std::sinf(m_t * 0.62f) * 0.65f + std::sinf(m_t * 1.85f + 0.7f) * 0.35f) * zoomAmp;

        float stretchX = 1.f + std::sinf(m_t * 1.10f + 0.2f) * 0.045f; // +/- 4.5%
        float stretchY = 1.f + std::cosf(m_t * 0.95f + 1.1f) * 0.040f; // +/- 4.0%

        float sxm = zoom * stretchX;
        float sym = zoom * stretchY;

        // Apply relative to base transforms
        auto bp = getBasePos();
        float br = getBaseRot();
        float bskx = getBaseSkewX();
        float bsky = getBaseSkewY();
        float bsx = getBaseScaleX();
        float bsy = getBaseScaleY();

        m_pl->setPosition(cocos2d::CCPoint(bp.x + ox, bp.y + oy));
        m_pl->setRotation(br + r);
        m_pl->setSkewX(bskx + skx);
        m_pl->setSkewY(bsky + sky);
        m_pl->setScaleX(bsx * sxm);
        m_pl->setScaleY(bsy * sym);

        m_lastPosOffset = cocos2d::CCPoint(ox, oy);
        m_lastRotOffset = r;
        m_lastSkewXOffset = skx;
        m_lastSkewYOffset = sky;
        m_lastScaleXMult = sxm;
        m_lastScaleYMult = sym;
    }

    void applyRainbow(cocos2d::CCLayerColor* overlay) {
        if (!overlay) return;

        // Faster, punchier rainbow
        float hue = std::fmod(m_t * 160.f, 360.f);

        auto col = hsvToRgb(hue, 1.0f, 1.0f);

        // More pulsing opacity
        float aPulse = 0.5f + 0.5f * std::sinf(m_t * 1.6f);
        unsigned char alpha = static_cast<unsigned char>(90 + aPulse * 110); // 90..200

        overlay->setColor(col);
        overlay->setOpacity(alpha);
    }

    void start(float durationSeconds) {
        if (!m_pl) return;

        m_total = durationSeconds;
        m_remaining = durationSeconds;

        m_t = 0.f;
        m_noiseTimer = 0.f;
        m_noiseTarget = cocos2d::CCPoint(0.f, 0.f);
        m_noiseCurrent = cocos2d::CCPoint(0.f, 0.f);

        m_lastPosOffset = cocos2d::CCPoint(0.f, 0.f);
        m_lastRotOffset = 0.f;
        m_lastSkewXOffset = 0.f;
        m_lastSkewYOffset = 0.f;
        m_lastScaleXMult = 1.f;
        m_lastScaleYMult = 1.f;

        (void)ensureOverlay();
        this->scheduleUpdate();
    }

    void update(float dt) override {
        if (m_restored) {
            this->unscheduleUpdate();
            return;
        }

        auto scene = getScene();
        if (!scene || !m_pl) {
            restoreAndRemove();
            return;
        }

        if (!isDescendantOf(scene, m_pl)) {
            restoreAndRemove();
            return;
        }

        auto overlay = ensureOverlay();
        bool paused = isPauseMenuOpen(m_pl);

        if (paused) {
            if (!m_wasPaused) {
                resetToBase();
                m_wasPaused = true;
            }
            // Keep the rainbow wash alive in pause
            applyRainbow(overlay);
            return;
        }

        if (m_wasPaused) {
            resetToBase();
            m_wasPaused = false;
        }

        // Clamp dt so unpause doesn't skip effect
        if (dt < 0.f) dt = 0.f;
        if (dt > 0.05f) dt = 0.05f;

        m_remaining -= dt;
        if (m_remaining <= 0.f) {
            restoreAndRemove();
            return;
        }

        m_t += dt;

        stepNoise(dt);
        applyWobble(dt);
        applyRainbow(overlay);
    }

    void restore() {
        if (m_restored) return;
        m_restored = true;

        this->unscheduleUpdate();

        if (m_pl) {
            resetToBase();
        }

        if (auto scene = getScene()) {
            if (auto ov = scene->getChildByTag(kDrunkOverlayTag)) {
                ov->removeFromParentAndCleanup(true);
            }
        }
    }

    void restoreAndRemove() {
        restore();
        this->removeFromParentAndCleanup(true);
    }

    void onExit() override {
        restore();
        cocos2d::CCNode::onExit();
    }
};

static void applyDrunkMode(PlayLayer* pl, float durationSeconds) {
    if (!pl) return;

    auto scene = cocos2d::CCDirector::sharedDirector()->getRunningScene();
    if (!scene) return;

    if (auto existing = scene->getChildByTag(kDrunkControllerTag)) {
        if (auto ctrl = typeinfo_cast<DrunkModeController*>(existing)) {
            ctrl->m_pl = pl;
            ctrl->start(durationSeconds);
            return;
        }
        existing->removeFromParentAndCleanup(true);
    }

    auto ctrl = DrunkModeController::create(pl);
    if (!ctrl) return;

    ctrl->setTag(kDrunkControllerTag);
    scene->addChild(ctrl, std::numeric_limits<int>::max());
    ctrl->start(durationSeconds);
}

void registerDrunkMode(EventRegistry& reg) {
    reg.add(EventDef(
        "drunk-mode",
        "Drunk Mode",
        kDurationSeconds,
        [](PlayLayer* pl) {
            applyDrunkMode(pl, kDurationSeconds);
        }
    ));
}

} // namespace chaosmod
