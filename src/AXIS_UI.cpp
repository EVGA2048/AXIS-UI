#include "AXIS_UI.h"
#include <math.h>
#include <U8g2lib.h>

// U8g2 中文字体（文泉驿 WQY，gb2312b 约6000字）
static const lgfx::U8g2font _font_zh_12(u8g2_font_wqy12_t_gb2312b);
static const lgfx::U8g2font _font_zh_16(u8g2_font_wqy16_t_gb2312b);

// ═════════════════════════════════════════════════
//  显示器绑定
// ═════════════════════════════════════════════════

void AXIS_UI::setMainDisplay(LGFX_Device* disp, AxisFlushCallback flushFn) {
    _main      = disp;
    _mainFlush = flushFn;
    delete _spr;
    _spr = new LGFX_Sprite(disp);
}

void AXIS_UI::setSubDisplay(LGFX_Device* disp, AxisFlushCallback flushFn) {
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
    // 同步到灵动消息栏 slot 0
    if (data.valid) {
        char tbuf[32] = {};
        if (data.co2 >= AXIS_CO2_WARN)
            snprintf(tbuf, sizeof(tbuf), "CO2 %dppm", data.co2);
        else
            snprintf(tbuf, sizeof(tbuf), "%.1fC %.0f%% %.0fhPa",
                     data.temp, data.humidity, data.pressure);
        strncpy(_ticker[0].text, tbuf, 31);
        _ticker[0].color = (data.co2 >= AXIS_CO2_WARN) ? AXIS_C_WARN : AXIS_C_DIM;
        _ticker[0].valid = true;
    }
}

void AXIS_UI::setTickerItem(uint8_t slot, const String& text, uint16_t color) {
    if (slot >= AXIS_MAX_TICKER) return;
    strncpy(_ticker[slot].text, text.c_str(), 31);
    _ticker[slot].text[31] = '\0';
    _ticker[slot].color = color;
    _ticker[slot].valid = true;
}

void AXIS_UI::clearTickerItem(uint8_t slot) {
    if (slot >= AXIS_MAX_TICKER) return;
    _ticker[slot].valid = false;
    _ticker[slot].text[0] = '\0';
}

void AXIS_UI::setTrackTitle (const String& s) { _trackTitle  = s; }
void AXIS_UI::setTrackArtist(const String& s) { _trackArtist = s; }
void AXIS_UI::setPlaying    (bool p)          { _playing     = p; _lightCmd(p ? AXIS_LIGHT_VU : AXIS_LIGHT_STATIC, 0x202000, 30); }
void AXIS_UI::setVolume     (uint8_t v)       { _volume      = constrain(v,0,100); _smoothVolume.setTarget(v/100.0f); }
void AXIS_UI::setProgress   (float r)         { _progress    = constrain(r,0.0f,1.0f); _smoothProgress.setTarget(r); }
void AXIS_UI::setBTConnected(bool c)          { _btConn      = c; }
void AXIS_UI::setBattery    (uint8_t p)       { _battery     = constrain(p,0,100); }
void AXIS_UI::setWalking    (bool w)          { _walking     = w; }
void AXIS_UI::setTime(const String& s)        { _timeStr     = s; }
void AXIS_UI::setDate(const String& s)        { _dateStr     = s; }

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

void AXIS_UI::setJoyRotation(uint8_t rot) {
    _rotation = (AxisRotation)(rot & 3);
    Serial.printf("[AXIS] JoyRotation: %d\n", rot & 3);
}

// 根据语言选字体
void AXIS_UI::_setFont(uint8_t size) {
    if (_lang == AXIS_LANG_ZH) {
        if (size == 1) _spr->setFont(&_font_zh_12);
        else           _spr->setFont(&_font_zh_16);
    } else {
        _spr->setTextFont(size);
    }
}

// 小立方体线框渲染
void AXIS_UI::_drawMiniCube(int16_t cx, int16_t cy,
                             float rx, float ry,
                             float sz, uint16_t col) {
    static const int8_t VX[8] = {-1, 1, 1,-1,-1, 1, 1,-1};
    static const int8_t VY[8] = {-1,-1, 1, 1,-1,-1, 1, 1};
    static const int8_t VZ[8] = {-1,-1,-1,-1, 1, 1, 1, 1};
    static const uint8_t ED[12][2] = {
        {0,1},{1,2},{2,3},{3,0},{4,5},{5,6},{6,7},{7,4},
        {0,4},{1,5},{2,6},{3,7}
    };
    int16_t px[8], py[8];
    for (int i = 0; i < 8; i++) {
        float x=(float)VX[i], y=(float)VY[i], z=(float)VZ[i];
        float tx=x*cosf(ry)+z*sinf(ry), tz=-x*sinf(ry)+z*cosf(ry);
        x=tx; z=tz;
        float ty=y*cosf(rx)-z*sinf(rx), tz2=y*sinf(rx)+z*cosf(rx);
        y=ty; z=tz2;
        float s = sz * 3.5f / (3.5f + z);
        px[i]=(int16_t)(cx + x*s);
        py[i]=(int16_t)(cy + y*s);
    }
    for (int e = 0; e < 12; e++)
        _line(px[ED[e][0]], py[ED[e][0]], px[ED[e][1]], py[ED[e][1]], col);
}

AxisInputEvent AXIS_UI::_remapInput(AxisInputEvent ev) {
    if (_rotation == AXIS_ROT_0) return ev;
    if (ev < AXIS_INPUT_UP || ev > AXIS_INPUT_RIGHT) return ev;

    const AxisInputEvent maps[4][4] = {
        {AXIS_INPUT_UP,    AXIS_INPUT_DOWN,  AXIS_INPUT_LEFT,  AXIS_INPUT_RIGHT},
        {AXIS_INPUT_RIGHT, AXIS_INPUT_LEFT,  AXIS_INPUT_UP,    AXIS_INPUT_DOWN},
        {AXIS_INPUT_DOWN,  AXIS_INPUT_UP,    AXIS_INPUT_RIGHT, AXIS_INPUT_LEFT},
        {AXIS_INPUT_LEFT,  AXIS_INPUT_RIGHT, AXIS_INPUT_DOWN,  AXIS_INPUT_UP},
    };
    int idx = ev - AXIS_INPUT_UP;
    return maps[(uint8_t)_rotation][idx];
}

// ═════════════════════════════════════════════════
//  初始化
// ═════════════════════════════════════════════════

bool AXIS_UI::begin(const AxisPinConfig& pins) {
    if (!_main || !_spr) {
        Serial.println("[AXIS] ERROR: setMainDisplay() first.");
        return false;
    }
    _pins = pins;

    // 创建离屏 sprite（16位色，128×128 = 32KB PSRAM/SRAM）
    _spr->setColorDepth(16);
    if (!_spr->createSprite(mainW(), mainH())) {
        Serial.println("[AXIS] ERROR: sprite alloc failed.");
        return false;
    }
    _spr->setTextWrap(false);   // 超出屏幕的文字直接截断，不环绕到另一侧
    // LovyanGFX 自动处理字节序，无需 setSwapBytes

    // 输入引脚
    int8_t pullupPins[] = {
        pins.joy_up, pins.joy_down, pins.joy_left,
        pins.joy_right, pins.joy_ok, pins.btn_a, pins.btn_b
    };
    for (int8_t p : pullupPins) if (p >= 0) pinMode(p, INPUT_PULLUP);

    // 初始化动画值
    _smoothProgress = AxisAnimValue(0.0f, 0.08f);
    _smoothVolume   = AxisAnimValue(0.75f, 0.1f);
    _menuCursorAnim = AxisAnimValue(0.0f, 0.15f);

    _sensorTimer  = AxisTimer(4000);
    _sensorTimer.reset();
    _tickerNextMs = millis() + 4000;  // 首条显示 4s 后再开始轮换
    _lastFrameAt  = millis();

    Serial.printf("[AXIS] Init OK. %dx%d sprite ready.\n", mainW(), mainH());
    return true;
}

void AXIS_UI::setFPS(uint8_t fps) {
    _frameMs = (fps > 0) ? (1000 / fps) : 33;
}

void AXIS_UI::setCustomFlag(bool on, uint16_t color) {
    _customFlag      = on;
    _customFlagColor = color;
}

void AXIS_UI::setFPSOverlay(bool on) {
    _showFPS  = on;
    _fpsCount = 0;
    _fpsLastMs = millis();
}

void AXIS_UI::triggerHomeCubePulse() {
    _homeCubePulse = 1.0f;   // sin 波：0→峰值→0，约 55 帧 ~0.8s
}

// ═════════════════════════════════════════════════
//  主循环
// ═════════════════════════════════════════════════

void AXIS_UI::update() {
    if (!_main || !_spr) return;
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

    if (_transActive) {
        float elapsed = (float)(now - _transStartMs);
        _transProgress = elapsed / (float)TRANS_DURATION_MS;
        if (_transProgress >= 1.0f) {
            _transProgress = 1.0f;
            _transActive   = false;
            _curID         = _transToID;
            _spr->fillScreen(AXIS_C_BG);
        }
    }

    _menuSlide = AxisAnim::smoothFollow(_menuSlide, _menuTarget, 0.14f);
    if (_menuSlide < 0.005f && _menuTarget < 0.01f) _menuSlide = 0.0f;

    // 信息区切换闪烁：_nodeInfoSlide 由 _homeNodeInput 触发为 1.0，此处自然衰减
    _nodeInfoSlide = AxisAnim::smoothFollow(_nodeInfoSlide, 0.0f, 0.12f);
    if (_nodeInfoSlide < 0.01f) _nodeInfoSlide = 0.0f;

    // 小立方体旋转衰减（切换节点时触发，然后停住）
    if (_cubeSpinV > 0.01f) {
        _homeCubeAngX += _cubeSpinV * 0.011f;
        _homeCubeAngY += _cubeSpinV * 0.017f;
        _cubeSpinV *= 0.93f;
        if (_cubeSpinV < 0.02f) _cubeSpinV = 0.0f;
    }

    // 节点图整体偏移动画（选中节点平移到中心）
    {
        float mapTargetX = 0.0f, mapTargetY = 0.0f;
        if (_nodeCursor >= 0 && _nodeCursor < (int8_t)_nodeCount) {
            int16_t tx, ty;
            _getNodeScreenPos(_nodeCursor, tx, ty);
            mapTargetX = (float)(AXIS_NODE_CX - tx);
            mapTargetY = (float)(AXIS_NODE_CY - ty);
        }
        _cursorAnimX = AxisAnim::smoothFollow(_cursorAnimX, mapTargetX, 0.18f);
        _cursorAnimY = AxisAnim::smoothFollow(_cursorAnimY, mapTargetY, 0.18f);
    }

    _menuCursorAnim.update();
    _smoothProgress.update();
    _smoothVolume.update();

    // 光标吸附进度动画（snapT 平滑趋向目标）
    float snapTarget = (_hoveredNode >= 0) ? 1.0f : 0.0f;
    _snapT = AxisAnim::smoothFollow(_snapT, snapTarget, 0.20f);
    if (_snapT < 0.01f) _snapT = 0.0f;
    if (_snapT > 0.99f) _snapT = 1.0f;

    if (_notif.active) {
        _notif.slideY = AxisAnim::smoothFollow(_notif.slideY, 1.0f, 0.18f);
        if (now >= _notif.expireAt) { _notif.active = false; _notif.slideY = 0.0f; }
    }

    if (!_sensorAlert && _sensor.valid && _sensorTimer.tick()) {
        _sensorCycle++;
        if (_sensorCycle > 2) _sensorCycle = 0;
    }

    // 灵动消息栏：每 4s 切换到下一个有效 slot
    if (now >= _tickerNextMs) {
        uint8_t next = _tickerIdx;
        for (uint8_t i = 1; i <= AXIS_MAX_TICKER; i++) {
            uint8_t c = (_tickerIdx + i) % AXIS_MAX_TICKER;
            if (_ticker[c].valid) { next = c; break; }
        }
        if (next != _tickerIdx) {
            _tickerIdx         = next;
            _tickerGlow        = 3.0f;  // 3 次闪烁出场（每 1.0 = 一次，0.5s）
            _tickerScrollOff   = 0;
            _tickerScrollPhase = 0;
            _tickerScrollMs    = 0;     // glow 结束后再初始化
        }
        _tickerNextMs = now + 5000;     // 显示 5s（含 ~1.5s 闪烁 + 滚动时间）
    }
    if (_tickerGlow > 0.0f) {
        _tickerGlow -= 0.03f;
        if (_tickerGlow <= 0.0f) {
            _tickerGlow     = 0.0f;
            _tickerScrollMs = now;      // 闪烁结束 → 立即进入 scroll phase 0
        }
    }

    if (_bubble.active) {
        _bubble.slideY = AxisAnim::smoothFollow(_bubble.slideY, 1.0f, 0.15f);
        if (now >= _bubble.expireAt) { _bubble.active = false; _bubble.slideY = 0.0f; }
    }

    // 立方体尺寸：HOME 节点上呼吸，其他节点收敛到默认尺寸 12
    {
        float szTarget;
        if (_curID == AXIS_SCR_HOME && _nodeCursor == -1) {
            float bt = fmodf((float)now / 2200.0f, 1.0f);
            float tri = bt < 0.5f ? bt * 2.0f : (1.0f - bt) * 2.0f;
            szTarget = 12.0f + tri * 3.6f - 1.8f;
        } else {
            szTarget = 12.0f;
        }
        _cubeSize = AxisAnim::smoothFollow(_cubeSize, szTarget, 0.12f);
    }

    // HOME 立方体：长时间不操作缓慢持续旋转；消息到达时脉冲大小
    if (_curID == AXIS_SCR_HOME && _nodeCursor == -1) {
        uint32_t idleMs = now - _idleLastInputMs;
        if (idleMs > 8000) {
            // 闲置 8s 后开始旋转，前 3s 线性加速至最高速
            float ramp = constrain((float)(idleMs - 8000) / 3000.0f, 0.0f, 1.0f);
            _homeCubeAngY += 0.006f * ramp;
            _homeCubeAngX += 0.0022f * ramp;
        }
    }
    // 脉冲衰减（无论在哪个屏幕都衰减，确保动画完整播放）
    if (_homeCubePulse > 0.0f) {
        _homeCubePulse -= 0.018f;
        if (_homeCubePulse < 0.0f) _homeCubePulse = 0.0f;
    }
}

// ═════════════════════════════════════════════════
//  输入处理
// ═════════════════════════════════════════════════

void AXIS_UI::_handleInput() {
    if (_transActive) return;

    AxisInputEvent raw = _pollInput();
    // 物理输入为空时，消费一次注入事件（重力光标等）
    if (raw == AXIS_INPUT_NONE) {
        if (_injectedInput == AXIS_INPUT_NONE) return;
        raw = _injectedInput;
        _injectedInput = AXIS_INPUT_NONE;
    }

    AxisInputEvent ev = _remapInput(raw);
    _idleLastInputMs = millis();   // 任何输入重置闲置计时，停止立方体旋转

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
            case AXIS_INPUT_OK: {
                const AxisMenuItem& item = _menuItems[_menuCursor];
                if (item.targetScreen != AXIS_SCR_NONE) {
                    // 跳转屏幕时立即关闭菜单（不走下滑动画），
                    // 避免下滑与水平过渡动画同时播放产生割裂感
                    _menuSlide  = 0.0f;
                    _menuTarget = 0.0f;
                    goTo(item.targetScreen, item.transition);
                } else {
                    hideMenu();  // 无跳转（自定义回调）时正常滑出
                    if (_menuOnSelect)
                        _menuOnSelect(ev, _menuSelData);
                }
                break;
            }
            case AXIS_INPUT_BTN_B:
                // 短按 B：关闭菜单，停在当前屏幕
                hideMenu();
                break;
            case AXIS_INPUT_BTN_B_LONG:
                // 长按 B：关闭菜单并直接回主页
                _menuSlide  = 0.0f;
                _menuTarget = 0.0f;
                goHome();
                break;
            case AXIS_INPUT_BTN_A:
                hideMenu();
                break;
            default: break;
        }
    } else {
        if (_curID == AXIS_SCR_HOME) _homeNodeInput(ev);

        // 非内置屏幕：BTN_B 全局返回
        if (_curID != AXIS_SCR_HOME && _curID != AXIS_SCR_PLAYER) {
            if (ev == AXIS_INPUT_BTN_B || ev == AXIS_INPUT_BTN_B_LONG) {
                goBack(AXIS_TRANS_SLIDE_RIGHT);
                return;
            }
        }

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
        // 光标叠加层：仅在空间屏且光标模式下渲染
        if (_isSpatialScreen() && _navMode == AXIS_NAVMODE_CURSOR) {
            _renderCursorOverlay(0, 0);
        }
        if (_notif.active)       _renderNotifOverlay();
        if (_bubble.active)      _renderBubbleOverlay();
        if (_menuSlide > 0.005f) _renderMenuOverlay();
    }

    // FPS 覆盖层（右下角，调试用）
    if (_showFPS) {
        uint32_t nowMs = millis();
        _fpsCount++;
        if (nowMs - _fpsLastMs >= 1000) {
            _fpsCurrent = _fpsCount;
            _fpsCount   = 0;
            _fpsLastMs  = nowMs;
        }
        char buf[6];
        snprintf(buf, sizeof(buf), "%d", _fpsCurrent);
        _spr->setTextFont(1);
        _spr->setTextColor(AXIS_C_ACCENT, AXIS_C_BG);
        _spr->setCursor(mainW() - 18, mainH() - 8);
        _spr->print(buf);
    }

    _flushMain();
}

void AXIS_UI::_renderScreen(AxisScreenID id, int16_t xOff, int16_t yOff) {
    if (id == AXIS_SCR_HOME)   { _renderHomeScreen(xOff, yOff);   return; }
    if (id == AXIS_SCR_PLAYER) { _renderPlayerScreen(xOff, yOff); return; }

    AxisScreen* scr = _findScreen(id);
    if (scr && scr->drawFn) scr->drawFn(*_spr, xOff, yOff, scr->userData);
}

// ═════════════════════════════════════════════════
//  内置屏幕：SCR_HOME
// ═════════════════════════════════════════════════
// 字体说明：
//   Font 1 = Glcdfont 6×8，替代 tom_thumb_4x6（稍大）
//   Font 2 = 16px 等宽，替代 logisoso16
// Y 坐标：TFT_eSPI 内置字体 setCursor y = 字符顶部（原 U8g2 y = 基线）
// ═════════════════════════════════════════════════

void AXIS_UI::_renderHomeScreen(int16_t xOff, int16_t yOff) {
    _fill(AXIS_C_BG);
    int16_t W = mainW();

    // ══ 顶部状态栏（y=0..11，12px）══════════════
    _rect(xOff + 2, yOff + 3, 5, 5,
          _btConn ? AXIS_C_ACCENT : AXIS_C_DIM);

    _spr->setTextFont(1);
    _spr->setTextColor(_playing ? AXIS_C_ACCENT : AXIS_C_DIM, AXIS_C_BG);
    _spr->setCursor(xOff + 10, yOff + 2);
    _spr->print(_playing ? ">" : "||");

    // 自定义标志位（调试模式等），显示在走路图标左侧
    if (_customFlag) {
        _rect(xOff + 22, yOff + 3, 5, 5, _customFlagColor);
    }

    // 走路图标（步行检测）
    if (_walking) {
        // 画一个小人走路：5×7 像素，橙色
        uint16_t wc = AXIS_C_ACCENT;
        int16_t wx = xOff + 29, wy = yOff + 2;
        _pixel(wx+2, wy,   wc);                          // 头
        _hLine(wx+1, wy+2, 3, wc);                       // 身
        _line (wx+1, wy+3, wx,   wy+6, wc);              // 左腿
        _line (wx+3, wy+3, wx+4, wy+6, wc);              // 右腿
        _line (wx+1, wy+2, wx,   wy+4, wc);              // 左手
        _line (wx+3, wy+2, wx+4, wy+4, wc);              // 右手
    }

    // 时间居中
    _spr->setTextColor(AXIS_C_TEXT, AXIS_C_BG);
    uint16_t tw = _spr->textWidth(_timeStr.c_str());
    _spr->setCursor(xOff + (W - tw) / 2, yOff + 2);
    _spr->print(_timeStr);

    drawBattery(xOff + W - 22, yOff + 3, _battery);
    _hLine(xOff, yOff + 11, W, AXIS_C_DIVIDER);

    // ══ 节点区域（y=12..69，58px）══════════════
    int16_t mapOffX = (int16_t)_cursorAnimX;
    int16_t mapOffY = (int16_t)_cursorAnimY;
    // HOME 原点 + 偏移
    int16_t hx = xOff + AXIS_NODE_CX + mapOffX;
    int16_t hy = yOff + AXIS_NODE_CY + 2 + mapOffY;
    // 光标环固定在屏幕中心（不随地图移动）
    int16_t fx = xOff + AXIS_NODE_CX;
    int16_t fy = yOff + AXIS_NODE_CY + 2;

    // 连线（选中的那条亮色；parentIdx>=0 时连向父节点而非 HOME）
    for (uint8_t i = 0; i < _nodeCount; i++) {
        if (!_nodes[i].valid) continue;
        int16_t nx, ny;
        _getNodeScreenPos((int8_t)i, nx, ny);
        nx += xOff + mapOffX; ny += yOff + 2 + mapOffY;
        // 线段终点：父节点坐标（-1=HOME）
        int16_t px, py;
        int8_t  pi = _nodes[i].parentIdx;
        if (pi >= 0 && pi < (int8_t)_nodeCount && _nodes[pi].valid) {
            _getNodeScreenPos(pi, px, py);
            px += xOff + mapOffX; py += yOff + 2 + mapOffY;
        } else {
            px = hx; py = hy;   // 默认连向 HOME
        }
        uint16_t lc = (_nodeCursor == (int8_t)i) ? _nodes[i].color : AXIS_C_DIVIDER;
        _line(px, py, nx, ny, lc);
    }

    // HOME 节点框（随地图偏移）
    int16_t hw = 24, hh = 10;
    _rect(hx - hw/2, hy - hh/2, hw, hh, AXIS_C_CARD);
    _spr->setTextFont(1);
    _spr->setTextColor(AXIS_C_ACCENT, AXIS_C_CARD);
    _spr->setCursor(hx - 12, hy - 3);
    _spr->print("HOME");

    // 功能节点框（随地图偏移）
    for (uint8_t i = 0; i < _nodeCount; i++) {
        if (!_nodes[i].valid) continue;
        int16_t nx, ny;
        _getNodeScreenPos((int8_t)i, nx, ny);
        nx += xOff + mapOffX; ny += yOff + 2 + mapOffY;

        // 盒子宽度随 shortTag 字符数自适应：6px/char + 10px 两侧留白，最小 26px
        uint8_t tl = (uint8_t)strnlen(_nodes[i].shortTag,
                                      sizeof(_nodes[i].shortTag));
        int16_t bw = (int16_t)(tl * 6 + 10); if (bw < 26) bw = 26;
        int16_t bh = 10;
        _rect(nx - bw/2, ny - bh/2, bw, bh, AXIS_C_CARD);
        _spr->setTextFont(1);
        _spr->setTextColor(_nodes[i].color, AXIS_C_CARD);
        uint16_t lw = _spr->textWidth(_nodes[i].shortTag);
        _spr->setCursor(nx - (int16_t)(lw / 2), ny - 3);
        _spr->print(_nodes[i].shortTag);
    }

    // 光标环：固定在屏幕中心，显示当前选中内容
    {
        bool onHome = (_nodeCursor == -1);
        int16_t cw, ch = 10;
        if (onHome) {
            cw = 24;
        } else {
            uint8_t ctl = (uint8_t)strnlen(_nodes[_nodeCursor].shortTag,
                                           sizeof(_nodes[_nodeCursor].shortTag));
            cw = (int16_t)(ctl * 6 + 10); if (cw < 26) cw = 26;
        }
        uint16_t cc = (_nodeCursor >= 0 && _nodeCursor < (int8_t)_nodeCount)
                      ? _nodes[_nodeCursor].color : AXIS_C_ACCENT;
        _rect(fx - cw/2, fy - ch/2, cw, ch, cc);
        _dRect(fx - cw/2 - 1, fy - ch/2 - 1, cw + 2, ch + 2, cc);
        _spr->setTextFont(1);
        _spr->setTextColor(AXIS_C_BG, cc);
        if (onHome) {
            _spr->setCursor(fx - 12, fy - 3);
            _spr->print("HOME");
        } else if (_nodeCursor < (int8_t)_nodeCount) {
            uint16_t lw = _spr->textWidth(_nodes[_nodeCursor].shortTag);
            _spr->setCursor(fx - (int16_t)(lw/2), fy - 3);
            _spr->print(_nodes[_nodeCursor].shortTag);
        }
    }

    // ══ 分割线（y=70）══════════════════════════
    _hLine(xOff, yOff + 70, W, AXIS_C_DIVIDER);

    // ══ 下半信息区（即时内容 + 切换时彩色边框闪烁）════
    uint16_t selCol = (_nodeCursor >= 0 && _nodeCursor < (int8_t)_nodeCount)
                      ? _nodes[_nodeCursor].color : AXIS_C_ACCENT;

    // 切换边框闪烁
    if (_nodeInfoSlide > 0.5f)
        _dRect(xOff + 1, yOff + 71, W - 2, 41, selCol);
    else if (_nodeInfoSlide > 0.08f)
        _dRect(xOff + 1, yOff + 71, W - 2, 41, AXIS_C_DIVIDER);

    int16_t iy = yOff + 74;
    if (_nodeCursor < 0) {
        // ── HOME 状态：时间 / 日期 / 传感器 + NODE0 立方体 ──
        _spr->setTextFont(2);
        _spr->setTextColor(AXIS_C_ACCENT, AXIS_C_BG);
        _spr->setCursor(xOff + 4, iy);
        _spr->print(_timeStr);

        _spr->setTextFont(1);
        _spr->setTextColor(AXIS_C_MUTED, AXIS_C_BG);
        _spr->setCursor(xOff + 4, iy + 17);   // 上移 3px
        _spr->print(_dateStr);

        // 灵动消息栏
        if (_ticker[_tickerIdx].valid) {
            const int16_t TX = xOff + 4;
            const int16_t TY = iy + 25;
            const int16_t TW = 94;   // 可用宽度（立方体在 xOff+110 左侧）

            _setFont(1);
            _spr->setTextDatum(0);

            if (_tickerGlow > 0.0f) {
                // 闪烁出场：每 1.0 单位一次亮暗交替（共 3 次）
                bool bright = (fmodf(_tickerGlow, 1.0f) > 0.5f);
                _spr->setTextColor(bright ? AXIS_C_TEXT : _ticker[_tickerIdx].color, AXIS_C_BG);
                _spr->drawString(_ticker[_tickerIdx].text, TX, TY);
            } else {
                _spr->setTextColor(_ticker[_tickerIdx].color, AXIS_C_BG);
                int16_t tw = (int16_t)_spr->textWidth(_ticker[_tickerIdx].text);

                if (tw <= TW) {
                    // 短文本：静态
                    _spr->drawString(_ticker[_tickerIdx].text, TX, TY);
                } else {
                    // 长文本：滚动播报
                    uint32_t tn = millis();
                    if (_tickerScrollMs == 0) _tickerScrollMs = tn;
                    int16_t maxOff = tw - TW + 4;
                    uint32_t el   = tn - _tickerScrollMs;
                    switch (_tickerScrollPhase) {
                        case 0:   // 起始停顿 0.8s
                            _tickerScrollOff = 0;
                            if (el > 800) { _tickerScrollPhase = 1; _tickerScrollMs = tn; }
                            break;
                        case 1:   // 匀速滚动（1px/28ms ≈ 35px/s）
                            _tickerScrollOff = (int16_t)(el / 28);
                            if (_tickerScrollOff >= maxOff) {
                                _tickerScrollOff   = maxOff;
                                _tickerScrollPhase = 2;
                                _tickerScrollMs    = tn;
                            }
                            break;
                        case 2:   // 末尾停顿 0.8s，然后回头重播
                            if (el > 800) {
                                _tickerScrollOff   = 0;
                                _tickerScrollPhase = 0;
                                _tickerScrollMs    = tn;
                            }
                            break;
                    }
                    _spr->setClipRect(TX, TY - 1, TW, 12);
                    _spr->drawString(_ticker[_tickerIdx].text, TX - _tickerScrollOff, TY);
                    _spr->clearClipRect();
                }
            }
        }

        // NODE0 小立方体：尺寸由 _cubeSize 平滑驱动（含呼吸），消息到达额外脉冲
        {
            float pulseAdd = (_homeCubePulse > 0.0f)
                             ? 5.0f * sinf(_homeCubePulse * 3.14159f) : 0.0f;
            _drawMiniCube(xOff + 110, iy + 19, _homeCubeAngX, _homeCubeAngY,
                          _cubeSize + pulseAdd, AXIS_C_ACCENT);
        }
    } else if (_nodeCursor < (int8_t)_nodeCount && _nodes[_nodeCursor].valid) {
        // ── 节点状态：名称 / 简介 ──
        const AxisNode& n = _nodes[_nodeCursor];
        _setFont(2);
        _spr->setTextColor(n.color, AXIS_C_BG);
        _spr->setTextDatum(0);
        _spr->drawString(n.label, xOff + 4, iy);

        if (n.description[0]) {
            _setFont(1);  // 中文时自动切 U8g2 WQY12
            _spr->setTextColor(AXIS_C_MUTED, AXIS_C_BG);
            // 限制描述文字宽度，不覆盖右侧立方体区域
            _spr->setClipRect(xOff, iy + 18, 98, 16);
            _spr->setTextDatum(0);
            _spr->drawString(n.description, xOff + 4, iy + 20);
            _spr->clearClipRect();
            _spr->setTextFont(1);  // 还原 ASCII 字体供后续使用
        }

        // 右侧小立方体（节点选中时显示，切换时旋转 + 收敛至默认尺寸）
        _drawMiniCube(xOff + 110, iy + 19, _homeCubeAngX, _homeCubeAngY,
                      _cubeSize, n.color);
    }

    // ── 固定底部 hint ──────────────────────────────
    _hLine(xOff, yOff + 113, W, AXIS_C_DIVIDER);
    _spr->setTextFont(1);
    _spr->setTextColor(AXIS_C_DIM, AXIS_C_BG);
    _spr->setCursor(xOff + 2, yOff + 118);
    if (_nodeCursor >= 0)
        _spr->print("[OK] ENTER  [B] BACK");
    else
        _spr->print(_nodeCount > 0 ? "DIR:MOVE  OK:MENU" : "OK:MENU");
}

// ═════════════════════════════════════════════════
//  内置屏幕：SCR_PLAYER
// ═════════════════════════════════════════════════

void AXIS_UI::_renderPlayerScreen(int16_t xOff, int16_t yOff) {
    _fill(AXIS_C_BG);
    int16_t W = mainW();

    _spr->setTextFont(1);
    _spr->setTextColor(_btConn ? AXIS_C_ACCENT : AXIS_C_DIM, AXIS_C_BG);
    _spr->setCursor(xOff + 2, yOff + 2);
    _spr->print(_btConn ? "BT" : "--");

    _spr->setTextColor(_playing ? AXIS_C_ACCENT : AXIS_C_MUTED, AXIS_C_BG);
    _spr->setCursor(xOff + 16, yOff + 2);
    _spr->print(_playing ? "PLAY" : "STOP");

    _spr->setTextColor(AXIS_C_MUTED, AXIS_C_BG);
    char vbuf[8]; snprintf(vbuf, sizeof(vbuf), "V%d", _volume);
    _spr->setCursor(xOff + W - 28, yOff + 2);
    _spr->print(vbuf);

    _hLine(xOff, yOff + 11, W, AXIS_C_DIVIDER);

    // 歌名（Font 2 = 16px）
    _spr->setTextFont(2);
    _spr->setTextColor(AXIS_C_TEXT, AXIS_C_BG);
    _spr->setCursor(xOff + 2, yOff + 14);
    _spr->print(truncate(_trackTitle, 10));

    // 艺术家
    _spr->setTextFont(1);
    _spr->setTextColor(AXIS_C_ACCENT, AXIS_C_BG);
    _spr->setCursor(xOff + 2, yOff + 34);
    _spr->print(truncate(_trackArtist, 22));

    _hLine(xOff, yOff + 44, W, AXIS_C_DIVIDER);

    // 进度条
    _hLine(xOff + 2, yOff + 58, W - 4, AXIS_C_DIM);
    int filled = (int)(_smoothProgress.value() * (W - 4));
    if (filled > 0)
        _hLine(xOff + 2, yOff + 58, filled, AXIS_C_ACCENT);
    if (filled > 0)
        _rect(xOff + 2 + filled - 2, yOff + 56, 4, 4, AXIS_C_WHITE);

    _spr->setTextFont(1);
    _spr->setTextColor(AXIS_C_MUTED, AXIS_C_BG);
    _spr->setCursor(xOff + 2, yOff + 66);
    _spr->print(formatTime((int32_t)(_smoothProgress.value() * 210)));
    String dur = formatTime(210);
    _spr->setCursor(xOff + W - dur.length()*6, yOff + 66);
    _spr->print(dur);

    // 音量
    _hLine(xOff, yOff + 76, W, AXIS_C_DIVIDER);
    _spr->setTextColor(AXIS_C_DIM, AXIS_C_BG);
    _spr->setCursor(xOff + 2, yOff + 80);
    _spr->print("VOL");
    _hLine(xOff + 22, yOff + 84, W - 24, AXIS_C_DIM);
    int vw = (int)(_smoothVolume.value() * (W - 24));
    if (vw > 0) _hLine(xOff + 22, yOff + 84, vw, AXIS_C_ACCENT);

    // VU 竖条
    if (_playing) {
        for (int i = 0; i < 4; i++) {
            float lv = 0.3f + (float)(millis() % (200 + i*37)) / (200.0f + i*37);
            lv = constrain(lv, 0.0f, 1.0f);
            int bh = (int)(lv * 16);
            _rect(xOff + 2 + i * 8, yOff + 100 - bh, 5, bh,
                  AxisAnim::dimColor(AXIS_C_ACCENT, lv));
        }
    }

    _hLine(xOff, yOff + 112, W, AXIS_C_DIVIDER);
    _spr->setTextColor(AXIS_C_DIM, AXIS_C_BG);
    _spr->setCursor(xOff + 2, yOff + 116);
    _spr->print("SET:V+  RST:V-  MID:PLAY");
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
            fromX = -(int16_t)(e * W);  toX = (int16_t)((1.0f-e) * W);  break;
        case AXIS_TRANS_SLIDE_RIGHT:
            fromX =  (int16_t)(e * W);  toX = -(int16_t)((1.0f-e) * W); break;
        case AXIS_TRANS_SLIDE_UP:
            fromY = -(int16_t)(e * H);  toY = (int16_t)((1.0f-e) * H);  break;
        case AXIS_TRANS_SLIDE_DOWN:
            fromY =  (int16_t)(e * H);  toY = -(int16_t)((1.0f-e) * H); break;
        default: break;
    }
}

void AXIS_UI::_renderTransition() {
    int16_t fx, fy, tx, ty;
    _calcTransOffsets(_transProgress, fx, fy, tx, ty);
    _spr->fillScreen(AXIS_C_BG);
    if (_transFromID != AXIS_SCR_NONE) _renderScreen(_transFromID, fx, fy);
    if (_transToID   != AXIS_SCR_NONE) _renderScreen(_transToID,   tx, ty);
}

// ═════════════════════════════════════════════════
//  菜单覆盖层
// ═════════════════════════════════════════════════

void AXIS_UI::_renderMenuOverlay() {
    float   ease = AxisAnim::easeOutCubic(_menuSlide);
    int16_t W    = mainW();
    int16_t H    = mainH();
    int16_t oy   = (int16_t)((1.0f - ease) * H);

    _rect(0, oy, W, H - oy, AXIS_C_BG2);
    _hLine(0, oy,   W, AXIS_C_ACCENT);
    _hLine(0, oy+1, W, AXIS_C_ACCENT);

    _spr->setTextFont(1);
    _spr->setTextColor(AXIS_C_ACCENT, AXIS_C_BG2);
    _spr->setCursor(2, oy + 3);
    _spr->print("MENU");

    _spr->setTextColor(AXIS_C_DIM, AXIS_C_BG2);
    _spr->setCursor(W - 42, oy + 3);
    _spr->print("[A:BACK]");

    _hLine(0, oy + 12, W, AXIS_C_DIVIDER);

    float   cY  = _menuCursorAnim.value();
    int16_t hlY = oy + 14 + (int16_t)(cY * 20);
    _rect(0, hlY, W, 18, AXIS_C_CARD);
    _rect(0, hlY, 2, 18, AXIS_C_ACCENT);

    for (uint8_t i = 0; i < _menuCount; i++) {
        int16_t iy  = oy + 14 + i * 20;
        bool    sel = (i == (uint8_t)_menuCursor);
        _setFont(1);
        _spr->setTextColor(sel ? _menuItems[i].color : AXIS_C_MUTED, AXIS_C_CARD);
        _spr->setTextDatum(0);
        _spr->drawString(_menuItems[i].label, 6, iy + 5);
        if (sel) _rect(W - 8, iy + 7, 4, 4, _menuItems[i].color);
    }
}

// ═════════════════════════════════════════════════
//  通知覆盖层（顶部滑入）
// ═════════════════════════════════════════════════

void AXIS_UI::_renderNotifOverlay() {
    int16_t W    = mainW();
    float   ease = AxisAnim::easeOutCubic(_notif.slideY);
    int16_t h    = 14;
    int16_t y    = (int16_t)((ease - 1.0f) * h);

    _rect(0, y, W, h, AXIS_C_BG2);
    _hLine(0, y + h - 1, W, _notif.color);
    _hLine(0, y + h - 2, W, _notif.color);

    _setFont(1);
    _spr->setTextColor(_notif.color, AXIS_C_BG2);
    _spr->setTextDatum(0);
    _spr->drawString(_notif.text.substring(0, (W - 4) / 6), 2, y + 3);
}

// ═════════════════════════════════════════════════
//  冒泡覆盖层（底部滑入）
// ═════════════════════════════════════════════════

void AXIS_UI::_renderBubbleOverlay() {
    int16_t W    = mainW();
    int16_t H    = mainH();
    float   ease = AxisAnim::easeOutCubic(_bubble.slideY);
    int16_t bh   = 18;
    int16_t y    = H - (int16_t)(ease * bh);

    _rect(0, y, W, bh, AXIS_C_BG2);
    _hLine(0, y,   W, _bubble.color);
    _hLine(0, y+1, W, _bubble.color);

    _setFont(1);
    _spr->setTextColor(_bubble.color, AXIS_C_BG2);
    _spr->setTextDatum(0);
    _spr->drawString(_bubble.text.substring(0, (W - 4) / 6), 2, y + 5);
}

// ═════════════════════════════════════════════════
//  副屏渲染
// ═════════════════════════════════════════════════

void AXIS_UI::_renderSubDisplay() {
    if (!_sub || !_statusCb) return;
    // 副屏暂用独立 sprite，或直接用 _spr 复用
    // 简单起见：直接传 *_spr（用户 statusCb 自行 clear）
    _statusCb(*_spr, _statusData);
    _flushSub();
}

void AXIS_UI::_flushMain() {
    if (_spr) _spr->pushSprite(0, 0);  // DMA push to display
    if (_mainFlush) _mainFlush();
}

void AXIS_UI::_flushSub()  { if (_subFlush) _subFlush(); }

// ═════════════════════════════════════════════════
//  屏幕管理
// ═════════════════════════════════════════════════

void AXIS_UI::registerScreen(AxisScreenID id,
                              AxisDrawCallback drawFn,
                              AxisInputCallback inputFn,
                              void* userData,
                              AxisNavType navType) {
    if (_screenCount >= AXIS_MAX_SCREENS) return;
    _screens[_screenCount++] = {id, drawFn, inputFn, userData, navType, true};
}

void AXIS_UI::registerInput(AxisScreenID id,
                             AxisInputCallback inputFn,
                             void* userData) {
    for (uint8_t i = 0; i < _screenCount; i++) {
        if (_screens[i].id == id) {
            _screens[i].inputFn  = inputFn;
            _screens[i].userData = userData;
            return;
        }
    }
    if (_screenCount >= AXIS_MAX_SCREENS) return;
    _screens[_screenCount++] = {id, nullptr, inputFn, userData, AXIS_NAVTYPE_AUTO, true};
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
    // 切屏时重置光标状态
    _curVX = _curVY = 0.0f;
    _snapT = 0.0f;
    _hoveredNode = -1;
    _escapeAcc = 0.0f;
    _discreteAccX = _discreteAccY = 0.0f;
    if (trans == AXIS_TRANS_NONE) {
        _curID = id;
        _spr->fillScreen(AXIS_C_BG);
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
            _spr->fillScreen(AXIS_C_BG);
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

void AXIS_UI::injectInput(AxisInputEvent ev) {
    _injectedInput = ev;
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
    _menuItems    = items;
    _menuCount    = min(count, (uint8_t)AXIS_MAX_MENU_ITEMS);
    _menuOnSelect = onSelect;
    _menuSelData  = userData;
    _menuCursor   = 0;
    _menuCursorAnim.snap(0.0f);
    _menuTarget   = 1.0f;
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
//  主页节点
// ═════════════════════════════════════════════════

void AXIS_UI::clearHomeNodes() {
    _nodeCount  = 0;
    _nodeCursor = -1;
    memset(_nodes, 0, sizeof(_nodes));
}

void AXIS_UI::registerHomeNode(const char* label, uint16_t color,
                                AxisScreenID target, AxisTransition transition,
                                const char* description, int8_t posIndex,
                                const char* shortTag, int8_t parentIdx) {
    if (_nodeCount >= AXIS_MAX_NODES) return;
    AxisNode& n    = _nodes[_nodeCount];
    // label: 存完整节点名（含中文），用于信息区大标题
    strncpy(n.label, label, 15);
    n.label[15]    = '\0';
    // shortTag: 节点图小框标签（ASCII）
    if (shortTag && shortTag[0]) {
        strncpy(n.shortTag, shortTag, 7);
        n.shortTag[7] = '\0';
    } else {
        // 从 label 截取 ASCII 字节；纯中文则回退用序号
        uint8_t si = 0;
        for (uint8_t i = 0; label[i] && si < 7; i++) {
            if ((uint8_t)label[i] < 0x80) n.shortTag[si++] = label[i];
        }
        n.shortTag[si] = '\0';
        if (si == 0) { n.shortTag[0]='N'; n.shortTag[1]='0'+(char)_nodeCount; n.shortTag[2]='\0'; }
    }
    strncpy(n.description, description ? description : "", 23);
    n.description[23] = '\0';
    n.color        = color;
    n.targetScreen = target;
    n.transition   = transition;
    n.posIndex     = (posIndex >= 0) ? (posIndex % 8) : (int8_t)(_nodeCount % 8);
    n.parentIdx    = parentIdx;
    n.valid        = true;
    _nodeCount++;
}

void AXIS_UI::bubble(const String& text, uint16_t color, uint32_t durationMs) {
    _bubble.text     = text;
    _bubble.color    = color;
    _bubble.expireAt = millis() + durationMs;
    _bubble.active   = true;
    _bubble.slideY   = 0.0f;
}

void AXIS_UI::_getNodeScreenPos(int8_t idx, int16_t& x, int16_t& y) const {
    if (idx < 0) { x = AXIS_NODE_CX; y = AXIS_NODE_CY; return; }
    int8_t  pi = _nodes[idx].parentIdx;
    int16_t px, py;
    _getNodeScreenPos(pi, px, py);
    uint8_t slot = (uint8_t)_nodes[idx].posIndex % 8;

    // shortTag 每超过 3 字符就把连线拉长 15%，防止标签与父节点盒子重叠
    // 上限 1.4×，避免深层节点连线过长跑出屏幕
    uint8_t tagLen = (uint8_t)strnlen(_nodes[idx].shortTag,
                                      sizeof(_nodes[idx].shortTag));
    float scale = 1.0f + (tagLen > 3 ? (tagLen - 3) * 0.15f : 0.0f);
    if (scale > 1.4f) scale = 1.4f;

    x = px + (int16_t)(AXIS_NODE_PX[slot] * scale);
    y = py + (int16_t)(AXIS_NODE_PY[slot] * scale);
}

void AXIS_UI::_homeNodeInput(AxisInputEvent ev) {
    if (_nodeCount == 0) return;

    // B 键：若当前在某个节点上，退回 HOME 光标（不离开主页）
    if (ev == AXIS_INPUT_BTN_B) {
        if (_nodeCursor != -1) {
            _nodeCursor    = -1;
            _nodeInfoSlide = 1.0f;
        }
        return;
    }

    if (ev == AXIS_INPUT_OK) {
        if (_nodeCursor >= 0 && _nodeCursor < (int8_t)_nodeCount) {
            const AxisNode& n = _nodes[_nodeCursor];
            if (n.targetScreen != AXIS_SCR_NONE)
                goTo(n.targetScreen, n.transition);
        }
        return;
    }

    int8_t dirX = 0, dirY = 0;
    switch (ev) {
        case AXIS_INPUT_UP:    dirY = -1; break;
        case AXIS_INPUT_DOWN:  dirY = +1; break;
        case AXIS_INPUT_LEFT:  dirX = -1; break;
        case AXIS_INPUT_RIGHT: dirX = +1; break;
        default: return;
    }

    int16_t cx, cy;
    _getNodeScreenPos(_nodeCursor, cx, cy);

    int8_t  best     = -2;
    int32_t bestDist = 0x7FFFFFFF;

    // 60° 锥形筛选 + 最近距离：保证只能沿节点连线方向移动
    // offAx*10 <= inDir*17  ≈ tan(60°)=1.73 的整数近似
    auto inCone = [&](int16_t dx, int16_t dy) -> bool {
        int32_t inDir = (int32_t)dx * dirX + (int32_t)dy * dirY;
        if (inDir <= 0) return false;
        int32_t offAx = (dirX != 0) ? (int32_t)abs(dy) : (int32_t)abs(dx);
        return offAx * 10 <= inDir * 17;
    };

    if (_nodeCursor != -1) {
        int16_t dx = AXIS_NODE_CX - cx, dy = AXIS_NODE_CY - cy;
        if (inCone(dx, dy)) {
            int32_t d = (int32_t)dx*dx + (int32_t)dy*dy;
            if (d < bestDist) { bestDist = d; best = -1; }
        }
    }

    for (uint8_t i = 0; i < _nodeCount; i++) {
        if (i == (uint8_t)_nodeCursor) continue;
        int16_t nx, ny;
        _getNodeScreenPos((int8_t)i, nx, ny);
        int16_t dx = nx - cx, dy = ny - cy;
        if (inCone(dx, dy)) {
            int32_t d = (int32_t)dx*dx + (int32_t)dy*dy;
            if (d < bestDist) { bestDist = d; best = (int8_t)i; }
        }
    }

    if (best > -2) {
        if (best != _nodeCursor) {
            _nodeInfoSlide = 1.0f;  // 触发边框闪烁
            _cubeSpinV     = 5.0f;  // 触发立方体旋转
        }
        _nodeCursor = best;
    }
}

// ═════════════════════════════════════════════════
//  导航模式 / 光标
// ═════════════════════════════════════════════════

void AXIS_UI::setNavMode(AxisNavMode mode) {
    _navMode = mode;
    // 切换模式时重置光标到屏幕中心
    _curX = (float)mainW() * 0.5f;
    _curY = (float)mainH() * 0.5f;
    _curVX = _curVY = 0.0f;
    _snapT = 0.0f;
    _hoveredNode = -1;
    _escapeAcc = 0.0f;
}

bool AXIS_UI::_isSpatialScreen() const {
    // HOME 内置屏：强制 SPATIAL
    if (_curID == AXIS_SCR_HOME) return true;
    // 查找注册屏幕的 navType
    for (uint8_t i = 0; i < _screenCount; i++) {
        if (_screens[i].id == _curID) {
            if (_screens[i].navType == AXIS_NAVTYPE_SPATIAL)  return true;
            if (_screens[i].navType == AXIS_NAVTYPE_DISCRETE) return false;
            break;  // AUTO：跟随全局
        }
    }
    return (_navMode == AXIS_NAVMODE_CURSOR);
}

void AXIS_UI::moveCursor(int16_t dx, int16_t dy) {
    _updateCursor(dx, dy);
}

void AXIS_UI::_updateCursor(int16_t rawDx, int16_t rawDy) {
    // ── 参数 ────────────────────────────────────────────────────────────────
    // 迷你轨迹球每次滚动产生脉冲极少（通常1-2个），用速度惯性模型：
    // 每个脉冲注入速度冲量，速度按帧衰减，光标跟着滑行
    const float IMPULSE        = 28.0f;  // 每脉冲注入的速度（px/帧）
    const float FRICTION       = 0.80f;  // 每帧速度保留比例（0=立停 1=永不停）
    const float MAX_SPEED      = 120.0f; // 最大速度上限（px/帧）
    const float SNAP_RADIUS    = 28.0f;  // 开始感受引力的距离
    const float SNAP_PULL      = 0.15f;  // 吸附拉力（每帧向节点移动的比例）
    const float ESCAPE_THRESH  = 18.0f;  // 脱离所需累积像素
    const float DISCRETE_THRESH = 1.0f;  // 离散屏触发阈值（脉冲）

    float W = (float)mainW();
    float H = (float)mainH();

    if (_isSpatialScreen()) {
        // ── 空间光标：速度惯性模型 ────────────────────────────────────────
        // 1. 注入速度冲量
        if (rawDx != 0) _curVX += (float)rawDx * IMPULSE;
        if (rawDy != 0) _curVY += (float)rawDy * IMPULSE;

        // 2. 速度上限
        float spd = sqrtf(_curVX*_curVX + _curVY*_curVY);
        if (spd > MAX_SPEED) {
            float s = MAX_SPEED / spd;
            _curVX *= s; _curVY *= s;
        }

        float mx = _curVX;
        float my = _curVY;

        // 3. 摩擦衰减
        _curVX *= FRICTION;
        _curVY *= FRICTION;

        // 4. 找最近节点
        int8_t nearest  = -1;
        float  nearDist = 1e9f;
        float  nearNx = 0, nearNy = 0;
        for (uint8_t i = 0; i < _nodeCount; i++) {
            int16_t nx, ny;
            _getNodeScreenPos((int8_t)i, nx, ny);
            float ddx = _curX - (float)nx;
            float ddy = _curY - (float)ny;
            float d   = sqrtf(ddx*ddx + ddy*ddy);
            if (d < nearDist) {
                nearDist = d; nearest = (int8_t)i;
                nearNx = (float)nx; nearNy = (float)ny;
            }
        }

        bool inSnap = (nearest >= 0 && nearDist < SNAP_RADIUS);

        if (inSnap) {
            // 吸附区内：判断是否在主动脱离
            // 脱离方向 = 输入方向与"远离节点方向"同向
            float awayX = _curX - nearNx;
            float awayY = _curY - nearNy;
            float awayLen = sqrtf(awayX*awayX + awayY*awayY);

            bool movingAway = false;
            if (awayLen > 0.5f && (rawDx != 0 || rawDy != 0)) {
                float dot = (mx * awayX + my * awayY) / awayLen;
                if (dot > 0) {
                    _escapeAcc += dot;
                    movingAway = true;
                }
            }

            if (_escapeAcc >= ESCAPE_THRESH) {
                // 脱离：正常移动，清除吸附状态
                _curX += mx;
                _curY += my;
                _escapeAcc   = 0.0f;
                _hoveredNode = -1;
            } else if (!movingAway) {
                // 没有主动离开：重置脱离累积，叠加吸附拉力
                _escapeAcc = 0.0f;

                // 拉力：每帧向节点中心靠近 SNAP_PULL 比例
                float pullT = SNAP_PULL * (1.0f - nearDist / SNAP_RADIUS);
                _curX += mx + (nearNx - _curX) * pullT;
                _curY += my + (nearNy - _curY) * pullT;

                // 更新悬停节点
                float newDist = sqrtf((_curX-nearNx)*(_curX-nearNx) + (_curY-nearNy)*(_curY-nearNy));
                if (newDist < SNAP_RADIUS) {
                    if (_hoveredNode != nearest) {
                        _hoveredNode   = nearest;
                        _nodeCursor    = nearest;
                        _nodeInfoSlide = 1.0f;
                        _cubeSpinV     = 3.0f;
                    }
                } else {
                    _hoveredNode = -1;
                }
            } else {
                // 主动离开但还没到阈值：正常移动，保持悬停
                _curX += mx;
                _curY += my;
            }
        } else {
            // 自由区：直接移动
            _escapeAcc   = 0.0f;
            _hoveredNode = -1;
            _curX += mx;
            _curY += my;
        }

        // 边界夹紧
        _curX = constrain(_curX, 4.0f, W - 4.0f);
        _curY = constrain(_curY, 4.0f, H - 4.0f);

    } else {
        // ── 离散屏：增量累积 → 方向事件 ────────────────────────────────────
        _discreteAccX += (float)rawDx;
        _discreteAccY += (float)rawDy;

        if (_discreteAccX >  DISCRETE_THRESH) {
            _injectedInput = AXIS_INPUT_RIGHT; _discreteAccX = 0;
        } else if (_discreteAccX < -DISCRETE_THRESH) {
            _injectedInput = AXIS_INPUT_LEFT;  _discreteAccX = 0;
        }
        if (_discreteAccY >  DISCRETE_THRESH) {
            _injectedInput = AXIS_INPUT_DOWN;  _discreteAccY = 0;
        } else if (_discreteAccY < -DISCRETE_THRESH) {
            _injectedInput = AXIS_INPUT_UP;    _discreteAccY = 0;
        }
    }
}

// ─────────────────────────────────────────────────
//  光标叠加层渲染（水珠吸附效果）
// ─────────────────────────────────────────────────
void AXIS_UI::_renderCursorOverlay(int16_t xOff, int16_t yOff) {
    // snapT: 0=自由光标  1=完全吸附到节点
    int16_t cx = (int16_t)_curX + xOff;
    int16_t cy = (int16_t)_curY + yOff;

    // ── 节点光晕（吸附时扩散）────────────────────
    if (_hoveredNode >= 0 && _hoveredNode < (int8_t)_nodeCount) {
        int16_t nx, ny;
        _getNodeScreenPos(_hoveredNode, nx, ny);
        nx += xOff; ny += yOff;

        // 光晕半径：0 → 14px
        float haloR = _snapT * 14.0f;
        if (haloR > 0.5f) {
            uint16_t nodeCol = _nodes[_hoveredNode].color;
            // 外圈（暗）
            _spr->drawCircle(nx, ny, (int16_t)(haloR + 2), AXIS_C_DIM);
            // 内圈（亮，用节点颜色）
            _spr->drawCircle(nx, ny, (int16_t)haloR, nodeCol);
        }

        // 脉冲扩散环（锁定瞬间触发，用 _nodeInfoSlide 驱动）
        if (_snapT > 0.9f && _nodeInfoSlide > 0.05f) {
            float pulseR = 14.0f + (1.0f - _nodeInfoSlide) * 18.0f;
            // 颜色随 slide 衰减
            uint8_t alpha = (uint8_t)(_nodeInfoSlide * 6.0f);  // 0-6 → 亮度级
            if (alpha > 0) {
                uint16_t pc = _nodes[_hoveredNode].color;
                _spr->drawCircle(nx, ny, (int16_t)pulseR, pc);
            }
        }
    }

    // ── 光标圆圈（吸附时收缩消失）────────────────
    // 自由：r=5 白色实心小圆  吸附中：r 收缩到 0
    float curR = (1.0f - _snapT) * 5.0f;
    if (curR > 0.8f) {
        int16_t r = (int16_t)curR;
        // 十字准星（轻量，不遮挡内容）
        uint16_t col = (_hoveredNode >= 0)
            ? _nodes[_hoveredNode].color
            : AXIS_C_WHITE;
        _spr->drawFastHLine(cx - r - 2, cy, r * 2 + 5, col);
        _spr->drawFastVLine(cx, cy - r - 2, r * 2 + 5, col);
        // 中心圆点
        if (r >= 2) _spr->fillCircle(cx, cy, r, col);
    }
}


void AXIS_UI::drawGradBar(int x, int y, int w, int h,
                           float ratio, uint16_t c1, uint16_t c2,
                           uint16_t bg) {
    _spr->fillRect(x, y, w, h, bg);
    int filled = constrain((int)(ratio * w), 0, w);
    for (int i = 0; i < filled; i++) {
        float t = (w > 1) ? (float)i / (float)(w-1) : 0.0f;
        _spr->drawFastVLine(x+i, y, h, AxisAnim::lerpColor(c1, c2, t));
    }
}

void AXIS_UI::drawProgressBar(int x, int y, int w, int h,
                               float ratio, uint16_t col) {
    _spr->drawRect(x, y, w, h, AXIS_C_DIVIDER);
    int filled = constrain((int)(ratio * (w-2)), 0, w-2);
    if (filled > 0) _spr->fillRect(x+1, y+1, filled, h-2, col);
}

void AXIS_UI::drawVUBar(int x, int y, int w, int h, float level) {
    level = constrain(level, 0.0f, 1.0f);
    _spr->fillRect(x, y, w, h, AXIS_C_CARD);
    int filled = (int)(level * h);
    for (int i = 0; i < filled; i++) {
        float t = (h > 1) ? (float)i / (float)(h-1) : 0.0f;
        _spr->drawFastHLine(x, y+h-1-i, w,
                            AxisAnim::lerpColor(AXIS_C_ACCENT, AXIS_C_DANGER, t));
    }
}

void AXIS_UI::drawBattery(int x, int y, uint8_t pct) {
    _spr->drawRect(x, y, 18, 7, AXIS_C_MUTED);
    _spr->fillRect(x+18, y+2, 2, 3, AXIS_C_MUTED);
    uint16_t col = pct > 20 ? AXIS_C_ACCENT : AXIS_C_DANGER;
    int fill = (int)(pct / 100.0f * 16);
    if (fill > 0) _spr->fillRect(x+1, y+1, fill, 5, col);
}

void AXIS_UI::drawSignalDots(int x, int y, uint8_t strength) {
    for (uint8_t i = 0; i < 4; i++) {
        uint16_t c = (i < strength) ? AXIS_C_ACCENT : AXIS_C_DIM;
        _spr->fillRect(x + i*5, y + (3-i)*3, 3, (i+1)*3, c);
    }
}

// ═════════════════════════════════════════════════
//  文字接口（TFT_eSPI 字体）
// ═════════════════════════════════════════════════

void AXIS_UI::setTextFont(uint8_t num)         { _spr->setTextFont(num); }
void AXIS_UI::setFreeFont(const GFXfont* font) { _spr->setFreeFont(font); }

void AXIS_UI::setFontColor(uint16_t fg, uint16_t bg) {
    _spr->setTextColor(fg, bg);
}

void AXIS_UI::drawText(int16_t x, int16_t y, const String& s) {
    _spr->setCursor(x, y); _spr->print(s);
}

void AXIS_UI::drawText(int16_t x, int16_t y, const char* s) {
    _spr->setCursor(x, y); _spr->print(s);
}

uint16_t AXIS_UI::textWidth(const String& s) {
    return _spr->textWidth(s.c_str());
}

uint16_t AXIS_UI::textHeight() {
    return _spr->fontHeight();
}

// ═════════════════════════════════════════════════
//  静态工具
// ═════════════════════════════════════════════════

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
    if (_longPressed(_pins.btn_a, 5)) return AXIS_INPUT_BTN_A_LONG;
    if (_pressed(_pins.btn_a,     5)) return AXIS_INPUT_BTN_A;
    if (_longPressed(_pins.btn_b, 6)) return AXIS_INPUT_BTN_B_LONG;
    if (_pressed(_pins.btn_b,     6)) return AXIS_INPUT_BTN_B;
    return AXIS_INPUT_NONE;
}

bool AXIS_UI::_pressed(int8_t pin, uint8_t idx) {
    if (pin < 0) return false;
    bool low = (digitalRead(pin) == LOW);
    if (low) {
        // 按下中：只在第一次（去抖窗口后）触发
        if (!_held[idx] && millis() - _debounce[idx] > DB_MS) {
            _held[idx]    = true;
            _debounce[idx] = millis();
            return true;
        }
    } else {
        // 松开：重置，允许下次触发
        if (_held[idx]) {
            _held[idx]    = false;
            _debounce[idx] = millis();
        }
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
