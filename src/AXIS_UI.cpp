#include "AXIS_UI.h"
#include <U8g2_for_Adafruit_GFX.h>

// ═════════════════════════════════════════════════
//  显示器绑定
// ═════════════════════════════════════════════════

void AXIS_UI::setMainDisplay(Adafruit_GFX* disp, AxisFlushCallback flushFn) {
    _main      = disp;
    _mainFlush = flushFn;
}

void AXIS_UI::setSubDisplay(Adafruit_GFX* disp, AxisFlushCallback flushFn) {
    _sub      = disp;
    _subFlush = flushFn;
}

void AXIS_UI::setStatusCallback(AxisStatusCallback cb, void* userData) {
    _statusCb   = cb;
    _statusData = userData;
}

void AXIS_UI::setLightCallback(AxisLightCallback cb) {
    _lightCb = cb;
}

// ═════════════════════════════════════════════════
//  数据注入
// ═════════════════════════════════════════════════

void AXIS_UI::setSensorData(const AxisSensorData& data) {
    _sensor = data;
    _checkSensorAlerts();
}

void AXIS_UI::setTrackTitle (const String& s) { _trackTitle  = s; }
void AXIS_UI::setTrackArtist(const String& s) { _trackArtist = s; }
void AXIS_UI::setPlaying    (bool p)          { _playing     = p; _lightCmd(p ? AXIS_LIGHT_VU : AXIS_LIGHT_STATIC, 0x202000, 30); }
void AXIS_UI::setVolume     (uint8_t v)       { _volume      = constrain(v,0,100); _smoothVolume.setTarget(v/100.0f); }
void AXIS_UI::setProgress   (float r)         { _progress    = constrain(r,0.0f,1.0f); _smoothProgress.setTarget(r); }
void AXIS_UI::setBTConnected(bool c)          { _btConn      = c; }
void AXIS_UI::setBattery    (uint8_t p)       { _battery     = constrain(p,0,100); }

// ═════════════════════════════════════════════════
//  传感器报警检测
// ═════════════════════════════════════════════════

void AXIS_UI::_checkSensorAlerts() {
    if (!_sensor.valid) return;

    bool danger = (_sensor.co2  >= AXIS_CO2_DANGER  ||
                   _sensor.tvoc >= AXIS_TVOC_DANGER);
    bool warn   = (_sensor.co2  >= AXIS_CO2_WARN    ||
                   _sensor.tvoc >= AXIS_TVOC_WARN);

    if (danger) {
        _sensorAlert = true;
        _lightCmd(AXIS_LIGHT_ALERT_DANGER, 0xFF0000, 0);
        notify("! DANGER: CO2/TVOC HIGH", AXIS_C_DANGER, 5000);
    } else if (warn) {
        _sensorAlert = true;
        _lightCmd(AXIS_LIGHT_ALERT_WARN, 0xFF6800, 0);
        notify("! WARN: CO2/TVOC ELEVATED", AXIS_C_WARN, 4000);
    } else {
        _sensorAlert = false;
    }
}

// 手动触发灯光
void AXIS_UI::triggerAlert(AxisLightEffect effect, uint16_t color) {
    uint8_t r = (color >> 11) & 0x1F; r = (r << 3) | (r >> 2);
    uint8_t g = (color >>  5) & 0x3F; g = (g << 2) | (g >> 4);
    uint8_t b =  color        & 0x1F; b = (b << 3) | (b >> 2);
    _lightCmd(effect, ((uint32_t)r<<16)|((uint32_t)g<<8)|b, 0);
}

void AXIS_UI::_lightCmd(AxisLightEffect e, uint32_t color, uint8_t param) {
    _curLightEffect = e;
    if (_lightCb) _lightCb(e, color, param);
}

// ═════════════════════════════════════════════════
//  屏幕旋转
// ═════════════════════════════════════════════════

void AXIS_UI::setRotation(AxisRotation rot) {
    if (_rotation == rot) return;
    _rotation = rot;
    if (_main) _main->setRotation((uint8_t)rot);
    if (_sub)  _sub->setRotation((uint8_t)rot);
    Serial.printf("[AXIS] Rotation: %d\n", rot);
}

// 旋转后重映射摇杆方向
AxisInputEvent AXIS_UI::_remapInput(AxisInputEvent ev) {
    if (_rotation == AXIS_ROT_0) return ev;

    // 只重映射方向键
    if (ev < AXIS_INPUT_UP || ev > AXIS_INPUT_RIGHT) return ev;

    const AxisInputEvent maps[4][4] = {
        // ROT_0：不变
        {AXIS_INPUT_UP, AXIS_INPUT_DOWN, AXIS_INPUT_LEFT, AXIS_INPUT_RIGHT},
        // ROT_90：UP→RIGHT RIGHT→DOWN DOWN→LEFT LEFT→UP
        {AXIS_INPUT_RIGHT, AXIS_INPUT_LEFT, AXIS_INPUT_UP, AXIS_INPUT_DOWN},
        // ROT_180：全部反转
        {AXIS_INPUT_DOWN, AXIS_INPUT_UP, AXIS_INPUT_RIGHT, AXIS_INPUT_LEFT},
        // ROT_270：UP→LEFT LEFT→DOWN DOWN→RIGHT RIGHT→UP
        {AXIS_INPUT_LEFT, AXIS_INPUT_RIGHT, AXIS_INPUT_DOWN, AXIS_INPUT_UP},
    };

    int idx = ev - AXIS_INPUT_UP;  // 0=UP 1=DOWN 2=LEFT 3=RIGHT
    return maps[(uint8_t)_rotation][idx];
}

// ═════════════════════════════════════════════════
//  初始化
// ═════════════════════════════════════════════════

bool AXIS_UI::begin(const AxisPinConfig& pins) {
    if (!_main) {
        Serial.println("[AXIS] ERROR: setMainDisplay() first.");
        return false;
    }
    _pins = pins;

    // U8g2 绑定主屏
    _u8f.begin(*_main);

    // 输入引脚
    int8_t pullupPins[] = {
        pins.joy_up, pins.joy_left, pins.joy_right,
        pins.joy_ok, pins.btn_a, pins.btn_b
    };
    for (int8_t p : pullupPins) if (p >= 0) pinMode(p, INPUT_PULLUP);
    if (pins.joy_down >= 0) pinMode(pins.joy_down, INPUT_PULLUP);

    // 初始化动画值
    _smoothProgress = AxisAnimValue(0.0f, 0.08f);
    _smoothVolume   = AxisAnimValue(0.75f, 0.1f);
    _menuCursorAnim = AxisAnimValue(0.0f, 0.15f);

    // 传感器轮换定时器（每4秒换一项）
    _sensorTimer = AxisTimer(4000);
    _sensorTimer.reset();

    _lastFrameAt = millis();

    Serial.printf("[AXIS] Init OK. %dx%d\n", mainW(), mainH());
    return true;
}

void AXIS_UI::setFPS(uint8_t fps) {
    _frameMs = (fps > 0) ? (1000 / fps) : 33;
}

// ═════════════════════════════════════════════════
//  主循环
// ═════════════════════════════════════════════════

void AXIS_UI::update() {
    if (!_main) return;
    uint32_t now = millis();
    if (now - _lastFrameAt < _frameMs) return;
    _lastFrameAt = now;

    _updateAnimations();
    _handleInput();
    _renderFrame();
    _renderSubDisplay();
}

// ═════════════════════════════════════════════════
//  动画状态更新
// ═════════════════════════════════════════════════

void AXIS_UI::_updateAnimations() {
    uint32_t now = millis();

    // 过渡进度
    if (_transActive) {
        float elapsed = (float)(now - _transStartMs);
        _transProgress = elapsed / (float)TRANS_DURATION_MS;
        if (_transProgress >= 1.0f) {
            _transProgress = 1.0f;
            _transActive   = false;
            _curID         = _transToID;
            _main->fillScreen(AXIS_C_BG);
        }
    }

    // 菜单滑动
    _menuSlide = AxisAnim::smoothFollow(_menuSlide, _menuTarget, 0.14f);
    if (_menuSlide < 0.005f && _menuTarget < 0.01f) _menuSlide = 0.0f;

    // 光标动画
    _menuCursorAnim.update();

    // 平滑数值
    _smoothProgress.update();
    _smoothVolume.update();

    // 通知滑入动画
    if (_notif.active) {
        _notif.slideY = AxisAnim::smoothFollow(_notif.slideY, 1.0f, 0.18f);
        if (now >= _notif.expireAt) {
            _notif.active  = false;
            _notif.slideY  = 0.0f;
        }
    }

    // 传感器显示轮换（报警项不参与轮换）
    if (!_sensorAlert && _sensor.valid && _sensorTimer.tick()) {
        _sensorCycle++;
        if (_sensorCycle > 2) _sensorCycle = 0;
    }
}

// ═════════════════════════════════════════════════
//  输入处理
// ═════════════════════════════════════════════════

void AXIS_UI::_handleInput() {
    if (_transActive) return;

    AxisInputEvent raw = _pollInput();
    if (raw == AXIS_INPUT_NONE) return;

    // 旋转重映射
    AxisInputEvent ev = _remapInput(raw);

    if (isMenuOpen()) {
        switch (ev) {
            case AXIS_INPUT_UP:
                if (_menuCursor > 0) {
                    _menuCursorAnim.snap((float)_menuCursor);
                    _menuCursor--;
                    _menuCursorAnim.setTarget((float)_menuCursor);
                }
                break;
            case AXIS_INPUT_DOWN:
                if (_menuCursor < (int8_t)(_menuCount-1)) {
                    _menuCursorAnim.snap((float)_menuCursor);
                    _menuCursor++;
                    _menuCursorAnim.setTarget((float)_menuCursor);
                }
                break;
            case AXIS_INPUT_OK:
            case AXIS_INPUT_BTN_B: {
                const AxisMenuItem& item = _menuItems[_menuCursor];
                hideMenu();
                if (item.targetScreen != AXIS_SCR_NONE)
                    goTo(item.targetScreen, item.transition);
                else if (_menuOnSelect)
                    _menuOnSelect(ev, _menuSelData);
                break;
            }
            case AXIS_INPUT_BTN_A:
                hideMenu();
                break;
            default: break;
        }
    } else {
        AxisScreen* scr = _findScreen(_curID);
        if (scr && scr->inputFn) scr->inputFn(ev, scr->userData);
    }
}

// ═════════════════════════════════════════════════
//  渲染主帧
// ═════════════════════════════════════════════════

void AXIS_UI::_renderFrame() {
    if (_transActive) {
        _renderTransition();
    } else {
        _renderScreen(_curID, 0, 0);
        if (_notif.active)       _renderNotifOverlay();
        if (_menuSlide > 0.005f) _renderMenuOverlay();
    }
    _flushMain();
}

void AXIS_UI::_renderScreen(AxisScreenID id, int16_t xOff, int16_t yOff) {
    // 内置屏幕
    if (id == AXIS_SCR_HOME)   { _renderHomeScreen(xOff, yOff);   return; }
    if (id == AXIS_SCR_PLAYER) { _renderPlayerScreen(xOff, yOff); return; }

    // 用户注册屏幕
    AxisScreen* scr = _findScreen(id);
    if (scr && scr->drawFn) scr->drawFn(*_main, xOff, yOff, scr->userData);
}

// ═════════════════════════════════════════════════
//  内置屏幕：SCR_HOME（桌面）
// ═════════════════════════════════════════════════

void AXIS_UI::_renderHomeScreen(int16_t xOff, int16_t yOff) {
    _main->fillScreen(AXIS_C_BG);
    int16_t W = mainW();

    // ── 左上角大时间 ──────────────────────────────
    // 用 U8g2 大字体显示时间（需要用户提供时间字符串）
    // 这里用占位，实际从外部注入
    _u8f.setFont(u8g2_font_logisoso28_tr);
    _u8f.setForegroundColor(AXIS_C_ACCENT);
    _u8f.setBackgroundColor(AXIS_C_BG);
    _u8f.setCursor(xOff + 2, yOff + 34);
    _u8f.print("21:46");

    // 日期（次大）
    _u8f.setFont(u8g2_font_logisoso16_tr);
    _u8f.setForegroundColor(AXIS_C_TEXT);
    _u8f.setCursor(xOff + 2, yOff + 54);
    _u8f.print("MON 16 MAR");

    // ── 当前最重要传感器（大字显示）─────────────────
    if (_sensor.valid) {
        // 判断最重要的一项
        String label, value;
        uint16_t color = AXIS_C_MUTED;

        if (_sensor.co2 >= AXIS_CO2_DANGER || _sensor.tvoc >= AXIS_TVOC_DANGER) {
            // 危险：红色，CO2或TVOC
            label = (_sensor.co2 >= AXIS_CO2_DANGER) ? "CO2" : "TVOC";
            value = (_sensor.co2 >= AXIS_CO2_DANGER)
                ? String(_sensor.co2) + "ppm"
                : String(_sensor.tvoc, 1) + "mg";
            color = AXIS_C_DANGER;
        } else if (_sensor.co2 >= AXIS_CO2_WARN || _sensor.tvoc >= AXIS_TVOC_WARN) {
            label = (_sensor.co2 >= AXIS_CO2_WARN) ? "CO2" : "TVOC";
            value = (_sensor.co2 >= AXIS_CO2_WARN)
                ? String(_sensor.co2) + "ppm"
                : String(_sensor.tvoc, 1) + "mg";
            color = AXIS_C_WARN;
        } else {
            // 正常：轮换显示
            switch (_sensorCycle) {
                case 0: label="TEMP"; value=String(_sensor.temp,1)+"C";    break;
                case 1: label="HUM";  value=String(_sensor.humidity,0)+"%"; break;
                case 2: label="PRES"; value=String(_sensor.pressure,0)+"hPa"; break;
            }
            color = AXIS_C_MUTED;
        }

        // 标签小字
        _u8f.setFont(u8g2_font_logisoso16_tr);
        _u8f.setForegroundColor(color);
        _u8f.setCursor(xOff + 2, yOff + 72);
        _u8f.print(label);

        // 值大字
        _u8f.setFont(u8g2_font_logisoso16_tr);
        _u8f.setCursor(xOff + 40, yOff + 72);
        _u8f.print(value);
    }

    // ── 分割线 ────────────────────────────────────
    _main->drawFastHLine(xOff, yOff + 78, W, AXIS_C_DIVIDER);

    // ── 媒体缩略 ──────────────────────────────────
    _u8f.setFont(u8g2_font_logisoso16_tr);
    _u8f.setForegroundColor(_playing ? AXIS_C_ACCENT : AXIS_C_DIM);
    _u8f.setCursor(xOff + 2, yOff + 94);
    _u8f.print(_playing ? ">" : "||");

    _u8f.setForegroundColor(AXIS_C_TEXT);
    _u8f.setCursor(xOff + 18, yOff + 94);
    _u8f.print(truncate(_trackTitle, 12));

    // 进度条（细）
    _main->drawFastHLine(xOff, yOff + 100, W, AXIS_C_DIVIDER);
    int pw = (int)(_smoothProgress.value() * W);
    if (pw > 0) _main->drawFastHLine(xOff, yOff + 100, pw, AXIS_C_ACCENT);

    // ── 状态栏（底部一行）────────────────────────
    _main->drawFastHLine(xOff, yOff + 114, W, AXIS_C_DIVIDER);

    // BT 状态
    _main->fillRect(xOff + 2, yOff + 117, 6, 6,
                    _btConn ? AXIS_C_ACCENT : AXIS_C_DIM);

    // 电량
    drawBattery(xOff + W - 28, yOff + 117, _battery);

    // 音量
    _u8f.setFont(u8g2_font_tom_thumb_4x6_tr);
    _u8f.setForegroundColor(AXIS_C_MUTED);
    _u8f.setCursor(xOff + 12, yOff + 123);
    char buf[12];
    snprintf(buf, sizeof(buf), "VOL%d%%", _volume);
    _u8f.print(buf);
}

// ═════════════════════════════════════════════════
//  内置屏幕：SCR_PLAYER（播放器）
// ═════════════════════════════════════════════════

void AXIS_UI::_renderPlayerScreen(int16_t xOff, int16_t yOff) {
    _main->fillScreen(AXIS_C_BG);
    int16_t W = mainW();

    // ── 顶栏 ──────────────────────────────────────
    // 硬直角，细线分割
    _u8f.setFont(u8g2_font_tom_thumb_4x6_tr);
    _u8f.setForegroundColor(_btConn ? AXIS_C_ACCENT : AXIS_C_DIM);
    _u8f.setCursor(xOff + 2, yOff + 7);
    _u8f.print(_btConn ? "BT" : "--");

    // 播放状态
    _u8f.setForegroundColor(_playing ? AXIS_C_ACCENT : AXIS_C_MUTED);
    _u8f.setCursor(xOff + 16, yOff + 7);
    _u8f.print(_playing ? "PLAY" : "STOP");

    // 音量
    _u8f.setForegroundColor(AXIS_C_MUTED);
    _u8f.setCursor(xOff + W - 28, yOff + 7);
    char vbuf[8]; snprintf(vbuf, sizeof(vbuf), "V%d", _volume);
    _u8f.print(vbuf);

    _main->drawFastHLine(xOff, yOff + 10, W, AXIS_C_DIVIDER);

    // ── 歌名（大字）──────────────────────────────
    _u8f.setFont(u8g2_font_logisoso16_tr);
    _u8f.setForegroundColor(AXIS_C_TEXT);
    _u8f.setCursor(xOff + 2, yOff + 32);
    _u8f.print(truncate(_trackTitle, 10));

    // ── 艺术家 ────────────────────────────────────
    _u8f.setFont(u8g2_font_tom_thumb_4x6_tr);
    _u8f.setForegroundColor(AXIS_C_ACCENT);
    _u8f.setCursor(xOff + 2, yOff + 42);
    _u8f.print(truncate(_trackArtist, 22));

    // ── 分割线 ────────────────────────────────────
    _main->drawFastHLine(xOff, yOff + 48, W, AXIS_C_DIVIDER);

    // ── 进度条（主视觉元素）──────────────────────
    // 背景轨道
    _main->drawFastHLine(xOff + 2, yOff + 62, W - 4, AXIS_C_DIM);
    // 填充（橙色）
    int filled = (int)(_smoothProgress.value() * (W - 4));
    if (filled > 0)
        _main->drawFastHLine(xOff + 2, yOff + 62, filled, AXIS_C_ACCENT);
    // 播放头（亮白方块，硬直角）
    if (filled > 0)
        _main->fillRect(xOff + 2 + filled - 2, yOff + 60, 4, 4, AXIS_C_WHITE);

    // 时间
    _u8f.setFont(u8g2_font_tom_thumb_4x6_tr);
    _u8f.setForegroundColor(AXIS_C_MUTED);
    _u8f.setCursor(xOff + 2, yOff + 74);
    _u8f.print(formatTime((int32_t)(_smoothProgress.value() * 210)));
    String dur = formatTime(210);
    _u8f.setCursor(xOff + W - dur.length()*4, yOff + 74);
    _u8f.print(dur);

    // ── 音量条 ────────────────────────────────────
    _main->drawFastHLine(xOff, yOff + 80, W, AXIS_C_DIVIDER);
    _u8f.setForegroundColor(AXIS_C_DIM);
    _u8f.setCursor(xOff + 2, yOff + 90);
    _u8f.print("VOL");
    // 音量填充
    _main->drawFastHLine(xOff + 20, yOff + 87, W - 22, AXIS_C_DIM);
    int vw = (int)(_smoothVolume.value() * (W - 22));
    if (vw > 0)
        _main->drawFastHLine(xOff + 20, yOff + 87, vw, AXIS_C_ACCENT);

    // ── VU 竖条（4根）────────────────────────────
    // 仅播放时显示
    if (_playing) {
        for (int i = 0; i < 4; i++) {
            float lv = 0.3f + (float)(millis() % (200 + i*37)) / (200.0f + i*37);
            lv = constrain(lv, 0.0f, 1.0f);
            int bh = (int)(lv * 16);
            _main->fillRect(xOff + 2 + i * 8, yOff + 100 - bh, 5, bh,
                            AxisAnim::dimColor(AXIS_C_ACCENT, lv));
        }
    }

    // ── 底部操作提示 ──────────────────────────────
    _main->drawFastHLine(xOff, yOff + 114, W, AXIS_C_DIVIDER);
    _u8f.setFont(u8g2_font_tom_thumb_4x6_tr);
    _u8f.setForegroundColor(AXIS_C_DIM);
    _u8f.setCursor(xOff + 2, yOff + 123);
    _u8f.print("SET:V+  RST:V-  MID:PLAY  L/R:SKIP");
}

// ═════════════════════════════════════════════════
//  过渡动画
// ═════════════════════════════════════════════════

void AXIS_UI::_calcTransOffsets(float progress,
                                 int16_t& fromX, int16_t& fromY,
                                 int16_t& toX,   int16_t& toY) {
    float e   = AxisAnim::easeOutCubic(progress);
    int16_t W = mainW(), H = mainH();
    fromX = fromY = toX = toY = 0;

    switch (_transType) {
        case AXIS_TRANS_SLIDE_LEFT:
            fromX = -(int16_t)(e * W);
            toX   =  (int16_t)((1.0f-e) * W);
            break;
        case AXIS_TRANS_SLIDE_RIGHT:
            fromX =  (int16_t)(e * W);
            toX   = -(int16_t)((1.0f-e) * W);
            break;
        case AXIS_TRANS_SLIDE_UP:
            fromY = -(int16_t)(e * H);
            toY   =  (int16_t)((1.0f-e) * H);
            break;
        case AXIS_TRANS_SLIDE_DOWN:
            fromY =  (int16_t)(e * H);
            toY   = -(int16_t)((1.0f-e) * H);
            break;
        default: break;
    }
}

void AXIS_UI::_renderTransition() {
    int16_t fx, fy, tx, ty;
    _calcTransOffsets(_transProgress, fx, fy, tx, ty);
    _main->fillScreen(AXIS_C_BG);
    if (_transFromID != AXIS_SCR_NONE) _renderScreen(_transFromID, fx, fy);
    if (_transToID   != AXIS_SCR_NONE) _renderScreen(_transToID,   tx, ty);
}

// ═════════════════════════════════════════════════
//  菜单覆盖层（硬直角终端风格）
// ═════════════════════════════════════════════════

void AXIS_UI::_renderMenuOverlay() {
    float   ease = AxisAnim::easeOutCubic(_menuSlide);
    int16_t W    = mainW();
    int16_t H    = mainH();
    int16_t oy   = (int16_t)((1.0f - ease) * H);

    // 面板背景（硬直角）
    _main->fillRect(0, oy, W, H - oy, AXIS_C_BG2);
    // 顶边线（橙色，粗一点）
    _main->drawFastHLine(0, oy, W, AXIS_C_ACCENT);
    _main->drawFastHLine(0, oy+1, W, AXIS_C_ACCENT);

    // 标题
    _u8f.setFont(u8g2_font_tom_thumb_4x6_tr);
    _u8f.setForegroundColor(AXIS_C_ACCENT);
    _u8f.setCursor(2, oy + 10);
    _u8f.print("MENU");

    // 右侧关闭提示
    _u8f.setForegroundColor(AXIS_C_DIM);
    _u8f.setCursor(W - 26, oy + 10);
    _u8f.print("[A:BACK]");

    _main->drawFastHLine(0, oy + 12, W, AXIS_C_DIVIDER);

    // 光标高亮（橙色左边条 + 暗背景）
    float   cY  = _menuCursorAnim.value();
    int16_t hlY = oy + 14 + (int16_t)(cY * 20);
    _main->fillRect(0, hlY, W, 18, AXIS_C_CARD);
    _main->fillRect(0, hlY, 2, 18, AXIS_C_ACCENT);  // 左边条

    // 菜单项
    for (uint8_t i = 0; i < _menuCount; i++) {
        int16_t iy  = oy + 14 + i * 20;
        bool    sel = (i == (uint8_t)_menuCursor);

        _u8f.setFont(u8g2_font_tom_thumb_4x6_tr);
        _u8f.setForegroundColor(sel ? _menuItems[i].color : AXIS_C_MUTED);
        _u8f.setCursor(6, iy + 13);
        _u8f.print(_menuItems[i].label);

        // 选中项右侧箭头
        if (sel) {
            _main->fillRect(W - 8, iy + 7, 4, 4, _menuItems[i].color);
        }
    }
}

// ═════════════════════════════════════════════════
//  通知覆盖层（从顶部滑入，硬直角）
// ═════════════════════════════════════════════════

void AXIS_UI::_renderNotifOverlay() {
    int16_t W    = mainW();
    float   ease = AxisAnim::easeOutCubic(_notif.slideY);
    int16_t h    = 14;
    int16_t y    = (int16_t)((ease - 1.0f) * h);  // 从顶部滑入

    // 背景（硬直角，无圆角）
    _main->fillRect(0, y, W, h, AXIS_C_BG2);
    // 底边线（通知色）
    _main->drawFastHLine(0, y + h - 1, W, _notif.color);
    _main->drawFastHLine(0, y + h - 2, W, _notif.color);

    // 文字
    _u8f.setFont(u8g2_font_tom_thumb_4x6_tr);
    _u8f.setForegroundColor(_notif.color);
    _u8f.setCursor(2, y + 9);
    _u8f.print(truncate(_notif.text, (W - 4) / 4));
}

// ═════════════════════════════════════════════════
//  副屏渲染
// ═════════════════════════════════════════════════

void AXIS_UI::_renderSubDisplay() {
    if (!_sub || !_statusCb) return;
    _statusCb(*_sub, _statusData);
    _flushSub();
}

void AXIS_UI::_flushMain() { if (_mainFlush) _mainFlush(); }
void AXIS_UI::_flushSub()  { if (_subFlush)  _subFlush();  }

// ═════════════════════════════════════════════════
//  屏幕管理
// ═════════════════════════════════════════════════

void AXIS_UI::registerScreen(AxisScreenID id,
                              AxisDrawCallback drawFn,
                              AxisInputCallback inputFn,
                              void* userData) {
    if (_screenCount >= AXIS_MAX_SCREENS) return;
    if (!drawFn) return;
    _screens[_screenCount++] = {id, drawFn, inputFn, userData, true};
}

void AXIS_UI::_stackPush(AxisScreenID id) {
    if (_stackDepth < 8) _stack[_stackDepth++] = id;
}

AxisScreenID AXIS_UI::_stackPop() {
    if (_stackDepth == 0) return AXIS_SCR_HOME;
    return _stack[--_stackDepth];
}

void AXIS_UI::goTo(AxisScreenID id, AxisTransition trans) {
    if (id == _curID) return;

    _stackPush(_curID);
    _prevID = _curID;

    if (trans == AXIS_TRANS_NONE) {
        _curID = id;
        _main->fillScreen(AXIS_C_BG);
    } else {
        _transFromID   = _curID;
        _transToID     = id;
        _transType     = trans;
        _transProgress = 0.0f;
        _transStartMs  = millis();
        _transActive   = true;
    }
}

void AXIS_UI::goBack(AxisTransition trans) {
    AxisScreenID prev = _stackPop();
    if (prev != AXIS_SCR_NONE) {
        _prevID = _curID;
        if (trans == AXIS_TRANS_NONE) {
            _curID = prev;
            _main->fillScreen(AXIS_C_BG);
        } else {
            _transFromID   = _curID;
            _transToID     = prev;
            _transType     = trans;
            _transProgress = 0.0f;
            _transStartMs  = millis();
            _transActive   = true;
        }
    }
}

void AXIS_UI::goHome() {
    _stackDepth = 0;
    goTo(AXIS_SCR_HOME, AXIS_TRANS_SLIDE_RIGHT);
}

AxisScreen* AXIS_UI::_findScreen(AxisScreenID id) {
    for (uint8_t i = 0; i < _screenCount; i++)
        if (_screens[i].valid && _screens[i].id == id) return &_screens[i];
    return nullptr;
}

// ═════════════════════════════════════════════════
//  菜单
// ═════════════════════════════════════════════════

void AXIS_UI::showMenu(const AxisMenuItem* items, uint8_t count,
                        AxisInputCallback onSelect, void* userData) {
    if (!items || count == 0) return;
    _menuItems      = items;
    _menuCount      = min(count, (uint8_t)AXIS_MAX_MENU_ITEMS);
    _menuOnSelect   = onSelect;
    _menuSelData    = userData;
    _menuCursor     = 0;
    _menuCursorAnim.snap(0.0f);
    _menuTarget     = 1.0f;
}

void AXIS_UI::hideMenu() { _menuTarget = 0.0f; }

// ═════════════════════════════════════════════════
//  通知
// ═════════════════════════════════════════════════

void AXIS_UI::notify(const String& text, uint16_t color, uint32_t durationMs) {
    _notif.text     = text;
    _notif.color    = color;
    _notif.expireAt = millis() + durationMs;
    _notif.active   = true;
    _notif.slideY   = 0.0f;
}

// ═════════════════════════════════════════════════
//  绘图工具
// ═════════════════════════════════════════════════

void AXIS_UI::drawGradBar(int x, int y, int w, int h,
                           float ratio, uint16_t c1, uint16_t c2,
                           uint16_t bg) {
    // 终端风格：无圆角，硬直角进度条
    _main->fillRect(x, y, w, h, bg);
    int filled = constrain((int)(ratio * w), 0, w);
    for (int i = 0; i < filled; i++) {
        float t = (w > 1) ? (float)i / (float)(w-1) : 0.0f;
        _main->drawFastVLine(x+i, y, h, AxisAnim::lerpColor(c1, c2, t));
    }
}

void AXIS_UI::drawProgressBar(int x, int y, int w, int h,
                               float ratio, uint16_t col) {
    _main->drawRect(x, y, w, h, AXIS_C_DIVIDER);
    int filled = constrain((int)(ratio * (w-2)), 0, w-2);
    if (filled > 0) _main->fillRect(x+1, y+1, filled, h-2, col);
}

void AXIS_UI::drawVUBar(int x, int y, int w, int h, float level) {
    level = constrain(level, 0.0f, 1.0f);
    _main->fillRect(x, y, w, h, AXIS_C_CARD);
    int filled = (int)(level * h);
    for (int i = 0; i < filled; i++) {
        float t = (h > 1) ? (float)i / (float)(h-1) : 0.0f;
        _main->drawFastHLine(x, y+h-1-i, w,
                             AxisAnim::lerpColor(AXIS_C_ACCENT, AXIS_C_DANGER, t));
    }
}

void AXIS_UI::drawBattery(int x, int y, uint8_t pct) {
    // 硬直角电量图标
    _main->drawRect(x, y, 18, 7, AXIS_C_MUTED);
    _main->fillRect(x+18, y+2, 2, 3, AXIS_C_MUTED);
    uint16_t col = pct > 20 ? AXIS_C_ACCENT : AXIS_C_DANGER;
    int fill = (int)(pct / 100.0f * 16);
    if (fill > 0) _main->fillRect(x+1, y+1, fill, 5, col);
}

void AXIS_UI::drawSignalDots(int x, int y, uint8_t strength) {
    for (uint8_t i = 0; i < 4; i++) {
        uint16_t c = (i < strength) ? AXIS_C_ACCENT : AXIS_C_DIM;
        _main->fillRect(x + i*5, y + (3-i)*3, 3, (i+1)*3, c);
    }
}

// ── U8g2 文字接口 ─────────────────────────────────

void AXIS_UI::setFont(const uint8_t* font) {
    _u8f.setFont(font);
}

void AXIS_UI::setFontColor(uint16_t color) {
    _u8f.setForegroundColor(color);
    _u8f.setBackgroundColor(AXIS_C_BG);
}

void AXIS_UI::drawText(int16_t x, int16_t y, const String& s) {
    _u8f.setCursor(x, y);
    _u8f.print(s);
}

void AXIS_UI::drawText(int16_t x, int16_t y, const char* s) {
    _u8f.setCursor(x, y);
    _u8f.print(s);
}

uint16_t AXIS_UI::textWidth(const String& s) {
    return _u8f.getUTF8Width(s.c_str());
}

uint16_t AXIS_UI::textHeight() {
    return _u8f.getFontAscent() - _u8f.getFontDescent();
}

// ── 静态工具 ──────────────────────────────────────

String AXIS_UI::truncate(const String& s, int maxChars) {
    if (maxChars <= 0) return "";
    if ((int)s.length() <= maxChars) return s;
    return s.substring(0, maxChars-1) + "~";
}

String AXIS_UI::formatTime(int32_t s) {
    if (s < 0) s = 0;
    char buf[9];
    snprintf(buf, sizeof(buf), "%d:%02d", (int)(s/60), (int)(s%60));
    return String(buf);
}

String AXIS_UI::padLeft(const String& s, int width, char c) {
    String r = s;
    while ((int)r.length() < width) r = c + r;
    return r;
}

// ═════════════════════════════════════════════════
//  输入
// ═════════════════════════════════════════════════

AxisInputEvent AXIS_UI::_pollInput() {
    if (_pressed(_pins.joy_up,    0)) return AXIS_INPUT_UP;
    if (_pressed(_pins.joy_down,  1)) return AXIS_INPUT_DOWN;
    if (_pressed(_pins.joy_left,  2)) return AXIS_INPUT_LEFT;
    if (_pressed(_pins.joy_right, 3)) return AXIS_INPUT_RIGHT;
    if (_pressed(_pins.joy_ok,    4)) return AXIS_INPUT_OK;

    // 长按检测
    if (_longPressed(_pins.btn_a, 5)) return AXIS_INPUT_BTN_A_LONG;
    if (_pressed(_pins.btn_a,     5)) return AXIS_INPUT_BTN_A;
    if (_longPressed(_pins.btn_b, 6)) return AXIS_INPUT_BTN_B_LONG;
    if (_pressed(_pins.btn_b,     6)) return AXIS_INPUT_BTN_B;

    return AXIS_INPUT_NONE;
}

bool AXIS_UI::_pressed(int8_t pin, uint8_t idx) {
    if (pin < 0) return false;
    if (digitalRead(pin) == LOW && millis() - _debounce[idx] > DB_MS) {
        _debounce[idx] = millis();
        return true;
    }
    return false;
}

bool AXIS_UI::_longPressed(int8_t pin, uint8_t idx) {
    if (pin < 0) return false;
    if (digitalRead(pin) == LOW) {
        if (_pressStart[idx] == 0) _pressStart[idx] = millis();
        if (!_held[idx] && millis() - _pressStart[idx] > AXIS_LONG_PRESS_MS) {
            _held[idx] = true;
            return true;
        }
    } else {
        _pressStart[idx] = 0;
        _held[idx] = false;
    }
    return false;
}