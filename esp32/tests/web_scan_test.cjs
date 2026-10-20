// 执行实际页面中的扫描脚本；模拟HTTP和DOM，不访问网络。
const fs = require('node:fs');
const vm = require('node:vm');
const assert = require('node:assert/strict');
const path = require('node:path');
const source = fs.readFileSync(path.join(__dirname, '../src/web_ui.cpp'), 'utf8');
const script = source.match(/R"HTML\(<script>([\s\S]*?)<\/script><\/html>\)HTML"/)[1];
const nodes = Object.fromEntries(['scan', 'scan-state', 'networks', 'ssid', 'token', 'brightness', 'brightness-value', 'brightness-state', 'brightness-form', 'brightness-save'].map(id => [id, {
    value: id === 'token' ? 'test-token' : '', disabled: false, textContent: '', options: [], events: {},
    addEventListener(name, action) { this.events[name] = action; },
    add(option) { this.options.push(option); },
    replaceChildren(...options) { this.options = options; }
}]));
let failure = false;
let pollCount = 0;
const timers = [];
const requests = [];
const sandbox = {
    document: {
        getElementById: id => nodes[id],
        querySelector: () => nodes.token
    },
    URLSearchParams,
    Option: function(text, value) { this.text = text; this.value = value; },
    setTimeout: callback => { timers.push(callback); return timers.length; },
    clearTimeout() {},
    fetch: async (url, options = {}) => {
        requests.push({url, options});
        if (failure) return {ok: false};
        if (options.method === 'POST') {
            assert.equal(options.body.get('token'), 'test-token');
            return {ok: true};
        }
        return {ok: true, json: async () => ++pollCount === 1
            ? {state: 'running'}
            : {state: 'ready', networks: [
                {ssid: 'Home & <WiFi>', rssi: -40, secure: true},
                {ssid: 'Guest', rssi: -70, secure: false}
            ]}};
    }
};
const flush = () => new Promise(resolve => setImmediate(resolve));
(async () => {
    vm.runInNewContext(script, sandbox);
    assert.equal(nodes.scan.disabled, true);
    await flush();
    assert.equal(timers.length, 1);
    timers.shift()();
    await flush();
    assert.equal(nodes.scan.disabled, false);
    assert.equal(nodes.networks.options.length, 3);
    assert.equal(nodes.networks.options[1].value, 'Home & <WiFi>');
    assert.match(nodes.networks.options[1].text, /加密/);
    assert.match(nodes.networks.options[2].text, /开放/);
    nodes.networks.value = 'Home & <WiFi>';
    nodes.networks.events.change();
    assert.equal(nodes.ssid.value, 'Home & <WiFi>');
    nodes.ssid.value = 'Hidden network';
    await nodes.scan.events.click();
    await flush();
    assert.equal(nodes.ssid.value, 'Hidden network');
    failure = true;
    await nodes.scan.events.click();
    assert.equal(nodes.scan.disabled, false);
    assert.match(nodes['scan-state'].textContent, /手动输入/);
    assert.equal(requests[0].options.method, 'POST');
    failure = false;
    nodes.brightness.value = '10';
    nodes.brightness.events.input();
    assert.equal(nodes['brightness-value'].textContent, '10%');
    timers.shift()();
    await flush();
    const brightnessRequests = () => requests.filter(r => r.url === '/display/brightness');
    assert.equal(brightnessRequests().at(-1).options.body.get('persist'), '0');
    await nodes['brightness-form'].events.submit({preventDefault() {}});
    assert.equal(brightnessRequests().at(-1).options.body.get('persist'), '1');
    assert.match(nodes['brightness-state'].textContent, /已保存/);
    nodes.brightness.value = '20';
    nodes.brightness.events.input();
    timers.shift()();
    nodes.brightness.value = '30';
    await nodes['brightness-form'].events.submit({preventDefault() {}});
    const lastTwo = brightnessRequests().slice(-2);
    assert.equal(lastTwo[0].options.body.get('brightness'), '20');
    assert.equal(lastTwo[1].options.body.get('brightness'), '30');
    assert.equal(lastTwo[1].options.body.get('persist'), '1');
    failure = true;
    await nodes['brightness-form'].events.submit({preventDefault() {}});
    assert.match(nodes['brightness-state'].textContent, /失败/);
    assert.equal(nodes['brightness-save'].disabled, false);
    console.log('Web checks passed: WiFi scan and OLED brightness preview/save ordering/failure');
})().catch(error => { console.error(error); process.exitCode = 1; });
