/**
 * AXIS-UI — P4 硬件测试
 *
 * 验证项目：
 *   SSD1351 128×128 SPI 彩屏
 *   MPU-6050 I2C (0x68)
 *   GY-271 磁力计 I2C (HMC5883L 0x1E / QMC5883L 0x0D)
 *   五轴摇杆 UP/DOWN/LEFT/RIGHT/OK
 *
 * 接线（ESP32-P4-Pico）：
 *   SSD1351 : SCK=15  MOSI=16  CS=17  DC=18  RST=19
 *   I2C     : SDA=7   SCL=8
 *   摇杆    : UP=2  DOWN=3  LEFT=4  RIGHT=5  OK=6
 *
 * 依赖：
 *   Adafruit SSD1351 Library
 *   Adafruit GFX Library
 */

#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1351.h>

// ── 引脚 ─────────────────────────────────────────
#define PIN_SCK     15
#define PIN_MOSI    16
#define PIN_CS      17
#define PIN_DC      18
#define PIN_RST     19

#define PIN_SDA      7
#define PIN_SCL      8

#define PIN_UP       2
#define PIN_DOWN     3
#define PIN_LEFT     4
#define PIN_RIGHT    5
#define PIN_OK       6

// ── 颜色 ─────────────────────────────────────────
#define C_BG      0x0000
#define C_ORANGE  0xFD20
#define C_WHITE   0xFFFF
#define C_GREEN   0x07E0
#define C_RED     0xF800
#define C_MUTED   0x7BCF
#define C_DIM     0x39E7

// ── 屏幕 ─────────────────────────────────────────
// 软件SPI（硬件验证用，速度慢但稳定）
Adafruit_SSD1351 tft(128, 128, PIN_CS, PIN_DC, PIN_MOSI, PIN_SCK, PIN_RST);

// ── I2C 设备状态 ──────────────────────────────────
bool mpuOk  = false;
bool gy271Ok = false;
uint8_t gy271Addr = 0;

// ── 传感器数据结构 ────────────────────────────────
struct MPUData { int16_t ax, ay, az, gx, gy, gz; };
struct MagData  { int16_t x, y, z; };

bool mpuInit() {
    Wire.beginTransmission(0x68);
    Wire.write(0x6B);  // PWR_MGMT_1
    Wire.write(0x00);  // 唤醒
    return Wire.endTransmission() == 0;
}

MPUData mpuRead() {
    MPUData d = {};
    Wire.beginTransmission(0x68);
    Wire.write(0x3B);  // ACCEL_XOUT_H
    Wire.endTransmission(false);
    Wire.requestFrom((uint8_t)0x68, (uint8_t)14);
    if (Wire.available() < 14) return d;
    d.ax = (Wire.read() << 8) | Wire.read();
    d.ay = (Wire.read() << 8) | Wire.read();
    d.az = (Wire.read() << 8) | Wire.read();
    Wire.read(); Wire.read();  // temp
    d.gx = (Wire.read() << 8) | Wire.read();
    d.gy = (Wire.read() << 8) | Wire.read();
    d.gz = (Wire.read() << 8) | Wire.read();
    return d;
}

// ── GY-271 读取 ───────────────────────────────────
// 支持 HMC5883L (0x1E) 和 QMC5883L (0x0D)

bool gy271Init(uint8_t addr) {
    if (addr == 0x1E) {
        // HMC5883L: 设置连续测量模式
        Wire.beginTransmission(0x1E);
        Wire.write(0x02); Wire.write(0x00);
        return Wire.endTransmission() == 0;
    } else {
        // QMC5883L: 连续模式 200Hz 8G 512过采样
        Wire.beginTransmission(0x0D);
        Wire.write(0x0B); Wire.write(0x01);  // reset
        Wire.endTransmission();
        Wire.beginTransmission(0x0D);
        Wire.write(0x09); Wire.write(0x1D);  // ctrl1
        return Wire.endTransmission() == 0;
    }
}

MagData gy271Read(uint8_t addr) {
    MagData d = {};
    if (addr == 0x1E) {
        Wire.beginTransmission(0x1E);
        Wire.write(0x03);
        Wire.endTransmission(false);
        Wire.requestFrom((uint8_t)0x1E, (uint8_t)6);
        if (Wire.available() < 6) return d;
        d.x = (Wire.read() << 8) | Wire.read();
        d.z = (Wire.read() << 8) | Wire.read();  // HMC883L顺序: X Z Y
        d.y = (Wire.read() << 8) | Wire.read();
    } else {
        Wire.beginTransmission(0x0D);
        Wire.write(0x00);
        Wire.endTransmission(false);
        Wire.requestFrom((uint8_t)0x0D, (uint8_t)6);
        if (Wire.available() < 6) return d;
        d.x = (Wire.read() | (Wire.read() << 8));  // QMC5883L: 低字节先
        d.y = (Wire.read() | (Wire.read() << 8));
        d.z = (Wire.read() | (Wire.read() << 8));
    }
    return d;
}

// ── 辅助：在屏幕上打印状态行 ─────────────────────
int _row = 0;
void printRow(const char* label, const char* value, uint16_t vcolor = C_WHITE) {
    int y = 10 + _row * 10;
    tft.setCursor(2, y);
    tft.setTextColor(C_MUTED);
    tft.print(label);
    tft.setCursor(50, y);
    tft.setTextColor(vcolor);
    tft.print(value);
    _row++;
}

// ── I2C 扫描 ─────────────────────────────────────
void scanI2C() {
    Serial.println("=== I2C SCAN ===");
    for (uint8_t a = 1; a < 127; a++) {
        Wire.beginTransmission(a);
        if (Wire.endTransmission() == 0) {
            Serial.printf("  Found 0x%02X", a);
            if (a == 0x68) Serial.print(" [MPU-6050]");
            if (a == 0x1E) Serial.print(" [HMC5883L]");
            if (a == 0x0D) Serial.print(" [QMC5883L]");
            Serial.println();
        }
    }
}

// ─────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println("\n=== AXIS-UI P4 HW TEST ===");

    // 软件SPI无需手动初始化SPI总线
    tft.begin();
    tft.fillScreen(C_ORANGE);
    delay(300);
    tft.fillScreen(C_BG);
    tft.setTextSize(1);
    Serial.println("SSD1351: OK");

    // I2C
    Wire.begin(PIN_SDA, PIN_SCL);
    scanI2C();

    // MPU-6050
    mpuOk = mpuInit();
    Serial.printf("MPU-6050: %s\n", mpuOk ? "OK" : "NOT FOUND");

    // GY-271（先试 HMC5883L，再试 QMC5883L）
    Wire.beginTransmission(0x1E);
    if (Wire.endTransmission() == 0) {
        gy271Addr = 0x1E;
        gy271Ok   = gy271Init(0x1E);
        Serial.println("GY-271: HMC5883L OK");
    } else {
        Wire.beginTransmission(0x0D);
        if (Wire.endTransmission() == 0) {
            gy271Addr = 0x0D;
            gy271Ok   = gy271Init(0x0D);
            Serial.println("GY-271: QMC5883L OK");
        } else {
            Serial.println("GY-271: NOT FOUND");
        }
    }

    // 摇杆输入
    int8_t jPins[] = {PIN_UP, PIN_DOWN, PIN_LEFT, PIN_RIGHT, PIN_OK};
    for (int8_t p : jPins) pinMode(p, INPUT_PULLUP);

    Serial.println("=== READY ===");
}

// ─────────────────────────────────────────────────
void loop() {
    tft.fillScreen(C_BG);
    _row = 0;

    // ── 标题 ──
    tft.drawFastHLine(0, 0, 128, C_ORANGE);
    tft.drawFastHLine(0, 1, 128, C_ORANGE);
    tft.setCursor(2, 3);
    tft.setTextColor(C_ORANGE);
    tft.print("AXIS-UI P4 TEST");
    tft.drawFastHLine(0, 9, 128, 0x2945);

    // ── 屏幕 ──
    printRow("SCREEN", "SSD1351 OK", C_GREEN);

    // ── MPU-6050 ──
    if (mpuOk) {
        MPUData m = mpuRead();
        char buf[24];
        snprintf(buf, sizeof(buf), "A %d %d %d", m.ax/1000, m.ay/1000, m.az/1000);
        printRow("MPU6050", buf, C_GREEN);
    } else {
        printRow("MPU6050", "NOT FOUND", C_RED);
    }

    // ── GY-271 ──
    if (gy271Ok) {
        MagData mg = gy271Read(gy271Addr);
        char buf[24];
        snprintf(buf, sizeof(buf), "%d %d %d", mg.x, mg.y, mg.z);
        const char* lbl = (gy271Addr == 0x1E) ? "HMC5883" : "QMC5883";
        printRow(lbl, buf, C_GREEN);
    } else {
        printRow("GY-271", "NOT FOUND", C_RED);
    }

    // ── 摇杆 ──
    tft.drawFastHLine(0, 10 + _row*10, 128, 0x2945);
    _row++;

    struct { const char* name; int pin; } joys[] = {
        {"UP",    PIN_UP},
        {"DOWN",  PIN_DOWN},
        {"LEFT",  PIN_LEFT},
        {"RIGHT", PIN_RIGHT},
        {"OK",    PIN_OK},
    };
    for (auto& j : joys) {
        bool pressed = !digitalRead(j.pin);
        printRow(j.name, pressed ? "PRESS" : "---",
                 pressed ? C_ORANGE : C_DIM);
    }

    // ── 摇杆可视化（中心十字）──
    int cx = 96, cy = 64, r = 14;
    tft.drawRect(cx-r, cy-r, r*2, r*2, 0x2945);
    if (!digitalRead(PIN_UP))    tft.fillRect(cx-3, cy-r,   6,  r/2, C_ORANGE);
    if (!digitalRead(PIN_DOWN))  tft.fillRect(cx-3, cy+r/2, 6,  r/2, C_ORANGE);
    if (!digitalRead(PIN_LEFT))  tft.fillRect(cx-r, cy-3,   r/2, 6,  C_ORANGE);
    if (!digitalRead(PIN_RIGHT)) tft.fillRect(cx+r/2, cy-3, r/2, 6,  C_ORANGE);
    if (!digitalRead(PIN_OK))    tft.fillRect(cx-3, cy-3,   6,  6,   C_ORANGE);
    else                         tft.drawRect(cx-3, cy-3,   6,  6,   C_MUTED);

    delay(80);
}
