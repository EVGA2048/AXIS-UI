/**
 * AXIS UI Framework — BasicMenu v4
 * Target: 微雪 ESP32-P4-Pico + SSD1351 128×128
 *
 * 接线：
 *   SSD1351: SCK=15  MOSI=16  CS=17  DC=18  RST=19
 *   摇杆:    UP=2    DOWN=3   LEFT=4  RIGHT=5  OK=6
 *   BTN_A=14 (保存/确认)    BTN_B=20 (返回)
 *   I2C:     SDA=7   SCL=8
 *   MPU-6050: 地址 0x68（AD0 接 GND）
 *   GY-271:   地址 0x0D (QMC5883L) 或 0x1E (HMC5883L)，自动识别
 *
 * 键位约定：
 *   [B]       = 返回（所有用户屏幕通用，框架自动处理）
 *   [A]       = 保存/确认（仅 Settings 等需要保存的屏幕）
 *   [OK 长按] = 打开菜单（HOME 上）
 */

#include <LovyanGFX.h>
#include <AXIS_UI.h>
#include <Wire.h>
#include <Preferences.h>
#include <math.h>
#include <U8g2lib.h>   // U8g2 字体数据
#include "lang.h"      // 语言配置（EN / ZH）
#include "buzzer.h"    // 无源蜂鸣器提示音（GPIO21）

// Arduino IDE 在最后一个 #include 之后插入函数原型，
// 凡是原型里用到的自定义类型必须在这里提前定义。
struct ScrollState {
    int16_t  offset  = 0;
    uint32_t phaseMs = 0;
    uint8_t  phase   = 0;   // 0=pause_start  1=scrolling  2=pause_end
};

// 走路检测：步伐峰值检测 + 节奏验证
// 原理：步伐产生周期性冲击峰值（1.15g↑），且相邻峰值间隔 250-1200ms（0.8-4Hz 步频）
// 相比 range 方法，能区分节律性步伐与随机晃动
struct WalkDetect {
    float    smoothMag  = 1.0f;    // EMA 平滑后的加速度模长
    bool     wasAbove   = false;   // 磁滞状态（避免在峰值附近重复触发）
    uint32_t stepTimes[8] = {};    // 最近8步的时间戳（环形缓冲）
    uint8_t  stepHead   = 0;
    uint32_t lastStepMs = 0;       // 上次步伐时间戳
    uint32_t lastMs     = 0;       // 上次采样时间戳
};

// 屏保几何体（须在 #include 区尾部，让 IDE 原型生成器能看到类型）
struct SSShape { float x,y,z,rx,ry,vrx,vry,sz; };

// ── U8g2 中文字体包装（供 LovyanGFX setFont() 使用）──
// wqy12/wqy16 = 文泉驿点阵，gb2312b 约6000字覆盖日常使用
static const lgfx::U8g2font font_zh_12(u8g2_font_wqy12_t_gb2312b);
static const lgfx::U8g2font font_zh_16(u8g2_font_wqy16_t_gb2312b);

// ── 字体辅助（根据语言选择，必须在 includes 区尾部定义）──
// 不能写成 inline 函数，因为依赖 ui（全局，后面才定义）；
// 改为宏，在 draw 函数内部展开调用 ui.language()。
#define SET_F1(spr) do { \
    if (ui.language() == AXIS_LANG_ZH) \
        (spr).setFont(&font_zh_12); \
    else \
        (spr).setTextFont(1); \
} while(0)

#define SET_F2(spr) do { \
    if (ui.language() == AXIS_LANG_ZH) \
        (spr).setFont(&font_zh_16); \
    else \
        (spr).setTextFont(2); \
} while(0)

// 恢复 ASCII 字体（用于后续不需要 CJK 的渲染）
#define RESET_F(spr) (spr).setTextFont(1)

// ─────────────────────────────────────────────────
//  LovyanGFX 配置（ESP32-P4-Pico + SSD1351）
// ─────────────────────────────────────────────────
class LGFX : public lgfx::LGFX_Device {
    lgfx::Panel_SSD1351 _panel;
    lgfx::Bus_SPI       _bus;
public:
    LGFX() {
        { auto cfg = _bus.config();
          cfg.spi_host    = SPI2_HOST;
          cfg.spi_mode    = 0;
          cfg.freq_write  = 40000000;   // 实际 20 MHz（P4 APB 2× 偏差）
          cfg.dma_channel = 0;          // 绕开 P4 AXI DMA LINK2 bug
          cfg.pin_sclk=15; cfg.pin_mosi=16; cfg.pin_miso=-1; cfg.pin_dc=18;
          _bus.config(cfg); _panel.setBus(&_bus); }
        { auto cfg = _panel.config();
          cfg.pin_cs=17; cfg.pin_rst=19;
          _panel.config(cfg); }
        setPanel(&_panel);
    }
};

// ─────────────────────────────────────────────────
//  全局实例
// ─────────────────────────────────────────────────
LGFX        tft;
AXIS_UI     ui;
Preferences prefs;

// 双语辅助（须在 ui 全局实例之后）
// L  = 当前语言的字符串表
// DST = drawString 封装，正确处理 UTF-8（print() 逐字节传入会破坏多字节序列）
#define L   (ui.language() == AXIS_LANG_ZH ? L_ZH : L_EN)
#define DST(spr, x, y, str) do { (spr).setTextDatum(0); (spr).drawString((str),(x),(y)); } while(0)

// ─────────────────────────────────────────────────
//  用户屏幕 ID（0x10 起）
// ─────────────────────────────────────────────────
enum UserScreenID : uint8_t {
    SCR_INFO     = 0x10,
    SCR_SETTINGS = 0x11,
    SCR_LEVEL    = 0x12,
    SCR_COMPASS  = 0x13,
    SCR_CUBE     = 0x14,
    SCR_SERIAL      = 0x15,   // 串口终端
    SCR_DEBUG       = 0x16,   // 调试模式子菜单
    SCR_SCREENSAVER = 0x17,   // 屏保
};

// ─────────────────────────────────────────────────
//  菜单定义（非 const，由 rebuildMenu() 在语言切换后重建）
// ─────────────────────────────────────────────────
static AxisMenuItem mainMenu[4];
const uint8_t MENU_COUNT = 4;

static void rebuildMenu() {
    mainMenu[0] = { L.menu_playing,  AXIS_C_ACCENT,  AXIS_SCR_PLAYER, AXIS_TRANS_SLIDE_LEFT  };
    mainMenu[1] = { L.menu_sysinfo,  AXIS_C_ACCENT2, SCR_INFO,        AXIS_TRANS_SLIDE_LEFT  };
    mainMenu[2] = { L.menu_settings, AXIS_C_MUTED,   SCR_SETTINGS,    AXIS_TRANS_SLIDE_LEFT  };
    mainMenu[3] = { L.menu_home,     AXIS_C_DIM,     AXIS_SCR_HOME,   AXIS_TRANS_SLIDE_RIGHT };
}

// Neural/Geek 非对称布局（递归定位）
//
//   [COMP](18,25)         [CUBE](64,17)
//      \NW                   |N
//   [LEVEL](38,39) ─── HOME(64,39)
//        W                      \SE
//                            [SERIAL](84,53)
//                                  \NE→
//                              [PLY](104,39)
//
// 左臂 L 形：COMP─LEVEL─HOME（水平+斜上），线方向两种
// 右臂 Z 形：HOME─SERIAL─PLY（斜下右+折回斜上右），线方向两种
// CUBE 独立垂直在上，整体完全非对称
//
// 导航：LEFT→LEVEL→COMP；UP→CUBE；RIGHT→SERIAL→PLY；B→回 HOME 光标
// 索引：0=LEVEL  1=COMP  2=CUBE  3=SERIAL  4=PLY
static void rebuildNodes() {
    ui.clearHomeNodes();
    //                label           color           target           trans                   desc             pos  tag      parent
    ui.registerHomeNode(L.node_level,   AXIS_C_OK,      SCR_LEVEL,       AXIS_TRANS_SLIDE_LEFT, L.desc_level,    6, "LEVEL", -1); // W  from HOME  → (38,39)
    ui.registerHomeNode(L.node_compass, AXIS_C_BLUE,    SCR_COMPASS,     AXIS_TRANS_SLIDE_LEFT, L.desc_compass,  7, "COMP",   0); // NW from LEVEL → (18,25)
    ui.registerHomeNode(L.node_cube,    AXIS_C_ACCENT2, SCR_CUBE,        AXIS_TRANS_SLIDE_LEFT, L.desc_cube,     0, "CUBE",  -1); // N  from HOME  → (64,17)
    ui.registerHomeNode("SERIAL",       AXIS_C_MUTED,   SCR_SERIAL,      AXIS_TRANS_SLIDE_LEFT, "Serial Monitor",3, "SER",  -1); // SE from HOME  → (84,53)
    const char* plyLabel = (ui.language() == AXIS_LANG_ZH) ? "播放器" : "PLAYER";
    const char* plyDesc  = (ui.language() == AXIS_LANG_ZH) ? "音乐播放器" : "Music Player";
    ui.registerHomeNode(plyLabel,       AXIS_C_ACCENT,  AXIS_SCR_PLAYER, AXIS_TRANS_SLIDE_UP,   plyDesc,         1, "PLY",   3); // NE from SERIAL→ (104,39)
}

// ─────────────────────────────────────────────────
//  全局状态
// ─────────────────────────────────────────────────
struct State {
    // 传感器
    float    temp     = 23.4f;
    float    humidity = 61.0f;
    float    pressure = 1013.0f;
    uint16_t co2      = 420;
    float    tvoc     = 0.1f;
    // 设备
    uint8_t  battery  = 88;
    bool     btConn   = false;
    String   timeStr  = "21:46";
    String   dateStr  = "MON 16 MAR";
    // 播放器
    uint8_t  volume   = 75;
    float    progress = 0.0f;
    bool     playing  = false;
    // IMU（MPU-6050）→ 真实数据 or 模拟（EMA 滤波后）
    float    roll     = 0.0f;
    float    pitch    = 0.0f;
    // 磁场计（GY-271）→ 真实数据 or 模拟
    float    heading  = 0.0f;
    // 设置
    uint8_t  langChoice  = 0;    // 0=EN  1=ZH
    bool     gyroEn      = false; // 重力光标开关
    bool     themeLight  = false; // false=深色  true=亮色
    // 调试
    bool     debugSerial = false;
    bool     debugFPS    = false;
    uint32_t gyroLastMs  = 0;
    bool     isWalking   = false;
    // 屏保
    uint8_t  ssTimeoutOpt  = 2;     // 0=OFF 1=30s 2=1min 3=5min
    bool     ssActive      = false;
    uint32_t lastActivityMs = 0;
} g;

// ── 主题颜色（用户屏幕用，框架 HOME/PLAYER 保持深色）──
struct Theme {
    uint16_t bg, card, divider, text, muted, dim, accent;
};
// 深色主题（默认，与框架一致）
static const Theme TH_DARK  = { 0x0000, 0x1082, 0x2945, 0xDEDB, 0x7BCF, 0x39E7, AXIS_C_ACCENT };
// 亮色主题
static const Theme TH_LIGHT = { 0xFFFF, 0xEF7B, 0xC618, 0x2104, 0x6B4D, 0xA534, 0xC880 };
// 当前主题指针（g.themeLight 改变时更新）
static const Theme* TH = &TH_DARK;

static void applyTheme() { TH = g.themeLight ? &TH_LIGHT : &TH_DARK; }

// 动态消息栏内容（语言切换/时间变化时刷新）
// slot 0 = 传感器（框架自动填），1 = 问候，2 = 通知，3 = 天气占位
static void refreshTickerContent() {
    bool zh = (ui.language() == AXIS_LANG_ZH);

    // slot 1: 时段问候
    uint8_t h = (uint8_t)atoi(g.timeStr.c_str());   // "HH:MM" → 小时
    const char* greet;
    if      (h >= 5  && h < 11) greet = zh ? "早上好"       : "GOOD MORNING";
    else if (h >= 11 && h < 14) greet = zh ? "中午好"       : "GOOD NOON";
    else if (h >= 14 && h < 18) greet = zh ? "下午好"       : "GOOD AFTERNOON";
    else if (h >= 18 && h < 23) greet = zh ? "晚上好"       : "GOOD EVENING";
    else                         greet = zh ? "怎么还不睡觉" : "GO TO SLEEP!";
    ui.setTickerItem(1, greet, AXIS_C_ACCENT);

    // slot 2: 通知状态
    ui.setTickerItem(2,
        zh ? "没有新通知" : "NO NEW MESSAGES",
        AXIS_C_MUTED);

    // slot 3: 天气占位（应用可随时用 setTickerItem(3,...) 覆盖）
    ui.setTickerItem(3,
        zh ? "天气  -- °C" : "WEATHER  -- °C",
        AXIS_C_ACCENT2);
}

static WalkDetect wdet;

// ─────────────────────────────────────────────────
//  I2C 传感器驱动（直接寄存器，无外部库依赖）
// ─────────────────────────────────────────────────
#define MPU_ADDR  0x68
#define QMC_ADDR  0x0D   // QMC5883L
#define HMC_ADDR  0x1E   // HMC5883L

bool mpuOk = false, gy271Ok = false, isQMC = false;

void sensorBegin() {
    Wire.begin(7, 8);   // SDA=GPIO7, SCL=GPIO8

    // MPU-6050 唤醒（清除 sleep bit）
    Wire.beginTransmission(MPU_ADDR);
    Wire.write(0x6B); Wire.write(0x00);
    mpuOk = (Wire.endTransmission() == 0);
    Serial.println(mpuOk ? "MPU-6050 OK" : "MPU-6050 not found (sim mode)");

    // 先探测 QMC5883L
    Wire.beginTransmission(QMC_ADDR);
    if (Wire.endTransmission() == 0) {
        isQMC = true; gy271Ok = true;
        // Mode: Continuous, ODR=200Hz, RNG=2G, OSR=512
        Wire.beginTransmission(QMC_ADDR);
        Wire.write(0x09); Wire.write(0x1D); Wire.endTransmission();
        Wire.beginTransmission(QMC_ADDR);
        Wire.write(0x0B); Wire.write(0x01); Wire.endTransmission();
        Serial.println("QMC5883L OK");
    } else {
        // 再探测 HMC5883L
        Wire.beginTransmission(HMC_ADDR);
        if (Wire.endTransmission() == 0) {
            isQMC = false; gy271Ok = true;
            Wire.beginTransmission(HMC_ADDR);
            Wire.write(0x02); Wire.write(0x00); Wire.endTransmission();
            Serial.println("HMC5883L OK");
        } else {
            Serial.println("GY-271 not found (sim mode)");
        }
    }
}

void readMPU6050() {
    if (!mpuOk) {
        // 模拟运动：roll 在 -90° ~ +90° 慢速摆动（覆盖竖立模式触发范围）
        static float vr=0.4f, vp=0.3f;
        g.roll  += vr;  g.pitch += vp;
        if (g.roll  >  85.0f || g.roll  < -85.0f) vr = -vr;
        if (g.pitch >  20.0f || g.pitch < -20.0f) vp = -vp;
        return;
    }
    Wire.beginTransmission(MPU_ADDR);
    Wire.write(0x3B); Wire.endTransmission(false);
    Wire.requestFrom(MPU_ADDR, 6, true);
    int16_t ax = (Wire.read()<<8)|Wire.read();
    int16_t ay = (Wire.read()<<8)|Wire.read();
    int16_t az = (Wire.read()<<8)|Wire.read();
    float axf=ax/16384.0f, ayf=ay/16384.0f, azf=az/16384.0f;

    // 原始角度（左右取反修正）
    float rawRoll  = -atan2f( ayf, azf)                     * 57.2958f;
    float rawPitch =  atan2f(-axf, sqrtf(ayf*ayf+azf*azf)) * 57.2958f;

    // EMA 低通滤波（alpha=0.15，减少静止噪声）
    // alpha 越小越平滑，但响应越慢；0.15 是噪声与响应速度的平衡点
    const float alpha = 0.15f;
    g.roll  = alpha * rawRoll  + (1.0f - alpha) * g.roll;
    g.pitch = alpha * rawPitch + (1.0f - alpha) * g.pitch;

    // ── 走路检测 ───────────────────────────────────────
    // 步伐峰值检测：带磁滞的上升沿检测 + 步频合法性验证
    // HI/LO 磁滞防止在峰值附近因噪声重复触发
    // 步频范围 250-1200ms（约0.8-4Hz），窗口4秒内≥4步才认为在走路
    static const float    WD_HI    = 1.18f;  // 步伐冲击峰值进入阈值（g）
    static const float    WD_LO    = 1.05f;  // 磁滞退出阈值（g）
    static const uint32_t WD_MIN   = 250;    // 步伐最小间隔 ms（≤4步/秒）
    static const uint32_t WD_MAX   = 1200;   // 步伐最大间隔 ms（≥0.8步/秒）
    static const uint32_t WD_WIN   = 4000;   // 步伐计数窗口 ms
    static const uint8_t  WD_STEPS = 4;      // 窗口内最少有效步数

    uint32_t _now = millis();
    if (_now - wdet.lastMs >= 25) {  // 40Hz 采样，足够捕捉步伐峰值
        wdet.lastMs = _now;
        float rawMag = sqrtf(axf*axf + ayf*ayf + azf*azf);
        // EMA 平滑（alpha=0.35），滤除高频毛刺但保留步伐冲击形状
        wdet.smoothMag = 0.35f * rawMag + 0.65f * wdet.smoothMag;

        // 磁滞峰值检测：上升沿越过 WD_HI 才触发，下降到 WD_LO 才复位
        float thr = wdet.wasAbove ? WD_LO : WD_HI;
        bool  above = (wdet.smoothMag > thr);
        if (above && !wdet.wasAbove) {
            // 上升沿：检测到一次冲击峰值
            uint32_t dt = _now - wdet.lastStepMs;
            if (wdet.lastStepMs == 0 || (dt >= WD_MIN && dt <= WD_MAX)) {
                // 步频合法，记录时间戳
                wdet.stepTimes[wdet.stepHead] = _now;
                wdet.stepHead = (wdet.stepHead + 1) % 8;
                wdet.lastStepMs = _now;
            }
        }
        wdet.wasAbove = above;
    }

    // 统计4秒窗口内的有效步数
    {
        uint32_t now2 = millis();
        uint8_t cnt = 0;
        for (uint8_t i = 0; i < 8; i++) {
            if (wdet.stepTimes[i] > 0 && now2 - wdet.stepTimes[i] < WD_WIN)
                cnt++;
        }
        g.isWalking = (cnt >= WD_STEPS);
    }
}

void readGY271() {
    if (!gy271Ok) {
        g.heading += 0.8f;
        if (g.heading >= 360.0f) g.heading -= 360.0f;
        return;
    }
    int16_t mx, my, mz;
    if (isQMC) {
        // QMC5883L: reg 0x00 = XL XH YL YH ZL ZH
        Wire.beginTransmission(QMC_ADDR); Wire.write(0x00); Wire.endTransmission(false);
        Wire.requestFrom(QMC_ADDR, 6, true);
        mx = Wire.read()|(Wire.read()<<8);
        my = Wire.read()|(Wire.read()<<8);
        mz = Wire.read()|(Wire.read()<<8);
    } else {
        // HMC5883L: reg 0x03 = XH XL ZH ZL YH YL
        Wire.beginTransmission(HMC_ADDR); Wire.write(0x03); Wire.endTransmission(false);
        Wire.requestFrom(HMC_ADDR, 6, true);
        mx = (Wire.read()<<8)|Wire.read();
        mz = (Wire.read()<<8)|Wire.read();
        my = (Wire.read()<<8)|Wire.read();
    }
    float h = atan2f(-(float)my, (float)mx) * 57.2958f;
    if (h < 0) h += 360.0f;
    g.heading = h;
}

// ─────────────────────────────────────────────────
//  3D 线框立方体
// ─────────────────────────────────────────────────
static const float CV[8][3] = {
    {-1,-1,-1},{1,-1,-1},{1,1,-1},{-1,1,-1},
    {-1,-1, 1},{1,-1, 1},{1,1, 1},{-1,1, 1}
};
static const uint8_t CE[12][2] = {
    {0,1},{1,2},{2,3},{3,0}, {4,5},{5,6},{6,7},{7,4},
    {0,4},{1,5},{2,6},{3,7}
};

static void drawWireCube(LGFX_Sprite& spr,
                          int16_t cx, int16_t cy,
                          float rx, float ry,
                          float scale, uint16_t col) {
    int16_t px[8], py[8];
    for (int i=0; i<8; i++) {
        float x=CV[i][0], y=CV[i][1], z=CV[i][2];
        // Y 轴旋转
        float tx=x*cosf(ry)+z*sinf(ry), tz=-x*sinf(ry)+z*cosf(ry);
        x=tx; z=tz;
        // X 轴旋转
        float ty=y*cosf(rx)-z*sinf(rx), tz2=y*sinf(rx)+z*cosf(rx);
        y=ty; z=tz2;
        // 透视投影
        float s = scale * 3.5f / (3.5f + z);
        px[i]=(int16_t)(cx + x*s);
        py[i]=(int16_t)(cy + y*s);
    }
    for (int e=0; e<12; e++)
        spr.drawLine(px[CE[e][0]],py[CE[e][0]],px[CE[e][1]],py[CE[e][1]],col);
}


// ─────────────────────────────────────────────────
//  SCR_SERIAL — 串口终端
//  loop() 里读 Serial 追加到环形缓冲；UP/DOWN 键滚动历史
// ─────────────────────────────────────────────────
#define SER_LINES  16   // 最多保留行数
#define SER_WIDTH  22   // 每行最多字符（6px 字体，128/6≈21，留1空白）

struct SerLine {
    char    buf[SER_WIDTH + 1];
    bool    sent;    // true=发送  false=接收
};
static SerLine  serBuf[SER_LINES];
static uint8_t  serHead   = 0;   // 环形头（最新行写入位置）
static uint8_t  serCount  = 0;   // 已存行数（最多 SER_LINES）
static int8_t   serScroll = 0;   // 向上翻页偏移（0=显示最新）
static String   serInBuf  = "";  // 串口输入累积缓冲

// 追加一行到环形缓冲（供 loop() 调用）
static void serialPushLine(const char* text, bool sent) {
    SerLine& sl = serBuf[serHead];
    strncpy(sl.buf, text, SER_WIDTH);
    sl.buf[SER_WIDTH] = '\0';
    sl.sent = sent;
    serHead = (serHead + 1) % SER_LINES;
    if (serCount < SER_LINES) serCount++;
    serScroll = 0;              // 收到新消息自动跳到最新
    ui.triggerHomeCubePulse();  // HOME 立方体脉冲提示
}

// 屏幕可显示行数（标题2行 + 分割1 + 底部back2 = 剩余约9行 × 12px）
#define SER_VISIBLE 9

void drawSerial(LGFX_Sprite& spr, int16_t xOff, int16_t yOff, void*) {
    spr.fillScreen(TH->bg);
    int16_t W = spr.width();

    // 标题栏
    uint16_t barC = TH->accent;
    spr.drawFastHLine(xOff, yOff,   W, barC);
    spr.drawFastHLine(xOff, yOff+1, W, barC);
    RESET_F(spr);
    spr.setTextColor(barC, TH->bg);
    spr.setCursor(xOff+2, yOff+4); spr.print(L.title_serial);
    // 波特率提示
    spr.setTextColor(TH->muted, TH->bg);
    spr.setCursor(xOff+50, yOff+4); spr.print("115200");
    spr.drawFastHLine(xOff, yOff+13, W, TH->divider);

    // 消息区：从最新往旧倒序，serScroll 向上偏移
    int16_t lineH = 12;
    int16_t y0    = yOff + 15;
    // 计算从哪条开始显示（最新 = serHead-1，往后是旧的）
    for (int8_t row = 0; row < SER_VISIBLE; row++) {
        // 对应逻辑行（0=最新）
        int8_t logIdx = row + serScroll;
        if (logIdx >= serCount) break;
        // 从环形缓冲取（head-1 是最新）
        int8_t bufIdx = ((int16_t)serHead - 1 - logIdx % SER_LINES + SER_LINES * 2) % SER_LINES;
        const SerLine& sl = serBuf[bufIdx];
        int16_t ty = y0 + (SER_VISIBLE - 1 - row) * lineH;  // 最新在底部

        uint16_t tc = sl.sent ? TH->accent : TH->text;
        spr.setTextColor(tc, TH->bg);
        spr.setCursor(xOff + (sl.sent ? 8 : 2), ty);
        spr.print(sl.sent ? "> " : "< ");
        spr.print(sl.buf);
    }

    // 滚动指示
    if (serScroll > 0) {
        spr.setTextColor(TH->muted, TH->bg);
        spr.setCursor(xOff + W - 14, yOff + 4);
        spr.print("^");
        char sbuf[6]; snprintf(sbuf, sizeof(sbuf), "+%d", serScroll);
        spr.setCursor(xOff + W - 28, yOff + 4); spr.print(sbuf);
    }

    spr.drawFastHLine(xOff, yOff + 125, W, TH->divider);
    spr.setTextColor(TH->dim, TH->bg);
    spr.setCursor(xOff+2, yOff+127-7); spr.print(L.ser_hint);
}

void inputSerial(AxisInputEvent ev, void*) {
    g.lastActivityMs = millis();
    if (ev == AXIS_INPUT_UP) {
        if (serScroll + SER_VISIBLE < serCount) serScroll++;
    } else if (ev == AXIS_INPUT_DOWN) {
        if (serScroll > 0) serScroll--;
    }
}

void drawCube(LGFX_Sprite& spr, int16_t xOff, int16_t yOff, void*) {
    spr.fillScreen(AXIS_C_BG);
    int16_t W = spr.width();

    spr.drawFastHLine(xOff, yOff,   W, AXIS_C_ACCENT2);
    spr.drawFastHLine(xOff, yOff+1, W, AXIS_C_ACCENT2);
    SET_F1(spr);
    spr.setTextColor(AXIS_C_ACCENT2, AXIS_C_BG);
    DST(spr, xOff+2, yOff+4, L.title_cube);

    float t = millis() / 1000.0f;
    drawWireCube(spr, xOff+64, yOff+60, t*0.7f, t*1.1f, 32, AXIS_C_ACCENT2);

    // FPS 粗略显示
    static uint32_t lastT=0; static uint8_t fps=0; static uint8_t cnt=0;
    cnt++;
    if (millis()-lastT >= 1000) { fps=cnt; cnt=0; lastT=millis(); }
    char fb[12]; snprintf(fb,sizeof(fb),"%dfps",fps);
    spr.setTextColor(AXIS_C_DIM, AXIS_C_BG);
    spr.setCursor(xOff+W-28, yOff+4); spr.print(fb);

    spr.drawFastHLine(xOff, yOff+114, W, AXIS_C_DIVIDER);
    spr.setTextColor(AXIS_C_DIM, AXIS_C_BG);
    DST(spr, xOff+2, yOff+119, L.back);
}

// ─────────────────────────────────────────────────
//  SCR_LEVEL — 水平仪（双模式：平放圆形 / 竖立线性）
// ─────────────────────────────────────────────────

// 平放模式：靶心气泡水平仪
static void drawLevelFlat(LGFX_Sprite& spr, int16_t xOff, int16_t yOff) {
    const int16_t W  = spr.width();
    const int16_t cx = xOff + W/2;
    const int16_t cy = yOff + 52;  // 给底部数值留空间
    const int16_t R  = 40;   // 外圆半径
    const int16_t Ri = 10;   // 居中判定圆
    const int16_t Rb = 6;    // 气泡半径

    // 气泡位置（roll/pitch → 偏移，最大 R-Rb）
    float bxf = g.roll  / 40.0f * (R - Rb);
    float byf = g.pitch / 40.0f * (R - Rb);
    float dist = sqrtf(bxf*bxf + byf*byf);
    if (dist > (R - Rb)) { float s=(R-Rb)/dist; bxf*=s; byf*=s; }
    int16_t bx = cx + (int16_t)bxf;
    int16_t by = cy + (int16_t)byf;
    bool isLevel = (dist < Ri);

    // ── 背景十字准星 ──
    uint16_t gridC = 0x18C3;  // 深灰
    spr.drawFastHLine(cx - R + 2, cy, (R-2)*2, gridC);
    spr.drawFastVLine(cx, cy - R + 2, (R-2)*2, gridC);

    // ── 同心圆刻度（三圈）──
    spr.drawCircle(cx, cy, R,      AXIS_C_DIVIDER);
    spr.drawCircle(cx, cy, R*2/3,  0x2945);
    spr.drawCircle(cx, cy, R*1/3,  0x2945);

    // ── 居中圆（目标区域，动态颜色）──
    uint16_t ringC = isLevel ? AXIS_C_OK : 0x39E7;
    spr.drawCircle(cx, cy, Ri,     ringC);
    spr.fillCircle(cx, cy, 2,      ringC);

    // ── 外圈刻度点（每 45° 一个）──
    for (int a = 0; a < 8; a++) {
        float ang = a * 3.14159f / 4.0f;
        int16_t tx = cx + (int16_t)((R-3) * cosf(ang));
        int16_t ty = cy + (int16_t)((R-3) * sinf(ang));
        spr.fillRect(tx-1, ty-1, 3, 3, AXIS_C_MUTED);
    }

    // ── 气泡光晕（居中时绿色，偏移时橙色）──
    uint16_t bc = isLevel ? AXIS_C_OK : AXIS_C_ACCENT;
    if (isLevel) {
        // 绿色脉冲：以 1.5Hz 呼吸
        float pulse = 0.5f + 0.5f * sinf(millis() / 333.0f);
        uint8_t alpha = (uint8_t)(pulse * 60);
        // 用叠加圆圈模拟光晕（大→小，颜色渐深）
        uint16_t halo = spr.color565(0, alpha, 0);
        spr.fillCircle(bx, by, Rb + 4, halo);
        spr.fillCircle(bx, by, Rb + 2, AXIS_C_OK);
    } else {
        // 偏移时：外圈暗橙作光晕
        spr.fillCircle(bx, by, Rb + 3, 0x7220);  // 暗橙
    }
    spr.fillCircle(bx, by, Rb,     bc);
    spr.fillCircle(bx, by, Rb - 3, 0xFFFF);       // 高光小点

    // ── 数值区 ──
    spr.drawFastHLine(xOff, yOff + 95, W, AXIS_C_DIVIDER);
    RESET_F(spr);
    // Roll
    char rbuf[10], pbuf[10];
    snprintf(rbuf, sizeof(rbuf), "%+.1f", g.roll);
    snprintf(pbuf, sizeof(pbuf), "%+.1f", g.pitch);
    spr.setTextColor(isLevel ? AXIS_C_OK : AXIS_C_ACCENT, AXIS_C_BG);
    spr.setCursor(xOff + 4,       yOff + 98); spr.print("R "); spr.print(rbuf);
    spr.setCursor(xOff + W/2 + 4, yOff + 98); spr.print("P "); spr.print(pbuf);
}

// 竖立模式：滚动标尺 + 固定中央指针（标尺随角度滚动，指针固定在屏幕中心）
static void drawLevelVertical(LGFX_Sprite& spr, int16_t xOff, int16_t yOff,
                               float angle) {
    const int16_t W   = spr.width();   // 128
    const int16_t H   = spr.height();  // 128
    const int16_t cx  = xOff + W / 2;
    const int16_t ry  = yOff + 52;    // 标尺中心 y

    bool     isLevel = (fabsf(angle) < 1.5f);
    uint16_t col     = isLevel ? AXIS_C_OK :
                       (fabsf(angle) < 10.0f ? AXIS_C_ACCENT : AXIS_C_WARN);

    // 每度对应像素（5px/°，±13°填满屏幕宽）
    const float PPD = 4.8f;

    // ── 标尺（随 angle 滚动，刻度中心 = cx - angle*PPD） ──
    // 画从 -20° 到 +20° 的刻度，只显示在屏幕范围内的部分
    for (int deg = -20; deg <= 20; deg++) {
        float tx = cx + (deg - angle) * PPD;
        if (tx < xOff + 1 || tx > xOff + W - 1) continue;
        int16_t ix = (int16_t)tx;

        bool isZero  = (deg == 0);
        bool isFive  = (deg % 5 == 0);
        int16_t th   = isZero ? 28 : (isFive ? 18 : 9);
        uint16_t tc  = isZero ? AXIS_C_WHITE : (isFive ? AXIS_C_MUTED : AXIS_C_DIVIDER);

        spr.drawFastVLine(ix, ry - th/2, th, tc);

        // 5° 整数刻度标签（0° 不显示，由中央指针代替）
        if (isFive && !isZero) {
            char lb[5]; snprintf(lb, sizeof(lb), "%d", abs(deg));
            RESET_F(spr);
            spr.setTextFont(1);
            spr.setTextColor(AXIS_C_MUTED, AXIS_C_BG);
            spr.setTextDatum(4);
            spr.drawString(lb, ix, ry + 24);
        }
    }

    // ── 标尺轨道线（贯穿全宽的水平线）──
    spr.drawFastHLine(xOff, ry,     W, 0x2104);
    spr.drawFastHLine(xOff, ry - 1, W, 0x2104);

    // ── 固定中央指针（上下各一个三角形 notch，颜色随状态变化）──
    // 上方 notch（倒三角，尖端指向标尺）
    for (int i = 0; i < 6; i++)
        spr.drawFastHLine(cx - i, ry - 12 - (5-i), i*2+1, col);
    // 下方 notch（正三角）
    for (int i = 0; i < 6; i++)
        spr.drawFastHLine(cx - i, ry + 12 + i, i*2+1, col);
    // 指针中心竖线（连接上下 notch）
    spr.drawFastVLine(cx, ry - 12, 24, col);

    // 居中时绿色光晕
    if (isLevel) {
        float pulse = 0.4f + 0.4f * sinf(millis() / 400.0f);
        uint8_t gv  = (uint8_t)(pulse * 60);
        spr.drawCircle(cx, ry, 8,  spr.color565(0, gv, 0));
        spr.drawCircle(cx, ry, 12, spr.color565(0, gv/2, 0));
    }

    // ── 角度大字（下方）──
    RESET_F(spr);
    spr.setTextDatum(4);
    if (isLevel) {
        spr.setTextFont(2);
        spr.setTextColor(AXIS_C_OK, AXIS_C_BG);
        spr.drawString("LEVEL", cx, yOff + 85);
    } else {
        char abuf[12]; snprintf(abuf, sizeof(abuf), "%+.1f", angle);
        spr.setTextFont(4);
        spr.setTextColor(col, AXIS_C_BG);
        spr.drawString(abuf, cx, yOff + 83);
        spr.setTextFont(1);
        spr.setTextColor(AXIS_C_MUTED, AXIS_C_BG);
        spr.drawString("deg", cx, yOff + 96);
    }
    spr.setTextDatum(0);

    // ── 底部 back hint ──
    spr.drawFastHLine(xOff, yOff + H - 14, W, AXIS_C_DIVIDER);
    spr.setTextFont(1);
    spr.setTextColor(AXIS_C_DIM, AXIS_C_BG);
    DST(spr, xOff + 2, yOff + H - 10, L.back);
}

void drawLevel(LGFX_Sprite& spr, int16_t xOff, int16_t yOff, void*) {
    spr.fillScreen(AXIS_C_BG);
    int16_t W = spr.width();
    int16_t H = spr.height();

    // 判断模式：任一轴超过 35° 则进入竖立模式
    bool vertical = (fabsf(g.pitch) > 35.0f || fabsf(g.roll) > 35.0f);

    if (vertical) {
        // 竖立模式：哪个轴倾斜更大就用哪个
        float angle = (fabsf(g.roll) > fabsf(g.pitch)) ? g.roll : g.pitch;
        drawLevelVertical(spr, xOff, yOff, angle);
    } else {
        // ── 标题栏 ──
        uint16_t barC = AXIS_C_OK;
        spr.drawFastHLine(xOff, yOff,   W, barC);
        spr.drawFastHLine(xOff, yOff+1, W, barC);
        SET_F1(spr); spr.setTextColor(barC, AXIS_C_BG);
        DST(spr, xOff + 2, yOff + 4, L.title_level);

        drawLevelFlat(spr, xOff, yOff + 12);

        spr.drawFastHLine(xOff, yOff + H - 14, W, AXIS_C_DIVIDER);
        spr.setTextColor(AXIS_C_DIM, AXIS_C_BG);
        DST(spr, xOff + 2, yOff + H - 10, L.back);
    }
}

// ─────────────────────────────────────────────────
//  SCR_COMPASS — 指南针
// ─────────────────────────────────────────────────
static const char* headingLabel(float h) {
    if (h<22.5f||h>=337.5f) return "N";
    if (h<67.5f)  return "NE";  if (h<112.5f) return "E";
    if (h<157.5f) return "SE";  if (h<202.5f) return "S";
    if (h<247.5f) return "SW";  if (h<292.5f) return "W";
    return "NW";
}

void drawCompass(LGFX_Sprite& spr, int16_t xOff, int16_t yOff, void*) {
    spr.fillScreen(AXIS_C_BG);
    int16_t W = spr.width();
    spr.drawFastHLine(xOff,yOff,  W,AXIS_C_BLUE);
    spr.drawFastHLine(xOff,yOff+1,W,AXIS_C_BLUE);
    SET_F1(spr); spr.setTextColor(AXIS_C_BLUE,AXIS_C_BG);
    DST(spr, xOff+2, yOff+4, L.title_compass);
    char db[8]; snprintf(db,sizeof(db),"%3.0f",g.heading);
    spr.setTextColor(AXIS_C_TEXT,AXIS_C_BG);
    spr.setCursor(xOff+W-22,yOff+4); spr.print(db);

    const int16_t cx=xOff+64, cy=yOff+59, R=42;
    spr.drawCircle(cx,cy,R,  AXIS_C_DIVIDER);
    spr.drawCircle(cx,cy,R-1,AXIS_C_CARD);
    for (int deg=0; deg<360; deg+=30) {
        float rad=deg*M_PI/180.0f, s=sinf(rad), c=cosf(rad);
        bool card=(deg%90==0); int16_t r0=card?R-8:R-5;
        spr.drawLine(cx+(int16_t)(s*r0),cy-(int16_t)(c*r0),
                     cx+(int16_t)(s*(R-1)),cy-(int16_t)(c*(R-1)),
                     card?AXIS_C_MUTED:AXIS_C_DIVIDER);
    }
    struct { const char* l; float d; uint16_t c; } dirs[]={
        {"N",0,AXIS_C_DANGER},{"E",90,AXIS_C_MUTED},
        {"S",180,AXIS_C_MUTED},{"W",270,AXIS_C_MUTED}
    };
    for (auto& d:dirs) {
        float rad=d.d*M_PI/180.0f;
        RESET_F(spr); spr.setTextColor(d.c,AXIS_C_BG);   // N/E/S/W 用 ASCII 字体
        spr.setCursor(cx+(int16_t)(sinf(rad)*(R-16))-2,
                      cy-(int16_t)(cosf(rad)*(R-16))-3);
        spr.print(d.l);
    }
    float hr=g.heading*M_PI/180.0f, s=sinf(hr), c=cosf(hr);
    spr.drawLine(cx,cy, cx-(int16_t)(s*14), cy+(int16_t)(c*14), AXIS_C_DIM);
    spr.drawLine(cx,cy, cx+(int16_t)(s*(R-12)), cy-(int16_t)(c*(R-12)), AXIS_C_ACCENT);
    spr.drawLine(cx,cy, cx+(int16_t)(s*(R-12))+1, cy-(int16_t)(c*(R-12)), AXIS_C_ACCENT);
    spr.fillCircle(cx,cy,3,AXIS_C_CARD); spr.drawCircle(cx,cy,3,AXIS_C_ACCENT);

    spr.drawFastHLine(xOff,yOff+108,W,AXIS_C_DIVIDER);
    RESET_F(spr);   // HDG 数值用 ASCII 字体
    char buf[24]; snprintf(buf,sizeof(buf),"HDG %.1f  %s",g.heading,headingLabel(g.heading));
    spr.setTextColor(AXIS_C_ACCENT,AXIS_C_BG);
    spr.setCursor(xOff+2,yOff+112); spr.print(buf);
    spr.drawFastHLine(xOff,yOff+120,W,AXIS_C_DIVIDER);
    spr.setTextColor(AXIS_C_DIM,AXIS_C_BG);
    DST(spr, xOff+2, yOff+122, L.back);
}

// ─────────────────────────────────────────────────
//  滚动文字辅助
// ─────────────────────────────────────────────────
static void drawScrollText(LGFX_Sprite& spr,
                           int16_t x, int16_t y, int16_t maxW,
                           const char* str, uint16_t col,
                           ScrollState& s) {
    int16_t tw = (int16_t)spr.textWidth(str);
    if (tw <= maxW) {
        spr.setTextColor(col,AXIS_C_BG);
        spr.setCursor(x,y); spr.print(str);
        s.offset=0; s.phase=0; s.phaseMs=0;
        return;
    }
    int16_t maxOff = tw-maxW+4;
    uint32_t now=millis();
    if (s.phaseMs==0) s.phaseMs=now;
    uint32_t el=now-s.phaseMs;
    switch(s.phase) {
        case 0: if(el>1000){s.phase=1;s.phaseMs=now;s.offset=0;} break;
        case 1: s.offset=(int16_t)(el/28);
                if(s.offset>=maxOff){s.offset=maxOff;s.phase=2;s.phaseMs=now;} break;
        case 2: if(el>1000){s.phase=0;s.phaseMs=now;s.offset=0;} break;
    }
    spr.setClipRect(x,y-1,maxW,10);
    spr.setTextColor(col,AXIS_C_BG);
    spr.setCursor(x-s.offset,y); spr.print(str);
    spr.clearClipRect();
}

// ─────────────────────────────────────────────────
//  SCR_INFO — 系统信息
// ─────────────────────────────────────────────────
void drawInfo(LGFX_Sprite& spr, int16_t xOff, int16_t yOff, void*) {
    spr.fillScreen(AXIS_C_BG);
    int16_t W = spr.width();
    spr.drawFastHLine(xOff,yOff,  W,AXIS_C_ACCENT);
    spr.drawFastHLine(xOff,yOff+1,W,AXIS_C_ACCENT);
    SET_F1(spr); spr.setTextColor(AXIS_C_ACCENT,AXIS_C_BG);
    DST(spr, xOff+2, yOff+4, L.title_info);
    spr.drawFastHLine(xOff,yOff+13,W,AXIS_C_DIVIDER);

    struct Row { const char* label; const char* value; uint16_t col; };
    Row rows[] = {
        { L.info_board,  "ESP32-P4-Pico",   AXIS_C_TEXT   },
        { L.info_ui,     "AXIS-UI v0.2",    AXIS_C_ACCENT },
        { L.info_screen, "SSD1351 128x128", AXIS_C_MUTED  },
        { L.info_lib,    "LovyanGFX",       AXIS_C_MUTED  },
        { L.info_phase,  L.info_phase_val,  AXIS_C_ACCENT2 },
    };
    static ScrollState ss[5];
    for (int i=0; i<5; i++) {
        int16_t ry=yOff+18+i*18;
        spr.drawFastHLine(xOff,ry-1,W,AXIS_C_DIVIDER);
        SET_F1(spr); spr.setTextColor(AXIS_C_DIM,AXIS_C_BG);
        DST(spr, xOff+2, ry+5, rows[i].label);
        RESET_F(spr);   // 值列是 ASCII，不用 CJK 字体
        drawScrollText(spr,xOff+48,ry+5,W-50,rows[i].value,rows[i].col,ss[i]);
    }
    spr.drawFastHLine(xOff,yOff+114,W,AXIS_C_DIVIDER);
    spr.setTextColor(AXIS_C_DIM,AXIS_C_BG);
    DST(spr, xOff+2, yOff+119, L.back);
}

// ─────────────────────────────────────────────────
//  SCR_SCREENSAVER — 飞越几何体屏保
// ─────────────────────────────────────────────────
#define SS_N 8

static SSShape ssShapes[SS_N];

// 根据深度返回颜色：近=橙，远=暗
static uint16_t ssColorAt(float z) {
    if (z > 10.0f) return AXIS_C_DIVIDER;
    if (z >  6.0f) return AXIS_C_DIM;
    if (z >  3.0f) return AXIS_C_MUTED;
    return AXIS_C_ACCENT;
}

static void ssRespawn(SSShape& s, float baseZ) {
    s.z   = baseZ + (float)random(0, 60) / 10.0f;
    s.x   = (float)random(-40, 40) / 10.0f;
    s.y   = (float)random(-40, 40) / 10.0f;
    s.sz  = 0.5f + (float)random(5, 25) / 10.0f;
    s.vrx = (float)random(4, 28) / 1000.0f * (random(0, 2) ? 1.0f : -1.0f);
    s.vry = (float)random(4, 28) / 1000.0f * (random(0, 2) ? 1.0f : -1.0f);
}

static void initScreensaver() {
    for (int i = 0; i < SS_N; i++) {
        ssShapes[i].rx = (float)random(0, 628) / 100.0f;
        ssShapes[i].ry = (float)random(0, 628) / 100.0f;
        // 均匀分布在 z=1.5..19.5，避免开始时一堆东西同时飞来
        ssRespawn(ssShapes[i], 1.5f + (float)i * (18.0f / SS_N));
    }
}

void drawScreensaver(LGFX_Sprite& spr, int16_t xOff, int16_t yOff, void*) {
    spr.fillScreen(0x0000);
    const int16_t CX    = xOff + 64, CY = yOff + 64;
    const float   FOV   = 60.0f;   // 视场比例（世界单位→像素）
    const float   SCALE = 15.0f;   // 形状基础像素尺寸（z=1 时）
    const float   SPEED = 0.055f;  // 每帧推进速度（≈70fps → 3.85 z/s）
    for (int i = 0; i < SS_N; i++) {
        SSShape& s = ssShapes[i];
        s.z  -= SPEED;
        s.rx += s.vrx;
        s.ry += s.vry;
        if (s.z < 0.4f) ssRespawn(s, 18.0f);
        float   inv_z = 1.0f / s.z;
        int16_t sx    = CX + (int16_t)(s.x * FOV * inv_z);
        int16_t sy    = CY + (int16_t)(s.y * FOV * inv_z);
        float   sc    = s.sz * SCALE * inv_z;
        if (sc < 2.0f || sc > 65.0f) continue;
        drawWireCube(spr, sx, sy, s.rx, s.ry, sc, ssColorAt(s.z));
    }
}

void inputScreensaver(AxisInputEvent ev, void*) {
    (void)ev;
    // 任意按键唤醒（拿起唤醒在 loop() 里处理）
    g.ssActive      = false;
    g.lastActivityMs = millis();
    ui.goHome();
}

// ─────────────────────────────────────────────────
//  SCR_SETTINGS — 设置（A键保存，B键放弃）
// ─────────────────────────────────────────────────
static int8_t settingsCursor = 0;  // 0=语言  1=重力光标  2=主题  3=调试模式
void drawSettings(LGFX_Sprite& spr, int16_t xOff, int16_t yOff, void*) {
    spr.fillScreen(AXIS_C_BG);
    int16_t W = spr.width();
    spr.drawFastHLine(xOff,yOff,  W,AXIS_C_ACCENT);
    spr.drawFastHLine(xOff,yOff+1,W,AXIS_C_ACCENT);
    SET_F1(spr); spr.setTextColor(AXIS_C_ACCENT,AXIS_C_BG);
    DST(spr, xOff+2, yOff+4, L.title_settings);
    spr.drawFastHLine(xOff,yOff+13,W,AXIS_C_DIVIDER);

    // 语言行  y=18
    int16_t ry=yOff+18;
    spr.drawFastHLine(xOff,ry-1,W,AXIS_C_DIVIDER);
    spr.setTextColor(AXIS_C_DIM,AXIS_C_BG);
    DST(spr, xOff+2, ry+5, L.set_lang);
    RESET_F(spr);
    spr.setTextColor(g.langChoice==0 ? AXIS_C_ACCENT : AXIS_C_DIM, AXIS_C_BG);
    spr.setCursor(xOff+48,ry+5); spr.print("EN");
    spr.setTextColor(AXIS_C_MUTED,AXIS_C_BG);
    spr.setCursor(xOff+62,ry+5); spr.print("/");
    SET_F1(spr);
    spr.setTextColor(g.langChoice==1 ? AXIS_C_ACCENT : AXIS_C_DIM, AXIS_C_BG);
    DST(spr, xOff+70, ry+5, L.set_lang_zh);

    // 重力光标行  y=36
    int16_t ry2=yOff+36;
    spr.drawFastHLine(xOff,ry2-1,W,AXIS_C_DIVIDER);
    spr.setTextColor(AXIS_C_DIM,AXIS_C_BG);
    DST(spr, xOff+2, ry2+5, L.set_gyro);
    spr.setTextColor(g.gyroEn ? AXIS_C_OK : AXIS_C_MUTED, AXIS_C_BG);
    DST(spr, xOff+80, ry2+5, g.gyroEn ? L.set_on : L.set_off);

    // 主题行  y=54
    int16_t ry3=yOff+54;
    spr.drawFastHLine(xOff,ry3-1,W,AXIS_C_DIVIDER);
    spr.setTextColor(AXIS_C_DIM,AXIS_C_BG);
    RESET_F(spr); spr.setCursor(xOff+2, ry3+5); spr.print("THEME");
    spr.setTextColor(!g.themeLight ? AXIS_C_ACCENT : AXIS_C_DIM, AXIS_C_BG);
    spr.setCursor(xOff+52, ry3+5); spr.print("DARK");
    spr.setTextColor(AXIS_C_MUTED, AXIS_C_BG);
    spr.setCursor(xOff+76, ry3+5); spr.print("/");
    spr.setTextColor(g.themeLight ? AXIS_C_ACCENT2 : AXIS_C_DIM, AXIS_C_BG);
    spr.setCursor(xOff+84, ry3+5); spr.print("LIGHT");

    // 调试模式行  y=72（进入子菜单）
    int16_t ry4=yOff+72;
    spr.drawFastHLine(xOff,ry4-1,W,AXIS_C_DIVIDER);
    spr.setTextColor(AXIS_C_DIM,AXIS_C_BG);
    RESET_F(spr); spr.setCursor(xOff+2, ry4+5); spr.print("DEBUG");
    spr.setTextColor((g.debugSerial||g.debugFPS) ? AXIS_C_WARN : AXIS_C_MUTED, AXIS_C_BG);
    spr.setCursor(xOff+55, ry4+5);
    spr.print((g.debugSerial||g.debugFPS) ? "ON  >" : "OFF >");

    // 屏保超时行  y=90
    int16_t ry5=yOff+90;
    spr.drawFastHLine(xOff,ry5-1,W,AXIS_C_DIVIDER);
    spr.setTextColor(AXIS_C_DIM,AXIS_C_BG);
    spr.setCursor(xOff+2, ry5+5); spr.print("SCREENSV");
    static const char* ssLabels[] = {"OFF","30s","1m","5m"};
    spr.setTextColor(g.ssTimeoutOpt ? AXIS_C_ACCENT : AXIS_C_MUTED, AXIS_C_BG);
    spr.setCursor(xOff+75, ry5+5);
    spr.print(ssLabels[constrain(g.ssTimeoutOpt,0,3)]);

    spr.drawFastHLine(xOff,yOff+120,W,AXIS_C_DIVIDER);
    spr.setTextColor(AXIS_C_MUTED,AXIS_C_BG);
    DST(spr, xOff+2, yOff+123, L.set_save_cancel);

    // 当前行箭头（5 行）
    static const int16_t arrowYs[5] = { 23, 41, 59, 77, 95 };
    int16_t arrowY = yOff + arrowYs[constrain(settingsCursor,0,4)];
    spr.setTextColor(AXIS_C_ACCENT,AXIS_C_BG);
    RESET_F(spr); spr.setCursor(xOff+112, arrowY); spr.print(">");
}

void inputSettings(AxisInputEvent ev, void*) {
    g.lastActivityMs = millis();
    if (ev == AXIS_INPUT_UP)   { if(settingsCursor>0) settingsCursor--; return; }
    if (ev == AXIS_INPUT_DOWN) { if(settingsCursor<4) settingsCursor++; return; }

    if (settingsCursor == 0) {
        if (ev == AXIS_INPUT_LEFT  || ev == AXIS_INPUT_OK) g.langChoice = 0;
        else if (ev == AXIS_INPUT_RIGHT) g.langChoice = 1;
    } else if (settingsCursor == 1) {
        if (ev == AXIS_INPUT_LEFT || ev == AXIS_INPUT_RIGHT || ev == AXIS_INPUT_OK)
            g.gyroEn = !g.gyroEn;
    } else if (settingsCursor == 2) {
        if (ev == AXIS_INPUT_LEFT || ev == AXIS_INPUT_RIGHT || ev == AXIS_INPUT_OK)
            g.themeLight = !g.themeLight;
    } else if (settingsCursor == 3) {
        // DEBUG 行：OK / RIGHT 进入子菜单（goTo 自动压栈，B 键框架负责返回）
        if (ev == AXIS_INPUT_OK || ev == AXIS_INPUT_RIGHT)
            ui.goTo(SCR_DEBUG, AXIS_TRANS_SLIDE_LEFT);
    } else {
        // SCREENSV 行：LEFT/RIGHT 循环切换超时选项
        if (ev == AXIS_INPUT_LEFT)  g.ssTimeoutOpt = (g.ssTimeoutOpt + 3) % 4;
        if (ev == AXIS_INPUT_RIGHT || ev == AXIS_INPUT_OK)
                                    g.ssTimeoutOpt = (g.ssTimeoutOpt + 1) % 4;
    }

    if (ev == AXIS_INPUT_BTN_A) {
        applyTheme();
        ui.setLanguage((AxisLang)g.langChoice);
        rebuildMenu();
        rebuildNodes();
        refreshTickerContent();
        prefs.begin("axis_ui", false);
        prefs.putUChar("lang",    g.langChoice);
        prefs.putBool ("gyro",    g.gyroEn);
        prefs.putBool ("light",   g.themeLight);
        prefs.putUChar("ss_opt",  g.ssTimeoutOpt);
        prefs.end();
        buzzer::confirm();
        ui.notify(L.saved, AXIS_C_OK, 1500);
        ui.goBack(AXIS_TRANS_SLIDE_RIGHT);
    }
    // AXIS_INPUT_BTN_B 由框架全局处理（自动返回上一屏）
}

// ─────────────────────────────────────────────────
//  HOME / Player 输入
// ─────────────────────────────────────────────────
// ─────────────────────────────────────────────────
//  SCR_DEBUG — 调试模式子菜单
//  B 键由框架自动返回 Settings（goBack）
// ─────────────────────────────────────────────────
static int8_t debugCursor = 0;  // 0=串口事件  1=FPS显示

void drawDebug(LGFX_Sprite& spr, int16_t xOff, int16_t yOff, void*) {
    spr.fillScreen(AXIS_C_BG);
    int16_t W = spr.width();
    spr.drawFastHLine(xOff,yOff,  W,AXIS_C_WARN);
    spr.drawFastHLine(xOff,yOff+1,W,AXIS_C_WARN);
    RESET_F(spr); spr.setTextColor(AXIS_C_WARN,AXIS_C_BG);
    spr.setCursor(xOff+2,yOff+4); spr.print("DEBUG MODE");
    spr.drawFastHLine(xOff,yOff+13,W,AXIS_C_DIVIDER);

    // 串口打印事件行  y=18
    int16_t r1=yOff+18;
    spr.drawFastHLine(xOff,r1-1,W,AXIS_C_DIVIDER);
    RESET_F(spr); spr.setTextColor(AXIS_C_DIM,AXIS_C_BG);
    spr.setCursor(xOff+2,r1+5); spr.print("SERIAL LOG");
    spr.setTextColor(g.debugSerial ? AXIS_C_OK : AXIS_C_MUTED,AXIS_C_BG);
    spr.setCursor(xOff+80,r1+5); spr.print(g.debugSerial ? "ON" : "OFF");

    // FPS 显示行  y=36
    int16_t r2=yOff+36;
    spr.drawFastHLine(xOff,r2-1,W,AXIS_C_DIVIDER);
    spr.setTextColor(AXIS_C_DIM,AXIS_C_BG);
    spr.setCursor(xOff+2,r2+5); spr.print("FPS OVERLAY");
    spr.setTextColor(g.debugFPS ? AXIS_C_OK : AXIS_C_MUTED,AXIS_C_BG);
    spr.setCursor(xOff+80,r2+5); spr.print(g.debugFPS ? "ON" : "OFF");

    spr.drawFastHLine(xOff,yOff+120,W,AXIS_C_DIVIDER);
    spr.setTextColor(AXIS_C_MUTED,AXIS_C_BG);
    spr.setCursor(xOff+2,yOff+123); spr.print("[A]SAVE  [B]BACK");

    // 箭头
    static const int16_t arrowYs[2] = { 23, 41 };
    spr.setTextColor(AXIS_C_WARN,AXIS_C_BG);
    spr.setCursor(xOff+112, yOff+arrowYs[constrain(debugCursor,0,1)]); spr.print(">");
}

void inputDebug(AxisInputEvent ev, void*) {
    g.lastActivityMs = millis();
    if (ev == AXIS_INPUT_UP)   { if(debugCursor>0) debugCursor--; return; }
    if (ev == AXIS_INPUT_DOWN) { if(debugCursor<1) debugCursor++; return; }

    if (ev == AXIS_INPUT_LEFT || ev == AXIS_INPUT_RIGHT || ev == AXIS_INPUT_OK) {
        if (debugCursor == 0) g.debugSerial = !g.debugSerial;
        else                  g.debugFPS    = !g.debugFPS;
    }

    if (ev == AXIS_INPUT_BTN_A) {
        // 应用调试设置
        ui.setCustomFlag(g.debugSerial || g.debugFPS, AXIS_C_WARN);
        ui.setFPSOverlay(g.debugFPS);
        if (g.debugSerial) Serial.println("[DBG] Debug serial enabled");
        // 持久化
        prefs.begin("axis_ui", false);
        prefs.putBool("dbg_ser", g.debugSerial);
        prefs.putBool("dbg_fps", g.debugFPS);
        prefs.end();
        ui.notify("SAVED", AXIS_C_OK, 1200);
        ui.goBack(AXIS_TRANS_SLIDE_RIGHT);
    }
    // BTN_B 由框架自动返回 Settings
}

void inputHome(AxisInputEvent ev, void*) {
    g.lastActivityMs = millis();
    if (ev == AXIS_INPUT_OK &&
        ui.currentScreen() == AXIS_SCR_HOME &&
        ui.nodeCursor() < 0)
        ui.showMenu(mainMenu, MENU_COUNT);
}

void inputPlayer(AxisInputEvent ev, void*) {
    g.lastActivityMs = millis();
    switch (ev) {
        case AXIS_INPUT_OK:
            g.playing = !g.playing;
            ui.setPlaying(g.playing);
            ui.notify(g.playing ? L.ply_play : L.ply_stop,
                      g.playing ? AXIS_C_ACCENT : AXIS_C_MUTED, 1500);
            break;
        case AXIS_INPUT_UP:
            g.volume=min(100,(int)g.volume+5); ui.setVolume(g.volume); break;
        case AXIS_INPUT_DOWN:
            g.volume=max(0,(int)g.volume-5);   ui.setVolume(g.volume); break;
        case AXIS_INPUT_LEFT:
            g.progress=0; ui.setProgress(0);
            ui.notify(L.ply_prev, AXIS_C_ACCENT, 1000); break;
        case AXIS_INPUT_RIGHT:
            g.progress=0; ui.setProgress(0);
            ui.notify(L.ply_next, AXIS_C_ACCENT, 1000); break;
        case AXIS_INPUT_BTN_B:
        case AXIS_INPUT_BTN_B_LONG:
            ui.goBack(AXIS_TRANS_SLIDE_RIGHT); break;   // Player 需手动处理 B
        case AXIS_INPUT_BTN_A_LONG:
            ui.goHome(); break;
        default: break;
    }
}

// ─────────────────────────────────────────────────
//  传感器 & 进度更新
// ─────────────────────────────────────────────────
void updateSensors() {
    static uint32_t lastEnv=0, lastLevel=0, lastHdg=0, lastGreet=0;
    if (millis()-lastLevel >= 50)   { lastLevel=millis(); readMPU6050(); }
    if (millis()-lastHdg   >= 80)   { lastHdg=millis();   readGY271();   }
    if (millis()-lastEnv   >= 2000) {
        lastEnv=millis();
        if (!mpuOk) {   // 没有 MPU，也模拟温湿度变化
            g.temp     += random(-10,10)/100.0f;
            g.humidity += random(-5,5)/100.0f;
            g.co2       = 400+random(0,50);
        }
        AxisSensorData d;
        d.temp=g.temp; d.humidity=g.humidity; d.pressure=g.pressure;
        d.co2=g.co2; d.tvoc=g.tvoc; d.valid=true;
        ui.setSensorData(d);
        ui.setWalking(g.isWalking);
    }
    // 每分钟刷新问候语（接入 RTC 后 g.timeStr 变化时会自动更新）
    if (millis() - lastGreet >= 60000) {
        lastGreet = millis();
        refreshTickerContent();
    }
}

void updateProgress() {
    static uint32_t last=0;
    if (!g.playing || millis()-last<1000) return;
    last=millis();
    g.progress += 1.0f/210.0f;
    if (g.progress>1.0f) g.progress=0;
    ui.setProgress(g.progress);
}

// ─────────────────────────────────────────────────
//  开机动画：Linux 终端风格 → 旋转线框大立方体
// ─────────────────────────────────────────────────
static void runBootAnimation() {
    tft.setTextWrap(false);

    // ── Phase 1: 终端滚屏 ─────────────────────────
    tft.fillScreen(0x0000);
    tft.setTextFont(1);

    struct BootLine { const char* txt; uint16_t col; uint16_t ms; };
    static const BootLine msgs[] = {
        { "[0.000] AXIS v0.2",     0x07E0, 80  },
        { "[0.001] CPU: ESP32-P4", 0x07E0, 60  },
        { "[0.012] SPI2 40MHz",    0x07E0, 90  },
        { "[0.023] SSD1351 init",  0x07E0, 120 },
        { "[0.034] LovyanGFX ok",  0x07E0, 80  },
        { "[0.045] I2C SDA7 SCL8", 0x07E0, 70  },
        { "[0.056] MPU-6050...",   0xFFE0, 180 },
        { "[0.067] GY-271...",     0xFFE0, 200 },
        { "[0.089] sensors OK",    0x07E0, 90  },
        { "[0.095] NVS loaded",    0x07E0, 60  },
        { "[0.100] AXIS_UI init",  0x07E0, 100 },
        { "[0.111] boot complete", 0xFFFF, 300 },
    };
    const uint8_t MSG_N = sizeof(msgs) / sizeof(msgs[0]);

    for (uint8_t i = 0; i < MSG_N; i++) {
        tft.setTextColor(msgs[i].col, 0x0000);
        tft.setCursor(2, 2 + i * 9);
        tft.print(msgs[i].txt);
        delay(msgs[i].ms);
    }
    delay(200);

    // ── Phase 2: 旋转立方体 + 进度条 ─────────────
    float rx = 0.3f, ry = 0.5f;
    uint32_t startMs = millis();
    const uint32_t CUBE_MS = 2000;

    while (millis() - startMs < CUBE_MS) {
        float prog = (float)(millis() - startMs) / (float)CUBE_MS;
        rx += 0.035f;
        ry += 0.055f;

        tft.fillScreen(0x0000);

        // 标题
        tft.setTextFont(2);
        tft.setTextColor(AXIS_C_ACCENT, 0x0000);
        tft.setTextDatum(4);   // MC_DATUM = middle-center
        tft.drawString("AXIS  UI", 64, 10);
        tft.setTextDatum(0);

        // 大线框立方体（cx=64, cy=55, scale=36）
        int16_t px[8], py[8];
        for (int v = 0; v < 8; v++) {
            float x = CV[v][0], y = CV[v][1], z = CV[v][2];
            float tx2 = x * cosf(ry) + z * sinf(ry);
            float tz  = -x * sinf(ry) + z * cosf(ry);
            x = tx2; z = tz;
            float ty2 = y * cosf(rx) - z * sinf(rx);
            float tz2 = y * sinf(rx) + z * cosf(rx);
            y = ty2; z = tz2;
            float s = 36.0f * 3.5f / (3.5f + z);
            px[v] = (int16_t)(64 + x * s);
            py[v] = (int16_t)(55 + y * s);
        }
        for (int e = 0; e < 12; e++)
            tft.drawLine(px[CE[e][0]], py[CE[e][0]],
                         px[CE[e][1]], py[CE[e][1]], AXIS_C_ACCENT);

        // 进度条
        tft.drawRect(8, 102, 112, 7, AXIS_C_MUTED);
        int16_t fw = (int16_t)(110.0f * prog);
        if (fw > 0) tft.fillRect(9, 103, fw, 5, AXIS_C_ACCENT);

        // 百分比
        char pbuf[6]; snprintf(pbuf, 6, "%3d%%", (int)(prog * 100.0f));
        tft.setTextFont(1);
        tft.setTextColor(AXIS_C_DIM, 0x0000);
        tft.setTextDatum(4);
        tft.drawString(pbuf, 64, 114);
        tft.setTextDatum(0);

        delay(16);
    }

    tft.fillScreen(0x0000);
    delay(80);
}

// ─────────────────────────────────────────────────
//  setup / loop
// ─────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    Serial.println("=== BasicMenu v4 ===");

    tft.init();
    tft.setRotation(5);   // 90° CW，与摇杆/MPU方向对齐

    runBootAnimation();

    // 读取持久化设置
    prefs.begin("axis_ui", true);
    g.langChoice    = prefs.getUChar("lang",     0);
    g.gyroEn        = prefs.getBool ("gyro",     false);
    g.themeLight    = prefs.getBool ("light",    false);
    g.debugSerial   = prefs.getBool ("dbg_ser",  false);
    g.debugFPS      = prefs.getBool ("dbg_fps",  false);
    g.ssTimeoutOpt  = prefs.getUChar("ss_opt",   2);
    prefs.end();
    applyTheme();

    ui.setMainDisplay(&tft);
    ui.setLanguage((AxisLang)g.langChoice);

    AxisPinConfig pins;
    pins.joy_up=2; pins.joy_down=3; pins.joy_left=4;
    pins.joy_right=5; pins.joy_ok=6;
    pins.btn_a=14;   // 保存/确认
    pins.btn_b=20;   // 返回

    if (!ui.begin(pins)) {
        Serial.println("AXIS UI begin FAILED");
        tft.fillScreen(0xF800);
        while(1) delay(1000);
    }

    // 应用调试设置（需在 begin() 之后）
    ui.setCustomFlag(g.debugSerial || g.debugFPS, AXIS_C_WARN);
    ui.setFPSOverlay(g.debugFPS);

    ui.setTime(g.timeStr);
    ui.setDate(g.dateStr);
    ui.setBattery(g.battery);
    ui.setVolume(g.volume);
    ui.setBTConnected(g.btConn);

    // 构建菜单（依赖语言，须在 setLanguage 之后）
    rebuildMenu();

    // 初始化传感器
    sensorBegin();

    // 注册屏幕（BTN_B 全局返回由框架处理，view-only 屏无需 inputFn）
    ui.registerScreen(SCR_INFO,     drawInfo,     nullptr,       nullptr);
    ui.registerScreen(SCR_SETTINGS, drawSettings, inputSettings, nullptr);
    ui.registerScreen(SCR_LEVEL,    drawLevel,    nullptr,       nullptr);
    ui.registerScreen(SCR_COMPASS,  drawCompass,  nullptr,       nullptr);
    ui.registerScreen(SCR_CUBE,     drawCube,     nullptr,      nullptr);
    ui.registerScreen(SCR_SERIAL,   drawSerial,   inputSerial,  nullptr);
    ui.registerScreen(SCR_DEBUG,       drawDebug,       inputDebug,       nullptr);
    ui.registerScreen(SCR_SCREENSAVER, drawScreensaver, inputScreensaver, nullptr);

    ui.registerInput(AXIS_SCR_HOME,   inputHome,   nullptr);
    ui.registerInput(AXIS_SCR_PLAYER, inputPlayer, nullptr);

    // HOME 节点（语言已设好，用 rebuildNodes 注册）
    rebuildNodes();

    AxisSensorData d;
    d.temp=g.temp; d.humidity=g.humidity; d.pressure=g.pressure;
    d.co2=g.co2; d.tvoc=g.tvoc; d.valid=true;
    ui.setSensorData(d);

    // 灵动消息栏：slot 0 = 传感器（框架自动填），1-3 = 动态内容
    refreshTickerContent();

    g.lastActivityMs = millis();

    buzzer::boot();
    ui.goHome();
    Serial.println("AXIS UI ready");
}

void loop() {
    // 串口接收：按行追加到终端缓冲
    while (Serial.available()) {
        char c = (char)Serial.read();
        if (c == '\n' || c == '\r') {
            if (serInBuf.length() > 0) {
                serialPushLine(serInBuf.c_str(), false);  // false=接收
                serInBuf = "";
            }
        } else if (serInBuf.length() < SER_WIDTH) {
            serInBuf += c;
        }
    }

    updateSensors();
    updateProgress();

    // 重力光标（仅 HOME 屏，MPU 有效时，非走路状态）
    // 两状态机：IDLE → 触发一次输入 → WAIT_RETURN（等设备回中 8° 以内才接受下次）
    if (g.gyroEn && mpuOk && !g.isWalking &&
        ui.currentScreen() == AXIS_SCR_HOME &&
        !ui.isMenuOpen() &&
        !ui.isTransitioning()) {

        static float baseRoll   = 0, basePitch = 0;
        static bool  baseSet    = false;
        static bool  waitReturn = false;

        if (!baseSet) { baseRoll = g.roll; basePitch = g.pitch; baseSet = true; }

        float dRoll  = g.roll  - baseRoll;
        float dPitch = g.pitch - basePitch;

        if (waitReturn) {
            // 等设备回到中立区域（8° 死区）
            if (fabsf(dRoll) < 8.0f && fabsf(dPitch) < 8.0f) {
                waitReturn = false;
                baseRoll  = g.roll;
                basePitch = g.pitch;
            }
        } else {
            const float THR = 20.0f;
            AxisInputEvent ginput = AXIS_INPUT_NONE;
            if      (dRoll  >  THR) ginput = AXIS_INPUT_RIGHT;
            else if (dRoll  < -THR) ginput = AXIS_INPUT_LEFT;
            else if (dPitch >  THR) ginput = AXIS_INPUT_DOWN;
            else if (dPitch < -THR) ginput = AXIS_INPUT_UP;

            if (ginput != AXIS_INPUT_NONE) {
                ui.injectInput(ginput);
                g.lastActivityMs = millis();
                waitReturn = true;   // 锁定，等回中后再接受下次
            }
        }
    }

    // 屏保：进入（HOME 静止 + 平放桌面）
    {
        static const uint32_t ssTimeouts[4] = { 0, 30000, 60000, 300000 };
        uint32_t ssto = ssTimeouts[g.ssTimeoutOpt];
        uint32_t now  = millis();
        bool flat = mpuOk && (fabsf(g.pitch) < 15.0f) && (fabsf(g.roll) < 15.0f);

        if (!g.ssActive && ssto > 0 &&
            ui.currentScreen() == AXIS_SCR_HOME &&
            flat &&
            (now - g.lastActivityMs > ssto)) {
            g.ssActive = true;
            buzzer::screenOff();
            initScreensaver();
            ui.goTo(SCR_SCREENSAVER, AXIS_TRANS_NONE);
        }

        // 屏保：退出（被拿起来，roll 或 pitch 超过 30°）
        if (g.ssActive && mpuOk &&
            (fabsf(g.pitch) > 30.0f || fabsf(g.roll) > 30.0f)) {
            g.ssActive       = false;
            g.lastActivityMs = now;
            buzzer::screenOn();
            ui.goHome();
        }
    }

    ui.update();
}
