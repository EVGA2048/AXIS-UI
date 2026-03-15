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
        Serial.println("[AXIS] ERROR: setMainDisplay() must be called first.");
        return false;
    }
    _pins = pins;

    // INPUT_PULLUP 引脚
    int8_t pullupPins[] = {
        pins.joy_up, pins.joy_left, pins.joy_right,
        pins.joy_ok, pins.btn_a,    pins.btn_b
    };
    for (int8_t p : pullupPins) {
        if (p >= 0) pinMode(p, INPUT_PULLUP);
    }
    // joy_down（GPIO34 等 input-only 无内部上拉）
    if (pins.joy_down >= 0) pinMode(pins.joy_down, INPUT);

    _lastFrameAt = millis();

    Serial.printf("[AXIS] Init OK. Main: %dx%d  Sub: %s\n",
                  mainW(), mainH(), _sub ? "yes" : "none");
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
            // 过渡结束，清屏准备正常渲染
            _main->fillScreen(AXIS_C_BG);
        }
    }

    // 菜单滑动
    _menuSlide = AxisAnim::smoothFollow(_menuSlide, _menuTarget, 0.12f);
    if (_menuSlide < 0.005f && _menuTarget < 0.01f) _menuSlide = 0.0f;

    // 光标弹性
    if (_cursorAnim < 1.0f) {
        _cursorAnim = min(1.0f, _cursorAnim + 0.08f);
    }

    // 通知超时
    if (_notif.active && now >= _notif.expireAt) {
        _notif.active = false;
    }
}

// ═════════════════════════════════════════════════
//  输入处理（过渡中屏蔽输入）
// ═════════════════════════════════════════════════

void AXIS_UI::_handleInput() {
    if (_transActive) return;  // 过渡中不处理输入，防止误触

    AxisInputEvent ev = _pollInput();
    if (ev == AXIS_INPUT_NONE) return;

    if (isMenuOpen()) {
        switch (ev) {
            case AXIS_INPUT_UP:
                if (_menuCursor > 0) {
                    _menuPrevCursor = _menuCursor--;
                    _cursorAnim = 0.0f;
                }
                break;

            case AXIS_INPUT_DOWN:
                if (_menuCursor < (int8_t)(_menuCount - 1)) {
                    _menuPrevCursor = _menuCursor++;
                    _cursorAnim = 0.0f;
                }
                break;

            case AXIS_INPUT_OK:
            case AXIS_INPUT_BTN_B: {
                const AxisMenuItem& item = _menuItems[_menuCursor];
                hideMenu();
                if (item.targetScreen != AXIS_SCR_NONE) {
                    goTo(item.targetScreen, item.transition);
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
        AxisScreen* scr = _findScreen(_curID);
        if (scr && scr->inputFn) {
            scr->inputFn(ev, scr->userData);
        }
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
        if (_notif.active)      _renderNotifOverlay();
        if (_menuSlide > 0.005f) _renderMenuOverlay();
    }
    _flushMain();
}

// ═════════════════════════════════════════════════
//  单屏渲染（带偏移）
// ═════════════════════════════════════════════════

void AXIS_UI::_renderScreen(AxisScreenID id, int16_t xOff, int16_t yOff) {
    AxisScreen* scr = _findScreen(id);
    if (scr && scr->drawFn) {
        scr->drawFn(*_main, xOff, yOff, scr->userData);
    }
}

// ═════════════════════════════════════════════════
//  过渡动画渲染
// ═════════════════════════════════════════════════

void AXIS_UI::_calcTransOffsets(float progress,
                                 int16_t& fromX, int16_t& fromY,
                                 int16_t& toX,   int16_t& toY) {
    float eased = AxisAnim::easeOutCubic(progress);
    int16_t W = mainW();
    int16_t H = mainH();

    fromX = fromY = toX = toY = 0;

    switch (_transType) {
        case AXIS_TRANS_SLIDE_LEFT:
            // 旧屏往左出，新屏从右进
            fromX = -(int16_t)(eased * W);
            toX   =  (int16_t)((1.0f - eased) * W);
            break;

        case AXIS_TRANS_SLIDE_RIGHT:
            // 旧屏往右出，新屏从左进
            fromX =  (int16_t)(eased * W);
            toX   = -(int16_t)((1.0f - eased) * W);
            break;

        case AXIS_TRANS_SLIDE_UP:
            // 旧屏往上出，新屏从下进
            fromY = -(int16_t)(eased * H);
            toY   =  (int16_t)((1.0f - eased) * H);
            break;

        case AXIS_TRANS_SLIDE_DOWN:
            // 旧屏往下出，新屏从上进
            fromY =  (int16_t)(eased * H);
            toY   = -(int16_t)((1.0f - eased) * H);
            break;

        default:
            break;
    }
}

void AXIS_UI::_renderTransition() {
    int16_t fromX, fromY, toX, toY;
    _calcTransOffsets(_transProgress, fromX, fromY, toX, toY);

    // 先清屏
    _main->fillScreen(AXIS_C_BG);

    // 渲染旧屏（带偏移，逐渐滑出）
    if (_transFromID != AXIS_SCR_NONE) {
        _renderScreen(_transFromID, fromX, fromY);
    }

    // 渲染新屏（带偏移，逐渐滑入）
    if (_transToID != AXIS_SCR_NONE) {
        _renderScreen(_transToID, toX, toY);
    }

    // 过渡进行中不渲染菜单和通知，避免叠层混乱
}

// ═════════════════════════════════════════════════
//  菜单覆盖层
// ═════════════════════════════════════════════════

void AXIS_UI::_renderMenuOverlay() {
    float   ease = AxisAnim::easeOutCubic(_menuSlide);
    int16_t W    = mainW();
    int16_t H    = mainH();
    int16_t oy   = (int16_t)((1.0f - ease) * H);

    _main->fillRoundRect(6, oy, W-12, H-8, 8, 0x0C4A);
    _main->drawRoundRect(6, oy, W-12, H-8, 8, AXIS_C_ACCENT);

    _main->setTextColor(AXIS_C_ACCENT);
    _main->setTextSize(1);
    _main->setCursor((W - 7*6) / 2, oy + 6);
    _main->print("[ MENU ]");
    _main->drawFastHLine(8, oy + 16, W-16, AXIS_C_DIVIDER);

    // 光标高亮
    float   cEase = AxisAnim::easeOutElastic(min(_cursorAnim, 1.0f));
    float   cY    = (float)_menuPrevCursor
                  + ((float)_menuCursor - (float)_menuPrevCursor) * cEase;
    int16_t hlY   = oy + 20 + (int16_t)(cY * 22);
    _main->fillRoundRect(10, hlY, W-20, 19, 4, 0x0C2E);
    _main->fillRect(10, hlY, 3, 19, _menuItems[_menuCursor].color);

    // 菜单项
    for (uint8_t i = 0; i < _menuCount; i++) {
        int16_t iy  = oy + 25 + i * 22;
        bool    sel = (i == (uint8_t)_menuCursor);
        _main->setTextColor(sel ? _menuItems[i].color : AXIS_C_GRAY);
        _main->setTextSize(1);
        _main->setCursor(18, iy);
        _main->print(_menuItems[i].label);
        if (sel) {
            _main->fillTriangle(W-18, iy+1,
                                W-18, iy+7,
                                W-14, iy+4,
                                _menuItems[i].color);
        }
    }

    _main->drawFastHLine(8, oy + H - 20, W-16, AXIS_C_DIVIDER);
    _main->setTextColor(AXIS_C_DKGRAY);
    _main->setTextSize(1);
    _main->setCursor(10, oy + H - 16);
    _main->print("U/D:Nav  OK:Enter  A:Back");
}

// ═════════════════════════════════════════════════
//  通知覆盖层
// ═════════════════════════════════════════════════

void AXIS_UI::_renderNotifOverlay() {
    int16_t W = mainW();
    _main->fillRoundRect(2, 20, W-4, 12, 3, 0x1084);
    _main->drawRoundRect(2, 20, W-4, 12, 3, _notif.color);
    _main->setTextColor(AXIS_C_WHITE);
    _main->setTextSize(1);
    _main->setCursor(5, 23);
    _main->print(truncate(_notif.text, (W - 10) / 6));
}

// ═════════════════════════════════════════════════
//  副屏渲染
// ═════════════════════════════════════════════════

void AXIS_UI::_renderSubDisplay() {
    if (!_sub || !_statusCb) return;
    _statusCb(*_sub, _statusData);
    _flushSub();
}

// ═════════════════════════════════════════════════
//  flush
// ═════════════════════════════════════════════════

void AXIS_UI::_flushMain() { if (_mainFlush) _mainFlush(); }
void AXIS_UI::_flushSub()  { if (_subFlush)  _subFlush();  }

// ═════════════════════════════════════════════════
//  屏幕管理
// ═════════════════════════════════════════════════

void AXIS_UI::registerScreen(AxisScreenID id,
                              AxisDrawCallback drawFn,
                              AxisInputCallback inputFn,
                              void* userData) {
    if (_screenCount >= AXIS_MAX_SCREENS) {
        Serial.println("[AXIS] WARN: max screens reached.");
        return;
    }
    if (!drawFn) {
        Serial.printf("[AXIS] WARN: screen %d has no drawFn.\n", id);
        return;
    }
    _screens[_screenCount++] = {id, drawFn, inputFn, userData, true};
}

void AXIS_UI::goTo(AxisScreenID id, AxisTransition trans) {
    if (id == _curID) return;
    if (!_findScreen(id)) {
        Serial.printf("[AXIS] WARN: screen %d not registered.\n", id);
        return;
    }

    _prevID = _curID;

    if (trans == AXIS_TRANS_NONE) {
        // 无动画，直接切换
        _curID = id;
        _main->fillScreen(AXIS_C_BG);
    } else {
        // 启动过渡
        _transFromID   = _curID;
        _transToID     = id;
        _transType     = trans;
        _transProgress = 0.0f;
        _transStartMs  = millis();
        _transActive   = true;
        // _curID 保持旧值，过渡结束后才更新
    }
}

void AXIS_UI::goBack(AxisTransition trans) {
    if (_prevID != AXIS_SCR_NONE) goTo(_prevID, trans);
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
    _menuItems      = items;
    _menuCount      = min(count, (uint8_t)AXIS_MAX_MENU_ITEMS);
    _menuOnSelect   = onSelect;
    _menuSelData    = userData;
    _menuCursor     = 0;
    _menuPrevCursor = 0;
    _cursorAnim     = 1.0f;
    _menuTarget     = 1.0f;
}

void AXIS_UI::hideMenu() { _menuTarget = 0.0f; }

// ═════════════════════════════════════════════════
//  通知
// ═════════════════════════════════════════════════

void AXIS_UI::notify(const String& text,
                      uint16_t color, uint32_t durationMs) {
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
    int filled = constrain((int)(ratio * w), 0, w);
    for (int i = 0; i < filled; i++) {
        float t = (w > 1) ? (float)i / (float)(w - 1) : 0.0f;
        _main->drawFastVLine(x + i, y, h, AxisAnim::lerpColor(c1, c2, t));
    }
    if (filled > 0) {
        _main->fillCircle(x + filled - 1, y + h/2, h/2 + 1, AXIS_C_WHITE);
    }
}

void AXIS_UI::drawProgressBar(int x, int y, int w, int h,
                               float ratio, uint16_t col) {
    _main->drawRect(x, y, w, h, AXIS_C_DKGRAY);
    int filled = constrain((int)(ratio * (w-2)), 0, w-2);
    if (filled > 0) _main->fillRect(x+1, y+1, filled, h-2, col);
}

void AXIS_UI::drawVUBar(int x, int y, int w, int h, float level) {
    level = constrain(level, 0.0f, 1.0f);
    _main->fillRect(x, y, w, h, AXIS_C_CARD);
    int filled = (int)(level * h);
    for (int i = 0; i < filled; i++) {
        float t = (h > 1) ? (float)i / (float)(h - 1) : 0.0f;
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
    snprintf(buf, sizeof(buf), "%d:%02d", (int)(s/60), (int)(s%60));
    return String(buf);
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
