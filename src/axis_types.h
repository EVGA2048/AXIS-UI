/**
 * AXIS UI Framework — Types
 * Terminal aesthetic / Industrial minimal
 * https://github.com/EVGA2048/AXIS-UI
 * MIT License
 */

 #pragma once
 #include <stdint.h>
 #include <Arduino.h>
 
 // ═════════════════════════════════════════════════
 //  颜色调色板 — 终端橙黄主题（RGB565）
 // ═════════════════════════════════════════════════
 
 // 背景层
 #define AXIS_C_BG        0x0000   // #080808 纯黑
 #define AXIS_C_BG2       0x0841   // #101010 次级背景
 #define AXIS_C_CARD      0x1082   // #101820 卡片背景
 #define AXIS_C_DIVIDER   0x2945   // #333333 分割线
 
 // 主色 — 终端橙黄
 #define AXIS_C_ACCENT    0xFD20   // #FFA500 终端橙
 #define AXIS_C_ACCENT2   0xFB60   // #FFD000 暖黄（次级强调）
 
 // 报警色
 #define AXIS_C_WARN      0xFB40   // #FF6800 橙色警告
 #define AXIS_C_DANGER    0xF800   // #FF0000 红色危险
 
 // 文字层
 #define AXIS_C_WHITE     0xFFFF   // 纯白
 #define AXIS_C_TEXT      0xDEDB   // #E0E0E0 主文字
 #define AXIS_C_MUTED     0x7BCF   // #808080 次要文字
 #define AXIS_C_DIM       0x39E7   // #404040 暗淡文字
 #define AXIS_C_BLACK     0x0000
 
 // 语义色（仅用于状态指示，不用于装饰）
 #define AXIS_C_OK        0x07E0   // 绿色 正常
 #define AXIS_C_BLUE      0x001F   // 蓝色 信息
 
 // ═════════════════════════════════════════════════
 //  传感器报警阈值
 // ═════════════════════════════════════════════════
 #define AXIS_CO2_WARN     1000    // ppm 橙色警告
 #define AXIS_CO2_DANGER   2000    // ppm 红色危险
 #define AXIS_TVOC_WARN    500     // μg/m³ 橙色警告
 #define AXIS_TVOC_DANGER  1000    // μg/m³ 红色危险
 #define AXIS_TEMP_WARN    35      // °C
 #define AXIS_HUM_WARN     80      // %
 
 // ═════════════════════════════════════════════════
 //  屏幕 ID
 // ═════════════════════════════════════════════════
 typedef uint8_t AxisScreenID;
 #define AXIS_SCR_NONE  0xFF
 
 // 内置屏幕 ID 保留段（0x00-0x0F）
 // 用户自定义从 0x10 开始
 #define AXIS_SCR_HOME   0x00
 #define AXIS_SCR_PLAYER 0x01
 #define AXIS_SCR_MENU   0x02  // 菜单是覆盖层，不是独立屏幕
 
 // ═════════════════════════════════════════════════
 //  屏幕旋转
 // ═════════════════════════════════════════════════
 enum AxisRotation : uint8_t {
     AXIS_ROT_0   = 0,   // 正常
     AXIS_ROT_90  = 1,   // 顺时针90°
     AXIS_ROT_180 = 2,   // 180°
     AXIS_ROT_270 = 3,   // 逆时针90°
 };
 
 // ═════════════════════════════════════════════════
 //  过渡动画类型
 // ═════════════════════════════════════════════════
 enum AxisTransition : uint8_t {
     AXIS_TRANS_NONE  = 0,
     AXIS_TRANS_SLIDE_LEFT,
     AXIS_TRANS_SLIDE_RIGHT,
     AXIS_TRANS_SLIDE_UP,
     AXIS_TRANS_SLIDE_DOWN,
 };
 
 // ═════════════════════════════════════════════════
 //  输入事件
 // ═════════════════════════════════════════════════
 enum AxisInputEvent : uint8_t {
     AXIS_INPUT_NONE = 0,
     AXIS_INPUT_UP,
     AXIS_INPUT_DOWN,
     AXIS_INPUT_LEFT,
     AXIS_INPUT_RIGHT,
     AXIS_INPUT_OK,
     AXIS_INPUT_BTN_A,
     AXIS_INPUT_BTN_B,
     AXIS_INPUT_BTN_A_LONG,  // 长按
     AXIS_INPUT_BTN_B_LONG,
     AXIS_INPUT_OK_LONG,
 };
 
 // ═════════════════════════════════════════════════
 //  菜单项
 // ═════════════════════════════════════════════════
 struct AxisMenuItem {
     const char*    label;
     uint16_t       color      = AXIS_C_ACCENT;
     AxisScreenID   targetScreen = AXIS_SCR_NONE;
     AxisTransition transition  = AXIS_TRANS_SLIDE_LEFT;
 };
 
 // ═════════════════════════════════════════════════
 //  传感器数据结构（供 SCR_HOME 使用）
 // ═════════════════════════════════════════════════
 struct AxisSensorData {
     float    temp     = 0;      // °C
     float    humidity = 0;      // %
     float    pressure = 0;      // hPa
     uint16_t co2      = 0;      // ppm
     float    tvoc     = 0;      // mg/m³
     bool     valid    = false;  // 数据是否有效
 };
 
 // ═════════════════════════════════════════════════
 //  WS2812 灯光效果类型
 // ═════════════════════════════════════════════════
 enum AxisLightEffect : uint8_t {
     AXIS_LIGHT_OFF = 0,
     AXIS_LIGHT_STATIC,      // 静态颜色
     AXIS_LIGHT_BREATHE,     // 呼吸
     AXIS_LIGHT_PULSE,       // 单次脉冲
     AXIS_LIGHT_VU,          // 音量跟随
     AXIS_LIGHT_ALERT_WARN,  // 橙色警告闪烁
     AXIS_LIGHT_ALERT_DANGER,// 红色危险快闪
 };
 
 // ═════════════════════════════════════════════════
 //  回调类型
 // ═════════════════════════════════════════════════
 typedef void (*AxisDrawCallback)(Adafruit_GFX& d,
                                   int16_t xOff, int16_t yOff,
                                   void* userData);
 typedef void (*AxisInputCallback)(AxisInputEvent event,
                                    void* userData);
 typedef void (*AxisStatusCallback)(Adafruit_GFX& d,
                                     void* userData);
 typedef void (*AxisFlushCallback)();
 
 // WS2812 灯光控制回调（用户实现，框架调用）
 typedef void (*AxisLightCallback)(AxisLightEffect effect,
                                    uint32_t color,   // RGB888
                                    uint8_t  param);  // 亮度/速度