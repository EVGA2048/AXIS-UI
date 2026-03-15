/**
 * AXIS UI Framework
 * A lightweight animated UI framework for small OLED displays on ESP32
 *
 * https://github.com/EVGA2048/AXIS-UI
 * MIT License
 */

#pragma once

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include "axis_types.h"
#include "axis_anim.h"

#define AXIS_MAX_SCREENS     16
#define AXIS_MAX_MENU_ITEMS  10

// ── 屏幕注册表项 ──────────────────────────────────
struct AxisScreen {
    AxisScreenID      id       = AXIS_SCR_NONE;
    AxisDrawCallback  drawFn   = nullptr;
    AxisInputCallback inputFn  = nullptr;
    void*             userData = nullptr;
    bool              valid    = false;
};

// ── 引脚配置 ─────────────────────────────────────
struct AxisPinConfig {
    int8_t joy_up    = -1;
    int8_t joy_down  = -1;   // GPIO34：外接 10k 上拉，无内部上拉
    int8_t joy_left  = -1;
    int8_t joy_right = -1;
    int8_t joy_ok    = -1;
    int8_t btn_a     = -1;
    int8_t btn_b     = -1;
};

// ═════════════════════════════════════════════════
//  AXIS_UI 主类
// ═════════════════════════════════════════════════
class AXIS_UI {
public:

    // ── 显示器绑定（begin 之前调用）──────────────
    void setMainDisplay(Adafruit_GFX* disp,
                        AxisFlushCallback flushFn = nullptr);

    void setSubDisplay(Adafruit_GFX* disp,
                       AxisFlushCallback flushFn = nullptr);

    void setStatusCallback(AxisStatusCallback cb,
                           void* userData = nullptr);

    // ── 初始化 ───────────────────────────────────
    bool begin(const AxisPinConfig& pins = AxisPinConfig());

    // ── 主循环（loop 里调用）──────────────────────
    void update();

    // ── 帧率 ─────────────────────────────────────
    void setFPS(uint8_t fps);

    // ── 屏幕管理 ─────────────────────────────────
    void registerScreen(AxisScreenID    id,
                        AxisDrawCallback drawFn,
                        AxisInputCallback inputFn = nullptr,
                        void*             userData = nullptr);

    // 带过渡效果的跳转
    void goTo(AxisScreenID id,
              AxisTransition trans = AXIS_TRANS_NONE);

    // 返回上一级，默认向右滑出
    void goBack(AxisTransition trans = AXIS_TRANS_SLIDE_RIGHT);

    AxisScreenID currentScreen()  const { return _curID;  }
    AxisScreenID previousScreen() const { return _prevID; }
    bool         isTransitioning() const { return _transActive; }

    // ── 菜单系统 ─────────────────────────────────
    void showMenu(const AxisMenuItem* items,
                  uint8_t             count,
                  AxisInputCallback   onSelect = nullptr,
                  void*               userData = nullptr);
    void hideMenu();
    bool isMenuOpen() const {
        return _menuTarget > 0.5f || _menuSlide > 0.01f;
    }

    // ── 通知系统 ─────────────────────────────────
    void notify(const String& text,
                uint16_t      color      = AXIS_C_ACCENT2,
                uint32_t      durationMs = 3000);

    // ── 绘图工具（供 drawFn 使用）────────────────
    // 渐变进度条（圆头，带白色端点）
    void drawGradBar(int x, int y, int w, int h,
                     float    ratio,
                     uint16_t c1, uint16_t c2,
                     uint16_t bg = AXIS_C_CARD);

    // 矩形进度条（带边框）
    void drawProgressBar(int x, int y, int w, int h,
                         float ratio, uint16_t col);

    // VU 竖条（绿→红渐变）
    void drawVUBar(int x, int y, int w, int h, float level);

    // ── 静态工具 ─────────────────────────────────
    static String truncate(const String& s, int maxChars);
    static String formatTime(int32_t seconds);

    // ── 底层访问 ─────────────────────────────────
    Adafruit_GFX* mainDisplay() const { return _main; }
    Adafruit_GFX* subDisplay()  const { return _sub;  }
    int16_t mainW() const { return _main ? _main->width()  : 128; }
    int16_t mainH() const { return _main ? _main->height() : 128; }

private:
    // ── 显示器 ───────────────────────────────────
    Adafruit_GFX*     _main      = nullptr;
    Adafruit_GFX*     _sub       = nullptr;
    AxisFlushCallback _mainFlush = nullptr;
    AxisFlushCallback _subFlush  = nullptr;

    // ── 副屏回调 ─────────────────────────────────
    AxisStatusCallback _statusCb   = nullptr;
    void*              _statusData = nullptr;

    // ── 屏幕注册表 ───────────────────────────────
    AxisScreen   _screens[AXIS_MAX_SCREENS];
    uint8_t      _screenCount = 0;
    AxisScreenID _curID       = AXIS_SCR_NONE;
    AxisScreenID _prevID      = AXIS_SCR_NONE;

    // ── 过渡动画状态 ─────────────────────────────
    bool           _transActive   = false;
    AxisTransition _transType     = AXIS_TRANS_NONE;
    AxisScreenID   _transFromID   = AXIS_SCR_NONE;
    AxisScreenID   _transToID     = AXIS_SCR_NONE;
    float          _transProgress = 0.0f;  // 0.0 → 1.0
    static const uint32_t TRANS_DURATION_MS = 280;
    uint32_t       _transStartMs  = 0;

    // ── 菜单状态 ─────────────────────────────────
    const AxisMenuItem* _menuItems       = nullptr;
    uint8_t             _menuCount       = 0;
    AxisInputCallback   _menuOnSelect    = nullptr;
    void*               _menuSelData     = nullptr;
    float               _menuSlide       = 0.0f;
    float               _menuTarget      = 0.0f;
    int8_t              _menuCursor      = 0;
    int8_t              _menuPrevCursor  = 0;
    float               _cursorAnim      = 1.0f;

    // ── 通知状态 ─────────────────────────────────
    struct {
        String   text;
        uint16_t color    = AXIS_C_ACCENT2;
        uint32_t expireAt = 0;
        bool     active   = false;
    } _notif;

    // ── 输入 ─────────────────────────────────────
    AxisPinConfig _pins;
    uint32_t      _debounce[7] = {};
    static const uint32_t DB_MS = 180;

    // ── 帧率 ─────────────────────────────────────
    uint32_t _frameMs     = 33;
    uint32_t _lastFrameAt = 0;

    // ── 内部方法 ─────────────────────────────────
    void _updateAnimations();
    void _handleInput();
    void _renderFrame();
    void _renderScreen(AxisScreenID id, int16_t xOff, int16_t yOff);
    void _renderTransition();
    void _renderMenuOverlay();
    void _renderNotifOverlay();
    void _renderSubDisplay();

    // 根据过渡类型和进度计算偏移量
    void _calcTransOffsets(float progress,
                           int16_t& fromX, int16_t& fromY,
                           int16_t& toX,   int16_t& toY);

    void           _flushMain();
    void           _flushSub();
    AxisInputEvent _pollInput();
    bool           _pressed(int8_t pin, uint8_t idx);
    AxisScreen*    _findScreen(AxisScreenID id);
};
