/*
 * AXIS UI — BasicMenu 示例
 * 测试双屏点亮 + 菜单动画 + 按键输入
 * 
 * 接线（YD-ESP32 WROOM-32E）：
 *   SSD1351: SCK=18, MOSI=23, CS=2, DC=17, RST=15
 *   SSD1306: SDA=21, SCL=33
 *   摇杆: UP=19, DOWN=34(外部上拉), LEFT=13, RIGHT=27, OK=4
 *   按键: A=14, B=15
 */

#include <AXIS_UI.h>

AXIS_UI ui;

// ── 屏幕 ID ──
#define SCR_MAIN    0
#define SCR_INFO    1
#define SCR_ABOUT   2

// ── 菜单定义 ──
const AxisMenuItem mainMenu[] = {
    { "Now Playing", AXIS_C_ACCENT,  AXIS_SCR_NONE },
    { "Info",        AXIS_C_GOLD,    SCR_INFO       },
    { "About",       AXIS_C_PURPLE,  SCR_ABOUT      },
};

// ── 状态数据（模拟，待接入实际功能）──
struct AppState {
    String  title   = "PRISM";
    String  artist  = "node0";
    bool    playing = false;
    int     volume  = 75;
    float   progress = 0.0f;
    bool    btConn  = false;
} app;

// ─────────────────────────────────────────
//  主屏 — 绘制回调
// ─────────────────────────────────────────
void drawMain(Adafruit_SSD1351& d, void*) {
    // 顶栏
    d.fillRect(0, 0, 128, 18, AXIS_C_TOPBAR);
    d.fillCircle(6, 9, 4, app.btConn ? AXIS_C_ACCENT : AXIS_C_DKGRAY);
    d.setTextSize(1);
    d.setTextColor(AXIS_C_WHITE);
    d.setCursor(14, 5);
    d.print(app.btConn ? "BT" : "---");
    d.setCursor(70, 5);
    d.setTextColor(AXIS_C_GOLD);
    d.print("PRISM v0.1");

    // 播放状态图标
    uint16_t stCol = app.playing ? AXIS_C_GREEN : AXIS_C_ORANGE;
    d.fillRoundRect(2, 21, 36, 36, 4, AXIS_C_CARD);
    d.drawRoundRect(2, 21, 36, 36, 4, stCol);
    if (app.playing) {
        d.fillRect(11, 30, 5, 18, stCol);
        d.fillRect(22, 30, 5, 18, stCol);
    } else {
        for (int i = 0; i < 10; i++)
            d.drawFastVLine(13+i, 30+i/2, 18-i, stCol);
    }

    // 歌曲信息
    d.setTextColor(AXIS_C_WHITE); d.setTextSize(1);
    d.setCursor(42, 23);
    d.print(AXIS_UI::truncate(app.title, 11));
    d.setTextColor(AXIS_C_GOLD);
    d.setCursor(42, 34);
    d.print(AXIS_UI::truncate(app.artist, 11));

    // 进度条（模拟增长）
    app.progress += 0.001f;
    if (app.progress > 1.0f) app.progress = 0.0f;
    d.drawFastHLine(0, 62, 128, AXIS_C_DIVIDER);
    ui.drawGradBar(4, 66, 120, 4, app.progress, AXIS_C_ACCENT, AXIS_C_ACCENT2);

    String pos = AXIS_UI::formatTime((int)(app.progress * 180));
    String dur = AXIS_UI::formatTime(180);
    d.setTextColor(AXIS_C_LTGRAY);
    d.setCursor(4, 74);   d.print(pos);
    d.setCursor(128 - dur.length()*6, 74); d.print(dur);

    // 音量
    d.setTextColor(AXIS_C_GRAY); d.setCursor(4, 85); d.print("VOL");
    d.setTextColor(AXIS_C_WHITE); d.setCursor(4, 95);
    d.print(app.volume); d.print("%");
    ui.drawGradBar(30, 88, 90, 5, app.volume/100.0f, AXIS_C_PURPLE, AXIS_C_ACCENT2);

    // 底部提示
    d.drawFastHLine(0, 115, 128, AXIS_C_DIVIDER);
    d.setTextColor(AXIS_C_DKGRAY); d.setTextSize(1);
    d.setCursor(2, 118);  d.print("A:Pause");
    d.setCursor(50, 118); d.print("OK:Menu");
    d.setCursor(95, 118); d.print("B:Next");
}

// ─────────────────────────────────────────
//  主屏 — 输入回调
// ─────────────────────────────────────────
void inputMain(AxisInputEvent ev, void*) {
    switch (ev) {
        case AXIS_INPUT_BTN_A:
            app.playing = !app.playing;
            ui.showNotification(app.playing ? "Playing" : "Paused",
                                app.playing ? AXIS_C_GREEN : AXIS_C_ORANGE);
            break;
        case AXIS_INPUT_UP:
            app.volume = min(100, app.volume + 5);
            break;
        case AXIS_INPUT_DOWN:
            app.volume = max(0, app.volume - 5);
            break;
        case AXIS_INPUT_OK:
            ui.showMenu(mainMenu, 3);
            break;
        default: break;
    }
}

// ─────────────────────────────────────────
//  Info 屏
// ─────────────────────────────────────────
void drawInfo(Adafruit_SSD1351& d, void*) {
    d.fillScreen(AXIS_C_BG);
    d.fillRect(0, 0, 128, 16, AXIS_C_TOPBAR);
    d.setTextColor(AXIS_C_GOLD); d.setTextSize(1);
    d.setCursor(30, 4); d.print("SYSTEM INFO");

    const char* labels[] = {"Board","UI","NodeLib","Phase"};
    const char* values[] = {"YD-ESP32","AXIS v0.1","ANCS v1.x","1 - Audio"};
    uint16_t    cols[]   = {AXIS_C_ACCENT, AXIS_C_ACCENT2, AXIS_C_GOLD, AXIS_C_GREEN};

    for (int i = 0; i < 4; i++) {
        int ry = 22 + i * 24;
        d.fillRoundRect(2, ry, 124, 20, 3, AXIS_C_CARD);
        d.setTextColor(AXIS_C_GRAY); d.setCursor(8, ry+6);
        d.print(labels[i]);
        d.setTextColor(cols[i]);    d.setCursor(70, ry+6);
        d.print(values[i]);
    }
    d.setTextColor(AXIS_C_DKGRAY); d.setCursor(20, 120);
    d.print("A: Back");
}

void inputInfo(AxisInputEvent ev, void*) {
    if (ev == AXIS_INPUT_BTN_A) ui.goBack();
}

// ─────────────────────────────────────────
//  About 屏
// ─────────────────────────────────────────
void drawAbout(Adafruit_SSD1351& d, void*) {
    d.fillScreen(AXIS_C_BG);
    d.fillRect(0, 0, 128, 16, AXIS_C_TOPBAR);
    d.setTextColor(AXIS_C_PURPLE); d.setTextSize(1);
    d.setCursor(38, 4); d.print("ABOUT");

    d.setTextColor(AXIS_C_WHITE); d.setTextSize(1);
    d.setCursor(10, 28); d.print("PRISM");
    d.setTextColor(AXIS_C_GRAY);
    d.setCursor(10, 42); d.print("Ambient computing");
    d.setCursor(10, 54); d.print("device by node0");
    d.setTextColor(AXIS_C_ACCENT);
    d.setCursor(10, 70); d.print("AXIS UI Framework");
    d.setTextColor(AXIS_C_DKGRAY);
    d.setCursor(10, 84); d.print("github.com/EVGA2048");
    d.setCursor(20, 120); d.print("A: Back");
}

void inputAbout(AxisInputEvent ev, void*) {
    if (ev == AXIS_INPUT_BTN_A) ui.goBack();
}

// ─────────────────────────────────────────
//  状态栏回调（SSD1306）
// ─────────────────────────────────────────
void drawStatusBar(Adafruit_SSD1306& bar, void*) {
    bar.setTextSize(1);
    bar.setTextColor(SSD1306_WHITE);

    // BT 状态
    if (app.btConn) bar.fillCircle(4, 4, 3, SSD1306_WHITE);
    else            bar.drawCircle(4, 4, 3, SSD1306_WHITE);

    // 播放状态
    bar.setCursor(12, 0);
    bar.print(app.playing ? ">" : "||");

    // 标题滚动（简化：固定显示）
    bar.setCursor(28, 0);
    bar.print(AXIS_UI::truncate(app.title + " - " + app.artist, 14));

    // 分割线
    bar.drawFastHLine(0, 9, 128, SSD1306_WHITE);

    // 进度条
    int progW = (int)(app.progress * 128);
    if (progW > 0) bar.fillRect(0, 28, progW, 4, SSD1306_WHITE);
    bar.drawRect(0, 28, 128, 4, SSD1306_WHITE);

    // 时间
    bar.setCursor(0, 12);
    bar.print(AXIS_UI::formatTime((int)(app.progress * 180)));
    String dur = AXIS_UI::formatTime(180);
    bar.setCursor(128 - dur.length()*6, 12);
    bar.print(dur);
}

// ─────────────────────────────────────────
//  setup & loop
// ─────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    Serial.println("[AXIS] Starting...");

    AxisPinConfig pins = {
        .oled_cs      = 2,
        .oled_dc      = 17,
        .oled_rst     = 15,
        .i2c_sda      = 21,
        .i2c_scl      = 33,
        .ssd1306_addr = 0x3C,
        .joy_up       = 19,
        .joy_down     = 34,
        .joy_left     = 13,
        .joy_right    = 27,
        .joy_ok       = 4,
        .btn_a        = 14,
        .btn_b        = 15
    };

    ui.begin(pins);
    ui.setFPS(30);

    // 注册屏幕
    ui.registerScreen(SCR_MAIN,  drawMain,  inputMain);
    ui.registerScreen(SCR_INFO,  drawInfo,  inputInfo);
    ui.registerScreen(SCR_ABOUT, drawAbout, inputAbout);

    // 状态栏
    ui.setStatusBarCallback(drawStatusBar);

    // 启动
    ui.goTo(SCR_MAIN);
    Serial.println("[AXIS] Ready");
}

void loop() {
    ui.update();
}
