# AXIS UI

A lightweight UI framework for small OLED displays on ESP32.
Smooth animations, minimal footprint, built for physical interaction.

## Features
- Animated menu system with easing curves
- Multi-screen state machine
- Gradient rendering on SSD1351 (RGB565)
- Status bar support for SSD1306
- Scroll, notification overlay, progress bar
- Designed for joystick + button input

## Target Hardware
- Display: SSD1351 128×128 RGB OLED (SPI)
- Status bar: SSD1306 0.91" (I2C)  
- MCU: ESP32 series (tested on WROOM-32E)

## Part of the PRISM project
https://github.com/EVGA2048/PRISM

## License
MIT

---

一个跑在小屏幕 OLED 上的 UI 框架，给 ESP32 用的。

起因是做 PRISM 的时候想要顺滑的界面动画，
写着写着觉得这部分可以单独拿出来，以后做其他带屏幕的设备也能用。

目前支持 SSD1351 彩色主屏和 SSD1306 状态栏的组合，
菜单滑动、通知弹出、进度条这些都有缓动动画。
交互方式是五维摇杆加按键，不打算支持触屏。

还在开发中，API 可能会变。
