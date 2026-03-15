/**
 * AXIS UI — BasicMenu 示例
 *
 * 演示：
 *   - 外部初始化屏幕后传入框架
 *   - 双屏（SSD1351 主屏 + SSD1306 状态栏）
 *   - 带方向的滑动过渡动画
 *   - drawFn 使用 xOff/yOff 参数实现平滑滑动
 *
 * 接线（YD-ESP32 WROOM-32E）：
 *   SSD1351 : SCK=18  MOSI=23  CS=2  DC=17  RST=15
 *   SSD1306 : SDA=21  SCL=33
 *   摇杆    : UP=19  DOWN=34(外部10k上拉)  LEFT=13  RIGHT=27  OK=4
 *   按键    : A=14  B=15
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

// ── 菜单定义（每项指定过渡方向）─────────────────
const AxisMenuItem mainMenu[] = {
    { "Now Playing", AXIS_C_ACCENT,  AXIS_SCR_NONE, AXIS_TRANS_NONE       },
    { "System Info", AXIS_C_GOLD,    SCR_INFO,       AXIS_TRANS_SLIDE_LEFT },
    { "About",       AXIS_C_PURPLE,  SCR_ABOUT,      AXIS_TRANS_SLIDE_UP   },
};
const uint8_t MENU_COUNT = sizeof(mainMenu) / sizeof(mainMenu[0]);

// ── 应用状态 ─────────────────────────────────────
struct AppState {
    String  title    = "PRISM";
    String  artist   = "node0";
    bool    playing  = false;
    int     volume   = 75;
    float   progress = 0.0f;
    bool    btConn   = false;
    int     battery  = 88;
} S;

// ═════════════════════════════════════════════════
//  工具：带 offset 的公共顶栏
//  所有屏幕都调用这个，保证顶栏参与滑动
// ═════════════════════════════════════════════════
void drawTopBar(Adafruit_GFX& d, int16_t xOff,
                const char* title, uint16_t titleColor) {
    d.fillRect(xOff, 0, d.width(), 18, AXIS_C_TOPBAR);
    d.setTextSize(1);
    d.setTextColor(titleColor);
    // 居中标题
    int16_t tx = xOff + (d.width() - (int16_t)strlen(title) * 6) / 2;
    d.setCursor(tx, 5);
    d.print(title);
}

// ═════════════════════════════════════════════════
//  SCR_MAIN
// ═════════════════════════════════════════════════
void drawMain(Adafruit_GFX& d, int16_t xOff, int16_t yOff, void*) {
    // xOff 由框架传入，过渡时非零，正常显示时为 0
    // 所有 x 坐标加上 xOff，元素就会跟着整体滑动

    // 顶栏
    d.fillRect(xOff, 0, d.width(), 18, AXIS_C_TOPBAR);
    d.fillCircle(xOff + 6, 9, 4, S.btConn ? AXIS_C_ACCENT : AXIS_C_DKGRAY);
    d.setTextSize(1);
    d.setTextColor(AXIS_C_WHITE);
    d.setCursor(xOff + 14, 5);
    d.print(S.btConn ? "BT" : "---");
    d.setTextColor(AXIS_C_GOLD);
    d.setCursor(xOff + 60, 5);
    d.print("PRISM v0.1");

    // 播放图标
    uint16_t stCol = S.playing ? AXIS_C_GREEN : AXIS_C_ORANGE;
    d.fillRoundRect(xOff + 2, 21, 36, 36, 4, AXIS_C_CARD);
    d.drawRoundRect(xOff + 2, 21, 36, 36, 4, stCol);
    if (S.playing) {
        d.fillRect(xOff + 11, 30, 5, 18, stCol);
        d.fillRect(xOff + 22, 30, 5, 18, stCol);
    } else {
        for (int i = 0; i < 10; i++)
            d.drawFastVLine(xOff + 13 + i, 30 + i/2, 18 - i, stCol);
    }

    // 歌曲信息
    d.setTextColor(AXIS_C_WHITE);
    d.setCursor(xOff + 42, 23);
    d.print(AXIS_UI::truncate(S.title, 11));
    d.setTextColor(AXIS_C_GOLD);
    d.setCursor(xOff + 42, 34);
    d.print(AXIS_UI::truncate(S.artist, 11));

    // 分割线
    d.drawFastHLine(xOff, 62, d.width(), AXIS_C_DIVIDER);

    // 进度条
    S.progress += 0.0008f;
    if (S.progress > 1.0f) S.progress = 0.0f;
    ui.drawGradBar(xOff + 4, 66, d.width()-8, 4,
                   S.progress, AXIS_C_ACCENT, AXIS_C_ACCENT2);

    // 时间
    d.setTextColor(AXIS_C_LTGRAY); d.setTextSize(1);
    String pos = AXIS_UI::formatTime((int32_t)(S.progress * 210));
    String dur = AXIS_UI::formatTime(210);
    d.setCursor(xOff + 4, 74); d.print(pos);
    d.setCursor(xOff + d.width() - (int16_t)dur.length()*6, 74);
    d.print(dur);

    // 音量
    d.setTextColor(AXIS_C_GRAY);  d.setCursor(xOff + 4, 84); d.print("VOL");
    d.setTextColor(AXIS_C_WHITE); d.setCursor(xOff + 4, 94);
    d.print(S.volume); d.print("%");
    ui.drawGradBar(xOff + 32, 87, d.width()-36, 5,
                   S.volume / 100.0f, AXIS_C_PURPLE, AXIS_C_ACCENT2);

    // 底部
    d.drawFastHLine(xOff, 115, d.width(), AXIS_C_DIVIDER);
    d.setTextColor(AXIS_C_DKGRAY);
    d.setCursor(xOff + 2,  118); d.print("A:Pause");
    d.setCursor(xOff + 50, 118); d.print("OK:Menu");
    d.setCursor(xOff + 94, 118); d.print("B:Next");
}

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
        default: break;
    }
}

// ═════════════════════════════════════════════════
//  SCR_INFO
// ═════════════════════════════════════════════════
void drawInfo(Adafruit_GFX& d, int16_t xOff, int16_t yOff, void*) {
    d.fillScreen(AXIS_C_BG);
    drawTopBar(d, xOff, "SYSTEM INFO", AXIS_C_GOLD);

    struct Row { const char* label; const char* value; uint16_t col; };
    Row rows[] = {
        { "Board",   "YD-ESP32",   AXIS_C_ACCENT  },
        { "UI",      "AXIS v0.1",  AXIS_C_ACCENT2 },
        { "Phase",   "1 - Audio",  AXIS_C_GREEN   },
        { "NodeLib", "ANCS v1.x",  AXIS_C_GOLD    },
    };
    for (int i = 0; i < 4; i++) {
        int ry = 22 + i * 24;
        d.fillRoundRect(xOff + 2, ry, 124, 20, 3, AXIS_C_CARD);
        d.setTextColor(AXIS_C_GRAY);
        d.setCursor(xOff + 8, ry + 6);
        d.print(rows[i].label);
        d.setTextColor(rows[i].col);
        d.setCursor(xOff + 70, ry + 6);
        d.print(rows[i].value);
    }
    d.setTextColor(AXIS_C_DKGRAY);
    d.setCursor(xOff + 22, 120);
    d.print("A: Back");
}

void inputInfo(AxisInputEvent ev, void*) {
    if (ev == AXIS_INPUT_BTN_A) ui.goBack(AXIS_TRANS_SLIDE_RIGHT);
}

// ═════════════════════════════════════════════════
//  SCR_ABOUT
// ═════════════════════════════════════════════════
void drawAbout(Adafruit_GFX& d, int16_t xOff, int16_t yOff, void*) {
    d.fillScreen(AXIS_C_BG);
    drawTopBar(d, xOff, "ABOUT", AXIS_C_PURPLE);

    d.setTextColor(AXIS_C_WHITE);
    d.setCursor(xOff + 10, 28); d.print("PRISM");
    d.setTextColor(AXIS_C_GRAY);
    d.setCursor(xOff + 10, 42); d.print("Ambient computing");
    d.setCursor(xOff + 10, 54); d.print("device by node0");
    d.setTextColor(AXIS_C_ACCENT);
    d.setCursor(xOff + 10, 70); d.print("AXIS UI Framework");
    d.setTextColor(AXIS_C_DKGRAY);
    d.setCursor(xOff + 10, 84); d.print("github.com/EVGA2048");
    d.setCursor(xOff + 22, 120); d.print("A: Back");
}

void inputAbout(AxisInputEvent ev, void*) {
    if (ev == AXIS_INPUT_BTN_A) ui.goBack(AXIS_TRANS_SLIDE_DOWN);
}

// ═════════════════════════════════════════════════
//  状态栏（SSD1306，无过渡，不用 xOff）
// ═════════════════════════════════════════════════
void drawStatusBar(Adafruit_GFX& d, void*) {
    d.fillScreen(0);
    d.setTextSize(1);
    d.setTextColor(1);

    if (S.btConn) d.fillCircle(4, 4, 3, 1);
    else          d.drawCircle(4, 4, 3, 1);

    d.setCursor(11, 0);
    d.print(S.playing ? ">" : "||");

    d.setCursor(26, 0);
    d.print(AXIS_UI::truncate(S.title + " - " + S.artist, 13));

    // 电量
    d.drawRect(100, 0, 22, 7, 1);
    d.fillRect(122, 2, 2, 3, 1);
    int fill = (int)(S.battery / 100.0f * 20);
    if (fill > 0) d.fillRect(101, 1, fill, 5, 1);

    d.drawFastHLine(0, 9, 128, 1);

    d.setCursor(0, 12);
    d.print(AXIS_UI::formatTime((int32_t)(S.progress * 210)));
    String dur = AXIS_UI::formatTime(210);
    d.setCursor(128 - (int16_t)dur.length() * 6, 12);
    d.print(dur);

    int progW = (int)(S.progress * 128);
    if (progW > 0) d.fillRect(0, 28, progW, 4, 1);
    d.drawRect(0, 28, 128, 4, 1);
}

// ═════════════════════════════════════════════════
//  setup
// ═════════════════════════════════════════════════
void setup() {
    Serial.begin(115200);
    Serial.println("[PRISM] Booting...");

    // 主屏初始化（用户负责）
    SPI.begin(18, -1, 23, PIN_OLED_CS);
    mainDisp.begin();
    mainDisp.fillScreen(AXIS_C_BG);

    // 副屏初始化（用户负责）
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
    bool subOk = statusDisp.begin(SSD1306_SWITCHCAPVCC, 0x3C);
    if (!subOk) Serial.println("[PRISM] WARN: SSD1306 not found");

    // 把屏幕交给框架
    // SSD1351 直写不需要 flush
    ui.setMainDisplay(&mainDisp, nullptr);

    // SSD1306 缓冲型，提供 flush lambda
    if (subOk) {
        ui.setSubDisplay(&statusDisp, []() { statusDisp.display(); });
        ui.setStatusCallback(drawStatusBar);
    }

    // 输入引脚
    AxisPinConfig pins;
    pins.joy_up    = 19;
    pins.joy_down  = 34;
    pins.joy_left  = 13;
    pins.joy_right = 27;
    pins.joy_ok    = 4;
    pins.btn_a     = 14;
    pins.btn_b     = 15;

    if (!ui.begin(pins)) {
        Serial.println("[PRISM] ERROR: AXIS UI init failed.");
        while (true) delay(1000);
    }

    // 注册屏幕
    ui.registerScreen(SCR_MAIN,  drawMain,  inputMain);
    ui.registerScreen(SCR_INFO,  drawInfo,  inputInfo);
    ui.registerScreen(SCR_ABOUT, drawAbout, inputAbout);

    ui.setFPS(30);
    ui.goTo(SCR_MAIN);

    Serial.println("[PRISM] Ready.");
}

// ═════════════════════════════════════════════════
//  loop
// ═════════════════════════════════════════════════
void loop() {
    ui.update();
}
