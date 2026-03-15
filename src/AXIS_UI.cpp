#include "AXIS_UI.h"

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

// ═════════════════════════════════════════════════
//  初始化
// ═════════════════════════════════════════════════

bool AXIS_UI::begin(const AxisPinConfig& pins) {
    if (!_main) {
        Serial.println("[AXIS] ERROR: Main display not set. Call setMainDisplay() first.");
        return false;
    }

    _pins = pins;

    // 输入引脚初始化
    // joy_down 通常是 GPIO34（input-only，无内部上拉），单独处理
    int8_t pullupPins[] = {
        pins.joy_up, pins.joy_left, pins.joy_right,
        pins.joy_ok, pins.btn_a, pins.btn_b
    };
    for (int8_t p : pullupPins) {
        if (p >= 0) pinMode(p, INPUT_PULLUP);
    }
    if (pins.joy_down >= 0) pinMode(pins.joy_down, INPUT); // 外部上拉

    _lastFrameAt = millis();

    Serial.printf("[AXIS] Ready. Main: %dx%d, Sub: %s\n",
                  mainW(), mainH(),
                  _sub ? "yes" : "none");
    return true;
}

// ═════════════════════════════════════════════════
//  帧率
// ═════════════════════════════════════════════════

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
    _renderMain();
    _renderSubDisplay();
}

// ═════════════════════════════════════════════════
//  动画状态更新
// ═════════════════════════════════════════════════

void AXIS_UI::_updateAnimations() {
    // 菜单滑动
    _menuSlide = AxisAnim::smoothFollow(_menuSlide, _menuTarget, 0.12f);
    if (_menuSlide < 0.005f && _menuTarget < 0.01f) _menuSlide = 0.0f;

    // 光标弹性
    if (_cursorAnim < 1.0f) {
        _cursorAnim = min(1.0f, _cursorAnim + 0.08f);
    }

    // 通知超时
    if (_notif.active && millis() >= _notif.expireAt) {
        _notif.active = false;
    }
}

// ═════════════════════════════════════════════════
//  输入处理
// ═════════════════════════════════════════════════

void AXIS_UI::_handleInput() {
    AxisInputEvent ev = _pollInput();
    if (ev == AXIS_INPUT_NONE) return;

    if (isMenuOpen()) {
        // ── 菜单接管输入 ──────────────────────────
        switch (ev) {
            case AXIS_INPUT_UP:
                if (_menuCursor > 0) {
                    _menuPrevCursor = _menuCursor;
                    _menuCursor--;
                    _cursorAnim = 0.0f;
                }
                break;

            case AXIS_INPUT_DOWN:
                if (_menuCursor < (int8_t)(_menuCount - 1)) {
                    _menuPrevCursor = _menuCursor;
                    _menuCursor++;
                    _cursorAnim = 0.0f;
                }
                break;

            case AXIS_INPUT_OK:
            case AXIS_INPUT_BTN_B: {
                const AxisMenuItem& item = _menuItems[_menuCursor];
                hideMenu();
                if (item.targetScreen != AXIS_SCR_NONE) {
                    goTo(item.targetScreen);
                } else if (_menuOnSelect) {
                    _menuOnSelect(ev, _menuSelData);
                }
                break;
            }

            case AXIS_INPUT_BTN_A:
                hideMenu();
                break;

            default:
                break;
        }
    } else {
        // ── 当前屏幕接收输入 ──────────────────────
        AxisScreen* scr = _findScreen(_curID);
        if (scr && scr->inputFn) {
            scr->inputFn(ev, scr->userData);
        }
    }
}

// ═════════════════════════════════════════════════
//  主屏渲染
// ═════════════════════════════════════════════════

void AXIS_UI::_renderMain() {
    // 当前屏幕内容
    AxisScreen* scr = _findScreen(_curID);
    if (scr && scr->drawFn) {
        scr->drawFn(*_main, scr->userData);
    }

    // 叠加层（绘制在屏幕内容之上）
    if (_notif.active)     _renderNotifOverlay();
    if (_menuSlide > 0.005f) _renderMenuOverlay();

    _flushMain();
}

// ═════════════════════════════════════════════════
//  副屏渲染
// ═════════════════════════════════════════════════

void AXIS_UI::_renderSubDisplay() {
    if (!_sub || !_statusCb) return;

    // 清空由用户回调决定（给用户完全控制权）
    _statusCb(*_sub, _statusData);
    _flushSub();
}

// ═════════════════════════════════════════════════
//  刷新（flush）
// ═════════════════════════════════════════════════

void AXIS_UI::_flushMain() {
    if (_mainFlush) _mainFlush();
}

void AXIS_UI::_flushSub() {
    if (_subFlush) _subFlush();
}

// ═════════════════════════════════════════════════
//  菜单覆盖层渲染
// ═════════════════════════════════════════════════

void AXIS_UI::_renderMenuOverlay() {
    float ease = AxisAnim::easeOutCubic(_menuSlide);
    int16_t H  = mainH();
    int16_t W  = mainW();
    int16_t oy = (int16_t)((1.0f - ease) * H);

    // 面板背景
    _main->fillRoundRect(6, oy, W-12, H-8, 8, 0x0C4A);
    _main->drawRoundRect(6, oy, W-12, H-8, 8, AXIS_C_ACCENT);

    // 标题
    _main->setTextColor(AXIS_C_ACCENT);
    _main->setTextSize(1);
    _main->setCursor((W - 7*6) / 2, oy + 6);
    _main->print("[ MENU ]");
    _main->drawFastHLine(8, oy + 16, W-16, AXIS_C_DIVIDER);

    // 光标滑动高亮块
    float cEase = AxisAnim::easeOutElastic(min(_cursorAnim, 1.0f));
    float cY    = (float)_menuPrevCursor +
                  ((float)_menuCursor - (float)_menuPrevCursor) * cEase;
    int16_t hlY = oy + 20 + (int16_t)(cY * 22);
    _main->fillRoundRect(10, hlY, W-20, 19, 4, 0x0C2E);
    // 左侧彩色边条
    _main->fillRect(10, hlY, 3, 19, _menuItems[_menuCursor].color);

    // 菜单项
    for (uint8_t i = 0; i < _menuCount; i++) {
        int16_t iy = oy + 25 + i * 22;
        bool active = (i == (uint8_t)_menuCursor);
        _main->setTextColor(active ? _menuItems[i].color : AXIS_C_GRAY);
        _main->setTextSize(1);
        _main->setCursor(18, iy);
        _main->print(_menuItems[i].label);
        // 右箭头
        if (active) {
            _main->fillTriangle(
                W-18, iy+1,
                W-18, iy+7,
                W-14, iy+4,
                _menuItems[i].color
            );
        }
    }

    // 底部提示
    _main->drawFastHLine(8, oy + H - 20, W-16, AXIS_C_DIVIDER);
    _main->setTextColor(AXIS_C_DKGRAY);
    _main->setTextSize(1);
    _main->setCursor(10, oy + H - 16);
    _main->print("U/D:Nav  OK:Enter  A:Back");
}

// ═════════════════════════════════════════════════
//  通知覆盖层渲染
// ═════════════════════════════════════════════════

void AXIS_UI::_renderNotifOverlay() {
    int16_t W = mainW();
    _main->fillRoundRect(2, 20, W-4, 12, 3, 0x1084);
    _main->drawRoundRect(2, 20, W-4, 12, 3, _notif.color);
    _main->setTextColor(AXIS_C_WHITE);
    _main->setTextSize(1);
    _main->setCursor(5, 23);
    _main->print(truncate(_notif.text, (W-10) / 6));
}

// ═════════════════════════════════════════════════
//  屏幕管理
// ═════════════════════════════════════════════════

void AXIS_UI::registerScreen(AxisScreenID id,
                              AxisDrawCallback drawFn,
                              AxisInputCallback inputFn,
                              void* userData) {
    if (_screenCount >= AXIS_MAX_SCREENS) {
        Serial.println("[AXIS] WARN: Max screen count reached.");
        return;
    }
    _screens[_screenCount++] = {id, drawFn, inputFn, userData, true};
}

void AXIS_UI::goTo(AxisScreenID id) {
    if (!_findScreen(id)) {
        Serial.printf("[AXIS] WARN: Screen %d not registered.\n", id);
        return;
    }
    _prevID = _curID;
    _curID  = id;
    if (_main) _main->fillScreen(AXIS_C_BG);
}

void AXIS_UI::goBack() {
    if (_prevID != AXIS_SCR_NONE) goTo(_prevID);
}

AxisScreen* AXIS_UI::_findScreen(AxisScreenID id) {
    for (uint8_t i = 0; i < _screenCount; i++) {
        if (_screens[i].valid && _screens[i].id == id) return &_screens[i];
    }
    return nullptr;
}

// ═════════════════════════════════════════════════
//  菜单
// ═════════════════════════════════════════════════

void AXIS_UI::showMenu(const AxisMenuItem* items, uint8_t count,
                        AxisInputCallback onSelect, void* userData) {
    if (!items || count == 0) return;
    _menuItems       = items;
    _menuCount       = min(count, (uint8_t)AXIS_MAX_MENU_ITEMS);
    _menuOnSelect    = onSelect;
    _menuSelData     = userData;
    _menuCursor      = 0;
    _menuPrevCursor  = 0;
    _cursorAnim      = 1.0f;
    _menuTarget      = 1.0f;
}

void AXIS_UI::hideMenu() {
    _menuTarget = 0.0f;
}

// ═════════════════════════════════════════════════
//  通知
// ═════════════════════════════════════════════════

void AXIS_UI::notify(const String& text, uint16_t color, uint32_t durationMs) {
    _notif.text     = text;
    _notif.color    = color;
    _notif.expireAt = millis() + durationMs;
    _notif.active   = true;
}

// ═════════════════════════════════════════════════
//  绘图工具
// ═════════════════════════════════════════════════

void AXIS_UI::drawGradBar(int x, int y, int w, int h,
                           float ratio,
                           uint16_t c1, uint16_t c2,
                           uint16_t bg) {
    _main->fillRoundRect(x, y, w, h, h/2, bg);
    int filled = max(h, (int)(ratio * w));
    filled = min(filled, w);
    for (int i = 0; i < filled; i++) {
        float t = (w > 1) ? (float)i / (w - 1) : 0.0f;
        _main->drawFastVLine(x + i, y, h, AxisAnim::lerpColor(c1, c2, t));
    }
    if (filled > 0) {
        _main->fillCircle(x + filled - 1, y + h/2, h/2 + 1, AXIS_C_WHITE);
    }
}

void AXIS_UI::drawProgressBar(int x, int y, int w, int h,
                               float ratio, uint16_t col) {
    _main->drawRect(x, y, w, h, AXIS_C_DKGRAY);
    int filled = (int)(constrain(ratio, 0.0f, 1.0f) * (w - 2));
    if (filled > 0) {
        _main->fillRect(x+1, y+1, filled, h-2, col);
    }
}

void AXIS_UI::drawVUBar(int x, int y, int w, int h, float level) {
    level = constrain(level, 0.0f, 1.0f);
    _main->fillRect(x, y, w, h, AXIS_C_CARD);
    int filled = (int)(level * h);
    for (int i = 0; i < filled; i++) {
        float t = (h > 1) ? (float)i / (h - 1) : 0.0f;
        _main->drawFastHLine(x, y + h - 1 - i, w,
                             AxisAnim::lerpColor(AXIS_C_GREEN, AXIS_C_RED, t));
    }
}

String AXIS_UI::truncate(const String& s, int maxChars) {
    if (maxChars <= 0) return "";
    if ((int)s.length() <= maxChars) return s;
    return s.substring(0, maxChars - 1) + "~";
}

String AXIS_UI::formatTime(int32_t s) {
    if (s < 0) s = 0;
    char buf[9];
    snprintf(buf, sizeof(buf), "%d:%02d", (int)(s / 60), (int)(s % 60));
    return String(buf);
}

// ═════════════════════════════════════════════════
//  输入轮询
// ═════════════════════════════════════════════════

AxisInputEvent AXIS_UI::_pollInput() {
    if (_pressed(_pins.joy_up,    0)) return AXIS_INPUT_UP;
    if (_pressed(_pins.joy_down,  1)) return AXIS_INPUT_DOWN;
    if (_pressed(_pins.joy_left,  2)) return AXIS_INPUT_LEFT;
    if (_pressed(_pins.joy_right, 3)) return AXIS_INPUT_RIGHT;
    if (_pressed(_pins.joy_ok,    4)) return AXIS_INPUT_OK;
    if (_pressed(_pins.btn_a,     5)) return AXIS_INPUT_BTN_A;
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
