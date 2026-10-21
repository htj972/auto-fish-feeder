#include "web_ui.h"
#include "app_state.h"
#include "board_config.h"
#include "duration_input.h"
#include "motor.h"
#include "network.h"
#include "oled_display.h"
#include "power_monitor.h"
#include "scheduler.h"
#include "settings_store.h"
#include <WebServer.h>

namespace {
WebServer web(80);
String token;

String durationSeconds(uint32_t ms) {
    if (ms % 1000 == 0) {
        return String(ms / 1000);
    }
    String seconds(ms / 1000.0f, 3);
    while (seconds.endsWith("0")) {
        seconds.remove(seconds.length() - 1);
    }
    return seconds;
}

String escapeHtml(String s) {
    s.replace("&", "&amp;");
    s.replace("<", "&lt;");
    s.replace(">", "&gt;");
    s.replace("\"", "&quot;");
    s.replace("'", "&#39;");
    return s;
}

String hiddenToken() {
    return "<input type='hidden' name='token' value='" + token + "'>";
}

bool authorized() {
    // 防止跨站表单直接提交；该令牌不替代局域网用户登录认证。
    if (web.arg("token") == token) {
        return true;
    }
    web.send(403, "text/plain", "Invalid token; reload page");
    return false;
}

void redirectHome() {
    web.sendHeader("Location", "/");
    web.send(303);
}

bool number(const String &s, uint32_t low, uint32_t high, uint32_t &out) {
    if (s.isEmpty() || s.length() > 8) {
        return false;
    }
    uint32_t n = 0;
    for (unsigned i = 0; i < s.length(); ++i) {
        if (s[i] < '0' || s[i] > '9') {
            return false;
        }
        n = n * 10 + s[i] - '0';
    }
    if (n < low || n > high) {
        return false;
    }
    out = n;
    return true;
}

void home() {
    time_t now = time(nullptr);
    String h =
        F("<!doctype html><html lang='zh-CN'><meta charset='utf-8'><meta name='viewport' "
          "content='width=device-width,initial-scale=1'><title>自动喂鱼器</"
          "title><style>body{font:16px system-ui;max-width:720px;margin:24px auto;padding:0 "
          "16px;background:#eff6f8;color:#183b48}section{background:white;padding:20px;margin:16px "
          "0;border-radius:12px}input,button,select{font:inherit;padding:8px;margin:4px;max-width:"
          "90%}"
          "table{width:100%}td "
          "input{width:78px}button{background:#086b79;color:white;border:0;border-radius:6px}</"
          "style><h1>自动喂鱼器</h1><section>");
    h += "<p>时间（北京时间）：" + clockText(now, "%Y-%m-%d %H:%M:%S") + "</p><p>下次任务：" +
         nextTask(now) + "</p>";
    h += String("<p>WiFi：") +
         (networkConnected() ? escapeHtml(networkSsid()) + " / " + networkAddress() : "未连接") +
         "</p>";
    h += String("<p>状态：") + (motorIsFeeding() ? "正在投喂" : "待机") + "</p><p>报警：" +
         escapeHtml(errorText) + "</p>";
    h += "<p>充电状态：" + String(chargeText()) + "（CHRG=" + String(chargeLevel()) +
         ", STDBY=" + String(chargeDoneLevel()) + "）</p>";
    if (!BATTERY_ADC_ENABLED) {
        h += "<p>电池采样禁用</p>";
    } else if (batteryIsOverrange()) {
        h += "<p>电池电压：ADC 超量程，需调整分压比例，当前数值无效。</p>";
    } else {
        h += "<p>电池电压：" + String(batteryVoltage(), 2) + " V</p>";
    }
    if (!clockReady(now)) {
        h += "<p>尚未同步网络时间，自动任务暂停；断电后需要重新联网校时。</p>";
    }
    h += "<form method='post' action='/feed'>" + hiddenToken() +
         "<button>手动投喂</button></form><form method='post' action='/stop'>" + hiddenToken() +
         "<button>立即停止</button></form></section>";
    h += "<section><h2>屏幕亮度</h2><form id='brightness-form' method='post' "
         "action='/display/brightness'>" +
         hiddenToken() +
         "<input type='hidden' name='persist' value='1'><label for='brightness'>OLED亮度 <output "
         "id='brightness-value'>" +
         String(displayBrightness()) +
         "%</output></label><br><input id='brightness' name='brightness' type='range' min='1' "
         "max='100' step='1' value='" +
         String(displayBrightness()) +
         "'><p>拖动预览，点击保存后重启仍有效。</p><button id='brightness-save'>保存亮度</button> "
         "<span id='brightness-state' role='status'></span></form></section>";
    h += "<section><h2>每日投喂</h2><p>时长单位秒（s），范围 "
         "0.1～30。相同时刻的任务只执行一项；离线期间错过的任务不补喂。</p><form method='post' "
         "action='/settings'>" +
         hiddenToken();
    h += "<input type='hidden' name='units' value='s'><label>手动时长（s） <input type='number' "
         "name='manual' min='0.1' max='30' step='0.001' required value='" +
         durationSeconds(settings.manualMs) +
         "'></label><table><tr><th>启用</th><th>时间</th><th>时长 s</th></tr>";
    for (int i = 0; i < TASK_COUNT; ++i) {
        char stamp[6];
        snprintf(stamp, sizeof(stamp), "%02u:%02u", settings.tasks[i].hour,
                 settings.tasks[i].minute);
        String id = String(i);
        h += "<tr><td><input type='checkbox' name='e" + id + "' " +
             (settings.tasks[i].enabled ? "checked" : "") + "></td><td><input type='time' name='t" +
             id + "' value='" + stamp + "' required></td><td><input type='number' name='d" + id +
             "' min='0.1' max='30' step='0.001' value='" +
             durationSeconds(settings.tasks[i].duration) + "' required></td></tr>";
    }
    h += "</table><button>保存任务</button></form></section><section><h2>WiFi 配网</h2><p>输入家庭 "
         "2.4 GHz WiFi 名称和密码。密码留空表示开放网络。连接成功后热点将在重启后关闭。</p><form "
         "method='post' action='/wifi'>" +
         hiddenToken();
    h += "<p><button type='button' id='scan'>重新扫描</button> <span id='scan-state' "
         "role='status'></span></p><label>附近WiFi <select id='networks'><option "
         "value=''>请选择，或手动输入隐藏网络</option></select></label><br><input id='ssid' "
         "name='ssid' maxlength='32' placeholder='WiFi 名称（支持手动输入）' required><input "
         "name='password' type='password' maxlength='63' placeholder='WiFi "
         "密码'><button>保存并连接</button></form></section>";
    h += R"HTML(<script>
const scanButton=document.getElementById('scan'),scanState=document.getElementById('scan-state'),networks=document.getElementById('networks');
const brightness=document.getElementById('brightness');
if(brightness){
 const value=document.getElementById('brightness-value'),status=document.getElementById('brightness-state'),saveButton=document.getElementById('brightness-save');
 let timer,queue=Promise.resolve();
 function changeBrightness(level,persist){
  queue=queue.then(async()=>{
   try{
    const form=new URLSearchParams();form.set('token',document.querySelector('input[name=token]').value);form.set('brightness',level);form.set('persist',persist?'1':'0');
    const response=await fetch('/display/brightness',{method:'POST',body:form});
    if(!response.ok)throw new Error();
    if(brightness.value===String(level))status.textContent=persist?'亮度已保存':'正在预览，点击保存后记住设置';
   }catch(e){status.textContent='亮度设置失败，请重试';}
  });
  return queue;
 }
 brightness.addEventListener('input',()=>{
  value.textContent=brightness.value+'%';status.textContent='未保存';clearTimeout(timer);
  timer=setTimeout(()=>changeBrightness(brightness.value,false),180);
 });
 document.getElementById('brightness-form').addEventListener('submit',async e=>{
  e.preventDefault();clearTimeout(timer);saveButton.disabled=true;
  await changeBrightness(brightness.value,true);saveButton.disabled=false;
 });
}
let scanTimer;
networks.addEventListener('change',()=>{if(networks.value)document.getElementById('ssid').value=networks.value;});
async function pollScan(){
 try{
  const response=await fetch('/wifi/scan',{cache:'no-store'});
  if(!response.ok)throw new Error();
  const data=await response.json();
  if(data.state==='running'){scanTimer=setTimeout(pollScan,700);return;}
  scanButton.disabled=false;
  if(data.state!=='ready'){scanState.textContent='扫描失败，请重试或手动输入';return;}
  networks.replaceChildren(new Option('请选择，或手动输入隐藏网络',''));
  data.networks.forEach(n=>networks.add(new Option(n.ssid+' · '+n.rssi+' dBm · '+(n.secure?'加密':'开放'),n.ssid)));
  scanState.textContent=data.networks.length?'请选择WiFi（按信号排序）':'未找到WiFi，可重试或手动输入';
 }catch(e){scanButton.disabled=false;scanState.textContent='扫描请求失败，请重试';}
}
async function scan(){
 clearTimeout(scanTimer);scanButton.disabled=true;scanState.textContent='正在扫描…';
 try{
  const form=new URLSearchParams();form.set('token',document.querySelector('input[name=token]').value);
  const response=await fetch('/wifi/scan',{method:'POST',body:form});
  if(!response.ok)throw new Error();
  pollScan();
 }catch(e){scanButton.disabled=false;scanState.textContent='暂时无法扫描，请稍后重试或手动输入';}
}
scanButton.addEventListener('click',scan);scan();
</script></html>)HTML";
    web.sendHeader("Cache-Control", "no-store");
    web.send(200, "text/html; charset=utf-8", h);
}

void saveSettings() {
    if (!authorized()) {
        return;
    }
    Settings next = settings;
    if (web.arg("units") != "s" ||
        !parseDurationSeconds(web.arg("manual").c_str(), next.manualMs)) {
        web.send(400, "text/plain", "Invalid duration");
        return;
    }
    for (int i = 0; i < TASK_COUNT; ++i) {
        String id(i), stamp = web.arg("t" + id);
        uint32_t hour, minute;
        if (stamp.length() != 5 || stamp[2] != ':' || !number(stamp.substring(0, 2), 0, 23, hour) ||
            !number(stamp.substring(3), 0, 59, minute) ||
            !parseDurationSeconds(web.arg("d" + id).c_str(), next.tasks[i].duration)) {
            web.send(400, "text/plain", "Invalid task");
            return;
        }
        next.tasks[i].hour = hour;
        next.tasks[i].minute = minute;
        next.tasks[i].enabled = web.hasArg("e" + id);
    }
    if (!saveConfiguration(next)) {
        web.send(500, "text/plain", "Storage failed");
        return;
    }
    redirectHome();
}
} // namespace

void beginWeb() {
    token = String(esp_random(), HEX) + String(esp_random(), HEX);
    web.on("/", HTTP_GET, home);
    web.on("/settings", HTTP_POST, saveSettings);
    web.on("/display/brightness", HTTP_POST, [] {
        if (!authorized()) {
            return;
        }
        uint32_t level;
        if (!number(web.arg("brightness"), 1, 100, level)) {
            web.send(400, "text/plain", "Brightness must be 1..100");
            return;
        }
        if (!setDisplayBrightness(uint8_t(level))) {
            web.send(503, "text/plain", "OLED unavailable");
            return;
        }
        wakeDisplay();
        if (web.arg("persist") == "1" && !saveOledBrightness(uint8_t(level))) {
            web.send(500, "text/plain", "Brightness preview applied, but storage failed");
            return;
        }
        web.send(200, "application/json", "{\"brightness\":" + String(level) + "}");
    });
    web.on("/wifi/scan", HTTP_POST, [] {
        if (!authorized()) {
            return;
        }
        if (motorIsFeeding() || !startNetworkScan()) {
            web.send(409, "text/plain", "Scan unavailable; retry later");
            return;
        }
        web.send(202, "application/json", "{\"state\":\"running\"}");
    });
    web.on("/wifi/scan", HTTP_GET, [] {
        web.sendHeader("Cache-Control", "no-store");
        web.send(200, "application/json; charset=utf-8", networkScanJson());
    });
    web.on("/feed", HTTP_POST, [] {
        if (!authorized()) {
            return;
        }
        wakeDisplay();
        if (!startFeed(settings.manualMs)) {
            web.send(409, "text/plain", "Motor unavailable or busy");
        } else {
            redirectHome();
        }
    });
    web.on("/stop", HTTP_POST, [] {
        if (!authorized()) {
            return;
        }
        stopFeed();
        redirectHome();
    });
    web.on("/wifi", HTTP_POST, [] {
        if (!authorized()) {
            return;
        }
        if (motorIsFeeding()) {
            web.send(409, "text/plain", "Stop feeding before WiFi setup");
            return;
        }
        String s = web.arg("ssid"), p = web.arg("password");
        if (s.isEmpty() || s.length() > 32 || p.length() > 63 || (!p.isEmpty() && p.length() < 8)) {
            web.send(400, "text/plain", "Invalid WiFi credentials");
            return;
        }
        if (!saveWifiCredentials(s, p)) {
            web.send(500, "text/plain", "Storage failed");
            return;
        }
        redirectHome();
        connectNetwork(s, p);
    });
    web.onNotFound([] {
        web.sendHeader("Location", "http://192.168.4.1/");
        web.send(302);
    });
    web.begin();
}

void tickWeb() {
    web.handleClient();
}
