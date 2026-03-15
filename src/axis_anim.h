#pragma once
#include <math.h>

// ─────────────────────────────────────────
//  缓动函数库
//  所有函数输入 t ∈ [0.0, 1.0]，输出 [0.0, 1.0]
// ─────────────────────────────────────────
class AxisAnim {
public:
    static float linear(float t) {
        return t;
    }

    static float easeOutCubic(float t) {
        return 1.0f - powf(1.0f - t, 3.0f);
    }

    static float easeInOutQuad(float t) {
        return t < 0.5f ? 2*t*t : 1.0f - powf(-2*t+2, 2)/2.0f;
    }

    static float easeOutElastic(float t) {
        if (t <= 0.0f || t >= 1.0f) return t;
        return powf(2.0f, -10.0f*t) * sinf((t*10.0f - 0.75f) * (2.0f*M_PI/3.0f)) + 1.0f;
    }

    static float easeOutBack(float t) {
        const float c1 = 1.70158f;
        const float c3 = c1 + 1.0f;
        return 1.0f + c3 * powf(t - 1.0f, 3.0f) + c1 * powf(t - 1.0f, 2.0f);
    }

    static float easeOutBounce(float t) {
        const float n1 = 7.5625f, d1 = 2.75f;
        if (t < 1.0f/d1)      return n1*t*t;
        else if (t < 2.0f/d1) { t -= 1.5f/d1;   return n1*t*t + 0.75f; }
        else if (t < 2.5f/d1) { t -= 2.25f/d1;  return n1*t*t + 0.9375f; }
        else                  { t -= 2.625f/d1;  return n1*t*t + 0.984375f; }
    }

    // RGB565 颜色线性插值
    static uint16_t lerpColor(uint16_t a, uint16_t b, float t) {
        if (t <= 0.0f) return a;
        if (t >= 1.0f) return b;
        uint8_t r1=(a>>11)&0x1F, g1=(a>>5)&0x3F, b1=a&0x1F;
        uint8_t r2=(b>>11)&0x1F, g2=(b>>5)&0x3F, b2=b&0x1F;
        return ((uint16_t)(r1+(r2-r1)*t) << 11) |
               ((uint16_t)(g1+(g2-g1)*t) << 5)  |
               ((uint16_t)(b1+(b2-b1)*t));
    }

    // 平滑跟随（用于平滑进度条、平滑光标等）
    // speed: 0.0~1.0，越大越快
    static float smoothFollow(float current, float target, float speed) {
        return current + (target - current) * speed;
    }
};
