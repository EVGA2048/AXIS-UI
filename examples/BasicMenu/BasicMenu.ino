/**
 * AXIS UI — BasicMenu 示例
 *
 * 演示：
 *   - 外部初始化屏幕后传入框架（新架构）
 *   - 双屏（SSD1351 主屏 + SSD1306 状态栏）
 *   - 菜单动画、通知、屏幕跳转
 *   - 所有引脚均与 YD-ESP32 WROOM-32E 对应
 *
 * 接线速查：
 *   SSD1351 : SCK=18, MOSI=23, CS=2, DC=17, RST=15
 *   SSD1306 : SDA=21, SCL=33  (注：GPIO22 被 I2S 占用)
 *   摇杆    : UP=19, DOWN=34*, LEFT=13, RIGHT=27, OK=4
 *   按键    : A=14, B=15
 *   * GPIO34 无内部上拉，需外接 10kΩ 上拉到 3.3V
 *
 * 依赖库（Library Manager）：
 *   Adafruit SSD1351 Library
 *   Adafruit SSD1306
 *   Adafruit GFX Library
 */

#include <SPI.h>
#include <Wire.h>
#include <Adafruit_SSD1351.h>
#include <Adafruit_SSD1306.h>
#include <AXIS_UI.h>

// ── 引脚 ─────────────────────────────────────────
#define PIN_OLED_CS   2
#define PIN_OLED_DC   17
#define PIN_OLED_RST  15
#define PIN_I2C_SDA   21
#define PIN_I2C_SCL   33

// ── 屏幕对象（用户负责创建和初始化）───────────────
Adafruit_SSD1351 mainDisp(128, 128, &SPI,
                           PIN_OLED_CS, PIN_OLED_DC, PIN_OLED_RST);
Adafruit_SSD1306 statusDisp(128, 32, &Wire, -1);

// ── AXIS UI 实例 ─────────────────────────────────
AXIS_UI ui;

// ── 屏幕 ID ──────────────────────────────────────
enum ScreenID : uint8_t {
    SCR_MAIN  = 0,
    SCR_INFO  = 1,
    SCR_ABOUT = 2,
};

// ── 菜单定义 ─────────────────────────────────────
const AxisMenuItem mainMenu[] = {
    { "Now Playing", AXIS_C_ACCENT,  AXIS_SCR_NONE },
    { "System Info", AXIS_C_GOLD,    SCR_INFO       },
    { "About",       AXIS_C_PURPLE,  SCR_ABOUT      },
};
const uint8_t MENU_COUNT = sizeof(mainMenu) / sizeof(mainMenu[0]);

// ── 应用状态（模拟数据，Phase 1 接入真实 A2DP）────
struct AppState {
    String  title    = "PRISM";
    String  artist   = "node0";
    bool    playing  = false;
    int     volume   = 75;
    float   progress = 0.0f;
    bool    btConn   = false;
    int     battery  = 88;
} S;

// ─────────────────────────────────────────────────
//  SCR_MAIN — 绘制
// ─────────────────────────────────────────────────
void drawMain(Adafruit_GFX& d, void*) {
    int16_t W = d.width();

    // 顶栏背景
    d.fillRect(0, 0, W, 18, AXIS_C_TOPBAR);

    // BT 状态点
    d.fillCircle(6, 9, 4, S.btConn ? AXIS_C_ACCENT : AXIS_C_DKGRAY);

    // 设备名
    d.setTextSize(1);
    d.setTextColor(AXIS_C_WHITE);
    d.setCursor(14, 5);
    d.print(S.btConn ? "BT" : "---");

    // 右侧标签
    d.setTextColor(AXIS_C_GOLD);
    d.setCursor(60, 5);
    d.print("PRISM v0.1");

    // 播放状态图标（左侧方块）
    uint16_t stCol = S.playing ? AXIS_C_GREEN : AXIS_C_ORANGE;
    d.fillRoundRect(2, 21, 36, 36, 4, AXIS_C_CARD);
    d.drawRoundRect(2, 21, 36, 36, 4, stCol);
    if (S.playing) {
        // 暂停符号
        d.fillRect(11, 30, 5, 18, stCol);
        d.fillRect(22, 30, 5, 18, stCol);
    } else {
        // 播放三角
        for (int i = 0; i < 10; i++) {
            d.drawFastVLine(13 + i, 30 + i/2, 18 - i, stCol);
        }
    }

    // 歌名
    d.setTextColor(AXIS_C_WHITE);
    d.setCursor(42, 23);
    d.print(AXIS_UI::truncate(S.title, 11));

    // 艺术家
    d.setTextColor(AXIS_C_GOLD);
    d.setCursor(42, 34);
    d.print(AXIS_UI::truncate(S.artist, 11));

    // 分割线
    d.drawFastHLine(0, 62, W, AXIS_C_DIVIDER);

    // 进度条
    S.progress += 0.0008f;
    if (S.progress > 1.0f) S.progress = 0.0f;
    ui.drawGradBar(4, 66, W-8, 4, S.progress,
                   AXIS_C_ACCENT, AXIS_C_ACCENT2);

    // 时间
    d.setTextColor(AXIS_C_LTGRAY);
    d.setTextSize(1);
    String pos = AXIS_UI::formatTime((int32_t)(S.progress * 210));
    String dur = AXIS_UI::formatTime(210);
    d.setCursor(4, 74);
    d.print(pos);
    d.setCursor(W - (int16_t)dur.length()*6, 74);
    d.print(dur);

    // 音量条
    d.setTextColor(AXIS_C_GRAY); d.setCursor(4, 84); d.print("VOL");
    d.setTextColor(AXIS_C_WHITE); d.setCursor(4, 94);
    d.print(S.volume); d.print("%");
    ui.drawGradBar(32, 87, W-36, 5, S.volume / 100.0f,
                   AXIS_C_PURPLE, AXIS_C_ACCENT2);

    // 底部提示
    d.drawFastHLine(0, 115, W, AXIS_C_DIVIDER);
    d.setTextColor(AXIS_C_DKGRAY);
    d.setCursor(2, 118);  d.print("A:Pause");
    d.setCursor(50, 118); d.print("OK:Menu");
    d.setCursor(94, 118); d.print("B:Next");
}

// ─────────────────────────────────────────────────
//  SCR_MAIN — 输入
// ─────────────────────────────────────────────────
void inputMain(AxisInputEvent ev, void*) {
    switch (ev) {
        case AXIS_INPUT_BTN_A:
            S.playing = !S.playing;
            ui.notify(S.playing ? "Playing" : "Paused",
                      S.playing ? AXIS_C_GREEN : AXIS_C_ORANGE, 2000);
            break;
        case AXIS_INPUT_UP:
            S.volume = min(100, S.volume + 5);
            break;
        case AXIS_INPUT_DOWN:
            S.volume = max(0, S.volume - 5);
            break;
        case AXIS_INPUT_LEFT:
            S.progress = 0.0f;
            ui.notify("Prev Track", AXIS_C_ACCENT, 1500);
            break;
        case AXIS_INPUT_RIGHT:
            S.progress = 0.0f;
            ui.notify("Next Track", AXIS_C_ACCENT, 1500);
            break;
        case AXIS_INPUT_OK:
            ui.showMenu(mainMenu, MENU_COUNT);
            break;
        default:
            break;
    }
}

// ─────────────────────────────────────────────────
//  SCR_INFO — 绘制
// ─────────────────────────────────────────────────
void drawInfo(Adafruit_GFX& d, void*) {
    d.fillScreen(AXIS_C_BG);
    d.fillRect(0, 0, d.width(), 16, AXIS_C_TOPBAR);
    d.setTextColor(AXIS_C_GOLD);
    d.setTextSize(1);
    d.setCursor(28, 4);
    d.print("SYSTEM INFO");

    struct Row { const char* label; const char* value; uint16_t col; };
    Row rows[] = {
        { "Board",   "YD-ESP32",   AXIS_C_ACCENT  },
        { "UI",      "AXIS v0.1",  AXIS_C_ACCENT2 },
        { "Phase",   "1 - Audio",  AXIS_C_GREEN   },
        { "NodeLib", "ANCS v1.x",  AXIS_C_GOLD    },
    };
    for (int i = 0; i < 4; i++) {
        int ry = 22 + i * 24;
        d.fillRoundRect(2, ry, 124, 20, 3, AXIS_C_CARD);
        d.setTextColor(AXIS_C_GRAY);
        d.setCursor(8, ry + 6);
        d.print(rows[i].label);
        d.setTextColor(rows[i].col);
        d.setCursor(70, ry + 6);
        d.print(rows[i].value);
    }
    d.setTextColor(AXIS_C_DKGRAY);
    d.setCursor(22, 120);
    d.print("A: Back");
}

void inputInfo(AxisInputEvent ev, void*) {
    if (ev == AXIS_INPUT_BTN_A) ui.goBack();
}

// ─────────────────────────────────────────────────
//  SCR_ABOUT — 绘制
// ─────────────────────────────────────────────────
void drawAbout(Adafruit_GFX& d, void*) {
    d.fillScreen(AXIS_C_BG);
    d.fillRect(0, 0, d.width(), 16, AXIS_C_TOPBAR);
    d.setTextColor(AXIS_C_PURPLE);
    d.setTextSize(1);
    d.setCursor(40, 4);
    d.print("ABOUT");

    d.setTextColor(AXIS_C_WHITE);  d.setCursor(10, 28); d.print("PRISM");
    d.setTextColor(AXIS_C_GRAY);   d.setCursor(10, 42); d.print("Ambient computing");
    d.setTextColor(AXIS_C_GRAY);   d.setCursor(10, 54); d.print("device by node0");
    d.setTextColor(AXIS_C_ACCENT); d.setCursor(10, 70); d.print("AXIS UI Framework");
    d.setTextColor(AXIS_C_DKGRAY); d.setCursor(10, 84); d.print("github.com/EVGA2048");
    d.setTextColor(AXIS_C_DKGRAY); d.setCursor(22, 120); d.print("A: Back");
}

void inputAbout(AxisInputEvent ev, void*) {
    if (ev == AXIS_INPUT_BTN_A) ui.goBack();
}

// ─────────────────────────────────────────────────
//  状态栏（SSD1306）
// ─────────────────────────────────────────────────
void drawStatusBar(Adafruit_GFX& d, void*) {
    d.fillScreen(0);  // SSD1306：0 = 黑

    d.setTextSize(1);
    d.setTextColor(1); // SSD1306：非零 = 白

    // BT 状态
    if (S.btConn) d.fillCircle(4, 4, 3, 1);
    else          d.drawCircle(4, 4, 3, 1);

    // 播放状态
    d.setCursor(11, 0);
    d.print(S.playing ? ">" : "||");

    // 标题（截断）
    d.setCursor(26, 0);
    d.print(AXIS_UI::truncate(S.title + " - " + S.artist, 13));

    // 电量（右侧）
    int batX = 100;
    d.drawRect(batX, 0, 22, 7, 1);
    d.fillRect(batX+22, 2, 2, 3, 1);
    int fill = (int)(S.battery / 100.0f * 20);
    if (fill > 0) d.fillRect(batX+1, 1, fill, 5, 1);

    // 分割线
    d.drawFastHLine(0, 9, 128, 1);

    // 时间文字
    d.setCursor(0, 12);
    d.print(AXIS_UI::formatTime((int32_t)(S.progress * 210)));
    String dur = AXIS_UI::formatTime(210);
    d.setCursor(128 - (int16_t)dur.length() * 6, 12);
    d.print(dur);

    // 进度条
    int progW = (int)(S.progress * 128);
    if (progW > 0) d.fillRect(0, 28, progW, 4, 1);
    d.drawRect(0, 28, 128, 4, 1);
}

// ─────────────────────────────────────────────────
//  setup
// ─────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    Serial.println("[PRISM] Booting...");

    // ── 主屏初始化（用户负责）──────────────────────
    SPI.begin(18, -1, 23, PIN_OLED_CS);
    mainDisp.begin();
    mainDisp.fillScreen(0x0000);

    // ── 副屏初始化（用户负责）──────────────────────
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
    bool subOk = statusDisp.begin(SSD1306_SWITCHCAPVCC, 0x3C);
    if (!subOk) Serial.println("[PRISM] WARN: SSD1306 not found");

    // ── 把屏幕交给 AXIS UI ─────────────────────────
    // SSD1351 直写，不需要 flush 函数
    ui.setMainDisplay(&mainDisp, nullptr);

    // SSD1306 是缓冲型，必须提供 flush 函数
    if (subOk) {
        ui.setSubDisplay(&statusDisp, []() { statusDisp.display(); });
        ui.setStatusCallback(drawStatusBar);
    }

    // ── 输入引脚配置 ───────────────────────────────
    AxisPinConfig pins;
    pins.joy_up    = 19;
    pins.joy_down  = 34;  // 外接 10kΩ 上拉
    pins.joy_left  = 13;
    pins.joy_right = 27;
    pins.joy_ok    = 4;
    pins.btn_a     = 14;
    pins.btn_b     = 15;

    if (!ui.begin(pins)) {
        Serial.println("[PRISM] ERROR: AXIS UI init failed.");
        while (true) delay(1000);
    }

    // ── 注册屏幕 ───────────────────────────────────
    ui.registerScreen(SCR_MAIN,  drawMain,  inputMain);
    ui.registerScreen(SCR_INFO,  drawInfo,  inputInfo);
    ui.registerScreen(SCR_ABOUT, drawAbout, inputAbout);

    // ── 帧率与启动 ─────────────────────────────────
    ui.setFPS(30);
    ui.goTo(SCR_MAIN);

    Serial.println("[PRISM] Ready.");
}

// ─────────────────────────────────────────────────
//  loop
// ─────────────────────────────────────────────────
void loop() {
    ui.update();
    // 其他任务（A2DP、BLE、传感器）也在这里调用
}
