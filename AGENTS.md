# AXIS-UI — AI 协作上下文

本文件面向参与 AXIS-UI 开发的 AI 助手（Claude、Cursor、GPT 等）。
读完此文件，你应该能直接开始工作，无需问基础问题。

---

## 项目是什么

AXIS-UI 是面向 **ESP32 + 小屏 OLED/TFT** 的轻量 UI 框架：

- 物理按键 / 摇杆导航
- 屏幕切换动画（Slide / Fade / None）
- 双屏支持（主屏 + 状态栏）
- 终端橙黄主题，工业极简风
- 双语支持（EN / ZH），NVS 持久化
- 重力光标（MPU-6050 联动）
- 灵动消息栏（ticker）
- 内置屏保

主用户项目：[PRISM](https://github.com/EVGA2048/PRISM)

---

## 仓库结构

```
AXIS-UI/
├── src/
│   ├── AXIS_UI.h         框架主体（类定义、API）
│   ├── AXIS_UI.cpp       框架实现
│   ├── axis_types.h      颜色常量、屏幕ID、输入事件、传感器结构
│   └── axis_anim.h       缓动函数与动画辅助
├── examples/
│   ├── BasicMenu/        完整功能演示（v4，Arduino IDE）
│   └── P4_HardwareTest/  P4-Pico 硬件验证
└── library.properties    Arduino 库元数据
```

---

## 技术栈

- **图形栈**：**LovyanGFX**（`LGFX_Device`、Sprite 离屏缓冲、`pushSprite`）
- **注意**：README 中仍有 Adafruit_GFX 的历史描述，以源码为准，实际用 LovyanGFX
- **平台**：Arduino，`architectures=esp32`
- **中文渲染**：`u8g2_font_wqy12_t_gb2312b` / `u8g2_font_wqy16_t_gb2312b`

---

## 核心 API

### 初始化

```cpp
AXIS_UI ui;
ui.setMainDisplay(&tft);       // 必须在 begin() 之前调用
ui.begin(pins);                // pins 是 AxisPins 结构体，含摇杆/按键引脚
```

### 屏幕管理

```cpp
ui.registerScreen(SCR_ID, drawFn, inputFn);  // inputFn=nullptr 表示 view-only
ui.goHome();
ui.goTo(SCR_ID, AXIS_TRANS_SLIDE);
```

内置屏幕 ID（`axis_types.h`）：`AXIS_SCR_HOME`、`AXIS_SCR_PLAYER` 等；用户自定义从 `0x10` 起。

### 数据注入

```cpp
ui.setTime("14:30");
ui.setDate("2025-01-01");
ui.setTrackTitle("Song Name");
ui.setTrackArtist("Artist");
ui.bubble("通知内容", COLOR);       // 灵动消息栏推送
ui.setTickerItem(n, "text", color); // ticker 固定位置内容
ui.setSensorData(data);
```

### 输入注入（重力光标用）

```cpp
ui.injectInput(AXIS_INPUT_LEFT);
```

### 其他

```cpp
ui.setLanguage(AXIS_LANG_EN);  // 或 AXIS_LANG_ZH
ui.language();                  // 返回当前语言
ui.nodeCursor();                // HOME 当前选中节点索引，-1=HOME中心
ui.setJoyRotation(1);          // 摇杆方向重映射（1=顺时针90°修正）
```

---

## 视觉规范

- **主色**：`AXIS_C_ACCENT`（#FFA500 橙黄，RGB565 见 `axis_types.h`）
- **背景**：纯黑 `AXIS_C_BG`
- **风格**：硬直角，无圆角，高信息密度克制
- **动效**：有重量感，easeOutElastic，120–200ms 量级
- **字体**：
  - ASCII：`setTextFont(1)`（6×8px）/ `setTextFont(2)`（16px）
  - 中文：`u8g2_font_wqy12/16_t_gb2312b`，用 `drawString()` 不用 `print()`

---

## 中文渲染注意事项

```cpp
// 正确方式
lgfx::U8g2font zhFont(u8g2_font_wqy12_t_gb2312b);
spr.setFont(&zhFont);
spr.setTextDatum(0);
spr.drawString("中文", x, y);

// 禁止
spr.print("中文");  // 逐字节输出，破坏 UTF-8
```

---

## P4-Pico + SSD1351 LovyanGFX 配置

```cpp
cfg.spi_host    = SPI2_HOST;
cfg.freq_write  = 40000000;  // 实际20MHz（P4 APB 2× 分频偏差）
cfg.dma_channel = 0;          // 必须！绕开 P4 AXI DMA LINK2 寄存器 bug
cfg.pin_sclk = 15;  cfg.pin_mosi = 16;  cfg.pin_dc = 18;
panel: cfg.pin_cs = 17;  cfg.pin_rst = 19;
tft.setRotation(6);  // 唯一正确值
```

LDO 需独立 5V 供电，不能接 VSYS。

---

## 安装

Arduino IDE：将仓库 symlink 或复制到 `~/Documents/Arduino/libraries/AXIS-UI/`

验证工程：`examples/BasicMenu/`（功能完整，包含所有特性演示）

---

## 协作约定

- 改公共 API 时：同步更新 `README.md` 和 PRISM 侧 `CLAUDE.md` 中 AXIS 相关段落
- 三者冲突时以**头文件 + cpp 实现**为准
- 变量/函数命名：`camelCase`（Arduino 风格）
- 与 PRISM 联调用注入式 setter，框架不直接绑 UART
