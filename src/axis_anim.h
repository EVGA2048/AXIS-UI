/**
 * AXIS UI Framework — Animation & Color Utilities
 * https://github.com/EVGA2048/AXIS-UI
 * MIT License
 */

#pragma once
#include <math.h>
#include <stdint.h>

class AxisAnim {
public:
    // ── 缓动函数（输入输出均为 [0,1]）──────────────

    static float linear(float t) {
        return t;
    }

    static float easeOutCubic(float t) {
        return 1.0f - powf(1.0f - t, 3.0f);
    }

    static float easeInOutQuad(float t) {
        return t < 0.5f
            ? 2.0f * t * t
            : 1.0f - powf(-2.0f * t + 2.0f, 2.0f) / 2.0f;
    }

    static float easeOutElastic(float t) {
        if (t <= 0.0f || t >= 1.0f) return t;
        return powf(2.0f, -10.0f * t)
             * sinf((t * 10.0f - 0.75f) * (2.0f * M_PI / 3.0f))
             + 1.0f;
    }

    static float easeOutBack(float t) {
        const float c1 = 1.70158f;
        const float c3 = c1 + 1.0f;
        return 1.0f
             + c3 * powf(t - 1.0f, 3.0f)
             + c1 * powf(t - 1.0f, 2.0f);
    }

    static float easeOutQuart(float t) {
        return 1.0f - powf(1.0f - t, 4.0f);
    }

    // ── 平滑跟随（用于进度条、光标等平滑值）─────────
    static float smoothFollow(float current, float target, float speed) {
        return current + (target - current) * speed;
    }

    // ── RGB565 颜色插值 ──────────────────────────
    static uint16_t lerpColor(uint16_t a, uint16_t b, float t) {
        if (t <= 0.0f) return a;
        if (t >= 1.0f) return b;
        int r1 = (a >> 11) & 0x1F;
        int g1 = (a >>  5) & 0x3F;
        int b1 =  a        & 0x1F;
        int r2 = (b >> 11) & 0x1F;
        int g2 = (b >>  5) & 0x3F;
        int b2 =  b        & 0x1F;
        return (uint16_t)(r1 + (r2 - r1) * t) << 11 |
               (uint16_t)(g1 + (g2 - g1) * t) << 5  |
               (uint16_t)(b1 + (b2 - b1) * t);
    }
};
