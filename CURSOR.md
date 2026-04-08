# AXIS-UI — Cursor 协作上下文

本文件给 **Cursor（Composer / Auto）** 用，与 PRISM 仓库里的 **`CLAUDE.md`**（Claude Code）并列。两边会一起改这个库时，**重要约定以源码为准**；改 API、依赖或视觉规范后，请同步更新本文件、`README.md` 以及 PRISM 侧 `CLAUDE.md` 里和 AXIS 相关的段落，避免各说各话。

## 项目是什么

- **AXIS-UI**：面向 ESP32 + 小屏 OLED 的轻量 UI 库，物理按键 / 摇杆，带过渡动画与双屏能力。
- **主用户项目**：[PRISM](https://github.com/EVGA2048/PRISM)（`~/Documents/git_repository/PRISM`）。UART 协议、整机硬件与 PRISM 文档在 PRISM 仓；本仓只负责 UI 框架。

## 仓库结构

| 路径 | 说明 |
|------|------|
| `src/AXIS_UI.{h,cpp}` | 框架主体 |
| `src/axis_types.h` | 颜色、屏幕 ID、输入事件、传感器结构等 |
| `src/axis_anim.h` | 缓动与动画辅助 |
| `examples/` | Arduino 示例（如 `BasicMenu`、`P4_HardwareTest`） |
| `library.properties` | Arduino 库元数据 |

## 技术栈（以当前代码为准）

- **图形栈**：**LovyanGFX**（`LGFX_Device`、Sprite 离屏缓冲、`pushSprite`）。  
  **不是** README 里仍写的 Adafruit_GFX 直驱路径；若文档或 `library.properties` 的 `depends=` 与代码不一致，应视为待修正的文档债。
- **平台**：Arduino，`architectures=esp32`。
- **内置屏幕 ID**：`AXIS_SCR_HOME`、`AXIS_SCR_PLAYER` 等见 `axis_types.h`；用户自定义屏一般从 `0x10` 起。

## 视觉与交互（与 PRISM 一致处）

- **终端橙黄主题**：主强调色与 PRISM 约定一致（如 `#FFA500` → `AXIS_C_ACCENT`）；具体 RGB565 定义以 `axis_types.h` 为准。
- **工业极简**：硬直角、信息密度克制；动效偏「有重量」（如 easeOutElastic / 过渡时长与 CLAUDE 里 120–200ms 量级一致）。
- **灯光 / 状态**：WS2812 等通过回调由上层接；AXIS 提供触发与语义，不把整机逻辑写死在库里。

## 协作约定

1. **改公共 API**（类 `AXIS_UI` 对外方法、回调签名、`axis_types.h` 里对外类型）时：更新 `README.md` 用法示例，并在本文件或 PRISM `CLAUDE.md` 记一句，方便另一助手接着干。
2. **大型行为变更**：在 commit 说明或简短注释里写清动机；v0.1 阶段 API 仍可能调整，但避免无声破坏示例工程。
3. **与 PRISM 联调**：媒体/通知/时间/传感器等多用「注入式」setter（如 `setTrackTitle`、`setSensorData`、`bubble`），与 PRISM `shared/prism_uart.h` 中的消息类型设计对齐，由 32E / P4 应用层填数据，框架不直接绑 UART。

## 给另一位助手的一句话

若你在 PRISM 里用 **Claude**，在 AXIS-UI 里用 **Cursor**：拉代码后先看 **`src/AXIS_UI.h`** 与 **`axis_types.h`**，再看 `README.md`；三者冲突时以 **头文件 + cpp 实现** 为准。本文件路径：`AXIS-UI/CURSOR.md`。
