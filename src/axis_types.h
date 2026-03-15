#pragma once
#include <stdint.h>
#include <Arduino.h>

// ─────────────────────────────────────────
//  RGB565 颜色调色板
// ─────────────────────────────────────────
#define AXIS_C_BG        0x0841
#define AXIS_C_CARD      0x1083
#define AXIS_C_CARD2     0x18C6
#define AXIS_C_TOPBAR    0x0862
#define AXIS_C_DIVIDER   0x2104
#define AXIS_C_ACCENT    0x07FF   // 青
#define AXIS_C_ACCENT2   0xF81F   // 洋红
#define AXIS_C_PURPLE    0x901E
#define AXIS_C_GOLD      0xFEA0
#define AXIS_C_GREEN     0x07E0
#define AXIS_C_RED       0xF800
#define AXIS_C_ORANGE    0xFD20
#define AXIS_C_WHITE     0xFFFF
#define AXIS_C_LTGRAY    0xC618
#define AXIS_C_GRAY      0x7BEF
#define AXIS_C_DKGRAY    0x39E7
#define AXIS_C_BLACK     0x0000

// ─────────────────────────────────────────
//  屏幕 ID（用户自定义扩展）
// ─────────────────────────────────────────
typedef uint8_t AxisScreenID;
#define AXIS_SCR_NONE    0xFF

// ─────────────────────────────────────────
//  输入事件
// ─────────────────────────────────────────
enum AxisInputEvent {
    AXIS_INPUT_NONE = 0,
    AXIS_INPUT_UP,
    AXIS_INPUT_DOWN,
    AXIS_INPUT_LEFT,
    AXIS_INPUT_RIGHT,
    AXIS_INPUT_OK,
    AXIS_INPUT_BTN_A,
    AXIS_INPUT_BTN_B
};

// ─────────────────────────────────────────
//  菜单项
// ─────────────────────────────────────────
struct AxisMenuItem {
    const char* label;
    uint16_t    color;
    AxisScreenID targetScreen;  // 选中后跳转的屏幕，AXIS_SCR_NONE 则由回调处理
};

// ─────────────────────────────────────────
//  通知结构
// ─────────────────────────────────────────
struct AxisNotification {
    String  text;
    uint16_t color;
    uint32_t showUntil;   // millis() 时间戳，到期自动消失
};
