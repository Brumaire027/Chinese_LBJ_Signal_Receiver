// wm_strings_zh.h - WiFiManager 中文语言包

#include <Arduino.h>

#ifndef _WM_STRINGS_ZH_H_
#define _WM_STRINGS_ZH_H_

#ifndef _WM_STRINGS_H_
#define _WM_STRINGS_H_

//系统必要定义
const char S_ssidpre[] PROGMEM = "esp"; // 默认热点前缀
const char S_brand[]   PROGMEM = "WiFiManager";
const char S_debugPrefix[] PROGMEM = "*wm:";
const char S_dosave[]  PROGMEM = "true";
const char S_y[]       PROGMEM = "Yes";
const char S_n[]       PROGMEM = "No";
const char S_enable[]  PROGMEM = "Enabled";
const char S_disable[] PROGMEM = "Disabled";
const char S_GET[]     PROGMEM = "GET";
const char S_POST[]    PROGMEM = "POST";
const char S_HEADER[]  PROGMEM = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: ";

// 网页CSS定义
const char HTTP_HEAD_START[] PROGMEM = "<!DOCTYPE html>" 
"<html><head><meta name='viewport' content='width=device-width,initial-scale=1,user-scalable=no'/>"
"<title>{v}</title>"
"<style>"
"body{text-align:center;font-family:verdana;background:#222;color:#fff;}"
"div,input,select{padding:5px;font-size:1em;margin:5px 0;box-sizing:border-box;}"
"input,button,select,textarea{width:100%;border-radius:5px;border:1px solid #444;background:#333;color:#fff;}"
"button{background:#28a745;color:#fff;cursor:pointer;border:0;padding:10px;font-size:1.2em;}"
"button:hover{background:#218838;}"
".c{text-align:center;} ul{list-style:none;padding:0;} li{padding:5px;margin:5px;background:#333;border-radius:5px;}"
"a{color:#fff;text-decoration:none;display:block;padding:5px;}"
".q{float:right;width:20px;text-align:right;}"
"</style></head><body>"
"<div style='text-align:left;display:inline-block;min-width:260px;'>";

const char HTTP_HEAD_END[] PROGMEM = "</div></body></html>";

// 核心界面翻译
const char S_title[] PROGMEM = "LBJ接收机配网"; 

const char S_configuration[] PROGMEM = "配置 WiFi";
const char S_information[]   PROGMEM = "设备信息";
const char S_exit[]          PROGMEM = "退出";
const char S_reboot[]        PROGMEM = "重启设备";

const char S_scan[]          PROGMEM = "扫描";
const char S_save[]          PROGMEM = "保存并连接";
const char S_back[]          PROGMEM = "返回";

const char S_ssid[]          PROGMEM = "WiFi 名称";
const char S_pass[]          PROGMEM = "WiFi 密码";
const char S_options[]       PROGMEM = "设置";

const char S_connect[]       PROGMEM = "连接";
const char S_failed[]        PROGMEM = "连接失败";
const char S_error[]         PROGMEM = "错误";

const char S_home[]          PROGMEM = "主页";

// HTML结构
const char HTTP_PORTAL_OPTIONS[] PROGMEM = 
"<form action='/wifi' method='get'><button>配置 WiFi</button></form><br/>"
"<form action='/0wifi' method='get'><button>刷新扫描</button></form><br/>"
"<form action='/i' method='get'><button>设备信息</button></form><br/>"
"<form action='/exit' method='post'><button>退出配网</button></form>";

const char HTTP_ITEM[] PROGMEM = "<div><a href='#p' onclick='c(this)'>{v}</a><div role='none' class='q {i}'>{r}%</div></div>";

// 其他补充定义
const char HTTP_SCRIPT[] PROGMEM = "<script>function c(l){document.getElementById('s').value=l.innerText||l.textContent;document.getElementById('p').focus();}</script>";
const char HTTP_STYLE[]  PROGMEM = "<style></style>";
const char HTTP_HELP[]   PROGMEM = "<br/>";
const char HTTP_FORM_START[] PROGMEM = "<form method='get' action='wifisave'><input id='s' name='s' length=32 placeholder='WiFi 名称'><br/><input id='p' name='p' length=64 type='password' placeholder='WiFi 密码'><br/>";
const char HTTP_FORM_END[]   PROGMEM = "<br/><button type='submit'>保存</button></form>";
const char HTTP_SAVED[]      PROGMEM = "<div>设置已保存<br />正在尝试连接...<br />如果连接失败，请重置设备并重试。</div>";
const char HTTP_END[]        PROGMEM = "</div></body></html>";

#endif
#endif