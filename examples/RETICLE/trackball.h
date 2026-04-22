/**
 * trackball.h — 黑莓轨迹球驱动
 * 光电编码脉冲计数（中断），支持双模式：
 *   NAV  模式 → 累积 delta 超过阈值后触发方向事件（HOME/菜单用）
 *   FREE 模式 → delta 直接移动光标坐标（自由光标屏用）
 *
 * 黑莓轨迹球（Bold 9000 型）电气特性：
 *   - 四路光电管输出（UP/DOWN/LEFT/RIGHT），运动时输出 LOW 脉冲
 *   - 内部上拉，静止高电平，移动产生 ~1ms 脉冲
 *   - 四色 LED（R/G/B/W），共阳极，LOW = 亮
 *   - 按键（CLK）：按下 LOW，释放 HIGH，内部上拉
 */

#pragma once
#include <Arduino.h>

// ── 引脚定义 ──────────────────────────────────
#define TB_UP     2
#define TB_DOWN   3
#define TB_LEFT   4
#define TB_RIGHT  5
#define TB_CLK    6   // 按键

#define TB_LED_R  7   // 红 LED（共阳，LOW=亮）
#define TB_LED_G  8   // 绿
#define TB_LED_B  14  // 蓝
#define TB_LED_W  20  // 白

#define BTN_BACK  21  // 返回键（独立按钮）

// ── ISR 计数器 ─────────────────────────────────
static volatile int16_t  _tb_dx = 0;
static volatile int16_t  _tb_dy = 0;

static void IRAM_ATTR tb_isr_up()    { _tb_dy--; }
static void IRAM_ATTR tb_isr_down()  { _tb_dy++; }
static void IRAM_ATTR tb_isr_left()  { _tb_dx--; }
static void IRAM_ATTR tb_isr_right() { _tb_dx++; }

// ── 轨迹球全局状态 ────────────────────────────
struct TrackballState {
    // 光标位置（FREE 模式，屏幕坐标）
    float    curX = 120.0f;
    float    curY = 160.0f;
    // 本帧原始 delta（每帧读完清零）
    int16_t  rawDx = 0;
    int16_t  rawDy = 0;
    // 按键状态
    bool     clicked  = false;   // 本帧单击
    bool     held     = false;   // 持续按住
    bool     _prevBtn = false;
    uint32_t _pressMs = 0;
    // 导航模式累积量
    int16_t  _navX = 0;
    int16_t  _navY = 0;
    uint32_t _navFireMs = 0;
    // 活跃时间戳（LED 闲置暗化用）
    uint32_t lastActiveMs = 0;
};

TrackballState tb;

// ── LED 控制 ──────────────────────────────────
// 因为是共阳，255 - value → analogWrite 实现亮度
void tbSetLED(uint8_t r, uint8_t g, uint8_t b, uint8_t w = 0) {
    analogWrite(TB_LED_R, 255 - r);
    analogWrite(TB_LED_G, 255 - g);
    analogWrite(TB_LED_B, 255 - b);
    analogWrite(TB_LED_W, 255 - w);
}

// LED 预设颜色
void tbLED_Orange() { tbSetLED(200, 60,  0);   }   // 橙 = 项目主色
void tbLED_Cyan()   { tbSetLED(0,   160, 160); }   // 青 = 自由光标模式
void tbLED_Off()    { tbSetLED(0,   0,   0);   }
void tbLED_Click()  { tbSetLED(255, 255, 255); }   // 白闪 = 点击反馈

// ── 初始化 ────────────────────────────────────
void trackballBegin() {
    pinMode(TB_UP,    INPUT_PULLUP);
    pinMode(TB_DOWN,  INPUT_PULLUP);
    pinMode(TB_LEFT,  INPUT_PULLUP);
    pinMode(TB_RIGHT, INPUT_PULLUP);
    pinMode(TB_CLK,   INPUT_PULLUP);
    pinMode(BTN_BACK, INPUT_PULLUP);

    attachInterrupt(digitalPinToInterrupt(TB_UP),    tb_isr_up,    FALLING);
    attachInterrupt(digitalPinToInterrupt(TB_DOWN),  tb_isr_down,  FALLING);
    attachInterrupt(digitalPinToInterrupt(TB_LEFT),  tb_isr_left,  FALLING);
    attachInterrupt(digitalPinToInterrupt(TB_RIGHT), tb_isr_right, FALLING);

    pinMode(TB_LED_R, OUTPUT);
    pinMode(TB_LED_G, OUTPUT);
    pinMode(TB_LED_B, OUTPUT);
    pinMode(TB_LED_W, OUTPUT);
    tbLED_Orange();
}

// ── 每帧读取 delta（主循环调用）──────────────
void trackballPoll() {
    noInterrupts();
    tb.rawDx = _tb_dx;
    tb.rawDy = _tb_dy;
    _tb_dx   = 0;
    _tb_dy   = 0;
    interrupts();

    if (tb.rawDx || tb.rawDy) tb.lastActiveMs = millis();

    bool btn = (digitalRead(TB_CLK) == LOW);
    tb.clicked = (btn && !tb._prevBtn);
    tb.held    = btn;
    if (tb.clicked) { tb.lastActiveMs = millis(); }
    tb._prevBtn = btn;
}

// ── FREE 模式：delta → 光标坐标 ───────────────
// accel: 脉冲连续时加速（abs > 3 触发）
void tbUpdateCursor(float maxX, float maxY, float accel = 1.6f) {
    float fx = (float)tb.rawDx;
    float fy = (float)tb.rawDy;
    if (abs(tb.rawDx) > 3) fx *= accel;
    if (abs(tb.rawDy) > 3) fy *= accel;
    tb.curX = constrain(tb.curX + fx, 2.0f, maxX - 3.0f);
    tb.curY = constrain(tb.curY + fy, 2.0f, maxY - 3.0f);
}

// ── NAV 模式：delta 累积 → 返回方向事件 ───────
// 每次最多返回一个方向；无事件返回 AXIS_INPUT_NONE
#include <AXIS_UI.h>
#define TB_NAV_THRESH  4    // 触发所需脉冲数
#define TB_NAV_RATE_MS 120  // 最快重复速率

AxisInputEvent tbNavEvent() {
    tb._navX += tb.rawDx;
    tb._navY += tb.rawDy;

    uint32_t now = millis();
    if (now - tb._navFireMs < TB_NAV_RATE_MS) return AXIS_INPUT_NONE;

    AxisInputEvent ev = AXIS_INPUT_NONE;

    if      (tb._navX <= -TB_NAV_THRESH) { ev = AXIS_INPUT_LEFT;  tb._navX = 0; }
    else if (tb._navX >=  TB_NAV_THRESH) { ev = AXIS_INPUT_RIGHT; tb._navX = 0; }
    else if (tb._navY <= -TB_NAV_THRESH) { ev = AXIS_INPUT_UP;    tb._navY = 0; }
    else if (tb._navY >=  TB_NAV_THRESH) { ev = AXIS_INPUT_DOWN;  tb._navY = 0; }

    if (ev != AXIS_INPUT_NONE) tb._navFireMs = now;
    return ev;
}
