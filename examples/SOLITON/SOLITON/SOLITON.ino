/**
 * SOLITON — AXIS-UI 验证工程 v1
 * Target: 微雪 ESP32-P4-Pico + 240×320 TFT SPI + 黑莓轨迹球 + ESP32-C3-OLED
 *
 * 接线：
 *   TFT:     SCK=15  MOSI=16  CS=17  DC=18  RST=19  BL=20
 *   轨迹球:  UP=2    DWN=3    LFT=4  RHT=5  BTN=6
 *   轨迹球短按=OK  长按=返回（无独立 A/B 键）
 *   I2C:     SDA=7   SCL=8
 *   C3 UART: TX=22   RX=23   (Serial1 115200)
 *
 * 屏幕型号两手准备：
 *   注释/取消注释下面的 #define 切换
 */

#define TFT_ILI9341   // 红色PCB ILI9341
// #define TFT_ST7789       // 红色PCB ST7789（默认先试这个）

#include <LovyanGFX.h>
#include <AXIS_UI.h>
#include <Wire.h>
#include <Preferences.h>
#include <math.h>
#include <U8g2lib.h>
#include "lang.h"
#include "trackball.h"

// ── 前置类型定义（Arduino IDE 原型生成器需要）──
struct ScrollState {
    int16_t  offset  = 0;
    uint32_t phaseMs = 0;
    uint8_t  phase   = 0;
};

struct WalkDetect {
    float    smoothMag  = 1.0f;
    bool     wasAbove   = false;
    uint32_t stepTimes[8] = {};
    uint8_t  stepHead   = 0;
    uint32_t lastStepMs = 0;
    uint32_t lastMs     = 0;
};

struct SSShape { float x,y,z,rx,ry,vrx,vry,sz; };

// ── U8g2 中文字体 ─────────────────────────────────
static const lgfx::U8g2font font_zh_12(u8g2_font_wqy12_t_gb2312b);
static const lgfx::U8g2font font_zh_16(u8g2_font_wqy16_t_gb2312b);

// ── 字体宏 ────────────────────────────────────────
#define SET_F1(spr) do { \
    if (ui.language() == AXIS_LANG_ZH) (spr).setFont(&font_zh_12); \
    else (spr).setTextFont(1); \
} while(0)

#define SET_F2(spr) do { \
    if (ui.language() == AXIS_LANG_ZH) (spr).setFont(&font_zh_16); \
    else (spr).setTextFont(2); \
} while(0)

#define RESET_F(spr) (spr).setTextFont(1)

// ─────────────────────────────────────────────────
//  LovyanGFX 配置（两手准备）
// ─────────────────────────────────────────────────
class LGFX : public lgfx::LGFX_Device {
#ifdef TFT_ILI9341
    lgfx::Panel_ILI9341 _panel;
#else
    lgfx::Panel_ST7789  _panel;
#endif
    lgfx::Bus_SPI _bus;
    lgfx::Light_PWM _bl;
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
          cfg.memory_width=240; cfg.memory_height=320;
          cfg.panel_width=240;  cfg.panel_height=320;
          _panel.config(cfg); }
        { auto cfg = _bl.config();
          cfg.pin_bl = 20;
          cfg.invert = false;
          cfg.freq   = 44100;
          cfg.pwm_channel = 7;
          _bl.config(cfg); _panel.setLight(&_bl); }
        setPanel(&_panel);
    }
};

// ─────────────────────────────────────────────────
//  全局实例
// ─────────────────────────────────────────────────
LGFX        tft;
AXIS_UI     ui;
Preferences prefs;

#define L   (ui.language() == AXIS_LANG_ZH ? L_ZH : L_EN)
#define DST(spr, x, y, str) do { (spr).setTextDatum(0); (spr).drawString((str),(x),(y)); } while(0)

// ─────────────────────────────────────────────────
//  用户屏幕 ID
// ─────────────────────────────────────────────────
enum UserScreenID : uint8_t {
    SCR_INFO        = 0x10,
    SCR_SETTINGS    = 0x11,
    SCR_LEVEL       = 0x12,
    SCR_COMPASS     = 0x13,
    SCR_CUBE        = 0x14,
    SCR_SERIAL      = 0x15,
    SCR_DEBUG       = 0x16,
    SCR_SCREENSAVER = 0x17,
};

// ─────────────────────────────────────────────────
//  菜单
// ─────────────────────────────────────────────────
static AxisMenuItem mainMenu[4];
const uint8_t MENU_COUNT = 4;

static void rebuildMenu() {
    mainMenu[0] = { L.menu_playing,  AXIS_C_ACCENT,  AXIS_SCR_PLAYER, AXIS_TRANS_SLIDE_LEFT  };
    mainMenu[1] = { L.menu_sysinfo,  AXIS_C_ACCENT2, SCR_INFO,        AXIS_TRANS_SLIDE_LEFT  };
    mainMenu[2] = { L.menu_settings, AXIS_C_MUTED,   SCR_SETTINGS,    AXIS_TRANS_SLIDE_LEFT  };
    mainMenu[3] = { L.menu_home,     AXIS_C_DIM,     AXIS_SCR_HOME,   AXIS_TRANS_SLIDE_RIGHT };
}

// HOME 节点布局（适配 240×320，节点坐标按比例放大）
static void rebuildNodes() {
    ui.clearHomeNodes();
    ui.registerHomeNode(L.node_level,   AXIS_C_OK,      SCR_LEVEL,       AXIS_TRANS_SLIDE_LEFT, L.desc_level,    6, "LEVEL", -1);
    ui.registerHomeNode(L.node_compass, AXIS_C_BLUE,    SCR_COMPASS,     AXIS_TRANS_SLIDE_LEFT, L.desc_compass,  7, "COMP",   0);
    ui.registerHomeNode(L.node_cube,    AXIS_C_ACCENT2, SCR_CUBE,        AXIS_TRANS_SLIDE_LEFT, L.desc_cube,     0, "CUBE",  -1);
    ui.registerHomeNode("SERIAL",       AXIS_C_MUTED,   SCR_SERIAL,      AXIS_TRANS_SLIDE_LEFT, "Serial Monitor",3, "SER",  -1);
    const char* plyLabel = (ui.language() == AXIS_LANG_ZH) ? "播放器" : "PLAYER";
    const char* plyDesc  = (ui.language() == AXIS_LANG_ZH) ? "音乐播放器" : "Music Player";
    ui.registerHomeNode(plyLabel, AXIS_C_ACCENT, AXIS_SCR_PLAYER, AXIS_TRANS_SLIDE_UP, plyDesc, 1, "PLY", 3);
}

// ─────────────────────────────────────────────────
//  全局状态
// ─────────────────────────────────────────────────
struct State {
    float    temp=23.4f, humidity=61.0f, pressure=1013.0f;
    uint16_t co2=420;
    float    tvoc=0.1f;
    uint8_t  battery=88;
    bool     btConn=false;
    String   timeStr="12:00";
    String   dateStr="TUE 15 APR";
    uint8_t  volume=75;
    float    progress=0.0f;
    bool     playing=false;
    float    roll=0.0f, pitch=0.0f, heading=0.0f;
    uint8_t  langChoice=0;
    bool     gyroEn=false, themeLight=false;
    bool     debugSerial=false, debugFPS=false;
    uint32_t gyroLastMs=0;
    bool     isWalking=false;
    uint8_t  ssTimeoutOpt=2;
    uint8_t  navMode=0;  // 0=JOYSTICK  1=CURSOR
    bool     ssActive=false;
    uint32_t lastActivityMs=0;
} g;

// ── 主题 ──────────────────────────────────────────
struct Theme { uint16_t bg,card,divider,text,muted,dim,accent; };
static const Theme TH_DARK  = { 0x0000,0x1082,0x2945,0xDEDB,0x7BCF,0x39E7,AXIS_C_ACCENT };
static const Theme TH_LIGHT = { 0xFFFF,0xEF7B,0xC618,0x2104,0x6B4D,0xA534,0xC880 };
static const Theme* TH = &TH_DARK;
static void applyTheme() { TH = g.themeLight ? &TH_LIGHT : &TH_DARK; }

// ── 灵动消息栏内容 ────────────────────────────────
static void refreshTickerContent() {
    bool zh = (ui.language() == AXIS_LANG_ZH);
    uint8_t h = (uint8_t)atoi(g.timeStr.c_str());
    const char* greet;
    if      (h>=5  && h<11) greet = zh ? "早上好"       : "GOOD MORNING";
    else if (h>=11 && h<14) greet = zh ? "中午好"       : "GOOD NOON";
    else if (h>=14 && h<18) greet = zh ? "下午好"       : "GOOD AFTERNOON";
    else if (h>=18 && h<23) greet = zh ? "晚上好"       : "GOOD EVENING";
    else                     greet = zh ? "怎么还不睡觉" : "GO TO SLEEP!";
    ui.setTickerItem(1, greet, AXIS_C_ACCENT);
    ui.setTickerItem(2, zh ? "没有新通知" : "NO NEW MESSAGES", AXIS_C_MUTED);
    ui.setTickerItem(3, zh ? "天气  -- °C" : "WEATHER  -- °C", AXIS_C_ACCENT2);
}

static WalkDetect wdet;

// ─────────────────────────────────────────────────
//  I2C 传感器
// ─────────────────────────────────────────────────
#define MPU_ADDR  0x68
#define QMC_ADDR  0x0D
#define HMC_ADDR  0x1E

bool mpuOk=false, gy271Ok=false, isQMC=false;

void sensorBegin() {
    Wire.begin(7, 8);
    Wire.beginTransmission(MPU_ADDR);
    Wire.write(0x6B); Wire.write(0x00);
    mpuOk = (Wire.endTransmission() == 0);
    Serial.println(mpuOk ? "MPU-6050 OK" : "MPU-6050 not found (sim)");

    Wire.beginTransmission(QMC_ADDR);
    if (Wire.endTransmission() == 0) {
        isQMC=true; gy271Ok=true;
        Wire.beginTransmission(QMC_ADDR); Wire.write(0x09); Wire.write(0x1D); Wire.endTransmission();
        Wire.beginTransmission(QMC_ADDR); Wire.write(0x0B); Wire.write(0x01); Wire.endTransmission();
        Serial.println("QMC5883L OK");
    } else {
        Wire.beginTransmission(HMC_ADDR);
        if (Wire.endTransmission() == 0) {
            isQMC=false; gy271Ok=true;
            Wire.beginTransmission(HMC_ADDR); Wire.write(0x02); Wire.write(0x00); Wire.endTransmission();
            Serial.println("HMC5883L OK");
        } else Serial.println("GY-271 not found (sim)");
    }
}

void readMPU6050() {
    if (!mpuOk) {
        static float vr=0.4f, vp=0.3f;
        g.roll+=vr; g.pitch+=vp;
        if (g.roll>85.0f||g.roll<-85.0f) vr=-vr;
        if (g.pitch>20.0f||g.pitch<-20.0f) vp=-vp;
        return;
    }
    Wire.beginTransmission(MPU_ADDR); Wire.write(0x3B); Wire.endTransmission(false);
    Wire.requestFrom(MPU_ADDR, 6, true);
    int16_t ax=(Wire.read()<<8)|Wire.read();
    int16_t ay=(Wire.read()<<8)|Wire.read();
    int16_t az=(Wire.read()<<8)|Wire.read();
    float axf=ax/16384.0f, ayf=ay/16384.0f, azf=az/16384.0f;
    float rawRoll  = -atan2f(ayf,azf)*57.2958f;
    float rawPitch =  atan2f(-axf,sqrtf(ayf*ayf+azf*azf))*57.2958f;
    const float alpha=0.15f;
    g.roll  = alpha*rawRoll  + (1.0f-alpha)*g.roll;
    g.pitch = alpha*rawPitch + (1.0f-alpha)*g.pitch;

    // 走路检测
    static const float WD_HI=1.18f, WD_LO=1.05f;
    static const uint32_t WD_MIN=250, WD_MAX=1200, WD_WIN=4000;
    static const uint8_t WD_STEPS=4;
    uint32_t _now=millis();
    if (_now-wdet.lastMs>=25) {
        wdet.lastMs=_now;
        float rawMag=sqrtf(axf*axf+ayf*ayf+azf*azf);
        wdet.smoothMag=0.35f*rawMag+0.65f*wdet.smoothMag;
        float thr=wdet.wasAbove?WD_LO:WD_HI;
        bool above=(wdet.smoothMag>thr);
        if (above&&!wdet.wasAbove) {
            uint32_t dt=_now-wdet.lastStepMs;
            if (wdet.lastStepMs==0||(dt>=WD_MIN&&dt<=WD_MAX)) {
                wdet.stepTimes[wdet.stepHead]=_now;
                wdet.stepHead=(wdet.stepHead+1)%8;
                wdet.lastStepMs=_now;
            }
        }
        wdet.wasAbove=above;
    }
    uint32_t now2=millis(); uint8_t cnt=0;
    for (uint8_t i=0;i<8;i++) if (wdet.stepTimes[i]>0&&now2-wdet.stepTimes[i]<WD_WIN) cnt++;
    g.isWalking=(cnt>=WD_STEPS);
}

void readGY271() {
    if (!gy271Ok) { g.heading+=0.8f; if(g.heading>=360.0f) g.heading-=360.0f; return; }
    int16_t mx,my,mz;
    if (isQMC) {
        Wire.beginTransmission(QMC_ADDR); Wire.write(0x00); Wire.endTransmission(false);
        Wire.requestFrom(QMC_ADDR,6,true);
        mx=Wire.read()|(Wire.read()<<8); my=Wire.read()|(Wire.read()<<8); mz=Wire.read()|(Wire.read()<<8);
    } else {
        Wire.beginTransmission(HMC_ADDR); Wire.write(0x03); Wire.endTransmission(false);
        Wire.requestFrom(HMC_ADDR,6,true);
        mx=(Wire.read()<<8)|Wire.read(); mz=(Wire.read()<<8)|Wire.read(); my=(Wire.read()<<8)|Wire.read();
    }
    float h=atan2f(-(float)my,(float)mx)*57.2958f;
    if (h<0) h+=360.0f;
    g.heading=h;
}

// ─────────────────────────────────────────────────
//  3D 线框立方体（公共几何数据）
// ─────────────────────────────────────────────────
static const float CV[8][3] = {
    {-1,-1,-1},{1,-1,-1},{1,1,-1},{-1,1,-1},
    {-1,-1, 1},{1,-1, 1},{1,1, 1},{-1,1, 1}
};
static const uint8_t CE[12][2] = {
    {0,1},{1,2},{2,3},{3,0},{4,5},{5,6},{6,7},{7,4},
    {0,4},{1,5},{2,6},{3,7}
};

static void drawWireCube(LGFX_Sprite& spr, int16_t cx, int16_t cy,
                          float rx, float ry, float scale, uint16_t col) {
    int16_t px[8], py[8];
    for (int i=0;i<8;i++) {
        float x=CV[i][0], y=CV[i][1], z=CV[i][2];
        float tx=x*cosf(ry)+z*sinf(ry), tz=-x*sinf(ry)+z*cosf(ry); x=tx; z=tz;
        float ty=y*cosf(rx)-z*sinf(rx), tz2=y*sinf(rx)+z*cosf(rx); y=ty; z=tz2;
        float s=scale*3.5f/(3.5f+z);
        px[i]=(int16_t)(cx+x*s); py[i]=(int16_t)(cy+y*s);
    }
    for (int e=0;e<12;e++)
        spr.drawLine(px[CE[e][0]],py[CE[e][0]],px[CE[e][1]],py[CE[e][1]],col);
}

// ─────────────────────────────────────────────────
//  滚动文字辅助
// ─────────────────────────────────────────────────
static void drawScrollText(LGFX_Sprite& spr, int16_t x, int16_t y, int16_t maxW,
                           const char* str, uint16_t col, ScrollState& s) {
    int16_t tw=(int16_t)spr.textWidth(str);
    if (tw<=maxW) {
        spr.setTextColor(col,AXIS_C_BG); spr.setCursor(x,y); spr.print(str);
        s.offset=0; s.phase=0; s.phaseMs=0; return;
    }
    int16_t maxOff=tw-maxW+4;
    uint32_t now=millis();
    if (s.phaseMs==0) s.phaseMs=now;
    uint32_t el=now-s.phaseMs;
    switch(s.phase) {
        case 0: if(el>1000){s.phase=1;s.phaseMs=now;s.offset=0;} break;
        case 1: s.offset=(int16_t)(el/28); if(s.offset>=maxOff){s.offset=maxOff;s.phase=2;s.phaseMs=now;} break;
        case 2: if(el>1000){s.phase=0;s.phaseMs=now;s.offset=0;} break;
    }
    spr.setClipRect(x,y-1,maxW,12);
    spr.setTextColor(col,AXIS_C_BG); spr.setCursor(x-s.offset,y); spr.print(str);
    spr.clearClipRect();
}


// ─────────────────────────────────────────────────
//  SCR_SERIAL — 串口终端
// ─────────────────────────────────────────────────
#define SER_LINES  20
#define SER_WIDTH  30   // 240px / 6px字体 ≈ 40，留余量

struct SerLine { char buf[SER_WIDTH+1]; bool sent; };
static SerLine  serBuf[SER_LINES];
static uint8_t  serHead=0, serCount=0;
static int8_t   serScroll=0;
static String   serInBuf="";

static void serialPushLine(const char* text, bool sent) {
    SerLine& sl=serBuf[serHead];
    strncpy(sl.buf,text,SER_WIDTH); sl.buf[SER_WIDTH]='\0'; sl.sent=sent;
    serHead=(serHead+1)%SER_LINES;
    if (serCount<SER_LINES) serCount++;
    serScroll=0;
    ui.triggerHomeCubePulse();
}

#define SER_VISIBLE 14   // 320px 高，标题+底部各留约30px，剩余约260px / 18px行高

void drawSerial(LGFX_Sprite& spr, int16_t xOff, int16_t yOff, void*) {
    spr.fillScreen(TH->bg);
    int16_t W=spr.width(), H=spr.height();
    uint16_t barC=TH->accent;
    spr.drawFastHLine(xOff,yOff,  W,barC);
    spr.drawFastHLine(xOff,yOff+1,W,barC);
    RESET_F(spr); spr.setTextColor(barC,TH->bg);
    spr.setCursor(xOff+4,yOff+6); spr.print(L.title_serial);
    spr.setTextColor(TH->muted,TH->bg);
    spr.setCursor(xOff+80,yOff+6); spr.print("115200");
    spr.drawFastHLine(xOff,yOff+18,W,TH->divider);

    int16_t lineH=18, y0=yOff+22;
    for (int8_t row=0;row<SER_VISIBLE;row++) {
        int8_t logIdx=row+serScroll;
        if (logIdx>=serCount) break;
        int8_t bufIdx=((int16_t)serHead-1-logIdx%SER_LINES+SER_LINES*2)%SER_LINES;
        const SerLine& sl=serBuf[bufIdx];
        int16_t ty=y0+(SER_VISIBLE-1-row)*lineH;
        uint16_t tc=sl.sent?TH->accent:TH->text;
        spr.setTextColor(tc,TH->bg);
        spr.setCursor(xOff+(sl.sent?12:4),ty);
        spr.print(sl.sent?"> ":"< "); spr.print(sl.buf);
    }
    if (serScroll>0) {
        spr.setTextColor(TH->muted,TH->bg);
        char sbuf[8]; snprintf(sbuf,sizeof(sbuf),"+%d^",serScroll);
        spr.setCursor(xOff+W-36,yOff+6); spr.print(sbuf);
    }
    spr.drawFastHLine(xOff,yOff+H-20,W,TH->divider);
    spr.setTextColor(TH->dim,TH->bg);
    spr.setCursor(xOff+4,yOff+H-14); spr.print(L.ser_hint);
}

void inputSerial(AxisInputEvent ev, void*) {
    g.lastActivityMs=millis();
    if (ev==AXIS_INPUT_UP)   { if(serScroll+SER_VISIBLE<serCount) serScroll++; }
    else if (ev==AXIS_INPUT_DOWN) { if(serScroll>0) serScroll--; }
}

// ─────────────────────────────────────────────────
//  SCR_CUBE — 旋转立方体
// ─────────────────────────────────────────────────
void drawCube(LGFX_Sprite& spr, int16_t xOff, int16_t yOff, void*) {
    spr.fillScreen(AXIS_C_BG);
    int16_t W=spr.width(), H=spr.height();
    spr.drawFastHLine(xOff,yOff,  W,AXIS_C_ACCENT2);
    spr.drawFastHLine(xOff,yOff+1,W,AXIS_C_ACCENT2);
    SET_F1(spr); spr.setTextColor(AXIS_C_ACCENT2,AXIS_C_BG);
    DST(spr,xOff+4,yOff+6,L.title_cube);
    float t=millis()/1000.0f;
    drawWireCube(spr,xOff+W/2,yOff+H/2,t*0.7f,t*1.1f,55,AXIS_C_ACCENT2);
    static uint32_t lastT=0; static uint8_t fps=0,cnt=0; cnt++;
    if (millis()-lastT>=1000){fps=cnt;cnt=0;lastT=millis();}
    char fb[12]; snprintf(fb,sizeof(fb),"%dfps",fps);
    spr.setTextColor(AXIS_C_DIM,AXIS_C_BG);
    spr.setCursor(xOff+W-36,yOff+6); spr.print(fb);
    spr.drawFastHLine(xOff,yOff+H-20,W,AXIS_C_DIVIDER);
    spr.setTextColor(AXIS_C_DIM,AXIS_C_BG);
    DST(spr,xOff+4,yOff+H-14,L.back);
}

// ─────────────────────────────────────────────────
//  SCR_LEVEL — 水平仪（双模式）
// ─────────────────────────────────────────────────
static void drawLevelFlat(LGFX_Sprite& spr, int16_t xOff, int16_t yOff) {
    const int16_t W=spr.width();
    const int16_t cx=xOff+W/2, cy=yOff+120;
    const int16_t R=70, Ri=14, Rb=9;
    float bxf=g.roll/40.0f*(R-Rb), byf=g.pitch/40.0f*(R-Rb);
    float dist=sqrtf(bxf*bxf+byf*byf);
    if (dist>(R-Rb)){float s=(R-Rb)/dist; bxf*=s; byf*=s;}
    int16_t bx=cx+(int16_t)bxf, by=cy+(int16_t)byf;
    bool isLevel=(dist<Ri);
    uint16_t gridC=0x18C3;
    spr.drawFastHLine(cx-R+2,cy,(R-2)*2,gridC);
    spr.drawFastVLine(cx,cy-R+2,(R-2)*2,gridC);
    spr.drawCircle(cx,cy,R,AXIS_C_DIVIDER);
    spr.drawCircle(cx,cy,R*2/3,0x2945);
    spr.drawCircle(cx,cy,R*1/3,0x2945);
    uint16_t ringC=isLevel?AXIS_C_OK:0x39E7;
    spr.drawCircle(cx,cy,Ri,ringC); spr.fillCircle(cx,cy,2,ringC);
    for (int a=0;a<8;a++){
        float ang=a*3.14159f/4.0f;
        int16_t tx=cx+(int16_t)((R-4)*cosf(ang)), ty=cy+(int16_t)((R-4)*sinf(ang));
        spr.fillRect(tx-1,ty-1,3,3,AXIS_C_MUTED);
    }
    uint16_t bc=isLevel?AXIS_C_OK:AXIS_C_ACCENT;
    if (isLevel){
        float pulse=0.5f+0.5f*sinf(millis()/333.0f);
        uint8_t alpha=(uint8_t)(pulse*60);
        spr.fillCircle(bx,by,Rb+5,spr.color565(0,alpha,0));
        spr.fillCircle(bx,by,Rb+3,AXIS_C_OK);
    } else {
        spr.fillCircle(bx,by,Rb+4,0x7220);
    }
    spr.fillCircle(bx,by,Rb,bc);
    spr.fillCircle(bx,by,Rb-4,0xFFFF);
    spr.drawFastHLine(xOff,yOff+200,W,AXIS_C_DIVIDER);
    RESET_F(spr);
    char rbuf[10],pbuf[10];
    snprintf(rbuf,sizeof(rbuf),"%+.1f",g.roll);
    snprintf(pbuf,sizeof(pbuf),"%+.1f",g.pitch);
    spr.setTextColor(isLevel?AXIS_C_OK:AXIS_C_ACCENT,AXIS_C_BG);
    spr.setCursor(xOff+8,yOff+206); spr.print("R "); spr.print(rbuf);
    spr.setCursor(xOff+W/2+8,yOff+206); spr.print("P "); spr.print(pbuf);
}

static void drawLevelVertical(LGFX_Sprite& spr, int16_t xOff, int16_t yOff, float angle) {
    const int16_t W=spr.width(), H=spr.height();
    const int16_t cx=xOff+W/2, ry=yOff+H/2;
    bool isLevel=(fabsf(angle)<1.5f);
    uint16_t col=isLevel?AXIS_C_OK:(fabsf(angle)<10.0f?AXIS_C_ACCENT:AXIS_C_WARN);
    const float PPD=6.0f;
    for (int deg=-20;deg<=20;deg++){
        float tx=cx+(deg-angle)*PPD;
        if (tx<xOff+1||tx>xOff+W-1) continue;
        int16_t ix=(int16_t)tx;
        bool isZero=(deg==0), isFive=(deg%5==0);
        int16_t th=isZero?36:(isFive?24:12);
        uint16_t tc=isZero?AXIS_C_WHITE:(isFive?AXIS_C_MUTED:AXIS_C_DIVIDER);
        spr.drawFastVLine(ix,ry-th/2,th,tc);
        if (isFive&&!isZero){
            char lb[5]; snprintf(lb,sizeof(lb),"%d",abs(deg));
            RESET_F(spr); spr.setTextFont(1); spr.setTextColor(AXIS_C_MUTED,AXIS_C_BG);
            spr.setTextDatum(4); spr.drawString(lb,ix,ry+30); spr.setTextDatum(0);
        }
    }
    spr.drawFastHLine(xOff,ry,W,0x2104);
    spr.drawFastHLine(xOff,ry-1,W,0x2104);
    for (int i=0;i<8;i++) spr.drawFastHLine(cx-i,ry-16-(7-i),i*2+1,col);
    for (int i=0;i<8;i++) spr.drawFastHLine(cx-i,ry+16+i,i*2+1,col);
    spr.drawFastVLine(cx,ry-16,32,col);
    if (isLevel){
        float pulse=0.4f+0.4f*sinf(millis()/400.0f);
        uint8_t gv=(uint8_t)(pulse*60);
        spr.drawCircle(cx,ry,10,spr.color565(0,gv,0));
        spr.drawCircle(cx,ry,16,spr.color565(0,gv/2,0));
    }
    RESET_F(spr); spr.setTextDatum(4);
    if (isLevel){
        spr.setTextFont(2); spr.setTextColor(AXIS_C_OK,AXIS_C_BG);
        spr.drawString("LEVEL",cx,yOff+H-60);
    } else {
        char abuf[12]; snprintf(abuf,sizeof(abuf),"%+.1f",angle);
        spr.setTextFont(4); spr.setTextColor(col,AXIS_C_BG);
        spr.drawString(abuf,cx,yOff+H-65);
        spr.setTextFont(1); spr.setTextColor(AXIS_C_MUTED,AXIS_C_BG);
        spr.drawString("deg",cx,yOff+H-48);
    }
    spr.setTextDatum(0);
    spr.drawFastHLine(xOff,yOff+H-20,W,AXIS_C_DIVIDER);
    spr.setTextFont(1); spr.setTextColor(AXIS_C_DIM,AXIS_C_BG);
    DST(spr,xOff+4,yOff+H-14,L.back);
}

void drawLevel(LGFX_Sprite& spr, int16_t xOff, int16_t yOff, void*) {
    spr.fillScreen(AXIS_C_BG);
    int16_t W=spr.width(), H=spr.height();
    bool vertical=(fabsf(g.pitch)>35.0f||fabsf(g.roll)>35.0f);
    if (vertical){
        float angle=(fabsf(g.roll)>fabsf(g.pitch))?g.roll:g.pitch;
        drawLevelVertical(spr,xOff,yOff,angle);
    } else {
        uint16_t barC=AXIS_C_OK;
        spr.drawFastHLine(xOff,yOff,W,barC); spr.drawFastHLine(xOff,yOff+1,W,barC);
        SET_F1(spr); spr.setTextColor(barC,AXIS_C_BG);
        DST(spr,xOff+4,yOff+6,L.title_level);
        drawLevelFlat(spr,xOff,yOff+18);
        spr.drawFastHLine(xOff,yOff+H-20,W,AXIS_C_DIVIDER);
        spr.setTextColor(AXIS_C_DIM,AXIS_C_BG);
        DST(spr,xOff+4,yOff+H-14,L.back);
    }
}


// ─────────────────────────────────────────────────
//  SCR_COMPASS — 指南针
// ─────────────────────────────────────────────────
static const char* headingLabel(float h) {
    if (h<22.5f||h>=337.5f) return "N";
    if (h<67.5f)  return "NE"; if (h<112.5f) return "E";
    if (h<157.5f) return "SE"; if (h<202.5f) return "S";
    if (h<247.5f) return "SW"; if (h<292.5f) return "W";
    return "NW";
}

void drawCompass(LGFX_Sprite& spr, int16_t xOff, int16_t yOff, void*) {
    spr.fillScreen(AXIS_C_BG);
    int16_t W=spr.width(), H=spr.height();
    spr.drawFastHLine(xOff,yOff,W,AXIS_C_BLUE);
    spr.drawFastHLine(xOff,yOff+1,W,AXIS_C_BLUE);
    SET_F1(spr); spr.setTextColor(AXIS_C_BLUE,AXIS_C_BG);
    DST(spr,xOff+4,yOff+6,L.title_compass);
    char db[8]; snprintf(db,sizeof(db),"%3.0f",g.heading);
    RESET_F(spr); spr.setTextColor(AXIS_C_TEXT,AXIS_C_BG);
    spr.setCursor(xOff+W-30,yOff+6); spr.print(db);

    const int16_t cx=xOff+W/2, cy=yOff+H/2-10, R=80;
    spr.drawCircle(cx,cy,R,AXIS_C_DIVIDER);
    spr.drawCircle(cx,cy,R-1,AXIS_C_CARD);
    for (int deg=0;deg<360;deg+=30){
        float rad=deg*M_PI/180.0f, s=sinf(rad), c=cosf(rad);
        bool card=(deg%90==0); int16_t r0=card?R-14:R-8;
        spr.drawLine(cx+(int16_t)(s*r0),cy-(int16_t)(c*r0),
                     cx+(int16_t)(s*(R-1)),cy-(int16_t)(c*(R-1)),
                     card?AXIS_C_MUTED:AXIS_C_DIVIDER);
    }
    struct { const char* l; float d; uint16_t c; } dirs[]={
        {"N",0,AXIS_C_DANGER},{"E",90,AXIS_C_MUTED},
        {"S",180,AXIS_C_MUTED},{"W",270,AXIS_C_MUTED}
    };
    for (auto& d:dirs){
        float rad=d.d*M_PI/180.0f;
        RESET_F(spr); spr.setTextColor(d.c,AXIS_C_BG);
        spr.setCursor(cx+(int16_t)(sinf(rad)*(R-26))-3,cy-(int16_t)(cosf(rad)*(R-26))-4);
        spr.print(d.l);
    }
    float hr=g.heading*M_PI/180.0f, s=sinf(hr), c=cosf(hr);
    spr.drawLine(cx,cy,cx-(int16_t)(s*20),cy+(int16_t)(c*20),AXIS_C_DIM);
    spr.drawLine(cx,cy,cx+(int16_t)(s*(R-18)),cy-(int16_t)(c*(R-18)),AXIS_C_ACCENT);
    spr.drawLine(cx,cy,cx+(int16_t)(s*(R-18))+1,cy-(int16_t)(c*(R-18)),AXIS_C_ACCENT);
    spr.fillCircle(cx,cy,4,AXIS_C_CARD); spr.drawCircle(cx,cy,4,AXIS_C_ACCENT);

    spr.drawFastHLine(xOff,yOff+H-36,W,AXIS_C_DIVIDER);
    RESET_F(spr);
    char buf[28]; snprintf(buf,sizeof(buf),"HDG %.1f  %s",g.heading,headingLabel(g.heading));
    spr.setTextColor(AXIS_C_ACCENT,AXIS_C_BG);
    spr.setCursor(xOff+4,yOff+H-28); spr.print(buf);
    spr.drawFastHLine(xOff,yOff+H-20,W,AXIS_C_DIVIDER);
    spr.setTextColor(AXIS_C_DIM,AXIS_C_BG);
    DST(spr,xOff+4,yOff+H-14,L.back);
}

// ─────────────────────────────────────────────────
//  SCR_INFO — 系统信息
// ─────────────────────────────────────────────────
void drawInfo(LGFX_Sprite& spr, int16_t xOff, int16_t yOff, void*) {
    spr.fillScreen(AXIS_C_BG);
    int16_t W=spr.width(), H=spr.height();
    spr.drawFastHLine(xOff,yOff,W,AXIS_C_ACCENT);
    spr.drawFastHLine(xOff,yOff+1,W,AXIS_C_ACCENT);
    SET_F1(spr); spr.setTextColor(AXIS_C_ACCENT,AXIS_C_BG);
    DST(spr,xOff+4,yOff+6,L.title_info);
    spr.drawFastHLine(xOff,yOff+18,W,AXIS_C_DIVIDER);

    struct Row { const char* label; const char* value; uint16_t col; };
    Row rows[]={
        {L.info_board,  "ESP32-P4-Pico",    AXIS_C_TEXT   },
        {L.info_ui,     "AXIS-UI v0.2",     AXIS_C_ACCENT },
        {L.info_screen, "240x320 SPI TFT",  AXIS_C_MUTED  },
        {L.info_lib,    "LovyanGFX",        AXIS_C_MUTED  },
        {L.info_phase,  L.info_phase_val,   AXIS_C_ACCENT2},
    };
    static ScrollState ss[5];
    for (int i=0;i<5;i++){
        int16_t ry=yOff+22+i*24;
        spr.drawFastHLine(xOff,ry-1,W,AXIS_C_DIVIDER);
        SET_F1(spr); spr.setTextColor(AXIS_C_DIM,AXIS_C_BG);
        DST(spr,xOff+4,ry+7,rows[i].label);
        RESET_F(spr);
        drawScrollText(spr,xOff+72,ry+7,W-76,rows[i].value,rows[i].col,ss[i]);
    }
    spr.drawFastHLine(xOff,yOff+H-20,W,AXIS_C_DIVIDER);
    spr.setTextColor(AXIS_C_DIM,AXIS_C_BG);
    DST(spr,xOff+4,yOff+H-14,L.back);
}

// ─────────────────────────────────────────────────
//  SCR_SCREENSAVER — 飞越几何体
// ─────────────────────────────────────────────────
#define SS_N 8
static SSShape ssShapes[SS_N];

static uint16_t ssColorAt(float z){
    if (z>10.0f) return AXIS_C_DIVIDER;
    if (z>6.0f)  return AXIS_C_DIM;
    if (z>3.0f)  return AXIS_C_MUTED;
    return AXIS_C_ACCENT;
}
static void ssRespawn(SSShape& s, float baseZ){
    s.z=(float)random(0,60)/10.0f+baseZ;
    s.x=(float)random(-60,60)/10.0f; s.y=(float)random(-60,60)/10.0f;
    s.sz=0.5f+(float)random(5,30)/10.0f;
    s.vrx=(float)random(4,28)/1000.0f*(random(0,2)?1.0f:-1.0f);
    s.vry=(float)random(4,28)/1000.0f*(random(0,2)?1.0f:-1.0f);
}
static void initScreensaver(){
    for (int i=0;i<SS_N;i++){
        ssShapes[i].rx=(float)random(0,628)/100.0f;
        ssShapes[i].ry=(float)random(0,628)/100.0f;
        ssRespawn(ssShapes[i],1.5f+(float)i*(18.0f/SS_N));
    }
}
void drawScreensaver(LGFX_Sprite& spr, int16_t xOff, int16_t yOff, void*){
    spr.fillScreen(0x0000);
    const int16_t CX=xOff+spr.width()/2, CY=yOff+spr.height()/2;
    const float FOV=80.0f, SCALE=20.0f, SPEED=0.055f;
    for (int i=0;i<SS_N;i++){
        SSShape& s=ssShapes[i];
        s.z-=SPEED; s.rx+=s.vrx; s.ry+=s.vry;
        if (s.z<0.4f) ssRespawn(s,18.0f);
        float inv_z=1.0f/s.z;
        int16_t sx=CX+(int16_t)(s.x*FOV*inv_z);
        int16_t sy=CY+(int16_t)(s.y*FOV*inv_z);
        float sc=s.sz*SCALE*inv_z;
        if (sc<2.0f||sc>90.0f) continue;
        drawWireCube(spr,sx,sy,s.rx,s.ry,sc,ssColorAt(s.z));
    }
}
void inputScreensaver(AxisInputEvent ev, void*){
    (void)ev;
    g.ssActive=false; g.lastActivityMs=millis();
    ui.goHome();
}

// ─────────────────────────────────────────────────
//  SCR_SETTINGS
// ─────────────────────────────────────────────────
static int8_t settingsCursor=0;

void drawSettings(LGFX_Sprite& spr, int16_t xOff, int16_t yOff, void*){
    spr.fillScreen(AXIS_C_BG);
    int16_t W=spr.width(), H=spr.height();
    spr.drawFastHLine(xOff,yOff,W,AXIS_C_ACCENT);
    spr.drawFastHLine(xOff,yOff+1,W,AXIS_C_ACCENT);
    SET_F1(spr); spr.setTextColor(AXIS_C_ACCENT,AXIS_C_BG);
    DST(spr,xOff+4,yOff+6,L.title_settings);
    spr.drawFastHLine(xOff,yOff+18,W,AXIS_C_DIVIDER);

    // 每行高 32px
    auto drawRow = [&](int16_t ry, const char* label, auto drawVal){
        spr.drawFastHLine(xOff,ry-1,W,AXIS_C_DIVIDER);
        SET_F1(spr); spr.setTextColor(AXIS_C_DIM,AXIS_C_BG);
        DST(spr,xOff+4,ry+10,label);
        drawVal(ry);
    };

    // 语言
    drawRow(yOff+22, L.set_lang, [&](int16_t ry){
        RESET_F(spr);
        spr.setTextColor(g.langChoice==0?AXIS_C_ACCENT:AXIS_C_DIM,AXIS_C_BG);
        spr.setCursor(xOff+100,ry+10); spr.print("EN");
        spr.setTextColor(AXIS_C_MUTED,AXIS_C_BG); spr.setCursor(xOff+118,ry+10); spr.print("/");
        SET_F1(spr); spr.setTextColor(g.langChoice==1?AXIS_C_ACCENT:AXIS_C_DIM,AXIS_C_BG);
        DST(spr,xOff+128,ry+10,L.set_lang_zh);
    });
    // 重力光标
    drawRow(yOff+54, L.set_gyro, [&](int16_t ry){
        spr.setTextColor(g.gyroEn?AXIS_C_OK:AXIS_C_MUTED,AXIS_C_BG);
        DST(spr,xOff+160,ry+10,g.gyroEn?L.set_on:L.set_off);
    });
    // 主题
    drawRow(yOff+86, "THEME", [&](int16_t ry){
        RESET_F(spr);
        spr.setTextColor(!g.themeLight?AXIS_C_ACCENT:AXIS_C_DIM,AXIS_C_BG);
        spr.setCursor(xOff+100,ry+10); spr.print("DARK");
        spr.setTextColor(AXIS_C_MUTED,AXIS_C_BG); spr.setCursor(xOff+130,ry+10); spr.print("/");
        spr.setTextColor(g.themeLight?AXIS_C_ACCENT2:AXIS_C_DIM,AXIS_C_BG);
        spr.setCursor(xOff+142,ry+10); spr.print("LIGHT");
    });
    // 调试
    drawRow(yOff+118, "DEBUG", [&](int16_t ry){
        RESET_F(spr);
        spr.setTextColor((g.debugSerial||g.debugFPS)?AXIS_C_WARN:AXIS_C_MUTED,AXIS_C_BG);
        spr.setCursor(xOff+100,ry+10);
        spr.print((g.debugSerial||g.debugFPS)?"ON  >":"OFF >");
    });
    // 屏保
    drawRow(yOff+150, "SLEEP", [&](int16_t ry){
        RESET_F(spr);
        static const char* ssLabels[]={"OFF","30s","1m","5m"};
        spr.setTextColor(g.ssTimeoutOpt?AXIS_C_ACCENT:AXIS_C_MUTED,AXIS_C_BG);
        spr.setCursor(xOff+160,ry+10);
        spr.print(ssLabels[constrain(g.ssTimeoutOpt,0,3)]);
    });
    // 导航模式
    drawRow(yOff+182, "NAV", [&](int16_t ry){
        RESET_F(spr);
        spr.setTextColor(g.navMode==0?AXIS_C_ACCENT:AXIS_C_DIM,AXIS_C_BG);
        spr.setCursor(xOff+100,ry+10); spr.print("JOY");
        spr.setTextColor(AXIS_C_MUTED,AXIS_C_BG); spr.setCursor(xOff+124,ry+10); spr.print("/");
        spr.setTextColor(g.navMode==1?AXIS_C_ACCENT:AXIS_C_DIM,AXIS_C_BG);
        spr.setCursor(xOff+136,ry+10); spr.print("CUR");
    });

    spr.drawFastHLine(xOff,yOff+H-28,W,AXIS_C_DIVIDER);
    spr.setTextColor(AXIS_C_MUTED,AXIS_C_BG);
    spr.setCursor(xOff+4,yOff+H-20); spr.print(L.set_save_cancel);

    // 箭头
    static const int16_t arrowYs[6]={32,64,96,128,160,192};
    spr.setTextColor(AXIS_C_ACCENT,AXIS_C_BG);
    RESET_F(spr); spr.setCursor(xOff+W-14,yOff+arrowYs[constrain(settingsCursor,0,5)]); spr.print(">");
}

void inputSettings(AxisInputEvent ev, void*){
    g.lastActivityMs=millis();
    if (ev==AXIS_INPUT_UP)   {if(settingsCursor>0) settingsCursor--; return;}
    if (ev==AXIS_INPUT_DOWN) {if(settingsCursor<5) settingsCursor++; return;}
    if (settingsCursor==0){
        if (ev==AXIS_INPUT_LEFT||ev==AXIS_INPUT_OK) g.langChoice=0;
        else if (ev==AXIS_INPUT_RIGHT) g.langChoice=1;
    } else if (settingsCursor==1){
        if (ev==AXIS_INPUT_LEFT||ev==AXIS_INPUT_RIGHT||ev==AXIS_INPUT_OK) g.gyroEn=!g.gyroEn;
    } else if (settingsCursor==2){
        if (ev==AXIS_INPUT_LEFT||ev==AXIS_INPUT_RIGHT||ev==AXIS_INPUT_OK) g.themeLight=!g.themeLight;
    } else if (settingsCursor==3){
        if (ev==AXIS_INPUT_OK||ev==AXIS_INPUT_RIGHT) ui.goTo(SCR_DEBUG,AXIS_TRANS_SLIDE_LEFT);
    } else if (settingsCursor==4){
        if (ev==AXIS_INPUT_LEFT)  g.ssTimeoutOpt=(g.ssTimeoutOpt+3)%4;
        if (ev==AXIS_INPUT_RIGHT||ev==AXIS_INPUT_OK) g.ssTimeoutOpt=(g.ssTimeoutOpt+1)%4;
    } else {
        // NAV MODE
        if (ev==AXIS_INPUT_LEFT||ev==AXIS_INPUT_RIGHT||ev==AXIS_INPUT_OK) {
            g.navMode = g.navMode ? 0 : 1;
            ui.setNavMode((AxisNavMode)g.navMode);
        }
    }
    if (ev==AXIS_INPUT_OK_LONG){  // 长按 = 保存并返回
        applyTheme();
        ui.setLanguage((AxisLang)g.langChoice);
        ui.setNavMode((AxisNavMode)g.navMode);
        rebuildMenu(); rebuildNodes(); refreshTickerContent();
        prefs.begin("axis_ui",false);
        prefs.putUChar("lang",g.langChoice); prefs.putBool("gyro",g.gyroEn);
        prefs.putBool("light",g.themeLight); prefs.putUChar("ss_opt",g.ssTimeoutOpt);
        prefs.putUChar("nav_mode",g.navMode);
        prefs.end();
        ui.notify(L.saved,AXIS_C_OK,1500);
        ui.goBack(AXIS_TRANS_SLIDE_RIGHT);
    }
}

// ─────────────────────────────────────────────────
//  SCR_DEBUG
// ─────────────────────────────────────────────────
static int8_t debugCursor=0;

void drawDebug(LGFX_Sprite& spr, int16_t xOff, int16_t yOff, void*){
    spr.fillScreen(AXIS_C_BG);
    int16_t W=spr.width(), H=spr.height();
    spr.drawFastHLine(xOff,yOff,W,AXIS_C_WARN);
    spr.drawFastHLine(xOff,yOff+1,W,AXIS_C_WARN);
    RESET_F(spr); spr.setTextColor(AXIS_C_WARN,AXIS_C_BG);
    spr.setCursor(xOff+4,yOff+6); spr.print("DEBUG MODE");
    spr.drawFastHLine(xOff,yOff+18,W,AXIS_C_DIVIDER);

    int16_t r1=yOff+22;
    spr.drawFastHLine(xOff,r1-1,W,AXIS_C_DIVIDER);
    spr.setTextColor(AXIS_C_DIM,AXIS_C_BG); spr.setCursor(xOff+4,r1+10); spr.print("SERIAL LOG");
    spr.setTextColor(g.debugSerial?AXIS_C_OK:AXIS_C_MUTED,AXIS_C_BG);
    spr.setCursor(xOff+140,r1+10); spr.print(g.debugSerial?"ON":"OFF");

    int16_t r2=yOff+54;
    spr.drawFastHLine(xOff,r2-1,W,AXIS_C_DIVIDER);
    spr.setTextColor(AXIS_C_DIM,AXIS_C_BG); spr.setCursor(xOff+4,r2+10); spr.print("FPS OVERLAY");
    spr.setTextColor(g.debugFPS?AXIS_C_OK:AXIS_C_MUTED,AXIS_C_BG);
    spr.setCursor(xOff+140,r2+10); spr.print(g.debugFPS?"ON":"OFF");

    spr.drawFastHLine(xOff,yOff+H-28,W,AXIS_C_DIVIDER);
    spr.setTextColor(AXIS_C_MUTED,AXIS_C_BG);
    spr.setCursor(xOff+4,yOff+H-20); spr.print("[A]SAVE  [B]BACK");

    static const int16_t arrowYs[2]={32,64};
    spr.setTextColor(AXIS_C_WARN,AXIS_C_BG);
    spr.setCursor(xOff+W-14,yOff+arrowYs[constrain(debugCursor,0,1)]); spr.print(">");
}

void inputDebug(AxisInputEvent ev, void*){
    g.lastActivityMs=millis();
    if (ev==AXIS_INPUT_UP)   {if(debugCursor>0) debugCursor--; return;}
    if (ev==AXIS_INPUT_DOWN) {if(debugCursor<1) debugCursor++; return;}
    if (ev==AXIS_INPUT_LEFT||ev==AXIS_INPUT_RIGHT||ev==AXIS_INPUT_OK){
        if (debugCursor==0) g.debugSerial=!g.debugSerial;
        else                g.debugFPS=!g.debugFPS;
    }
    if (ev==AXIS_INPUT_OK_LONG){  // 长按 = 保存并返回
        ui.setCustomFlag(g.debugSerial||g.debugFPS,AXIS_C_WARN);
        ui.setFPSOverlay(g.debugFPS);
        prefs.begin("axis_ui",false);
        prefs.putBool("dbg_ser",g.debugSerial); prefs.putBool("dbg_fps",g.debugFPS);
        prefs.end();
        ui.notify("SAVED",AXIS_C_OK,1200);
        ui.goBack(AXIS_TRANS_SLIDE_RIGHT);
    }
}


// ─────────────────────────────────────────────────
//  HOME / Player 输入
// ─────────────────────────────────────────────────
void inputHome(AxisInputEvent ev, void*){
    g.lastActivityMs=millis();
    if (ev==AXIS_INPUT_OK &&
        ui.currentScreen()==AXIS_SCR_HOME &&
        ui.nodeCursor()<0)
        ui.showMenu(mainMenu,MENU_COUNT);
}

void inputPlayer(AxisInputEvent ev, void*){
    g.lastActivityMs=millis();
    switch(ev){
        case AXIS_INPUT_OK:
            g.playing=!g.playing; ui.setPlaying(g.playing);
            ui.notify(g.playing?L.ply_play:L.ply_stop,
                      g.playing?AXIS_C_ACCENT:AXIS_C_MUTED,1500); break;
        case AXIS_INPUT_UP:
            g.volume=min(100,(int)g.volume+5); ui.setVolume(g.volume); break;
        case AXIS_INPUT_DOWN:
            g.volume=max(0,(int)g.volume-5); ui.setVolume(g.volume); break;
        case AXIS_INPUT_LEFT:
            g.progress=0; ui.setProgress(0);
            ui.notify(L.ply_prev,AXIS_C_ACCENT,1000); break;
        case AXIS_INPUT_RIGHT:
            g.progress=0; ui.setProgress(0);
            ui.notify(L.ply_next,AXIS_C_ACCENT,1000); break;
        case AXIS_INPUT_OK_LONG:
            ui.goBack(AXIS_TRANS_SLIDE_RIGHT); break;
        default: break;
    }
}

// ─────────────────────────────────────────────────
//  传感器 & 进度更新
// ─────────────────────────────────────────────────
void updateSensors(){
    static uint32_t lastEnv=0,lastLevel=0,lastHdg=0,lastGreet=0;
    if (millis()-lastLevel>=50)   {lastLevel=millis(); readMPU6050();}
    if (millis()-lastHdg  >=80)   {lastHdg=millis();   readGY271();}
    if (millis()-lastEnv  >=2000) {
        lastEnv=millis();
        if (!mpuOk){
            g.temp+=random(-10,10)/100.0f;
            g.humidity+=random(-5,5)/100.0f;
            g.co2=400+random(0,50);
        }
        AxisSensorData d;
        d.temp=g.temp; d.humidity=g.humidity; d.pressure=g.pressure;
        d.co2=g.co2; d.tvoc=g.tvoc; d.valid=true;
        ui.setSensorData(d);
        ui.setWalking(g.isWalking);
    }
    if (millis()-lastGreet>=60000){lastGreet=millis(); refreshTickerContent();}
}

void updateProgress(){
    static uint32_t last=0;
    if (!g.playing||millis()-last<1000) return;
    last=millis();
    g.progress+=1.0f/210.0f;
    if (g.progress>1.0f) g.progress=0;
    ui.setProgress(g.progress);
}

// ─────────────────────────────────────────────────
//  C3 副屏通信（Serial1 UART）
//  P4 → C3: 每500ms推送状态行
//  C3 → P4: 暂不处理（单向推送）
// ─────────────────────────────────────────────────
static void updateSubDisplay(){
    static uint32_t lastMs=0;
    if (millis()-lastMs<500) return;
    lastMs=millis();
    Serial1.printf("TIME:%s\n", g.timeStr.c_str());
    Serial1.printf("BATT:%d\n", g.battery);
    Serial1.printf("WALK:%d\n", (int)g.isWalking);
    Serial1.printf("HDG:%.0f\n", g.heading);
    Serial1.printf("TEMP:%.1f\n", g.temp);
}

// ─────────────────────────────────────────────────
//  开机动画
// ─────────────────────────────────────────────────
static void runBootAnimation(){
    tft.setTextWrap(false);
    tft.fillScreen(0x0000);
    tft.setTextFont(1);

    struct BootLine { const char* txt; uint16_t col; uint16_t ms; };
    static const BootLine msgs[]={
        {"[0.000] SOLITON v1",    0x07E0, 80 },
        {"[0.001] CPU: ESP32-P4", 0x07E0, 60 },
        {"[0.012] SPI2 40MHz",    0x07E0, 90 },
#ifdef TFT_ILI9341
        {"[0.023] ILI9341 init",  0x07E0,120 },
#else
        {"[0.023] ST7789 init",   0x07E0,120 },
#endif
        {"[0.034] LovyanGFX ok",  0x07E0, 80 },
        {"[0.045] I2C SDA7 SCL8", 0x07E0, 70 },
        {"[0.050] UART1 C3 init", 0x07E0, 60 },
        {"[0.056] MPU-6050...",   0xFFE0,180 },
        {"[0.067] GY-271...",     0xFFE0,200 },
        {"[0.089] sensors OK",    0x07E0, 90 },
        {"[0.095] NVS loaded",    0x07E0, 60 },
        {"[0.100] AXIS_UI init",  0x07E0,100 },
        {"[0.111] boot complete", 0xFFFF,300 },
    };
    const uint8_t MSG_N=sizeof(msgs)/sizeof(msgs[0]);
    for (uint8_t i=0;i<MSG_N;i++){
        tft.setTextColor(msgs[i].col,0x0000);
        tft.setCursor(4,4+i*11);
        tft.print(msgs[i].txt);
        delay(msgs[i].ms);
    }
    delay(200);

    // 旋转立方体 + 进度条
    float rx=0.3f, ry=0.5f;
    uint32_t startMs=millis();
    const uint32_t CUBE_MS=2000;
    while (millis()-startMs<CUBE_MS){
        float prog=(float)(millis()-startMs)/(float)CUBE_MS;
        rx+=0.035f; ry+=0.055f;
        tft.fillScreen(0x0000);
        tft.setTextFont(2); tft.setTextColor(AXIS_C_ACCENT,0x0000);
        tft.setTextDatum(4); tft.drawString("SOLITON",120,16); tft.setTextDatum(0);

        // 大立方体（cx=120, cy=150, scale=55）
        int16_t px[8],py[8];
        for (int v=0;v<8;v++){
            float x=CV[v][0],y=CV[v][1],z=CV[v][2];
            float tx2=x*cosf(ry)+z*sinf(ry), tz=-x*sinf(ry)+z*cosf(ry); x=tx2; z=tz;
            float ty2=y*cosf(rx)-z*sinf(rx), tz2=y*sinf(rx)+z*cosf(rx); y=ty2; z=tz2;
            float s=55.0f*3.5f/(3.5f+z);
            px[v]=(int16_t)(120+x*s); py[v]=(int16_t)(160+y*s);
        }
        for (int e=0;e<12;e++)
            tft.drawLine(px[CE[e][0]],py[CE[e][0]],px[CE[e][1]],py[CE[e][1]],AXIS_C_ACCENT);

        // 进度条
        tft.drawRect(20,270,200,10,AXIS_C_MUTED);
        int16_t fw=(int16_t)(198.0f*prog);
        if (fw>0) tft.fillRect(21,271,fw,8,AXIS_C_ACCENT);
        char pbuf[8]; snprintf(pbuf,8,"%3d%%",(int)(prog*100.0f));
        tft.setTextFont(1); tft.setTextColor(AXIS_C_DIM,0x0000);
        tft.setTextDatum(4); tft.drawString(pbuf,120,290); tft.setTextDatum(0);
        delay(16);
    }
    tft.fillScreen(0x0000);
    delay(80);
}

// ─────────────────────────────────────────────────
//  setup / loop
// ─────────────────────────────────────────────────
void setup(){
    Serial.begin(115200);
    Serial.println("=== SOLITON v1 ===");

    // C3 副屏通信 UART（TX=22, RX=23）
    Serial1.begin(115200, SERIAL_8N1, 23, 22);

    tft.init();
    tft.setRotation(0);   // 240×320 竖屏，根据实际方向调整（0/2/4/6）

    runBootAnimation();

    // 读取 NVS
    prefs.begin("axis_ui",true);
    g.langChoice   = prefs.getUChar("lang",     0);
    g.gyroEn       = prefs.getBool ("gyro",     false);
    g.themeLight   = prefs.getBool ("light",    false);
    g.debugSerial  = prefs.getBool ("dbg_ser",  false);
    g.debugFPS     = prefs.getBool ("dbg_fps",  false);
    g.ssTimeoutOpt = prefs.getUChar("ss_opt",   2);
    g.navMode      = prefs.getUChar("nav_mode", 0);
    prefs.end();
    applyTheme();

    // AXIS_UI 初始化（setMainDisplay 必须在 begin 之前）
    ui.setMainDisplay(&tft);
    ui.setLanguage((AxisLang)g.langChoice);
    ui.setNavMode((AxisNavMode)g.navMode);

    // 轨迹球替代摇杆：joy_* 全部 -1，由 tbPoll() 注入
    // 短按=OK  长按=返回；无独立 A/B 键
    AxisPinConfig pins;
    pins.joy_up=-1; pins.joy_down=-1; pins.joy_left=-1;
    pins.joy_right=-1; pins.joy_ok=-1;
    pins.btn_a=-1;
    pins.btn_b=-1;

    if (!ui.begin(pins)){
        Serial.println("AXIS UI begin FAILED");
        tft.fillScreen(0xF800);
        while(1) delay(1000);
    }

    ui.setCustomFlag(g.debugSerial||g.debugFPS,AXIS_C_WARN);
    ui.setFPSOverlay(g.debugFPS);
    ui.setTime(g.timeStr);
    ui.setDate(g.dateStr);
    ui.setBattery(g.battery);
    ui.setVolume(g.volume);
    ui.setBTConnected(g.btConn);

    rebuildMenu();
    sensorBegin();
    tbBegin();   // 轨迹球中断初始化

    ui.registerScreen(SCR_INFO,        drawInfo,        nullptr,          nullptr, AXIS_NAVTYPE_DISCRETE);
    ui.registerScreen(SCR_SETTINGS,    drawSettings,    inputSettings,    nullptr, AXIS_NAVTYPE_DISCRETE);
    ui.registerScreen(SCR_LEVEL,       drawLevel,       nullptr,          nullptr, AXIS_NAVTYPE_AUTO);
    ui.registerScreen(SCR_COMPASS,     drawCompass,     nullptr,          nullptr, AXIS_NAVTYPE_AUTO);
    ui.registerScreen(SCR_CUBE,        drawCube,        nullptr,          nullptr, AXIS_NAVTYPE_AUTO);
    ui.registerScreen(SCR_SERIAL,      drawSerial,      inputSerial,      nullptr, AXIS_NAVTYPE_DISCRETE);
    ui.registerScreen(SCR_DEBUG,       drawDebug,       inputDebug,       nullptr, AXIS_NAVTYPE_DISCRETE);
    ui.registerScreen(SCR_SCREENSAVER, drawScreensaver, inputScreensaver, nullptr, AXIS_NAVTYPE_DISCRETE);

    ui.registerInput(AXIS_SCR_HOME,   inputHome,   nullptr);
    ui.registerInput(AXIS_SCR_PLAYER, inputPlayer, nullptr);

    rebuildNodes();

    AxisSensorData d;
    d.temp=g.temp; d.humidity=g.humidity; d.pressure=g.pressure;
    d.co2=g.co2; d.tvoc=g.tvoc; d.valid=true;
    ui.setSensorData(d);

    refreshTickerContent();
    g.lastActivityMs=millis();
    ui.goHome();
    Serial.println("SOLITON ready");
}

void loop(){
    // 串口接收（USB Serial → 串口终端）
    while (Serial.available()){
        char c=(char)Serial.read();
        if (c=='\n'||c=='\r'){
            if (serInBuf.length()>0){
                serialPushLine(serInBuf.c_str(),false);
                serInBuf="";
            }
        } else if (serInBuf.length()<SER_WIDTH) serInBuf+=c;
    }

    // 轨迹球输入注入
    // 光标模式：增量直接送 moveCursor，框架内部做物理吸附
    // 摇杆模式：离散事件送 injectInput
    if (ui.navMode() == AXIS_NAVMODE_CURSOR) {
        TbDelta d = tbPollDelta();
        if (d.dx || d.dy) {
            ui.moveCursor(d.dx, d.dy);
            g.lastActivityMs = millis();
        }
        // 按钮事件仍走离散通道
        AxisInputEvent tbEv = tbPoll();
        if (tbEv == AXIS_INPUT_OK || tbEv == AXIS_INPUT_OK_LONG) {
            ui.injectInput(tbEv);
            g.lastActivityMs = millis();
        }
    } else {
        AxisInputEvent tbEv = tbPoll();
        if (tbEv != AXIS_INPUT_NONE) {
            ui.injectInput(tbEv);
            g.lastActivityMs = millis();
        }
    }

    updateSensors();
    updateProgress();
    updateSubDisplay();

    // 重力光标（HOME 屏，MPU 有效，非走路）
    if (g.gyroEn&&mpuOk&&!g.isWalking&&
        ui.currentScreen()==AXIS_SCR_HOME&&
        !ui.isMenuOpen()&&!ui.isTransitioning()){
        static float baseRoll=0,basePitch=0;
        static bool baseSet=false,waitReturn=false;
        if (!baseSet){baseRoll=g.roll;basePitch=g.pitch;baseSet=true;}
        float dRoll=g.roll-baseRoll, dPitch=g.pitch-basePitch;
        if (waitReturn){
            if (fabsf(dRoll)<8.0f&&fabsf(dPitch)<8.0f){
                waitReturn=false; baseRoll=g.roll; basePitch=g.pitch;
            }
        } else {
            const float THR=20.0f;
            AxisInputEvent ginput=AXIS_INPUT_NONE;
            if      (dRoll > THR) ginput=AXIS_INPUT_RIGHT;
            else if (dRoll <-THR) ginput=AXIS_INPUT_LEFT;
            else if (dPitch> THR) ginput=AXIS_INPUT_DOWN;
            else if (dPitch<-THR) ginput=AXIS_INPUT_UP;
            if (ginput!=AXIS_INPUT_NONE){
                ui.injectInput(ginput);
                g.lastActivityMs=millis();
                waitReturn=true;
            }
        }
    }

    // 屏保
    {
        static const uint32_t ssTimeouts[4]={0,30000,60000,300000};
        uint32_t ssto=ssTimeouts[g.ssTimeoutOpt];
        uint32_t now=millis();
        bool flat=mpuOk&&(fabsf(g.pitch)<15.0f)&&(fabsf(g.roll)<15.0f);
        if (!g.ssActive&&ssto>0&&
            ui.currentScreen()==AXIS_SCR_HOME&&flat&&
            (now-g.lastActivityMs>ssto)){
            g.ssActive=true;
            initScreensaver();
            ui.goTo(SCR_SCREENSAVER,AXIS_TRANS_NONE);
        }
        if (g.ssActive&&mpuOk&&
            (fabsf(g.pitch)>30.0f||fabsf(g.roll)>30.0f)){
            g.ssActive=false; g.lastActivityMs=now;
            ui.goHome();
        }
    }

    ui.update();
}
