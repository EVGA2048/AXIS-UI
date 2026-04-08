/**
 * PRISM / BasicMenu — 无源蜂鸣器提示音
 * 接法：蜂鸣器正极 → GPIO21，负极 → GND
 *
 * 依赖：ESP32 Arduino Core 3.x（tone() 内部使用 LEDC）
 * 用法：
 *   buzzer::boot();        // 开机音
 *   buzzer::click();       // 按键点击
 *   buzzer::notify();      // 新消息/通知
 *   buzzer::confirm();     // 确认/保存
 *   buzzer::error();       // 错误/失败
 *   buzzer::screenOff();   // 进入屏保
 *   buzzer::screenOn();    // 退出屏保/唤醒
 */
#pragma once
#include <Arduino.h>

namespace buzzer {

static constexpr uint8_t PIN = 21;

// ── 底层：发单个音符 ──────────────────────────────
static inline void note(uint16_t freq, uint16_t ms, uint16_t pause_ms = 20) {
    tone(PIN, freq, ms);
    delay(ms + pause_ms);
    noTone(PIN);
}

// ── 开机音：三音上行，末尾拖长 ─────────────────────
static inline void boot() {
    note(523, 80);    // C5
    note(659, 80);    // E5
    note(784, 160);   // G5  ← 拖长收尾
}

// ── 按键点击：极短高频 ────────────────────────────
static inline void click() {
    note(1200, 18, 0);
}

// ── 新消息/通知：双音上扬 ────────────────────────
static inline void notify() {
    note(880, 60);    // A5
    note(1175, 100);  // D6
}

// ── 确认/保存：上行三连 ──────────────────────────
static inline void confirm() {
    note(784, 60);    // G5
    note(988, 60);    // B5
    note(1175, 90);   // D6
}

// ── 错误：下行双音 ───────────────────────────────
static inline void error() {
    note(660, 100);   // E5
    note(494, 150);   // B4（低沉收尾）
}

// ── 进入屏保：缓降扫频 ───────────────────────────
static inline void screenOff() {
    for (uint16_t f = 800; f >= 300; f -= 50) {
        tone(PIN, f);
        delay(18);
    }
    noTone(PIN);
}

// ── 退出屏保/唤醒：急升扫频 ─────────────────────
static inline void screenOn() {
    for (uint16_t f = 400; f <= 900; f += 50) {
        tone(PIN, f);
        delay(12);
    }
    noTone(PIN);
}

} // namespace buzzer
