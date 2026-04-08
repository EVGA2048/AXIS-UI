/**
 * AXIS UI Framework
 * Terminal aesthetic / Industrial minimal
 * https://github.com/EVGA2048/AXIS-UI
 * MIT License
 */

#pragma once

#include <Arduino.h>
#include <LovyanGFX.h>
#include "axis_types.h"
#include "axis_anim.h"

#define AXIS_MAX_SCREENS     16
#define AXIS_MAX_MENU_ITEMS  10
#define AXIS_LONG_PRESS_MS   600

// ── 屏幕注册表 ────────────────────────────────────
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
    int8_t joy_down  = -1;
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

    // ── 显示器绑定 ────────────────────────────────
    void setMainDisplay(LGFX_Device*      disp,
                        AxisFlushCallback flushFn = nullptr);
    void setSubDisplay(LGFX_Device*       disp,
                       AxisFlushCallback  flushFn = nullptr);
    void setStatusCallback(AxisStatusCallback cb,
                           void* userData = nullptr);

    // ── WS2812 灯光回调 ───────────────────────────
    void setLightCallback(AxisLightCallback cb);

    // ── 传感器数据注入 ────────────────────────────
    void setSensorData(const AxisSensorData& data);

    // ── 媒体数据注入 ──────────────────────────────
    void setTrackTitle(const String& s);
    void setTrackArtist(const String& s);
    void setPlaying(bool playing);
    void setVolume(uint8_t vol);         // 0-100
    void setProgress(float ratio);       // 0.0-1.0
    void setBTConnected(bool connected);
    void setBattery(uint8_t pct);        // 0-100
    void setWalking(bool walking);       // 走路检测状态

    // ── 时间/日期注入 ─────────────────────────────
    void setTime(const String& s);       // "21:46"
    void setDate(const String& s);       // "MON 16 MAR"

    // ── 主页节点注册 ──────────────────────────────
    void registerHomeNode(const char*    label,
                          uint16_t       color       = AXIS_C_ACCENT,
                          AxisScreenID   target      = AXIS_SCR_NONE,
                          AxisTransition transition  = AXIS_TRANS_SLIDE_LEFT,
                          const char*    description = "",
                          int8_t         posIndex    = -1,
                          const char*    shortTag    = nullptr,
                          int8_t         parentIdx   = -1);  // -1=HOME，>=0=父节点索引
    void clearHomeNodes();  // 清空节点，用于语言切换后重新注册

    // ── 灵动消息栏（HOME 信息区轮播）─────────────
    // slot 0 由框架自动填传感器数据；1-7 供应用注册
    void setTickerItem(uint8_t slot, const String& text,
                       uint16_t color = AXIS_C_DIM);
    void clearTickerItem(uint8_t slot);

    // ── 底部消息冒泡（ANCS / LoRa）────────────────
    void bubble(const String& text,
                uint16_t      color      = AXIS_C_ACCENT,
                uint32_t      durationMs = 4000);

    // ── 语言 ─────────────────────────────────────
    void     setLanguage(AxisLang lang) { _lang = lang; }
    AxisLang language()  const          { return _lang; }

    // ── 屏幕旋转 ──────────────────────────────────
    void setRotation(AxisRotation rot);
    void setJoyRotation(uint8_t rot);  // 仅重映射摇杆方向，不旋转画面
    AxisRotation rotation() const { return _rotation; }

    // ── 初始化 ───────────────────────────────────
    bool begin(const AxisPinConfig& pins = AxisPinConfig());

    // ── 主循环 ───────────────────────────────────
    void update();

    // ── 帧率 ─────────────────────────────────────
    void setFPS(uint8_t fps);

    // ── 调试工具 ─────────────────────────────────
    void setCustomFlag(bool on, uint16_t color = AXIS_C_WARN); // 状态栏自定义标志位
    void setFPSOverlay(bool on);                               // 右下角 FPS 显示

    // ── HOME 立方体 ───────────────────────────────
    void triggerHomeCubePulse();  // 触发一次大小脉冲（消息到达时调用）

    // ── 屏幕管理 ─────────────────────────────────
    void registerScreen(AxisScreenID     id,
                        AxisDrawCallback  drawFn,
                        AxisInputCallback inputFn = nullptr,
                        void*             userData = nullptr);

    // 为内置屏幕（HOME/PLAYER）注册额外输入回调
    void registerInput(AxisScreenID       id,
                       AxisInputCallback  inputFn,
                       void*              userData = nullptr);

    void goTo(AxisScreenID id,
              AxisTransition trans = AXIS_TRANS_NONE);
    void goBack(AxisTransition trans = AXIS_TRANS_SLIDE_RIGHT);
    void goHome();

    // ── 外部注入输入（用于重力光标等）────────────
    void injectInput(AxisInputEvent ev);

    AxisScreenID currentScreen()   const { return _curID; }
    AxisScreenID previousScreen()  const { return _prevID; }
    bool         isTransitioning() const { return _transActive; }
    int8_t       nodeCursor()      const { return _nodeCursor; }

    // ── 菜单 ─────────────────────────────────────
    void showMenu(const AxisMenuItem* items,
                  uint8_t             count,
                  AxisInputCallback   onSelect = nullptr,
                  void*               userData = nullptr);
    void hideMenu();
    bool isMenuOpen() const {
        return _menuTarget > 0.5f || _menuSlide > 0.01f;
    }

    // ── 通知 ─────────────────────────────────────
    void notify(const String& text,
                uint16_t      color      = AXIS_C_ACCENT,
                uint32_t      durationMs = 3000);

    // ── 传感器报警 ────────────────────────────────
    void triggerAlert(AxisLightEffect effect, uint16_t color);

    // ── 绘图工具（供 drawFn 使用）────────────────
    void drawGradBar(int x, int y, int w, int h,
                     float ratio, uint16_t c1, uint16_t c2,
                     uint16_t bg = AXIS_C_CARD);

    void drawProgressBar(int x, int y, int w, int h,
                         float ratio, uint16_t col);

    void drawVUBar(int x, int y, int w, int h, float level);

    void drawBattery(int x, int y, uint8_t pct);

    void drawSignalDots(int x, int y, uint8_t strength);  // 0-4

    // ── 文字接口（TFT_eSPI 字体）─────────────────
    void setTextFont(uint8_t num);                        // 内置字体 1-8
    void setFreeFont(const GFXfont* font);                // Free Font
    void setFontColor(uint16_t fg, uint16_t bg = AXIS_C_BG);
    void drawText(int16_t x, int16_t y, const String& s);
    void drawText(int16_t x, int16_t y, const char* s);
    uint16_t textWidth(const String& s);
    uint16_t textHeight();

    // ── 静态工具 ─────────────────────────────────
    static String truncate(const String& s, int maxChars);
    static String formatTime(int32_t seconds);
    static String padLeft(const String& s, int width, char c = ' ');

    // ── 底层访问 ─────────────────────────────────
    LGFX_Device* mainDisplay() const { return _main; }
    LGFX_Device* subDisplay()  const { return _sub;  }
    LGFX_Sprite* sprite()            { return _spr;  }  // 供 drawFn 访问

    int16_t mainW() const { return _main ? _main->width()  : 128; }
    int16_t mainH() const { return _main ? _main->height() : 128; }

private:
    // ── 显示器 ───────────────────────────────────
    LGFX_Device*      _main      = nullptr;
    LGFX_Device*      _sub       = nullptr;
    LGFX_Sprite*      _spr       = nullptr;   // 离屏缓冲（pushSprite）
    AxisFlushCallback _mainFlush = nullptr;
    AxisFlushCallback _subFlush  = nullptr;

    // ── 副屏 ─────────────────────────────────────
    AxisStatusCallback _statusCb   = nullptr;
    void*              _statusData = nullptr;

    // ── 灯光 ─────────────────────────────────────
    AxisLightCallback  _lightCb = nullptr;
    AxisLightEffect    _curLightEffect = AXIS_LIGHT_OFF;

    // ── 语言 ─────────────────────────────────────
    AxisLang _lang = AXIS_LANG_EN;

    // ── 旋转 ─────────────────────────────────────
    AxisRotation _rotation = AXIS_ROT_0;

    // ── 媒体数据 ─────────────────────────────────
    String   _trackTitle  = "";
    String   _trackArtist = "";
    bool     _playing     = false;
    uint8_t  _volume      = 75;
    float    _progress    = 0.0f;
    bool     _btConn      = false;
    uint8_t  _battery     = 100;
    bool     _walking     = false;

    // ── 传感器数据 ───────────────────────────────
    AxisSensorData _sensor;
    uint8_t        _sensorCycle   = 0;
    AxisTimer      _sensorTimer;
    bool           _sensorAlert   = false;

    // ── 主页节点 ─────────────────────────────────
    AxisNode  _nodes[AXIS_MAX_NODES];
    uint8_t   _nodeCount  = 0;
    int8_t    _nodeCursor = -1;   // -1 = HOME
    float     _nodeInfoSlide = 0.0f;  // 0=HOME info, 1=node info
    float     _cursorAnimX = 0.0f;   // 节点图 X 偏移
    float     _cursorAnimY = 0.0f;   // 节点图 Y 偏移

    // ── 主页小立方体动画 ─────────────────────────
    float     _cubeSpinV = 0.0f;   // 切换节点时触发，衰减停止

    // ── 灵动消息栏 ────────────────────────────────
    AxisTickerItem _ticker[AXIS_MAX_TICKER];
    uint8_t        _tickerIdx         = 0;
    uint32_t       _tickerNextMs      = 0;
    float          _tickerGlow        = 0.0f;  // 3→0，3次闪烁出场
    int16_t        _tickerScrollOff   = 0;     // 水平滚动偏移（像素）
    uint8_t        _tickerScrollPhase = 0;     // 0=停顿起 1=滚动中 2=停顿末
    uint32_t       _tickerScrollMs    = 0;     // 当前阶段起始时刻

    // ── 时间 / 日期 ───────────────────────────────
    String _timeStr = "--:--";
    String _dateStr = "";

    // ── 底部冒泡 ──────────────────────────────────
    struct {
        String   text;
        uint16_t color    = AXIS_C_ACCENT;
        uint32_t expireAt = 0;
        bool     active   = false;
        float    slideY   = 0.0f;
    } _bubble;

    // ── 平滑动画值 ───────────────────────────────
    AxisAnimValue  _smoothProgress;
    AxisAnimValue  _smoothVolume;

    // ── 屏幕注册表 ───────────────────────────────
    AxisScreen   _screens[AXIS_MAX_SCREENS];
    uint8_t      _screenCount = 0;
    AxisScreenID _curID       = AXIS_SCR_NONE;
    AxisScreenID _prevID      = AXIS_SCR_NONE;
    AxisScreenID _stack[8]    = {};
    uint8_t      _stackDepth  = 0;

    // ── 过渡动画 ─────────────────────────────────
    bool           _transActive   = false;
    AxisTransition _transType     = AXIS_TRANS_NONE;
    AxisScreenID   _transFromID   = AXIS_SCR_NONE;
    AxisScreenID   _transToID     = AXIS_SCR_NONE;
    float          _transProgress = 0.0f;
    static const uint32_t TRANS_DURATION_MS = 240;
    uint32_t       _transStartMs  = 0;

    // ── 菜单状态 ─────────────────────────────────
    const AxisMenuItem* _menuItems      = nullptr;
    uint8_t             _menuCount      = 0;
    AxisInputCallback   _menuOnSelect   = nullptr;
    void*               _menuSelData    = nullptr;
    float               _menuSlide      = 0.0f;
    float               _menuTarget     = 0.0f;
    int8_t              _menuCursor     = 0;
    AxisAnimValue       _menuCursorAnim;

    // ── 通知状态 ─────────────────────────────────
    struct {
        String   text;
        uint16_t color    = AXIS_C_ACCENT;
        uint32_t expireAt = 0;
        bool     active   = false;
        float    slideY   = 0.0f;
    } _notif;

    // ── 输入 ─────────────────────────────────────
    AxisPinConfig _pins;
    uint32_t      _debounce[7]   = {};
    uint32_t      _pressStart[7] = {};
    bool          _held[7]       = {};
    static const uint32_t DB_MS = 250;
    AxisInputEvent _injectedInput = AXIS_INPUT_NONE;  // 重力光标等外部注入

    // ── 帧率 ─────────────────────────────────────
    uint32_t _frameMs     = 14;   // ~70fps
    uint32_t _lastFrameAt = 0;

    // ── HOME 立方体（闲置缓转 + 消息脉冲）──────────
    float    _homeCubeAngX      = 0.3f;
    float    _homeCubeAngY      = 0.5f;
    float    _homeCubePulse     = 0.0f;   // 0=正常，>0 衰减中（sin波变大缩小）
    float    _cubeSize          = 12.0f;  // 平滑尺寸：HOME 追呼吸目标，节点收敛到 12
    uint32_t _idleLastInputMs   = 0;      // 最后一次输入时间（闲置旋转用）

    // ── 调试标志（状态栏 / FPS 覆盖层）──────────
    bool     _customFlag      = false;
    uint16_t _customFlagColor = AXIS_C_WARN;
    bool     _showFPS         = false;
    uint32_t _fpsLastMs       = 0;
    uint8_t  _fpsCount        = 0;
    uint8_t  _fpsCurrent      = 0;

    // ── 内部方法 ─────────────────────────────────
    void _updateAnimations();
    void _handleInput();
    void _checkSensorAlerts();
    void _renderFrame();
    void _renderScreen(AxisScreenID id, int16_t xOff, int16_t yOff);
    void _renderTransition();
    void _renderMenuOverlay();
    void _renderNotifOverlay();
    void _renderBubbleOverlay();
    void _renderSubDisplay();
    void _renderHomeScreen(int16_t xOff, int16_t yOff);
    void _renderPlayerScreen(int16_t xOff, int16_t yOff);
    void _calcTransOffsets(float progress,
                           int16_t& fromX, int16_t& fromY,
                           int16_t& toX,   int16_t& toY);
    void _homeNodeInput(AxisInputEvent ev);
    void _getNodeScreenPos(int8_t idx, int16_t& x, int16_t& y) const;
    AxisInputEvent _remapInput(AxisInputEvent ev);
    void           _flushMain();
    void           _flushSub();
    void           _lightCmd(AxisLightEffect e, uint32_t color, uint8_t param=0);
    AxisInputEvent _pollInput();
    bool           _pressed(int8_t pin, uint8_t idx);
    bool           _longPressed(int8_t pin, uint8_t idx);
    void           _setFont(uint8_t size);   // 根据语言选字体（1=小, 2=大）
    void           _drawMiniCube(int16_t cx, int16_t cy,
                                 float rx, float ry,
                                 float sz, uint16_t col);
    void           _stackPush(AxisScreenID id);
    AxisScreenID   _stackPop();
    AxisScreen*    _findScreen(AxisScreenID id);

    // ── sprite 绘图内联辅助（省去 _spr-> 重复）───
    inline void _fill(uint16_t c)                                          { _spr->fillScreen(c); }
    inline void _rect(int x,int y,int w,int h,uint16_t c)                  { _spr->fillRect(x,y,w,h,c); }
    inline void _dRect(int x,int y,int w,int h,uint16_t c)                 { _spr->drawRect(x,y,w,h,c); }
    inline void _hLine(int x,int y,int w,uint16_t c)                       { _spr->drawFastHLine(x,y,w,c); }
    inline void _vLine(int x,int y,int h,uint16_t c)                       { _spr->drawFastVLine(x,y,h,c); }
    inline void _line(int x0,int y0,int x1,int y1,uint16_t c)              { _spr->drawLine(x0,y0,x1,y1,c); }
    inline void _pixel(int x,int y,uint16_t c)                             { _spr->drawPixel(x,y,c); }
};
