#pragma once

#include <Arduino.h>
#include <Adafruit_SSD1351.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_GFX.h>

#include "axis_types.h"
#include "axis_anim.h"

// 最大菜单项数
#define AXIS_MAX_MENU_ITEMS  8
// 最大注册屏幕数
#define AXIS_MAX_SCREENS     16

// 屏幕渲染回调类型
typedef void (*AxisDrawCallback)(Adafruit_SSD1351& disp, void* userData);
typedef void (*AxisInputCallback)(AxisInputEvent event, void* userData);
typedef void (*AxisStatusBarCallback)(Adafruit_SSD1306& bar, void* userData);

// ─────────────────────────────────────────
//  引脚配置结构
// ─────────────────────────────────────────
struct AxisPinConfig {
    // SSD1351 主屏（VSPI 硬件 SPI）
    int8_t oled_cs;
    int8_t oled_dc;
    int8_t oled_rst;

    // SSD1306 状态栏（I2C）
    int8_t i2c_sda;
    int8_t i2c_scl;
    uint8_t ssd1306_addr;

    // 输入
    int8_t joy_up;
    int8_t joy_down;   // 注意 GPIO34 无内部上拉，需外部上拉
    int8_t joy_left;
    int8_t joy_right;
    int8_t joy_ok;
    int8_t btn_a;
    int8_t btn_b;
};

// ─────────────────────────────────────────
//  屏幕注册表项
// ─────────────────────────────────────────
struct AxisScreen {
    AxisScreenID      id;
    AxisDrawCallback  drawFn;
    AxisInputCallback inputFn;
    void*             userData;
    bool              valid;
};

// ─────────────────────────────────────────
//  AXIS UI 主类
// ─────────────────────────────────────────
class AXIS_UI {
public:
    // ── 初始化 ──
    bool begin(const AxisPinConfig& pins);

    // ── 主循环，在 loop() 里调用 ──
    void update();

    // ── 屏幕注册 ──
    void registerScreen(AxisScreenID id,
                        AxisDrawCallback drawFn,
                        AxisInputCallback inputFn = nullptr,
                        void* userData = nullptr);

    // ── 屏幕跳转 ──
    void goTo(AxisScreenID id);
    void goBack();
    AxisScreenID currentScreenID() const { return _curScreenID; }

    // ── 内置菜单系统 ──
    void showMenu(const AxisMenuItem* items, uint8_t count,
                  AxisInputCallback onSelect = nullptr,
                  void* userData = nullptr);
    void hideMenu();
    bool isMenuVisible() const { return _menuSlide > 0.01f; }

    // ── 通知系统 ──
    void showNotification(const String& text,
                          uint16_t color = AXIS_C_ACCENT2,
                          uint32_t durationMs = 3000);

    // ── 状态栏回调 ──
    void setStatusBarCallback(AxisStatusBarCallback cb, void* userData = nullptr);

    // ── 绘图工具（公开供外部使用）──
    void drawGradBar(int x, int y, int w, int h,
                     float ratio, uint16_t c1, uint16_t c2,
                     uint16_t bg = AXIS_C_CARD);

    void drawProgressBar(int x, int y, int w, int h,
                         float ratio, uint16_t col);

    // 快速文本截断
    static String truncate(const String& s, int maxChars);

    // 时间格式化 mm:ss
    static String formatTime(int32_t seconds);

    // 显示对象访问（供 drawFn 使用）
    Adafruit_SSD1351& display()   { return _disp; }
    Adafruit_SSD1306& statusBar() { return _bar;  }

    // ── 帧率控制 ──
    void setFPS(uint8_t fps) { _frameMs = 1000 / fps; }

private:
    // 显示对象
    Adafruit_SSD1351 _disp;
    Adafruit_SSD1306 _bar;

    // 引脚配置
    AxisPinConfig _pins;

    // 屏幕管理
    AxisScreen    _screens[AXIS_MAX_SCREENS];
    uint8_t       _screenCount = 0;
    AxisScreenID  _curScreenID = AXIS_SCR_NONE;
    AxisScreenID  _prevScreenID = AXIS_SCR_NONE;

    // 菜单状态
    const AxisMenuItem* _menuItems    = nullptr;
    uint8_t             _menuCount    = 0;
    AxisInputCallback   _menuOnSelect = nullptr;
    void*               _menuUserData = nullptr;
    float               _menuSlide    = 0.0f;
    float               _menuTarget   = 0.0f;
    int8_t              _menuCursor   = 0;
    int8_t              _menuPrevCursor = 0;
    float               _cursorAnim   = 1.0f;

    // 通知状态
    AxisNotification _notification;
    bool             _hasNotification = false;

    // 状态栏
    AxisStatusBarCallback _statusBarCb    = nullptr;
    void*                 _statusBarData  = nullptr;

    // 帧率
    uint32_t _frameMs      = 33;  // ~30fps
    uint32_t _lastFrameMs  = 0;

    // 按键去抖
    uint32_t _debounce[8]  = {};
    static const uint32_t DEBOUNCE_MS = 180;

    // 内部方法
    AxisInputEvent _pollInput();
    bool           _pressed(int8_t pin, uint8_t idx);
    void           _renderMenu();
    void           _renderNotification();
    void           _renderStatusBar();
    AxisScreen*    _findScreen(AxisScreenID id);
};
