/**
 * SOLITON — trackball.h
 * 国产轨迹球模块（正交编码器输出）→ AXIS_UI 输入转换
 *
 * 模块引脚标注 UP/DWN/LFT/RHT，但实为正交编码器：
 *   X轴：RHT(A相) + LFT(B相)  向右 → RHT超前LFT，RHT脉冲多
 *   Y轴：UP(A相)  + DWN(B相)  向上 → UP超前DWN，UP脉冲多
 *   两相同时有脉冲，差值决定方向和速度
 *
 * 采样策略：
 *   固定窗口内累积两相脉冲差，差值即为有符号增量。
 *   慢推时差值小但稳定，快推时差值大。
 */

#pragma once
#include <Arduino.h>
#include <AXIS_UI.h>

// ── 引脚（对应 P4-Pico 接线）─────────────────────────
#define TB_PIN_UP   2
#define TB_PIN_DWN  3
#define TB_PIN_LFT  4
#define TB_PIN_RHT  5
#define TB_PIN_BTN  6   // 轨迹球按下 = OK / 确认

// ── 调参 ──────────────────────────────────────────────
#define TB_POLL_MS      20    // 采样周期（ms）
#define TB_THRESHOLD    1     // 触发方向事件所需最少脉冲数
#define TB_REPEAT_MS    180   // 长按重复间隔（ms）
#define TB_BTN_LONG_MS  600   // 长按判定时间（ms）

// ── ISR 计数器 ────────────────────────────────────────
// 正交编码器：每相独立计脉冲，差值 = 方向+速度
static volatile int32_t _tb_cntX = 0;   // RHT脉冲 - LFT脉冲
static volatile int32_t _tb_cntY = 0;   // UP脉冲  - DWN脉冲

// A相上升沿时读B相电平，判断方向
void IRAM_ATTR _tb_isrRHT() { _tb_cntX += (digitalRead(TB_PIN_LFT) ? +1 : -1); }
void IRAM_ATTR _tb_isrLFT() { _tb_cntX -= (digitalRead(TB_PIN_RHT) ? +1 : -1); }
void IRAM_ATTR _tb_isrUP()  { _tb_cntY += (digitalRead(TB_PIN_DWN) ? +1 : -1); }
void IRAM_ATTR _tb_isrDWN() { _tb_cntY -= (digitalRead(TB_PIN_UP)  ? +1 : -1); }

// ── 状态 ─────────────────────────────────────────────
static uint32_t _tb_lastPollMs   = 0;   // tbPoll() 离散采样时间戳
static uint32_t _tb_lastDeltaMs  = 0;   // tbPollDelta() 采样时间戳
static uint32_t _tb_lastRepeatMs = 0;
static AxisInputEvent _tb_lastDir = AXIS_INPUT_NONE;

static bool     _tb_btnDown      = false;
static uint32_t _tb_btnDownMs    = 0;
static bool     _tb_btnLongFired = false;

void tbBegin() {
    pinMode(TB_PIN_UP,  INPUT_PULLUP);
    pinMode(TB_PIN_DWN, INPUT_PULLUP);
    pinMode(TB_PIN_LFT, INPUT_PULLUP);
    pinMode(TB_PIN_RHT, INPUT_PULLUP);
    pinMode(TB_PIN_BTN, INPUT_PULLUP);

    // 两相都挂中断，CHANGE = 上升+下降沿都捕获，分辨率翻倍
    attachInterrupt(TB_PIN_RHT, _tb_isrRHT, CHANGE);
    attachInterrupt(TB_PIN_LFT, _tb_isrLFT, CHANGE);
    attachInterrupt(TB_PIN_UP,  _tb_isrUP,  CHANGE);
    attachInterrupt(TB_PIN_DWN, _tb_isrDWN, CHANGE);
}

/**
 * 每帧调用，返回当前应注入 AXIS_UI 的事件（NONE = 无事件）
 * 用法：ui.injectInput(tbPoll());
 */
// ── 内部：读取并清空计数器（原子操作）────────────
static void _tb_readCounters(int32_t& x, int32_t& y) {
    noInterrupts();
    x = _tb_cntX; _tb_cntX = 0;
    y = _tb_cntY; _tb_cntY = 0;
    interrupts();
}

// ── 仅按钮（光标模式下调用，不碰方向计数器）─────
AxisInputEvent tbPollBtn() {
    uint32_t now = millis();
    bool pressed = (digitalRead(TB_PIN_BTN) == LOW);
    if (pressed && !_tb_btnDown) {
        _tb_btnDown      = true;
        _tb_btnDownMs    = now;
        _tb_btnLongFired = false;
    } else if (!pressed && _tb_btnDown) {
        _tb_btnDown = false;
        if (!_tb_btnLongFired) return AXIS_INPUT_OK;
    }
    if (_tb_btnDown && !_tb_btnLongFired &&
        (now - _tb_btnDownMs >= TB_BTN_LONG_MS)) {
        _tb_btnLongFired = true;
        return AXIS_INPUT_OK_LONG;
    }
    return AXIS_INPUT_NONE;
}

/**
 * 每帧调用，返回当前应注入 AXIS_UI 的事件（NONE = 无事件）
 * 摇杆模式专用，会消耗方向计数器
 * 用法：ui.injectInput(tbPoll());
 */
AxisInputEvent tbPoll() {
    uint32_t now = millis();

    // ── 按钮处理 ──────────────────────────────────────
    AxisInputEvent btnEv = tbPollBtn();
    if (btnEv != AXIS_INPUT_NONE) return btnEv;

    // ── 方向采样（20ms 门控）─────────────────────────
    if (now - _tb_lastPollMs < TB_POLL_MS) return AXIS_INPUT_NONE;
    _tb_lastPollMs = now;

    int32_t x, y;
    _tb_readCounters(x, y);

    // 180° 安装修正：取反
    x = -x; y = -y;

    AxisInputEvent dir = AXIS_INPUT_NONE;
    int32_t ax = abs(x), ay = abs(y);
    if (ax >= TB_THRESHOLD || ay >= TB_THRESHOLD) {
        if (ax >= ay) dir = (x > 0) ? AXIS_INPUT_RIGHT : AXIS_INPUT_LEFT;
        else          dir = (y > 0) ? AXIS_INPUT_DOWN  : AXIS_INPUT_UP;
    }

    if (dir == AXIS_INPUT_NONE) {
        _tb_lastDir = AXIS_INPUT_NONE;
        return AXIS_INPUT_NONE;
    }

    if (dir != _tb_lastDir) {
        _tb_lastDir      = dir;
        _tb_lastRepeatMs = now;
        return dir;
    } else if (now - _tb_lastRepeatMs >= TB_REPEAT_MS) {
        _tb_lastRepeatMs = now;
        return dir;
    }

    return AXIS_INPUT_NONE;
}

// ── 原始增量（光标模式用）────────────────────────
// 返回本帧脉冲增量，不做方向离散化
// 调用方：ui.moveCursor(d.dx, d.dy);
struct TbDelta { int16_t dx; int16_t dy; };

TbDelta tbPollDelta() {
    // 8ms 采样窗口
    uint32_t now = millis();
    if (now - _tb_lastDeltaMs < 8) return {0, 0};
    _tb_lastDeltaMs = now;

    int32_t x, y;
    _tb_readCounters(x, y);

    // 180° 安装修正：取反
    return {(int16_t)(-x), (int16_t)(-y)};
}
