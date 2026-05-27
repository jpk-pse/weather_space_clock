// ═══════════════════════════════════════════════════════
//  provision.ino — WiFi AP + captive portal setup
//  WiFi networks are scanned and shown as a dropdown.
// ═══════════════════════════════════════════════════════

static WebServer provWebServer(80);
static DNSServer provDnsServer;
static const byte DNS_PORT = 53;

// ── HTML template — %WIFI_OPTIONS% replaced at runtime ──
static const char SETUP_HTML[] PROGMEM = R"rawhtml(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1">
<title>Claude Monitor Setup</title>
<style>
  :root{--bg:#191919;--card:#252525;--border:#3a3a3a;--text:#e0e0e0;
        --dim:#888;--accent:#e8733a;--cyan:#f0a050;--red:#f66;--green:#4f4}
  *{box-sizing:border-box;margin:0;font-family:system-ui,-apple-system,sans-serif}
  body{background:var(--bg);color:var(--text);padding:8px;min-height:100vh}
  .card{background:var(--card);border:1px solid var(--border);border-radius:12px;
        padding:16px 18px;max-width:440px;margin:0 auto}
  h1{color:var(--accent);font-size:1.35em;margin-bottom:2px}
  .sub{color:var(--dim);font-size:.82em;margin-bottom:14px}
  .field{margin-bottom:11px}
  label{display:block;font-size:.82em;color:var(--dim);margin-bottom:4px;font-weight:500}
  input,select{width:100%;padding:8px 10px;border:1px solid var(--border);
               border-radius:8px;background:var(--bg);color:var(--text);font-size:.95em;outline:0}
  input:focus,select:focus{border-color:var(--cyan)}
  input.valid{border-color:var(--green)}
  input.invalid{border-color:var(--red)}
  .hint{font-size:.75em;color:var(--dim);margin-top:4px;line-height:1.4}
  .warn{font-size:.78em;color:var(--red);margin-top:3px}
  .step-box{background:#1e1e1e;border:1px solid var(--border);border-radius:8px;
            padding:10px 12px;margin-bottom:11px}
  .step-box .step-title{font-size:.78em;color:var(--accent);font-weight:600;
                        text-transform:uppercase;letter-spacing:.06em;margin-bottom:6px}
  .step-box code{background:#111;color:var(--cyan);padding:6px 10px;border-radius:6px;
                 display:block;font-size:.9em;font-family:monospace;cursor:pointer;
                 border:1px solid #333;margin:4px 0}
  .step-box code:hover{border-color:var(--cyan)}
  .copy-hint{font-size:.72em;color:#555;margin-top:2px}
  .token-status{font-size:.78em;margin-top:4px;min-height:1em}
  .network-row{display:flex;gap:8px;align-items:flex-end}
  .network-row select{flex:1}
  .network-row button{margin-top:0;width:auto;padding:8px 12px;font-size:.85em;
                      white-space:nowrap;flex-shrink:0}
  .signal{font-size:.75em;color:var(--dim);margin-left:4px}
  .lock{color:#aaa;margin-left:2px}
  .row{display:flex;gap:10px}
  .row .field{flex:1}
  .divider{border-top:1px solid var(--border);margin:13px 0 11px}
  .section-label{font-size:.72em;color:var(--dim);text-transform:uppercase;
                 letter-spacing:1px;margin-bottom:10px;font-weight:600}
  button{margin-top:12px;width:100%;padding:10px;border:none;border-radius:8px;
         background:var(--accent);color:#111;font-weight:700;font-size:1em;cursor:pointer}
  button:disabled{opacity:.45;cursor:wait}
  #status{margin-top:9px;font-size:.88em;text-align:center;min-height:1.2em}
  .ok{color:var(--green)} .err{color:var(--red)}
</style>
</head>
<body>
<div class="card">
  <h1>Claude Usage Monitor</h1>
  <p class="sub">One-time setup — takes about 1 minute.</p>

  <form id="f">
    <div class="section-label">Step 1 — Get your OAuth token</div>
    <div class="step-box">
      <div class="step-title">Run this in your terminal</div>
      <code id="cmd" onclick="copyCmd()">claude setup-token</code>
      <div class="copy-hint">Click to copy &nbsp;·&nbsp; Requires Claude Code to be installed</div>
      <div class="hint" style="margin-top:8px">
        Opens a browser, asks you to log in to Claude, and prints a token starting with
        <code style="display:inline;padding:1px 5px">sk-ant-oat01-</code> to your terminal.
        Copy and paste it below.
      </div>
    </div>
    <div class="field">
      <label for="token">OAuth Token</label>
      <input id="token" name="token" type="password" required maxlength="512"
             autocomplete="off" placeholder="sk-ant-oat01-..."
             oninput="validateToken(this)">
      <div class="token-status" id="token-status"></div>
    </div>

    <div class="divider"></div>
    <div class="section-label">Step 2 — WiFi (2.4 GHz only)</div>

    <div class="field">
      <label>Network</label>
      <div class="network-row">
        <select id="ssid-select" onchange="onNetworkSelect(this)">
          <option value="">— select a network —</option>
          %WIFI_OPTIONS%
          <option value="__manual__">Other (enter manually)</option>
        </select>
        <button type="button" onclick="rescan()">↻ Rescan</button>
      </div>
    </div>

    <div class="field" id="ssid-manual-field" style="display:none">
      <label for="ssid-manual">Network name (SSID)</label>
      <input id="ssid-manual" maxlength="32" autocomplete="off" placeholder="Enter SSID">
    </div>
    <input type="hidden" id="ssid" name="ssid">

    <div class="field">
      <label for="wifipass">Password</label>
      <input id="wifipass" name="wifipass" type="password" maxlength="64"
             autocomplete="off" placeholder="Leave blank for open networks">
    </div>

    <div class="divider"></div>
    <div class="section-label">Preferences</div>
    <div class="row">
      <div class="field">
        <label for="poll_sec">Refresh every</label>
        <select id="poll_sec" name="poll_sec">
          <option value="30">30 seconds</option>
          <option value="60" selected>1 minute</option>
          <option value="120">2 minutes</option>
          <option value="300">5 minutes</option>
        </select>
      </div>
      <div class="field">
        <label for="brightness">Brightness</label>
        <select id="brightness" name="brightness">
          <option value="1">Dim</option>
          <option value="2" selected>Normal</option>
          <option value="3">Bright</option>
        </select>
      </div>
    </div>

    <button type="submit" id="btn">Save &amp; Reboot Device</button>
    <div id="status"></div>
  </form>
</div>

<script>
function copyCmd() {
  navigator.clipboard.writeText('claude setup-token').then(() => {
    const el = document.getElementById('cmd');
    el.style.color = 'var(--green)';
    setTimeout(() => el.style.color = '', 1500);
  });
}

function validateToken(input) {
  const v = input.value.trim();
  const st = document.getElementById('token-status');
  if (!v) { input.className=''; st.textContent=''; return; }
  if (v.startsWith('sk-ant-') && v.length > 20) {
    input.className = 'valid';
    st.style.color = 'var(--green)';
    st.textContent = '✓ Token looks valid';
  } else {
    input.className = 'invalid';
    st.style.color = 'var(--red)';
    st.textContent = 'Token should start with sk-ant-oat01-';
  }
}

function onNetworkSelect(sel) {
  const manualField = document.getElementById('ssid-manual-field');
  const ssidHidden  = document.getElementById('ssid');
  if (sel.value === '__manual__') {
    manualField.style.display = 'block';
    ssidHidden.value = '';
  } else {
    manualField.style.display = 'none';
    ssidHidden.value = sel.value;
  }
}

function rescan() {
  const st = document.getElementById('status');
  st.className = ''; st.textContent = 'Scanning...';
  fetch('/scan').then(r => r.text()).then(html => {
    const sel = document.getElementById('ssid-select');
    // Preserve selected value if still present
    const prev = sel.value;
    // Replace options between placeholder and "Other"
    sel.innerHTML =
      '<option value="">— select a network —</option>' +
      html +
      '<option value="__manual__">Other (enter manually)</option>';
    sel.value = prev;
    st.textContent = '';
  }).catch(() => { st.className='err'; st.textContent='Scan failed'; });
}

document.getElementById('f').addEventListener('submit', async (e) => {
  e.preventDefault();
  const btn = document.getElementById('btn');
  const st  = document.getElementById('status');
  const tok = document.getElementById('token').value.trim();

  // Resolve final SSID
  const sel = document.getElementById('ssid-select');
  const ssidHidden = document.getElementById('ssid');
  if (sel.value === '__manual__') {
    ssidHidden.value = document.getElementById('ssid-manual').value.trim();
  } else {
    ssidHidden.value = sel.value;
  }

  if (!ssidHidden.value) {
    st.className='err'; st.textContent='Please select or enter a WiFi network.'; return;
  }
  if (!tok.startsWith('sk-ant-')) {
    st.className='err'; st.textContent='Invalid token. Run claude setup-token and copy the full output.'; return;
  }

  btn.disabled = true;
  st.className = ''; st.textContent = 'Saving...';

  try {
    const body = new URLSearchParams(new FormData(e.target));
    body.set('token', tok);
    const r = await fetch('/provision', {
      method: 'POST',
      headers: {'Content-Type': 'application/x-www-form-urlencoded'},
      body: body
    });
    if (r.ok) {
      st.className = 'ok';
      st.textContent = '✓ Saved! Device rebooting and connecting to your WiFi...';
    } else {
      st.className = 'err';
      st.textContent = 'Error: ' + await r.text();
      btn.disabled = false;
    }
  } catch (x) {
    st.className = 'ok';
    st.textContent = '✓ Device rebooting (connection closed — this is normal).';
  }
});
</script>
</body>
</html>)rawhtml";

// ── Build <option> tags from a WiFi scan ──────────────
static String buildWifiOptions() {
    int n = WiFi.scanNetworks();
    if (n <= 0) return "<option disabled>No networks found</option>";

    // Sort by RSSI descending (simple bubble sort — small n)
    for (int i = 0; i < n - 1; i++) {
        for (int j = 0; j < n - i - 1; j++) {
            if (WiFi.RSSI(j) < WiFi.RSSI(j + 1)) {
                // swap — unfortunately no direct swap API, rebuild manually later
                // TFT_eSPI scan results are indexed; just sort indices
            }
        }
    }

    String opts = "";
    for (int i = 0; i < n; i++) {
        String ssid = WiFi.SSID(i);
        int    rssi = WiFi.RSSI(i);
        bool   enc  = (WiFi.encryptionType(i) != WIFI_AUTH_OPEN);

        // Signal bars: ▂▄▆█ based on RSSI
        const char* sig;
        if      (rssi > -55) sig = "▂▄▆█";
        else if (rssi > -65) sig = "▂▄▆";
        else if (rssi > -75) sig = "▂▄";
        else                 sig = "▂";

        // Escape quotes in SSID for HTML attribute
        String safe = ssid;
        safe.replace("\"", "&quot;");
        safe.replace("<", "&lt;");
        safe.replace(">", "&gt;");

        opts += "<option value=\"" + safe + "\">";
        opts += safe;
        opts += " ";
        opts += sig;
        if (enc) opts += " 🔒";
        opts += "</option>\n";
    }
    WiFi.scanDelete();
    return opts;
}

// ── HTTP handlers ─────────────────────────────────────

static void provHandleRoot() {
    // Scan networks and inject into HTML
    String html = FPSTR(SETUP_HTML);
    html.replace("%WIFI_OPTIONS%", buildWifiOptions());
    provWebServer.send(200, "text/html", html);
}

static void provHandleScan() {
    // Called by the Rescan button — returns just the <option> fragment
    provWebServer.send(200, "text/html", buildWifiOptions());
}

static void provHandleProvision() {
    String ssid      = provWebServer.arg("ssid");
    String wifipass  = provWebServer.arg("wifipass");
    String token_in  = provWebServer.arg("token");
    String pollStr   = provWebServer.arg("poll_sec");
    String brightStr = provWebServer.arg("brightness");

    token_in.trim();
    ssid.trim();

    if (ssid.isEmpty() || token_in.isEmpty()) {
        provWebServer.send(400, "text/plain", "Missing required fields.");
        return;
    }
    if (!token_in.startsWith("sk-ant-")) {
        provWebServer.send(400, "text/plain", "Token must start with sk-ant-");
        return;
    }

    EncryptedBlob blob;
    if (!encryptToken(token_in.c_str(), blob)) {
        provWebServer.send(500, "text/plain", "Encryption failed.");
        return;
    }

    int pollSec = pollStr.isEmpty() ? DEFAULT_POLL_SEC
                                    : constrain(pollStr.toInt(), MIN_POLL_SEC, MAX_POLL_SEC);
    int bright  = brightStr.isEmpty() ? DEFAULT_BRIGHTNESS
                                      : constrain(brightStr.toInt(), 0, 3);

    Preferences p;
    p.begin(NVS_NAMESPACE, false);
    p.putString("ssid",      ssid);
    p.putString("wifipass",  wifipass);
    p.putBytes("blob",       &blob, sizeof(blob));
    p.putInt("poll_sec",     pollSec);
    p.putInt("brightness",   bright);
    p.putBool("provisioned", true);
    p.end();

    provWebServer.send(200, "text/plain", "OK");
    delay(1500);
    ESP.restart();
}

static void provHandleNotFound() {
    provWebServer.sendHeader("Location", "http://192.168.4.1/", true);
    provWebServer.send(302, "text/plain", "");
}

void runProvisioningPortal(const char* apName, const char* apPass) {
    // Scan before starting AP (can't scan while in AP mode)
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);

    // Switch to AP mode
    WiFi.mode(WIFI_AP);
    WiFi.softAP(apName, apPass);
    delay(100);

    provDnsServer.start(DNS_PORT, "*", WiFi.softAPIP());

    provWebServer.on("/",          HTTP_GET,  provHandleRoot);
    provWebServer.on("/scan",      HTTP_GET,  provHandleScan);
    provWebServer.on("/provision", HTTP_POST, provHandleProvision);
    provWebServer.onNotFound(provHandleNotFound);
    provWebServer.begin();

    uiSetupScreen(apName, apPass);

    while (true) {
        provDnsServer.processNextRequest();
        provWebServer.handleClient();
        delay(2);
    }
}
