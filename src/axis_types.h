/**
 * AXIS UI Framework — Types
 * https://github.com/EVGA2048/AXIS-UI
 * MIT License
 */

#pragma once
#include <stdint.h>
#include <Arduino.h>

// ═════════════════════════════════════════════════
//  颜色调色板（RGB565）
// ═════════════════════════════════════════════════
#define AXIS_C_BG        0x0841
#define AXIS_C_CARD      0x1083
#define AXIS_C_CARD2     0x18C6
#define AXIS_C_TOPBAR    0x0862
#define AXIS_C_DIVIDER   0x2104
#define AXIS_C_ACCENT    0x07FF
#define AXIS_C_ACCENT2   0xF81F
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

// ═════════════════════════════════════════════════
//  屏幕 ID
// ═════════════════════════════════════════════════
typedef uint8_t AxisScreenID;
#define AXIS_SCR_NONE  0xFF

// ═════════════════════════════════════════════════
//  输入事件
// ═════════════════════════════════════════════════
enum AxisInputEvent : uint8_t {
    AXIS_INPUT_NONE = 0,
    AXIS_INPUT_UP,
    AXIS_INPUT_DOWN,
    AXIS_INPUT_LEFT,
    AXIS_INPUT_RIGHT,
    AXIS_INPUT_OK,
    AXIS_INPUT_BTN_A,
    AXIS_INPUT_BTN_B
};

// ═════════════════════════════════════════════════
//  过渡动画类型
// ═════════════════════════════════════════════════
enum AxisTransition : uint8_t {
    AXIS_TRANS_NONE = 0,
    AXIS_TRANS_SLIDE_LEFT,    // 新屏从右进，旧屏往左出（进入子页面）
    AXIS_TRANS_SLIDE_RIGHT,   // 新屏从左进，旧屏往右出（返回上级）
    AXIS_TRANS_SLIDE_UP,      // 新屏从下进，旧屏往上出
    AXIS_TRANS_SLIDE_DOWN,    // 新屏从上进，旧屏往下出
};

// ═════════════════════════════════════════════════
//  菜单项
// ═════════════════════════════════════════════════
struct AxisMenuItem {
    const char*    label;
    uint16_t       color;
    AxisScreenID   targetScreen;
    AxisTransition transition = AXIS_TRANS_SLIDE_LEFT;
};

// ═════════════════════════════════════════════════
//  回调类型
//
//  xOff / yOff：过渡动画期间的偏移量
//  正常显示时为 0，过渡时框架传入非零值
//  用户在 drawFn 里把所有 x 坐标加上 xOff 即可获得滑动效果
//  如果不用，动画期间元素会原地显示（也不会出错）
// ═════════════════════════════════════════════════
typedef void (*AxisDrawCallback)(Adafruit_GFX& d,
                                  int16_t xOff, int16_t yOff,
                                  void* userData);

typedef void (*AxisInputCallback)(AxisInputEvent event,
                                   void* userData);

typedef void (*AxisStatusCallback)(Adafruit_GFX& d,
                                    void* userData);

typedef void (*AxisFlushCallback)();
