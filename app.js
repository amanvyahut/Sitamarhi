/* ============================================================
   TSCRIC-LoRa Dashboard — app.js v4.0 RESEARCH EDITION
   Original v3.0 fully preserved + new systems added
   ============================================================ */

// ============================================================
// PWA INSTALL — v5.0 Enterprise Shell
// ============================================================
let _pwaPrompt = null;
window.addEventListener('beforeinstallprompt', (e) => {
  e.preventDefault();
  _pwaPrompt = e;
  // Show install button if available
  const btn = document.getElementById('pwaInstallBtn');
  if (btn) btn.style.display = 'flex';
});
window.addEventListener('appinstalled', () => {
  _pwaPrompt = null;
  const btn = document.getElementById('pwaInstallBtn');
  if (btn) btn.style.display = 'none';
  showToast('success', '✅ App Installed', 'Dashboard ab home screen pe hai.', 5000);
});
function triggerPWAInstall() {
  if (_pwaPrompt) {
    _pwaPrompt.prompt();
    _pwaPrompt.userChoice.then(r => {
      if (r.outcome === 'accepted') showToast('success', '✅ Installing', 'App install ho raha hai…', 5000);
      _pwaPrompt = null;
    });
  } else {
    // Fallback instructions
    showToast('info', '📱 App Install', 'Browser menu → "Add to Home Screen" ya "Install App" select karo', 6000);
  }
}

// ============================================================
// SETUP WIZARD — Crop Selection First Screen
// ============================================================
function launchSetupWizard() {
  const sw = document.getElementById('setupWizard');
  if (sw) { sw.style.display = 'flex'; return; }
}
function setupWizardDone() {
  const cropEl = document.getElementById('wizardCrop');
  const areaEl = document.getElementById('wizardArea');
  const locEl  = document.getElementById('wizardLocation');
  if (cropEl) {
    const v = parseInt(cropEl.value);
    localConfig.crop = v;
    const sel = document.getElementById('cropSelectMain');
    if (sel) sel.value = v;
  }
  if (areaEl && areaEl.value) {
    const m2 = parseFloat(areaEl.value);
    if (!isNaN(m2) && m2 > 0) {
      localConfig.plotArea_m2 = m2;
      localConfig.plotArea_bigha = m2 / BIGHA_TO_M2;
      const inpM2 = document.getElementById('plotAreaM2');
      const inpBigha = document.getElementById('plotAreaBigha');
      if (inpM2) inpM2.value = m2.toFixed(2);
      if (inpBigha) inpBigha.value = localConfig.plotArea_bigha.toFixed(4);
    }
  }
  if (locEl) {
    const loc = locEl.value;
    localConfig.weatherLocation = loc;
    const sel2 = document.getElementById('weatherLocation');
    if (sel2) sel2.value = loc;
    onWeatherLocationChange(loc);
  }
  const sw = document.getElementById('setupWizard');
  if (sw) sw.style.display = 'none';
  sessionStorage.setItem('tscric_setup_done', '1');
  updatePreviewCard(localConfig.crop, localConfig.plotArea_m2);
  loadChartJS();
  initFirebase();
  showToast('success', '✅ Configuration Saved', 'Dashboard ready hai.', 5000);
}

// ============================================================
// LOGIN — Password removed, direct launch
// ============================================================
function doLogin() { /* No-op — password system removed */ }

function doLogout() {
  sessionStorage.removeItem('tscric_setup_done');
  const loginScreen = document.getElementById('loginScreen');
  const appShell    = document.getElementById('appShell');
  if (appShell)    appShell.classList.remove('visible');
  if (loginScreen) loginScreen.style.display = 'none';
  // Show setup wizard again on logout
  const sw = document.getElementById('setupWizard');
  if (sw) sw.style.display = 'flex';
}