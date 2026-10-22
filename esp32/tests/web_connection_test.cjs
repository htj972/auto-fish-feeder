// 执行真实配网提交与轮询脚本，验证用户可见的成功、认证失败和提交错误。
const fs = require('node:fs');
const vm = require('node:vm');
const assert = require('node:assert/strict');
const path = require('node:path');
const source = fs.readFileSync(path.join(__dirname, '../src/web_ui.cpp'), 'utf8');
const full = source.match(/R"HTML\(<script>([\s\S]*?)<\/script><\/html>\)HTML"/)[1];
const script = 'let scanTimer;\n' + full.slice(full.indexOf("const wifiForm="));
const nodes = Object.fromEntries(['wifi-form', 'wifi-state', 'wifi-connect'].map(id => [id, {
    textContent: '', disabled: false, events: {},
    addEventListener(event, callback) { this.events[event] = callback; }
}]));
let now = 0;
let state = 'connecting';
let rejectSubmit = false;
let transportFailed = false;
const timers = new Map();
let timerId = 0;
const requests = [];
vm.runInNewContext(script, {
    document: {getElementById: id => nodes[id]}, URLSearchParams,
    FormData: function() { return [['token', 'test-token'], ['ssid', 'Home'], ['password', 'test-password']]; },
    Date: {now: () => now},
    setTimeout: callback => { timers.set(++timerId, callback); return timerId; },
    clearTimeout: id => timers.delete(id),
    fetch: async (url, options = {}) => {
        requests.push({url, options});
        if (transportFailed) throw new Error('network unavailable');
        if (url === '/wifi') return {ok: !rejectSubmit, text: async () => 'Invalid WiFi credentials'};
        return {ok: true, json: async () => ({state, ip: state === 'connected' ? '192.168.1.100' : ''})};
    }
});
const submit = () => nodes['wifi-form'].events.submit({preventDefault() {}});
async function poll() {
    const [id, callback] = timers.entries().next().value;
    timers.delete(id);
    now += 700;
    await callback();
}
(async () => {
    await submit();
    assert.equal(requests[0].options.body.get('async'), '1');
    assert.equal(requests[0].options.body.get('token'), 'test-token');
    assert.equal(nodes['wifi-connect'].disabled, true);
    await poll();
    assert.match(nodes['wifi-state'].textContent, /正在连接并验证密码/);
    state = 'auth_failed';
    await poll();
    assert.match(nodes['wifi-state'].textContent, /密码可能不正确/);
    assert.equal(nodes['wifi-connect'].disabled, false);
    assert.equal(timers.size, 0);
    for (const [result, message] of [['not_found', /未找到/], ['timeout', /连接超时/], ['connected', /192\.168\.1\.100/]]) {
        state = result;
        await submit();
        await poll();
        assert.match(nodes['wifi-state'].textContent, message);
        assert.equal(nodes['wifi-connect'].disabled, false);
        assert.equal(timers.size, 0);
    }
    rejectSubmit = true;
    await submit();
    assert.match(nodes['wifi-state'].textContent, /提交失败/);
    assert.equal(timers.size, 0);
    rejectSubmit = false;
    await submit();
    transportFailed = true;
    now += 30000;
    await poll();
    assert.match(nodes['wifi-state'].textContent, /未能确认连接结果/);
    assert.equal(nodes['wifi-connect'].disabled, false);
    assert.equal(timers.size, 0);
    console.log('Web connection checks passed');
})().catch(error => { console.error(error); process.exitCode = 1; });
