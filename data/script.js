/* =========================================================
   Injector Tester
   script.js
   ========================================================= */

const $ = id => document.getElementById(id);

//=========================================================
// Тексти
//=========================================================

const STATE_TEXT = {
    idle: 'Готовий',
    pulse: 'Імпульс',
    gap: 'Пауза',
    burst_wait: 'Очікування пакета',
    hold: 'Відкрито',
    done: 'Завершено'
};

const MODE_TEXT = {
    single: 'Одиничний',
    burst: 'Пакет',
    continuous: 'Безперервний',
    hold: 'Відкрито'
};

const MODE_HINTS = {
    single: 'Один імпульс заданої довжини. Період та параметри пакета не використовуються.',
    burst: 'Пакет з N імпульсів (довжина / період). Пакети повторюються кожні "Період пакета" (відлік від початку пакета).',
    continuous: 'Безперервна послідовність імпульсів (довжина / період) до натискання "Стоп".',
    hold: 'Форсунка постійно відкрита (безперервна подача живлення) до зупинки або тайм-ауту безпеки. Використовувати обережно!'
};

//=========================================================
// API
//=========================================================

async function api(path, body) {
    const opt = body
        ? { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(body) }
        : {};

    const r = await fetch(path, opt);

    return r.json();
}

//=========================================================
// Toast
//=========================================================

let toastTimer = null;

function toast(msg) {
    const t = $('toast');

    t.textContent = msg;
    t.classList.add('show');

    clearTimeout(toastTimer);

    toastTimer = setTimeout(() => t.classList.remove('show'), 2500);
}

//=========================================================
// Підказки та обчислення
//=========================================================

function updateModeUI() {
    const m = $('pMode').value;

    $('pPulse').disabled = (m === 'hold');
    $('pPeriod').disabled = (m === 'single' || m === 'hold');
    $('pPulses').disabled = (m !== 'burst');
    $('pBurstPeriod').disabled = (m !== 'burst');

    $('modeHint').textContent = MODE_HINTS[m] || '';

    computeInfo();
}

function computeInfo() {
    const m = $('pMode').value;

    const pulse = parseFloat($('pPulse').value) || 0;
    const period = parseFloat($('pPeriod').value) || 0;
    const n = parseInt($('pPulses').value) || 0;
    const bp = parseFloat($('pBurstPeriod').value) || 0;

    //---------------------------------------------------
    // Частота та заповнення
    //---------------------------------------------------

    let f = '—';

    if ((m === 'burst' || m === 'continuous') && period > 0) {
        f = 'Частота: ' + (1000 / period).toFixed(2) + ' Гц'
          + ' · Заповнення: ' + (pulse / period * 100).toFixed(1) + '%';
    }

    $('infoFreq').textContent = f;

    //---------------------------------------------------
    // Пакет
    //---------------------------------------------------

    let b = '—';

    if (m === 'burst') {
        if (n > 0 && period > 0) {
            const dur = n * period / 1000;

            b = 'Тривалість пакета: ' + dur.toFixed(3) + ' с';

            if (bp > 0)
                b += ' · Пауза між пакетами: ' + Math.max(0, bp / 1000 - dur).toFixed(3) + ' с';
            else
                b += ' · Паузи між пакетами немає';
        } else {
            b = 'Пакет: без обмежень (імпульси йдуть безперервно до зупинки)';
        }
    }

    $('infoBurst').textContent = b;
}

//=========================================================
// Конфігурація
//=========================================================

function updateDhcp() {
    const dhcp = $('dhcp').checked;

    ['ip', 'gateway', 'subnet'].forEach(id => $(id).disabled = dhcp);
}

function loadConfig() {
    fetch('/api/config')
        .then(r => r.json())
        .then(c => {
            if (c.device)
                $('deviceName').value = c.device.deviceName || '';

            if (c.network) {
                $('ssid').value = c.network.ssid || '';
                $('password').value = c.network.password || '';
                $('dhcp').checked = !!c.network.dhcp;
                $('ip').value = c.network.ip || '';
                $('gateway').value = c.network.gateway || '';
                $('subnet').value = c.network.subnet || '';
                $('dns1').value = c.network.dns1 || '';
                $('dns2').value = c.network.dns2 || '';

                updateDhcp();
            }

            if (c.injector) {
                $('pMode').value = c.injector.mode || 'burst';
                $('pPulse').value = c.injector.pulseMs;
                $('pPeriod').value = c.injector.periodMs;
                $('pPulses').value = c.injector.pulses;
                $('pBurstPeriod').value = c.injector.burstPeriodMs;
                $('hHoldTimeout').value = c.injector.holdTimeoutSec;
                $('hAutoStop').value = c.injector.autoStopSec;
                $('hInvert').checked = !!c.injector.invertOutput;
            }

            updateModeUI();
        })
        .catch(() => toast('Помилка завантаження конфігурації'));
}

//=========================================================
// Статус (ополювання 500 мс)
//=========================================================

function fmtTime(ms) {
    const s = Math.floor(ms / 1000);

    const mm = Math.floor(s / 60);
    const ss = s % 60;

    return mm + ':' + String(ss).padStart(2, '0');
}

function poll() {
    fetch('/api/status')
        .then(r => r.json())
        .then(s => {
            $('stState').textContent = STATE_TEXT[s.state] || s.state;
            $('stState').className = 'badge ' + (s.state || 'idle');

            $('stMode').textContent = MODE_TEXT[s.mode] || s.mode;
            $('stPulses').textContent = s.pulses;
            $('stBursts').textContent = s.bursts;
            $('stTime').textContent = fmtTime(s.elapsedMs || 0);

            if (s.wifi)
                $('stWifi').textContent = s.wifi.mode + ' ' + (s.wifi.ip || '');
        })
        .catch(() => {});
}

//=========================================================
// Керування
//=========================================================

function collectParams() {
    return {
        mode: $('pMode').value,
        pulseMs: parseFloat($('pPulse').value) || 0,
        periodMs: parseFloat($('pPeriod').value) || 0,
        pulses: parseInt($('pPulses').value) || 0,
        burstPeriodMs: parseFloat($('pBurstPeriod').value) || 0
    };
}

function collectInjectorConfig() {
    return {
        injector: {
            mode: $('pMode').value,
            pulseMs: parseFloat($('pPulse').value) || 0,
            periodMs: parseFloat($('pPeriod').value) || 0,
            pulses: parseInt($('pPulses').value) || 0,
            burstPeriodMs: parseFloat($('pBurstPeriod').value) || 0,
            holdTimeoutSec: parseInt($('hHoldTimeout').value) || 0,
            autoStopSec: parseInt($('hAutoStop').value) || 0,
            invertOutput: $('hInvert').checked
        }
    };
}

//=========================================================
// Обробники кнопок
//=========================================================

$('btnStart').onclick = async () => {
    try {
        const r = await api('/api/start', collectParams());
        if (r.error) toast('Помилка: ' + r.error);
        else toast('Запущено (' + (MODE_TEXT[r.mode] || r.mode) + ')');
    } catch (e) {
        toast('Помилка зв\u2019язку');
    }
};

$('btnStop').onclick = async () => {
    try {
        await api('/api/stop', {});
        toast('Зупинено');
    } catch (e) {
        toast('Помилка зв\u2019язку');
    }
};

$('btnSingle').onclick = async () => {
    try {
        const r = await api('/api/start', {
            mode: 'single',
            pulseMs: parseFloat($('pPulse').value) || 0
        });
        if (r.error) toast('Помилка: ' + r.error);
        else toast('Одиничний імпульс');
    } catch (e) {
        toast('Помилка зв\u2019язку');
    }
};

$('btnDefaults').onclick = async () => {
    try {
        await api('/api/config', collectInjectorConfig());
        toast('Типові параметри збережено');
    } catch (e) {
        toast('Помилка збереження');
    }
};

$('btnSave').onclick = async () => {
    try {
        const body = {
            device: { deviceName: $('deviceName').value },
            network: {
                ssid: $('ssid').value,
                password: $('password').value,
                dhcp: $('dhcp').checked,
                ip: $('ip').value,
                gateway: $('gateway').value,
                subnet: $('subnet').value,
                dns1: $('dns1').value,
                dns2: $('dns2').value
            }
        };

        Object.assign(body, collectInjectorConfig());

        const r = await api('/api/config', body);

        if (r.restart) {
            toast('Збережено. Перезавантаження...');
            setTimeout(() => location.reload(), 4000);
        } else {
            toast('Збережено');
        }
    } catch (e) {
        toast('Помилка збереження');
    }
};

$('btnRestart').onclick = async () => {
    if (!confirm('Перезавантажити пристрій?')) return;

    try {
        await api('/api/restart', {});
        toast('Перезавантаження...');
        setTimeout(() => location.reload(), 4000);
    } catch (e) {
        toast('Помилка зв\u2019язку');
    }
};

$('btnFactory').onclick = async () => {
    if (!confirm('Скинути всі налаштування до заводських?')) return;

    try {
        await api('/api/factory', {});
        toast('Скидання. Перезавантаження...');
        setTimeout(() => location.reload(), 4000);
    } catch (e) {
        toast('Помилка зв\u2019язку');
    }
};

//=========================================================
// Дрібні обробники
//=========================================================

$('showPassword').onclick = () => {
    const p = $('password');
    p.type = (p.type === 'password') ? 'text' : 'password';
};

$('dhcp').onchange = updateDhcp;

$('pMode').onchange = updateModeUI;

['pPulse', 'pPeriod', 'pPulses', 'pBurstPeriod'].forEach(id => {
    $(id).addEventListener('input', computeInfo);
});

//=========================================================
// Ініціалізація
//=========================================================

loadConfig();

poll();

setInterval(poll, 500);
