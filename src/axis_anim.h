/**
 * AXIS UI Framework — Animation & Color Utilities
 * https://github.com/EVGA2048/AXIS-UI
 * MIT License
 */

 #pragma once
 #include <math.h>
 #include <stdint.h>
 
 // ═════════════════════════════════════════════════
 //  AxisAnimValue — 自动平滑插值数值
 //  用法：
 //    AxisAnimValue vol(0.0f);
 //    vol.setTarget(0.75f);
 //    vol.update();          // 每帧调用
 //    float v = vol.value(); // 读取当前平滑值
 // ═════════════════════════════════════════════════
 class AxisAnimValue {
 public:
     AxisAnimValue(float init = 0.0f, float speed = 0.12f)
         : _val(init), _target(init), _speed(speed) {}
 
     void  setTarget(float t)    { _target = t; }
     void  setSpeed(float s)     { _speed = s; }
     void  snap(float v)         { _val = _target = v; }
     float value()   const       { return _val; }
     float target()  const       { return _target; }
     bool  settled() const       { return fabsf(_val - _target) < 0.001f; }
 
     void update() {
         _val += (_target - _val) * _speed;
         if (settled()) _val = _target;
     }
 
 private:
     float _val, _target, _speed;
 };
 
 // ═════════════════════════════════════════════════
 //  缓动函数（输入输出均为 [0,1]）
 // ═════════════════════════════════════════════════
 class AxisAnim {
 public:
 
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
 
     // 平滑跟随
     static float smoothFollow(float current, float target, float speed) {
         return current + (target - current) * speed;
     }
 
     // ── RGB565 工具 ───────────────────────────────
 
     // 颜色线性插值
     static uint16_t lerpColor(uint16_t a, uint16_t b, float t) {
         if (t <= 0.0f) return a;
         if (t >= 1.0f) return b;
         int r1=(a>>11)&0x1F, g1=(a>>5)&0x3F, b1=a&0x1F;
         int r2=(b>>11)&0x1F, g2=(b>>5)&0x3F, b2=b&0x1F;
         return (uint16_t)(r1+(r2-r1)*t)<<11 |
                (uint16_t)(g1+(g2-g1)*t)<<5  |
                (uint16_t)(b1+(b2-b1)*t);
     }
 
     // RGB888 → RGB565
     static uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) {
         return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
     }
 
     // RGB565 亮度缩放（0.0-1.0）
     static uint16_t dimColor(uint16_t c, float brightness) {
         brightness = constrain(brightness, 0.0f, 1.0f);
         uint8_t r = ((c >> 11) & 0x1F) * brightness;
         uint8_t g = ((c >>  5) & 0x3F) * brightness;
         uint8_t b = ( c        & 0x1F) * brightness;
         return (r << 11) | (g << 5) | b;
     }
 };
 
 // ═════════════════════════════════════════════════
 //  AxisTimer — 非阻塞计时器
 //  用法：
 //    AxisTimer t(3000);   // 3秒
 //    t.reset();
 //    if (t.expired()) { ... }
 // ═════════════════════════════════════════════════
 class AxisTimer {
 public:
     AxisTimer(uint32_t intervalMs = 1000)
         : _interval(intervalMs), _start(0), _running(false) {}
 
     void     reset()    { _start = millis(); _running = true; }
     void     stop()     { _running = false; }
     bool     running()  const { return _running; }
 
     bool expired() {
         if (!_running) return false;
         if (millis() - _start >= _interval) {
             _running = false;
             return true;
         }
         return false;
     }
 
     // 周期性触发（自动重置）
     bool tick() {
         if (!_running) { reset(); return false; }
         if (millis() - _start >= _interval) {
             _start = millis();
             return true;
         }
         return false;
     }
 
     uint32_t elapsed() const { return millis() - _start; }
     float    progress() const {
         return constrain((float)(millis()-_start)/_interval, 0.0f, 1.0f);
     }
 
 private:
     uint32_t _interval, _start;
     bool     _running;
 };