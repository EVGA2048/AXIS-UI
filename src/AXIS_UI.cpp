#include "AXIS_UI.h"
#include <Wire.h>
#include <SPI.h>

// ─────────────────────────────────────────
//  构造（直接使用 SPI 硬件，引脚由 begin 传入）
// ─────────────────────────────────────────
// 注意：Adafruit_SSD1351 和 SSD1306 在 begin() 前先用占位构造

bool AXIS_UI::begin(const AxisPinConfig& pins) {
    _pins = pins;

    // 初始化 SSD1351 主屏（VSPI）
    SPI.begin(18, -1, 23, pins.oled_cs);
    new (&_disp) Adafruit_SSD1351(128, 128, &SPI,
                                   pins.oled_cs,
                                   pins.oled_dc,
                                   pins.oled_rst);
    _disp.begin();
    _disp.fillScreen(AXIS_C_BG);

    // 初始化 SSD1306 状态栏（I2C）
    Wire.begin(pins.i2c_sda, pins.i2c_scl);
    new (&_bar) Adafruit_SSD1306(128, 32, &Wire, -1);
    bool barOk = _bar.begin(SSD1306_SWITCHCAPVCC, pins.ssd1306_addr);
    if (barOk) {
        _bar.clearDisplay();
        _bar.display();
    }

    // 输入引脚
    int8_t pullupPins[] = {
        pins.joy_up, pins.joy_left, pins.joy_right,
        pins.joy_ok, pins.btn_a, pins.btn_b
    };
    for (int8_t p : pullupPins) {
        if (p >= 0) pinMode(p, INPUT_PULLUP);
    }
    // GPIO34 无内部上拉，设为纯输入
    if (pins.joy_down >= 0) pinMode(pins.joy_down, INPUT);

    _lastFrameMs = millis();
    return barOk;
}

// ─────────────────────────────────────────
//  主循环
// ─────────────────────────────────────────
void AXIS_UI::update() {
    uint32_t now = millis();
    if (now - _lastFrameMs < _frameMs) return;
    _lastFrameMs = now;

    // 动画状态更新
    _menuSlide   = AxisAnim::smoothFollow(_menuSlide, _menuTarget, 0.12f);
    if (_menuSlide < 0.01f && _menuTarget == 0.0f) _menuSlide = 0.0f;
    if (_cursorAnim < 1.0f) _cursorAnim = min(1.0f, _cursorAnim + 0.08f);

    // 通知超时检查
    if (_hasNotification && now >= _notification.showUntil) {
        _hasNotification = false;
    }

    // 输入处理
    AxisInputEvent ev = _pollInput();

    if (ev != AXIS_INPUT_NONE) {
        if (isMenuVisible()) {
            // 菜单输入
            if (ev == AXIS_INPUT_UP && _menuCursor > 0) {
                _menuPrevCursor = _menuCursor;
                _menuCursor--;
                _cursorAnim = 0.0f;
            }
            else if (ev == AXIS_INPUT_DOWN && _menuCursor < _menuCount - 1) {
                _menuPrevCursor = _menuCursor;
                _menuCursor++;
                _cursorAnim = 0.0f;
            }
            else if (ev == AXIS_INPUT_OK || ev == AXIS_INPUT_BTN_B) {
                const AxisMenuItem& item = _menuItems[_menuCursor];
                if (item.targetScreen != AXIS_SCR_NONE) {
                    hideMenu();
                    goTo(item.targetScreen);
                } else if (_menuOnSelect) {
                    _menuOnSelect(ev, _menuUserData);
                }
            }
            else if (ev == AXIS_INPUT_BTN_A) {
                hideMenu();
            }
        } else {
            // 当前屏幕输入
            AxisScreen* scr = _findScreen(_curScreenID);
            if (scr && scr->inputFn) {
                scr->inputFn(ev, scr->userData);
            }
        }
    }

    // 渲染当前屏幕
    AxisScreen* scr = _findScreen(_curScreenID);
    if (scr && scr->drawFn) {
        scr->drawFn(_disp, scr->userData);
    }

    // 渲染叠加层
    if (_hasNotification)  _renderNotification();
    if (_menuSlide > 0.01f) _renderMenu();

    // 状态栏
    _renderStatusBar();
}

// ─────────────────────────────────────────
//  屏幕管理
// ─────────────────────────────────────────
void AXIS_UI::registerScreen(AxisScreenID id,
                              AxisDrawCallback drawFn,
                              AxisInputCallback inputFn,
                              void* userData) {
    if (_screenCount >= AXIS_MAX_SCREENS) return;
    _screens[_screenCount++] = {id, drawFn, inputFn, userData, true};
}

void AXIS_UI::goTo(AxisScreenID id) {
    _prevScreenID = _curScreenID;
    _curScreenID  = id;
    _disp.fillScreen(AXIS_C_BG);
}

void AXIS_UI::goBack() {
    if (_prevScreenID != AXIS_SCR_NONE) goTo(_prevScreenID);
}

AxisScreen* AXIS_UI::_findScreen(AxisScreenID id) {
    for (uint8_t i = 0; i < _screenCount; i++) {
        if (_screens[i].valid && _screens[i].id == id) return &_screens[i];
    }
    return nullptr;
}

// ─────────────────────────────────────────
//  菜单系统
// ─────────────────────────────────────────
void AXIS_UI::showMenu(const AxisMenuItem* items, uint8_t count,
                        AxisInputCallback onSelect, void* userData) {
    _menuItems    = items;
    _menuCount    = count;
    _menuOnSelect = onSelect;
    _menuUserData = userData;
    _menuCursor   = 0;
    _menuPrevCursor = 0;
    _cursorAnim   = 1.0f;
    _menuTarget   = 1.0f;
}

void AXIS_UI::hideMenu() {
    _menuTarget = 0.0f;
}

void AXIS_UI::_renderMenu() {
    float ease = AxisAnim::easeOutCubic(_menuSlide);
    int oy = (int)((1.0f - ease) * 128);

    _disp.fillRoundRect(6, oy, 116, 120, 8, 0x0C4A);
    _disp.drawRoundRect(6, oy, 116, 120, 8, AXIS_C_ACCENT);

    _disp.setTextColor(AXIS_C_ACCENT);
    _disp.setTextSize(1);
    _disp.setCursor(34, oy + 6);
    _disp.print("[ MENU ]");
    _disp.drawFastHLine(8, oy + 16, 112, AXIS_C_DIVIDER);

    // 光标滑动
    float cEase = AxisAnim::easeOutElastic(min(_cursorAnim, 1.0f));
    float cY    = _menuPrevCursor + (_menuCursor - _menuPrevCursor) * cEase;
    int   hlY   = oy + 20 + (int)(cY * 22);

    _disp.fillRoundRect(10, hlY, 108, 19, 4, 0x0C2E);
    _disp.fillRect(10, hlY, 3, 19, _menuItems[_menuCursor].color);

    for (uint8_t i = 0; i < _menuCount; i++) {
        int iy = oy + 25 + i * 22;
        _disp.setTextColor(i == _menuCursor ? _menuItems[i].color : AXIS_C_GRAY);
        _disp.setTextSize(1);
        _disp.setCursor(18, iy);
        _disp.print(_menuItems[i].label);
        if (i == (uint8_t)_menuCursor) {
            _disp.fillTriangle(108, iy+1, 108, iy+7, 112, iy+4,
                               _menuItems[i].color);
        }
    }

    _disp.drawFastHLine(8, oy + 110, 112, AXIS_C_DIVIDER);
    _disp.setTextColor(AXIS_C_DKGRAY);
    _disp.setTextSize(1);
    _disp.setCursor(10, oy + 113);
    _disp.print("U/D:Nav  OK:Enter  A:Back");
}

// ─────────────────────────────────────────
//  通知系统
// ─────────────────────────────────────────
void AXIS_UI::showNotification(const String& text,
                                uint16_t color,
                                uint32_t durationMs) {
    _notification.text      = text;
    _notification.color     = color;
    _notification.showUntil = millis() + durationMs;
    _hasNotification        = true;
}

void AXIS_UI::_renderNotification() {
    _disp.fillRoundRect(2, 20, 124, 12, 3, 0x9824);
    _disp.drawRoundRect(2, 20, 124, 12, 3, _notification.color);
    _disp.setTextColor(AXIS_C_WHITE);
    _disp.setTextSize(1);
    _disp.setCursor(5, 23);
    _disp.print(truncate(_notification.text, 20));
}

// ─────────────────────────────────────────
//  状态栏
// ─────────────────────────────────────────
void AXIS_UI::setStatusBarCallback(AxisStatusBarCallback cb, void* userData) {
    _statusBarCb   = cb;
    _statusBarData = userData;
}

void AXIS_UI::_renderStatusBar() {
    if (!_statusBarCb) return;
    _bar.clearDisplay();
    _statusBarCb(_bar, _statusBarData);
    _bar.display();
}

// ─────────────────────────────────────────
//  绘图工具
// ─────────────────────────────────────────
void AXIS_UI::drawGradBar(int x, int y, int w, int h,
                           float ratio, uint16_t c1, uint16_t c2,
                           uint16_t bg) {
    _disp.fillRoundRect(x, y, w, h, h/2, bg);
    int filled = max(h, (int)(ratio * w));
    for (int i = 0; i < filled; i++) {
        _disp.drawFastVLine(x + i, y, h,
                            AxisAnim::lerpColor(c1, c2, (float)i / max(w,1)));
    }
    if (filled > 0) _disp.fillCircle(x + filled - 1, y + h/2, h/2 + 1, AXIS_C_WHITE);
}

void AXIS_UI::drawProgressBar(int x, int y, int w, int h,
                               float ratio, uint16_t col) {
    _disp.drawRect(x, y, w, h, AXIS_C_GRAY);
    int filled = (int)(ratio * (w - 2));
    if (filled > 0) _disp.fillRect(x+1, y+1, filled, h-2, col);
}

String AXIS_UI::truncate(const String& s, int maxChars) {
    if ((int)s.length() <= maxChars) return s;
    return s.substring(0, maxChars - 1) + "~";
}

String AXIS_UI::formatTime(int32_t s) {
    if (s < 0) s = 0;
    char buf[8];
    snprintf(buf, sizeof(buf), "%d:%02d", s/60, s%60);
    return String(buf);
}

// ─────────────────────────────────────────
//  输入处理
// ─────────────────────────────────────────
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
    if (digitalRead(pin) == LOW && millis() - _debounce[idx] > DEBOUNCE_MS) {
        _debounce[idx] = millis();
        return true;
    }
    return false;
}
