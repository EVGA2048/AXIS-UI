/**
 * AXIS UI Framework — BasicMenu 示例
 * 终端橙黄主题 / 工业极简风格
 *
 * 演示：
 *   - 内置 SCR_HOME 桌面（时间+传感器+媒体缩略）
 *   - 内置 SCR_PLAYER 播放器界面
 *   - 用户自定义屏幕（SCR_INFO / SCR_SETTINGS）
 *   - 传感器数据注入 + 自动报警
 *   - WS2812 灯光联动回调
 *   - MPU6050 屏幕旋转（模拟，实际接入替换）
 *   - U8g2 UTF-8 文字
 *   - 菜单系统 + 通知
 *
 * 接线（YD-ESP32 WROOM-32E）：
 *   SSD1351 : SCK=18  MOSI=23  CS=2  DC=17  RST=15
 *   SSD1306 : SDA=21  SCL=33
 *   摇杆    : UP=19  DOWN=32  LEFT=13  RIGHT=27  OK=4
 *   按键    : A=14   B=12
 *   WS2812  : GPIO=16（板载）或外接
 *
 * 依赖库（Library Manager）：
 *   Adafruit SSD1351 Library
 *   Adafruit SSD1306
 *   Adafruit GFX Library
 *   U8g2_for_Adafruit_GFX
 *   Adafruit NeoPixel
 */

 #include <SPI.h>
 #include <Wire.h>
 #include <Adafruit_SSD1351.h>
 #include <Adafruit_SSD1306.h>
 #include <Adafruit_NeoPixel.h>
 #include <AXIS_UI.h>
 
 // ── 引脚 ─────────────────────────────────────────
 #define PIN_OLED_CS    2
 #define PIN_OLED_DC    17
 #define PIN_OLED_RST   15
 #define PIN_I2C_SDA    21
 #define PIN_I2C_SCL    33
 #define PIN_WS2812     16   // YD-ESP32 板载 RGB LED
 #define WS2812_COUNT   9    // 1板载 + 8外接
 
 // ── 屏幕对象 ─────────────────────────────────────
 Adafruit_SSD1351  mainDisp(128, 128, &SPI,
                             PIN_OLED_CS, PIN_OLED_DC, PIN_OLED_RST);
 Adafruit_SSD1306  statusDisp(128, 32, &Wire, -1);
 Adafruit_NeoPixel pixels(WS2812_COUNT, PIN_WS2812, NEO_GRB + NEO_KHZ800);
 
 // ── AXIS UI 实例 ─────────────────────────────────
 AXIS_UI ui;
 
 // ── 用户屏幕 ID（0x10 起）────────────────────────
 enum UserScreenID : uint8_t {
     SCR_INFO     = 0x10,
     SCR_SETTINGS = 0x11,
 };
 
 // ── 菜单定义 ─────────────────────────────────────
 const AxisMenuItem mainMenu[] = {
     { "NOW PLAYING", AXIS_C_ACCENT,  AXIS_SCR_PLAYER, AXIS_TRANS_SLIDE_LEFT  },
     { "SYSTEM INFO", AXIS_C_ACCENT2, SCR_INFO,         AXIS_TRANS_SLIDE_LEFT  },
     { "SETTINGS",    AXIS_C_MUTED,   SCR_SETTINGS,     AXIS_TRANS_SLIDE_LEFT  },
     { "HOME",        AXIS_C_DIM,     AXIS_SCR_HOME,    AXIS_TRANS_SLIDE_RIGHT },
 };
 const uint8_t MENU_COUNT = sizeof(mainMenu) / sizeof(mainMenu[0]);
 
 // ── 模拟数据（后续替换为真实传感器读值）─────────
 struct SimState {
     float    temp      = 23.4f;
     float    humidity  = 61.0f;
     float    pressure  = 1013.0f;
     uint16_t co2       = 420;
     float    tvoc      = 0.1f;
     uint8_t  battery   = 88;
     uint8_t  volume    = 75;
     float    progress  = 0.0f;
     bool     playing   = false;
     bool     btConn    = false;
     // 时间（后续接 RTC 或从手机同步）
     String   timeStr   = "21:46";
     String   dateStr   = "MON 16 MAR";
 } sim;
 
 // WS2812 灯光状态
 struct LightState {
     AxisLightEffect effect   = AXIS_LIGHT_OFF;
     uint32_t        color    = 0;
     uint8_t         param    = 0;
     float           breatheT = 0.0f;
 } light;
 
 // ─────────────────────────────────────────────────
 //  WS2812 灯光回调
 //  框架调用这个函数，用户控制实际硬件
 // ─────────────────────────────────────────────────
 void lightCallback(AxisLightEffect effect, uint32_t color, uint8_t param) {
     light.effect = effect;
     light.color  = color;
     light.param  = param;
 }
 
 // WS2812 每帧更新（在 loop 里调用）
 void updateLights() {
     uint32_t now = millis();
 
     switch (light.effect) {
         case AXIS_LIGHT_OFF:
             pixels.fill(0);
             break;
 
         case AXIS_LIGHT_STATIC:
             pixels.fill(light.color);
             break;
 
         case AXIS_LIGHT_BREATHE: {
             // 正弦呼吸
             float t   = (float)(now % 3000) / 3000.0f;
             float bri = (sinf(t * 2 * M_PI) + 1.0f) / 2.0f;
             uint8_t r = ((light.color >> 16) & 0xFF) * bri;
             uint8_t g = ((light.color >>  8) & 0xFF) * bri;
             uint8_t b = ( light.color        & 0xFF) * bri;
             pixels.fill(pixels.Color(r, g, b));
             break;
         }
 
         case AXIS_LIGHT_VU: {
             // 音量跟随：橙色，亮度跟音量
             float bri = sim.volume / 100.0f;
             // 播放时加节拍律动（简单模拟）
             if (sim.playing) {
                 float beat = (float)(now % 600) / 600.0f;
                 bri *= (0.6f + 0.4f * sinf(beat * 2 * M_PI));
             }
             uint8_t r = 0xFF * bri;
             uint8_t g = 0x80 * bri;
             uint8_t b = 0;
             pixels.fill(pixels.Color(r, g, b));
             break;
         }
 
         case AXIS_LIGHT_PULSE: {
             // 单次脉冲：快速亮起后衰减
             float t   = fmod((float)(now % 1000) / 1000.0f, 1.0f);
             float bri = t < 0.1f ? (t / 0.1f) : (1.0f - (t - 0.1f) / 0.9f);
             bri = max(0.0f, bri);
             uint8_t r = ((light.color >> 16) & 0xFF) * bri;
             uint8_t g = ((light.color >>  8) & 0xFF) * bri;
             uint8_t b = ( light.color        & 0xFF) * bri;
             pixels.fill(pixels.Color(r, g, b));
             break;
         }
 
         case AXIS_LIGHT_ALERT_WARN: {
             // 橙色慢闪
             bool on = (now % 1200) < 600;
             pixels.fill(on ? 0xFF6800 : 0);
             break;
         }
 
         case AXIS_LIGHT_ALERT_DANGER: {
             // 红色快闪
             bool on = (now % 400) < 200;
             pixels.fill(on ? 0xFF0000 : 0);
             break;
         }
     }
     pixels.show();
 }
 
 // ─────────────────────────────────────────────────
 //  状态栏（SSD1306 0.91"）
 // ─────────────────────────────────────────────────
 void drawStatusBar(Adafruit_GFX& d, void*) {
     d.fillScreen(0);
     d.setTextSize(1);
     d.setTextColor(SSD1306_WHITE);
 
     // BT 状态方块
     if (sim.btConn) d.fillRect(0, 0, 7, 7, SSD1306_WHITE);
     else            d.drawRect(0, 0, 7, 7, SSD1306_WHITE);
 
     // 播放状态
     d.setCursor(10, 0);
     d.print(sim.playing ? ">" : "||");
 
     // 标题
     d.setCursor(22, 0);
     String title = ui.truncate(sim.playing ? "Blinding Lights" : "PRISM", 14);
     d.print(title);
 
     // 电量（右侧）
     d.drawRect(108, 0, 18, 7, SSD1306_WHITE);
     d.fillRect(126, 2, 2, 3, SSD1306_WHITE);
     int fill = (int)(sim.battery / 100.0f * 16);
     if (fill > 0) d.fillRect(109, 1, fill, 5, SSD1306_WHITE);
 
     // 分割线
     d.drawFastHLine(0, 9, 128, SSD1306_WHITE);
 
     // 传感器最重要一项
     d.setCursor(0, 12);
     if (sim.co2 >= AXIS_CO2_WARN) {
         d.print("! CO2:");
         d.print(sim.co2);
         d.print("ppm");
     } else if (sim.tvoc >= AXIS_TVOC_WARN) {
         d.print("! TVOC:");
         d.print(sim.tvoc, 1);
     } else {
         d.print("T:");
         d.print(sim.temp, 1);
         d.print("  H:");
         d.print((int)sim.humidity);
         d.print("%");
     }
 
     // 进度条（底部）
     d.drawRect(0, 28, 128, 4, SSD1306_WHITE);
     int pw = (int)(sim.progress * 128);
     if (pw > 0) d.fillRect(0, 28, pw, 4, SSD1306_WHITE);
 }
 
 // ─────────────────────────────────────────────────
 //  SCR_INFO — 用户自定义屏幕
 // ─────────────────────────────────────────────────
 void drawInfo(Adafruit_GFX& d, int16_t xOff, int16_t yOff, void*) {
     d.fillScreen(AXIS_C_BG);
     int16_t W = d.width();
 
     // 顶部标题行
     d.drawFastHLine(xOff, yOff, W, AXIS_C_ACCENT);
     d.drawFastHLine(xOff, yOff+1, W, AXIS_C_ACCENT);
     ui.setFont(u8g2_font_tom_thumb_4x6_tr);
     ui.setFontColor(AXIS_C_ACCENT);
     ui.drawText(xOff + 2, yOff + 10, "SYSTEM INFO");
 
     d.drawFastHLine(xOff, yOff + 12, W, AXIS_C_DIVIDER);
 
     // 信息行
     struct Row { const char* label; String value; uint16_t col; };
     Row rows[] = {
         { "BOARD",   "YD-ESP32",                AXIS_C_TEXT   },
         { "CORE",    "ESP32 WROOM-32E",          AXIS_C_MUTED  },
         { "UI",      "AXIS UI v0.1",             AXIS_C_ACCENT },
         { "PHASE",   "1 - AUDIO",                AXIS_C_ACCENT2},
         { "NODELIB", "ANCS v1.x",                AXIS_C_MUTED  },
         { "BT",      sim.btConn ? "ON" : "OFF",  sim.btConn ? AXIS_C_OK : AXIS_C_DIM },
     };
 
     ui.setFont(u8g2_font_tom_thumb_4x6_tr);
     for (int i = 0; i < 6; i++) {
         int16_t ry = yOff + 22 + i * 15;
         d.drawFastHLine(xOff, ry - 1, W, AXIS_C_DIVIDER);
         ui.setFontColor(AXIS_C_DIM);
         ui.drawText(xOff + 2, ry + 6, rows[i].label);
         ui.setFontColor(rows[i].col);
         ui.drawText(xOff + 50, ry + 6, rows[i].value);
     }
 
     // 底部
     d.drawFastHLine(xOff, yOff + 114, W, AXIS_C_DIVIDER);
     ui.setFontColor(AXIS_C_DIM);
     ui.drawText(xOff + 2, yOff + 123, "[A] BACK");
 }
 
 void inputInfo(AxisInputEvent ev, void*) {
     if (ev == AXIS_INPUT_BTN_A) ui.goBack(AXIS_TRANS_SLIDE_RIGHT);
 }
 
 // ─────────────────────────────────────────────────
 //  SCR_SETTINGS
 // ─────────────────────────────────────────────────
 void drawSettings(Adafruit_GFX& d, int16_t xOff, int16_t yOff, void*) {
     d.fillScreen(AXIS_C_BG);
     int16_t W = d.width();
 
     d.drawFastHLine(xOff, yOff, W, AXIS_C_ACCENT);
     d.drawFastHLine(xOff, yOff+1, W, AXIS_C_ACCENT);
     ui.setFont(u8g2_font_tom_thumb_4x6_tr);
     ui.setFontColor(AXIS_C_ACCENT);
     ui.drawText(xOff + 2, yOff + 10, "SETTINGS");
     d.drawFastHLine(xOff, yOff + 12, W, AXIS_C_DIVIDER);
 
     // 设置项（Phase 1 暂用占位）
     const char* items[] = {
         "BRIGHTNESS  [##########]",
         "ROTATION    AUTO",
         "BT NAME     PRISM",
         "SLEEP       5MIN",
         "THEME       TERMINAL",
     };
     for (int i = 0; i < 5; i++) {
         int16_t ry = yOff + 22 + i * 18;
         d.drawFastHLine(xOff, ry - 1, W, AXIS_C_DIVIDER);
         ui.setFontColor(AXIS_C_MUTED);
         ui.drawText(xOff + 2, ry + 8, items[i]);
     }
 
     d.drawFastHLine(xOff, yOff + 114, W, AXIS_C_DIVIDER);
     ui.setFontColor(AXIS_C_DIM);
     ui.drawText(xOff + 2, yOff + 123, "[A] BACK");
 }
 
 void inputSettings(AxisInputEvent ev, void*) {
     if (ev == AXIS_INPUT_BTN_A) ui.goBack(AXIS_TRANS_SLIDE_RIGHT);
 }
 
 // ─────────────────────────────────────────────────
 //  主屏和播放器输入
 // ─────────────────────────────────────────────────
 void inputHome(AxisInputEvent ev, void*) {
     switch (ev) {
         case AXIS_INPUT_OK:
             ui.showMenu(mainMenu, MENU_COUNT);
             break;
         case AXIS_INPUT_BTN_A:
             ui.goTo(AXIS_SCR_PLAYER, AXIS_TRANS_SLIDE_LEFT);
             break;
         // 长按 A = 返回 Home（从任何页面）
         case AXIS_INPUT_BTN_A_LONG:
             ui.goHome();
             break;
         default: break;
     }
 }
 
 void inputPlayer(AxisInputEvent ev, void*) {
     switch (ev) {
         case AXIS_INPUT_OK:
             sim.playing = !sim.playing;
             ui.setPlaying(sim.playing);
             ui.notify(sim.playing ? "PLAY" : "STOP",
                       sim.playing ? AXIS_C_ACCENT : AXIS_C_MUTED, 1500);
             break;
         case AXIS_INPUT_BTN_A:
             sim.volume = min(100, (int)sim.volume + 5);
             ui.setVolume(sim.volume);
             break;
         case AXIS_INPUT_BTN_B:
             sim.volume = max(0, (int)sim.volume - 5);
             ui.setVolume(sim.volume);
             break;
         case AXIS_INPUT_LEFT:
             sim.progress = 0.0f;
             ui.setProgress(0.0f);
             ui.notify("PREV", AXIS_C_ACCENT, 1000);
             break;
         case AXIS_INPUT_RIGHT:
             sim.progress = 0.0f;
             ui.setProgress(0.0f);
             ui.notify("NEXT", AXIS_C_ACCENT, 1000);
             break;
         case AXIS_INPUT_BTN_A_LONG:
             ui.goHome();
             break;
         default: break;
     }
 }
 
 // ─────────────────────────────────────────────────
 //  模拟传感器数据更新（后续替换为真实读值）
 // ─────────────────────────────────────────────────
 void updateSimSensors() {
     static unsigned long last = 0;
     if (millis() - last < 2000) return;
     last = millis();
 
     // 模拟缓慢变化
     sim.temp     += (random(-10, 10) / 100.0f);
     sim.humidity += (random(-5, 5)   / 100.0f);
     sim.co2       = 400 + random(0, 50);
 
     AxisSensorData data;
     data.temp     = sim.temp;
     data.humidity = sim.humidity;
     data.pressure = sim.pressure;
     data.co2      = sim.co2;
     data.tvoc     = sim.tvoc;
     data.valid    = true;
     ui.setSensorData(data);
 }
 
 // 模拟进度推进
 void updateSimProgress() {
     static unsigned long last = 0;
     if (!sim.playing || millis() - last < 1000) return;
     last = millis();
     sim.progress += 1.0f / 210.0f;
     if (sim.progress > 1.0f) sim.progress = 0.0f;
     ui.setProgress(sim.progress);
 }
 
 // ─────────────────────────────────────────────────
 //  setup
 // ─────────────────────────────────────────────────
 void setup() {
     Serial.begin(115200);
     Serial.println("[PRISM] Booting...");
 
     // WS2812
     pixels.begin();
     pixels.setBrightness(80);
     pixels.fill(0xFF8000);  // 开机橙色
     pixels.show();
 
     // 主屏（SSD1351）
     SPI.begin(18, -1, 23, PIN_OLED_CS);
     mainDisp.begin();
     mainDisp.fillScreen(AXIS_C_BG);
 
     // 副屏（SSD1306）
     Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
     bool subOk = statusDisp.begin(SSD1306_SWITCHCAPVCC, 0x3C);
     if (!subOk) Serial.println("[WARN] SSD1306 not found");
 
     // ── 传入屏幕给框架 ────────────────────────────
     ui.setMainDisplay(&mainDisp, nullptr);
     if (subOk) {
         ui.setSubDisplay(&statusDisp, []() { statusDisp.display(); });
         ui.setStatusCallback(drawStatusBar);
     }
 
     // ── WS2812 回调 ───────────────────────────────
     ui.setLightCallback(lightCallback);
 
     // ── 输入引脚 ──────────────────────────────────
     AxisPinConfig pins;
     pins.joy_up    = 19;
     pins.joy_down  = 32;
     pins.joy_left  = 13;
     pins.joy_right = 27;
     pins.joy_ok    = 4;
     pins.btn_a     = 14;
     pins.btn_b     = 12;
 
     if (!ui.begin(pins)) {
         Serial.println("[ERROR] AXIS UI init failed");
         while (true) delay(1000);
     }
 
     // ── 注册用户屏幕 ──────────────────────────────
     ui.registerScreen(SCR_INFO,     drawInfo,     inputInfo);
     ui.registerScreen(SCR_SETTINGS, drawSettings, inputSettings);
 
     // ── 注册 Home/Player 的输入（内置屏幕需要输入）
     ui.registerScreen(AXIS_SCR_HOME,   nullptr, inputHome);
     ui.registerScreen(AXIS_SCR_PLAYER, nullptr, inputPlayer);
 
     // ── 注入初始数据 ──────────────────────────────
     ui.setTrackTitle("Blinding Lights");
     ui.setTrackArtist("The Weeknd");
     ui.setVolume(sim.volume);
     ui.setProgress(0.0f);
     ui.setBTConnected(false);
     ui.setBattery(sim.battery);
 
     AxisSensorData data;
     data.temp = sim.temp; data.humidity = sim.humidity;
     data.pressure = sim.pressure; data.co2 = sim.co2;
     data.tvoc = sim.tvoc; data.valid = true;
     ui.setSensorData(data);
 
     // ── 设置帧率，进入 Home ───────────────────────
     ui.setFPS(30);
     ui.goTo(AXIS_SCR_HOME);
 
     // 开机动画：橙色渐灭
     for (int b = 80; b >= 0; b -= 4) {
         pixels.setBrightness(b);
         pixels.show();
         delay(20);
     }
 
     Serial.println("[PRISM] Ready");
 }
 
 // ─────────────────────────────────────────────────
 //  loop
 // ─────────────────────────────────────────────────
 void loop() {
     // 框架主循环
     ui.update();
 
     // 模拟数据更新（后续替换为真实传感器）
     updateSimSensors();
     updateSimProgress();
 
     // WS2812 更新
     updateLights();
 
     // MPU6050 旋转检测（后续替换为真实读值）
     // 示例：模拟每10秒旋转一次（实际用 MPU6050 的 getRotation）
     // static unsigned long lastRot = 0;
     // if (millis() - lastRot > 10000) {
     //     lastRot = millis();
     //     static uint8_t rot = 0;
     //     ui.setRotation((AxisRotation)(rot++ % 4));
     // }
 }
 ```
 
 ---
 
 四个文件全部写完：
 ```
 src/axis_types.h    ✅
 src/axis_anim.h     ✅
 src/AXIS_UI.h       ✅
 src/AXIS_UI.cpp     ✅
 examples/BasicMenu/BasicMenu.ino  ✅
 ```
 
 注意 `library.properties` 里依赖要加上 `U8g2_for_Adafruit_GFX` 和 `Adafruit NeoPixel`，更新一下：
 ```
 depends=Adafruit SSD1351 Library,Adafruit SSD1306,Adafruit GFX Library,U8g2_for_Adafruit_GFX,Adafruit NeoPixel