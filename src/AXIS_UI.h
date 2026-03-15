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

// ── 容量上限 ──────────────────────────────────────
#define AXIS_MAX_SCREENS      16
#define AXIS_MAX_MENU_ITEMS   10

// ── 回调类型 ──────────────────────────────────────

// 屏幕绘制回调：框架调用，传入主显示对象和用户数据
typedef void (*AxisDrawCallback)(Adafruit_GFX& disp, void* userData);

// 输入回调：屏幕或菜单收到输入事件时调用
typedef void (*AxisInputCallback)(AxisInputEvent event, void* userData);

// 副屏绘制回调：用户实现状态栏内容
typedef void (*AxisStatusCallback)(Adafruit_GFX& disp, void* userData);

// 显示刷新回调：用于需要手动 flush 的屏幕（如 SSD1306）
// 不需要的屏幕（如 SSD1351 直写型）不设置此项
typedef void (*AxisFlushCallback)();

// ── 引脚配置 ──────────────────────────────────────
struct AxisPinConfig {
    int8_t joy_up    = -1;
    int8_t joy_down  = -1;   // 注意：GPIO34 无内部上拉，需外接 10k 上拉
    int8_t joy_left  = -1;
    int8_t joy_right = -1;
    int8_t joy_ok    = -1;
    int8_t btn_a     = -1;
    int8_t btn_b     = -1;
};

// ── 屏幕注册表 ────────────────────────────────────
struct AxisScreen {
    AxisScreenID      id         = AXIS_SCR_NONE;
    AxisDrawCallback  drawFn     = nullptr;
    AxisInputCallback inputFn    = nullptr;
    void*             userData   = nullptr;
    bool              valid      = false;
};

// ═════════════════════════════════════════════════
//  AXIS_UI 主类
// ═════════════════════════════════════════════════
class AXIS_UI {
public:

    // ── 显示器绑定 ────────────────────────────────
    // 主屏：必须在 begin() 之前调用
    // disp      已初始化的 Adafruit_GFX 子类对象指针
    // flushFn   如果屏幕是缓冲型（SSD1306等）需要传入 flush 函数
    //           直写型屏幕（SSD1351等）传 nullptr 即可
    void setMainDisplay(Adafruit_GFX* disp,
                        AxisFlushCallback flushFn = nullptr);

    // 副屏（状态栏）：可选，不调用则禁用副屏功能
    void setSubDisplay(Adafruit_GFX* disp,
                       AxisFlushCallback flushFn = nullptr);

    // 副屏内容回调：用户实现状态栏绘制
    void setStatusCallback(AxisStatusCallback cb,
                           void* userData = nullptr);

    // ── 初始化 ────────────────────────────────────
    // 仅负责输入引脚配置，显示器由用户在外部初始化
    // 返回 false 表示主屏未绑定
    bool begin(const AxisPinConfig& pins = AxisPinConfig());

    // ── 主循环 ────────────────────────────────────
    // 必须在 loop() 中每帧调用
    void update();

    // ── 帧率控制 ──────────────────────────────────
    void setFPS(uint8_t fps);

    // ── 屏幕管理 ──────────────────────────────────
    void registerScreen(AxisScreenID id,
                        AxisDrawCallback drawFn,
                        AxisInputCallback inputFn = nullptr,
                        void* userData = nullptr);

    void goTo(AxisScreenID id);
    void goBack();

    AxisScreenID currentScreen() const { return _curID; }
    AxisScreenID previousScreen() const { return _prevID; }

    // ── 菜单系统 ──────────────────────────────────
    void showMenu(const AxisMenuItem* items,
                  uint8_t count,
                  AxisInputCallback onSelect = nullptr,
                  void* userData = nullptr);
    void hideMenu();
    bool isMenuOpen() const { return _menuTarget > 0.5f || _menuSlide > 0.01f; }

    // ── 通知系统 ──────────────────────────────────
    void notify(const String& text,
                uint16_t color = AXIS_C_ACCENT2,
                uint32_t durationMs = 3000);

    // ── 工具函数（静态，供用户的 drawFn 调用）──────
    // 渐变进度条（带圆头白点）
    void drawGradBar(int x, int y, int w, int h,
                     float ratio,
                     uint16_t c1, uint16_t c2,
                     uint16_t bg = AXIS_C_CARD);

    // 普通进度条（矩形边框）
    void drawProgressBar(int x, int y, int w, int h,
                         float ratio, uint16_t col);

    // VU 竖条（绿→红渐变）
    void drawVUBar(int x, int y, int w, int h, float level);

    // 文字截断（超出 maxChars 加 ~）
    static String truncate(const String& s, int maxChars);

    // 秒数格式化为 m:ss
    static String formatTime(int32_t seconds);

    // ── 访问底层显示对象 ──────────────────────────
    // 供 drawFn 内部需要调用屏幕特有 API 时使用
    Adafruit_GFX* mainDisplay() const { return _main; }
    Adafruit_GFX* subDisplay()  const { return _sub;  }

    // ── 尺寸查询 ──────────────────────────────────
    int16_t mainW() const { return _main ? _main->width()  : 128; }
    int16_t mainH() const { return _main ? _main->height() : 128; }

private:
    // ── 显示器 ────────────────────────────────────
    Adafruit_GFX*    _main       = nullptr;
    Adafruit_GFX*    _sub        = nullptr;
    AxisFlushCallback _mainFlush = nullptr;
    AxisFlushCallback _subFlush  = nullptr;

    // ── 副屏回调 ──────────────────────────────────
    AxisStatusCallback _statusCb    = nullptr;
    void*              _statusData  = nullptr;

    // ── 屏幕注册表 ────────────────────────────────
    AxisScreen   _screens[AXIS_MAX_SCREENS];
    uint8_t      _screenCount = 0;
    AxisScreenID _curID       = AXIS_SCR_NONE;
    AxisScreenID _prevID      = AXIS_SCR_NONE;

    // ── 菜单动画状态 ──────────────────────────────
    const AxisMenuItem* _menuItems    = nullptr;
    uint8_t             _menuCount    = 0;
    AxisInputCallback   _menuOnSelect = nullptr;
    void*               _menuSelData  = nullptr;
    float               _menuSlide    = 0.0f;
    float               _menuTarget   = 0.0f;
    int8_t              _menuCursor   = 0;
    int8_t              _menuPrevCursor = 0;
    float               _cursorAnim   = 1.0f;

    // ── 通知状态 ──────────────────────────────────
    struct {
        String   text;
        uint16_t color    = AXIS_C_ACCENT2;
        uint32_t expireAt = 0;
        bool     active   = false;
    } _notif;

    // ── 输入引脚 ──────────────────────────────────
    AxisPinConfig _pins;
    uint32_t      _debounce[7]    = {};
    static const uint32_t DB_MS   = 180;

    // ── 帧率 ──────────────────────────────────────
    uint32_t _frameMs     = 33;
    uint32_t _lastFrameAt = 0;

    // ── 内部方法 ──────────────────────────────────
    void _updateAnimations();
    void _handleInput();
    void _renderMain();
    void _renderMenuOverlay();
    void _renderNotifOverlay();
    void _renderSubDisplay();
    void _flushMain();
    void _flushSub();

    AxisInputEvent _pollInput();
    bool           _pressed(int8_t pin, uint8_t idx);
    AxisScreen*    _findScreen(AxisScreenID id);
};
