/**
 * AXIS UI Framework
 * Terminal aesthetic / Industrial minimal
 * https://github.com/EVGA2048/AXIS-UI
 * MIT License
 */

 #pragma once

 #include <Arduino.h>
 #include <Adafruit_GFX.h>
 #include <U8g2_for_Adafruit_GFX.h>
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
     void setMainDisplay(Adafruit_GFX* disp,
                         AxisFlushCallback flushFn = nullptr);
     void setSubDisplay(Adafruit_GFX*  disp,
                        AxisFlushCallback flushFn = nullptr);
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
 
     // ── 屏幕旋转 ──────────────────────────────────
     // 根据 MPU6050 数据自动调用，或手动调用
     void setRotation(AxisRotation rot);
     AxisRotation rotation() const { return _rotation; }
 
     // ── 初始化 ───────────────────────────────────
     bool begin(const AxisPinConfig& pins = AxisPinConfig());
 
     // ── 主循环 ───────────────────────────────────
     void update();
 
     // ── 帧率 ─────────────────────────────────────
     void setFPS(uint8_t fps);
 
     // ── 屏幕管理 ─────────────────────────────────
     void registerScreen(AxisScreenID     id,
                         AxisDrawCallback  drawFn,
                         AxisInputCallback inputFn = nullptr,
                         void*             userData = nullptr);
 
     void goTo(AxisScreenID id,
               AxisTransition trans = AXIS_TRANS_NONE);
     void goBack(AxisTransition trans = AXIS_TRANS_SLIDE_RIGHT);
     void goHome();
 
     AxisScreenID currentScreen()   const { return _curID; }
     AxisScreenID previousScreen()  const { return _prevID; }
     bool         isTransitioning() const { return _transActive; }
 
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
 
     // ── 传感器报警（框架内部自动检测，也可手动触发）─
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
 
     // ── U8g2 文字接口（UTF-8支持）────────────────
     void setFont(const uint8_t* font);
     void setFontColor(uint16_t color);
     void drawText(int16_t x, int16_t y, const String& s);
     void drawText(int16_t x, int16_t y, const char* s);
     uint16_t textWidth(const String& s);
     uint16_t textHeight();
 
     // ── 静态工具 ─────────────────────────────────
     static String truncate(const String& s, int maxChars);
     static String formatTime(int32_t seconds);
     static String padLeft(const String& s, int width, char c = ' ');
 
     // ── 底层访问 ─────────────────────────────────
     Adafruit_GFX*          mainDisplay() const { return _main; }
     Adafruit_GFX*          subDisplay()  const { return _sub;  }
     U8g2_for_Adafruit_GFX& u8f()               { return _u8f;  }
 
     int16_t mainW() const { return _main ? _main->width()  : 128; }
     int16_t mainH() const { return _main ? _main->height() : 128; }
 
 private:
     // ── 显示器 ───────────────────────────────────
     Adafruit_GFX*          _main      = nullptr;
     Adafruit_GFX*          _sub       = nullptr;
     AxisFlushCallback      _mainFlush = nullptr;
     AxisFlushCallback      _subFlush  = nullptr;
     U8g2_for_Adafruit_GFX _u8f;
 
     // ── 副屏 ─────────────────────────────────────
     AxisStatusCallback _statusCb   = nullptr;
     void*              _statusData = nullptr;
 
     // ── 灯光 ─────────────────────────────────────
     AxisLightCallback  _lightCb = nullptr;
     AxisLightEffect    _curLightEffect = AXIS_LIGHT_OFF;
 
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
 
     // ── 传感器数据 ───────────────────────────────
     AxisSensorData _sensor;
     uint8_t        _sensorCycle   = 0;   // 轮换索引
     AxisTimer      _sensorTimer;         // 轮换计时器
     bool           _sensorAlert   = false;
 
     // ── 平滑动画值 ───────────────────────────────
     AxisAnimValue  _smoothProgress;
     AxisAnimValue  _smoothVolume;
 
     // ── 屏幕注册表 ───────────────────────────────
     AxisScreen   _screens[AXIS_MAX_SCREENS];
     uint8_t      _screenCount = 0;
     AxisScreenID _curID       = AXIS_SCR_NONE;
     AxisScreenID _prevID      = AXIS_SCR_NONE;
     // 屏幕栈（支持 goHome）
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
     int8_t              _menuPrevCursor = 0;
     AxisAnimValue       _menuCursorAnim;
 
     // ── 通知状态 ─────────────────────────────────
     struct {
         String   text;
         uint16_t color    = AXIS_C_ACCENT;
         uint32_t expireAt = 0;
         bool     active   = false;
         float    slideY   = 0.0f;    // 滑入动画
     } _notif;
 
     // ── 输入 ─────────────────────────────────────
     AxisPinConfig _pins;
     uint32_t      _debounce[7]  = {};
     uint32_t      _pressStart[7]= {};
     bool          _held[7]      = {};
     static const uint32_t DB_MS = 150;
 
     // ── 帧率 ─────────────────────────────────────
     uint32_t _frameMs     = 33;
     uint32_t _lastFrameAt = 0;
 
     // ── 内部方法 ─────────────────────────────────
     void _updateAnimations();
     void _handleInput();
     void _checkSensorAlerts();
     void _renderFrame();
     void _renderScreen(AxisScreenID id, int16_t xOff, int16_t yOff);
     void _renderTransition();
     void _renderMenuOverlay();
     void _renderNotifOverlay();
     void _renderSubDisplay();
     void _renderHomeScreen(int16_t xOff, int16_t yOff);
     void _renderPlayerScreen(int16_t xOff, int16_t yOff);
     void _calcTransOffsets(float progress,
                            int16_t& fromX, int16_t& fromY,
                            int16_t& toX,   int16_t& toY);
     AxisInputEvent _remapInput(AxisInputEvent ev);  // 旋转后重映射
     void           _flushMain();
     void           _flushSub();
     void           _lightCmd(AxisLightEffect e, uint32_t color, uint8_t param=0);
     AxisInputEvent _pollInput();
     bool           _pressed(int8_t pin, uint8_t idx);
     bool           _longPressed(int8_t pin, uint8_t idx);
     AxisScreen*    _findScreen(AxisScreenID id);
     void           _stackPush(AxisScreenID id);
     AxisScreenID   _stackPop();
 };