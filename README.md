# AXIS UI

[→ 中文说明](#中文说明)

A lightweight UI framework...
A lightweight UI framework for small OLED displays on ESP32.
Smooth animations, minimal footprint, built for physical interaction.

---


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

[→ 中文说明](#中文说明)

---

## 中文说明                    ← 原来那条分割线下面换成这个标题

[→ Back to English](#axis-ui)

[→ Back to English](#axis-ui)

一个跑在小屏幕 OLED 上的 UI 框架，给 ESP32 用的。

起因是做 PRISM 的时候想要顺滑的界面动画，写着写着觉得这部分可以单独拿出来，
以后做其他带屏幕的设备也能用，就把它独立成了一个库。

## 设计思路

市面上大多数 ESP32 的屏幕库要么只管驱动，要么把 UI 逻辑和硬件绑死在一起。
AXIS UI 的思路是把两件事分开：屏幕怎么初始化是你的事，框架只负责画什么和怎么画。
用 `Adafruit_GFX` 作为统一接口，理论上所有 Adafruit 系的驱动都能用，
换屏幕不需要改框架代码。

## 核心功能

**屏幕状态机**
把每个界面注册为一个屏幕，`goTo()` 跳转，`goBack()` 返回，支持历史记录。
每个屏幕有自己的绘制回调和输入回调，互不干扰。

**滑动过渡动画**
屏幕之间切换时可以带方向——进子页面往左滑入，返回往右滑出，或者上下方向。
动画用 easeOutCubic 缓动，不是匀速线性，看起来更自然。
`drawFn` 回调会收到 `xOff` / `yOff` 偏移量，把它加到元素的 x 坐标上，
元素就会跟着整体移动，实现真正的内容滑动而不只是黑屏切换。

**菜单系统**
菜单从屏幕底部滑入，每个菜单项可以设置独立颜色和跳转目标。
光标在菜单项之间移动时有弹性动画（easeOutElastic），
每个选中项左侧有彩色边条作为视觉标记。

**通知系统**
调用 `notify()` 在屏幕顶部弹出一条提示，可以设置颜色和持续时间，到时间自动消失，
不需要手动管理，不影响当前屏幕的正常渲染。

**双屏支持**
主屏负责完整的 UI 界面，副屏可以用来做状态栏（比如 0.91 寸 SSD1306），
两者独立渲染，副屏的内容通过回调函数自定义。
对于需要手动刷新的缓冲型屏幕（如 SSD1306），传入一个 flush 函数即可，
直写型屏幕（如 SSD1351）不需要。

**输入处理**
支持五维摇杆和两个独立按键，内置去抖，按键事件通过回调分发给当前屏幕或菜单。
过渡动画进行中会屏蔽输入，防止误触。

## 局限性

目前所有绘图操作直接调用 `Adafruit_GFX` 的逐像素接口，没有帧缓冲，
在 SSD1351 这类直写屏上全屏刷新会有一定撕裂感，帧率大约在 20-30fps。
后续计划在 ESP32-P4 上引入 DMA + 帧缓冲方案提升流畅度，
目前阶段够用。

这个库还在早期开发中，API 随时可能调整，不建议在正式产品里使用。

