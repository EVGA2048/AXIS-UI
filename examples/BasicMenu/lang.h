/**
 * AXIS-UI BasicMenu — 语言配置
 * 单文件，后续可按屏幕拆分为多文件
 */
#pragma once

struct LangTable {
    // ── 菜单 ──────────────────────────────────────
    const char* menu_playing;
    const char* menu_sysinfo;
    const char* menu_settings;
    const char* menu_home;

    // ── 节点标签 & 描述 ───────────────────────────
    const char* node_level;
    const char* node_compass;
    const char* node_cube;
    const char* desc_level;
    const char* desc_compass;
    const char* desc_cube;

    // ── 屏幕标题 ──────────────────────────────────
    const char* title_level;
    const char* title_compass;
    const char* title_cube;
    const char* title_info;
    const char* title_settings;

    // ── 系统信息行 ────────────────────────────────
    const char* info_board;
    const char* info_ui;
    const char* info_screen;
    const char* info_lib;
    const char* info_phase;
    const char* info_phase_val;

    // ── 设置页 ────────────────────────────────────
    const char* set_lang;
    const char* set_lang_zh;      // "ZH" / "中文"
    const char* set_gyro;
    const char* set_on;
    const char* set_off;
    const char* set_bright;
    const char* set_rotation;
    const char* set_sleep;
    const char* set_nav_hint;
    const char* set_save_cancel;

    // ── 播放器 ────────────────────────────────────
    const char* ply_play;
    const char* ply_stop;
    const char* ply_prev;
    const char* ply_next;

    // ── 串口终端 ──────────────────────────────────
    const char* title_serial;
    const char* ser_hint;      // 底部操作提示

    // ── 通用 ──────────────────────────────────────
    const char* back;
    const char* saved;
};

// ════════════════════════════════════════════════
//  English
// ════════════════════════════════════════════════
static const LangTable L_EN = {
    // menu
    "NOW PLAYING", "SYSTEM INFO", "SETTINGS", "HOME",
    // nodes
    "LEVEL", "COMPASS", "CUBE",
    "Spirit Level", "Compass", "Cube Demo",
    // titles
    "LEVEL", "COMPASS", "CUBE  NODE-0", "SYSTEM INFO", "SETTINGS",
    // info
    "BOARD", "UI", "SCREEN", "LIB", "PHASE", "1-HW TEST",
    // settings
    "LANG", "ZH",
    "GYRO CUR", "ON", "OFF",
    "BRIGHTNESS  HIGH", "ROTATION    AUTO", "SLEEP       5MIN",
    "U/D:SEL  L/R:CHANGE", "[A]SAVE  [B]CANCEL",
    // player
    "PLAY", "STOP", "PREV", "NEXT",
    // serial
    "SERIAL", "[B]BACK U/D:SCROLL",
    // common
    "[B] BACK", "SAVED",
};

// ════════════════════════════════════════════════
//  简体中文
// ════════════════════════════════════════════════
static const LangTable L_ZH = {
    // menu
    "正在播放", "系统信息", "设置", "主页",
    // nodes
    "水平仪", "指南针", "立方体",
    "水平仪", "指南针", "立方体",
    // titles
    "水平仪", "指南针", "立方体节点", "系统信息", "设置",
    // info
    "主板", "界面", "屏幕", "图形库", "阶段", "1-硬件测试",
    // settings
    "语言", "中文",
    "重力光标", "开", "关",
    "亮度  高", "旋转  自动", "睡眠  5分钟",
    "上下选行  左右切换", "[A]保存  [B]取消",
    // player
    "播放", "停止", "上一首", "下一首",
    // serial
    "串口", "[B]返回  上下滚动",
    // common
    "[B] 返回", "已保存",
};
