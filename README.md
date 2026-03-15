# AXIS UI

A lightweight UI framework for small OLED displays on ESP32.
Smooth animations, minimal footprint, built for physical interaction.

## Features

- Works with any `Adafruit_GFX` compatible display — pass your own screen object, framework doesn't care about the driver
- Screen transition animations: slide left / right / up / down with easing
- Multi-screen state machine with `goTo()` / `goBack()` history
- Animated menu overlay — slides in from bottom, elastic cursor
- Notification overlay with auto-dismiss timer
- Dual display support — main screen + optional status bar on a second display
- Gradient progress bars, VU meter bars, utility drawing functions
- Joystick + button input with debounce, no touchscreen

## Usage
```cpp
// You initialize the display. Framework takes a pointer.
Adafruit_SSD1351 mainDisp(128, 128, &SPI, CS, DC, RST);
mainDisp.begin();

ui.setMainDisplay(&mainDisp);         // SSD1351: no flush needed
ui.setSubDisplay(&statusDisp,         // SSD1306: needs flush
    []() { statusDisp.display(); });

ui.begin(pins);
ui.registerScreen(SCR_MAIN, drawMain, inputMain);
ui.goTo(SCR_MAIN);
```

Screen transitions:
```cpp
ui.goTo(SCR_INFO, AXIS_TRANS_SLIDE_LEFT);    // into sub-page
ui.goBack(AXIS_TRANS_SLIDE_RIGHT);           // back
```

drawFn signature — xOff/yOff are nonzero during transitions:
```cpp
void drawMain(Adafruit_GFX& d, int16_t xOff, int16_t yOff, void* userData) {
    d.setCursor(xOff + 10, 20);   // add xOff to all x positions
    d.print("Hello PRISM");
}
```

## Target Hardware

- Main display: SSD1351 128×128 RGB OLED (SPI)
- Status bar: SSD1306 0.91" (I2C)
- MCU: ESP32 series (tested on WROOM-32E)
- Any other Adafruit_GFX compatible display should work

## Dependencies

Install via Arduino Library Manager:
- Adafruit GFX Library
- Adafruit SSD1351 Library
- Adafruit SSD1306

## Part of the PRISM project

https://github.com/EVGA2048/PRISM

## License

MIT

---

一个跑在小屏幕 OLED 上的 UI 框架，给 ESP32 用的。

起因是做 PRISM 的时候想要顺滑的界面动画，
写着写着觉得这部分可以单独拿出来，以后做其他带屏幕的设备也能用。

屏幕驱动和框架完全分离，用户自己初始化好屏幕对象，传个指针进来就行，
换屏幕不需要改框架代码。

屏幕之间可以带方向的滑动过渡，进子页面往左，返回往右，
drawFn 里把 xOff 加到 x 坐标上，元素就跟着整体滑动。

交互方式是五维摇杆加按键，不打算支持触屏。

还在开发中，API 可能会变。
