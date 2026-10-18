/* ==========================================================================
   K2 Dynamic Island - Vanilla JavaScript Application Controller
   ========================================================================== */

(() => {
  'use strict';

  // --- App State ---
  const state = {
    isExpanded: false,
    activeTab: 'files',
    files: [],
    reminders: [],
    settings: {
      alwaysOnTop: true,
      reducedMotion: false,
      launchAtStartup: false,
      compactWidth: 290,
      compactHeight: 44,
      expandedWidth: 480,
      expandedHeight: 360,
      themeMode: 'dark',
      backgroundType: 'glass',
      customBackground: 'preset1',
      islandAnimation: 'spiderverse',
      autoCheckUpdates: true,
      autoDownloadUpdates: true,
      lastUpdateCheck: ''
    },
    reminderFilter: 'all',
    deleteTargetId: null
  };

  // --- Bridge Interface ---
  let nextReqId = 1;
  const pendingRequests = new Map();

  function callNative(command, payload = {}) {
    return new Promise((resolve) => {
      const id = String(nextReqId++);
      const msg = { id, command, ...payload };

      // Safety timeout: Never leave callers hanging if native bridge is busy
      const timer = setTimeout(() => {
        if (pendingRequests.has(id)) {
          pendingRequests.delete(id);
          resolve({ success: false, timeout: true });
        }
      }, 2500);

      pendingRequests.set(id, {
        resolve: (val) => {
          clearTimeout(timer);
          resolve(val);
        }
      });

      if (window.chrome && window.chrome.webview) {
        window.chrome.webview.postMessage(JSON.stringify(msg));
      } else {
        // Fallback for standalone browser testing / mock
        handleMockResponse(msg);
      }
    });
  }

  function handleMockResponse(msg) {
    setTimeout(() => {
      const req = pendingRequests.get(msg.id);
      if (!req) return;
      pendingRequests.delete(msg.id);

      if (msg.command === 'get_settings') {
        req.resolve({
          success: true,
          settings: {
            alwaysOnTop: true,
            reducedMotion: false,
            launchAtStartup: false,
            compactWidth: 290,
            compactHeight: 44,
            expandedWidth: 480,
            expandedHeight: 360,
            activeTab: 'files',
            themeMode: 'dark',
            backgroundType: 'glass',
            customBackground: 'preset1'
          }
        });
      } else if (msg.command === 'get_system_stats') {
        req.resolve({
          success: true,
          stats: { cpuPercent: 14.2, ramPercent: 48, ramUsedMb: 7680, ramTotalMb: 16000, batteryPercent: 98, isCharging: true, hasBattery: true }
        });
      } else if (msg.command === 'get_media_status') {
        req.resolve({
          success: true,
          media: { hasActiveSession: false, isPlaying: false, title: '', artist: '', appName: '', statusText: 'No active audio session' }
        });
      } else if (msg.command === 'get_reminders') {
        req.resolve({ success: true, reminders: [] });
      } else {
        req.resolve({ success: true });
      }
    }, 20);
  }

  // --- Native Message Handler (Registered IMMEDIATELY at script load) ---
  function onNativeMessage(event) {
    let data = event.data;
    if (!data) return;
    if (typeof data === 'string') {
      try { data = JSON.parse(data); } catch (_) {}
    }

    // Pending response resolution
    if (data.id && pendingRequests.has(String(data.id))) {
      const req = pendingRequests.get(String(data.id));
      pendingRequests.delete(String(data.id));
      req.resolve(data);
      return;
    }

    // Native push events
    if (data.event === 'filesDropped') {
      if (Array.isArray(data.data)) {
        // If user dropped an image or animated GIF
        const first = data.data[0];
        const isImageOrGif = data.data.length === 1 && first && /\.(gif|png|jpe?g|webp|bmp)$/i.test(first.path || first.name || '');
        if (isImageOrGif && (state.activeTab === 'settings' || state.settings.backgroundType === 'photo')) {
          callNative('read_image_base64', { path: first.path }).then(res => {
            if (res && res.success && res.dataUrl) {
              setCustomPhoto(res.dataUrl);
              showToast('Dynamic Island GIF wallpaper applied!');
            }
          });
        }
        state.files = [...data.data, ...state.files];
        renderFileList();
        expandIsland();
        if (!isImageOrGif || state.activeTab !== 'settings') {
          switchTab('files');
        }
        showToast(`${data.data.length} file(s) received`);
      }
    } else if (data.event === 'dragStateChanged') {
      const isDragging = !!(data.data && data.data.isDragging);
      if (elements.capsuleCompact) elements.capsuleCompact.classList.toggle('drag-over', isDragging);
      if (elements.fileDropZone) elements.fileDropZone.classList.toggle('dragover', isDragging);
    } else if (data.event === 'switchTab') {
      expandIsland();
      switchTab(data.data);
    } else if (data.event === 'updateStatusChanged') {
      handleUpdateStatusChange(data.data);
    }
  }

  if (window.chrome && window.chrome.webview) {
    window.chrome.webview.addEventListener('message', onNativeMessage);
  }

  // --- DOM Elements Cache ---
  let elements = {};

  function initElements() {
    elements = {
      container: document.getElementById('island-container'),
      capsuleCompact: document.getElementById('capsule-compact'),
      compactChevronBtn: document.getElementById('compact-chevron-btn'),
      compactCenterContent: document.getElementById('compact-center-content'),
      compactBrand: document.getElementById('compact-brand'),
      compactSeparator: document.getElementById('compact-separator'),
      compactStatusDot: document.getElementById('compact-status-dot'),
      compactSubtext: document.getElementById('compact-subtext'),
      compactEqualizer: document.getElementById('compact-equalizer'),
      compactFileBadge: document.getElementById('compact-file-badge'),
      compactBadgeCount: document.getElementById('compact-badge-count'),

      capsuleExpanded: document.getElementById('capsule-expanded'),
      btnCollapse: document.getElementById('btn-collapse'),
      btnPinToggle: document.getElementById('btn-pin-toggle'),
      navTabs: document.querySelectorAll('.seg-btn'),
      tabPanels: document.querySelectorAll('.tab-panel'),
      toast: document.getElementById('toast'),

      // Files
      fileDropZone: document.getElementById('file-drop-zone'),
      filesList: document.getElementById('files-list'),
      filesCount: document.getElementById('files-count'),
      btnCopyAllPaths: document.getElementById('btn-copy-all-paths'),
      btnClearFiles: document.getElementById('btn-clear-files'),

      // Media
      mediaTitle: document.getElementById('media-title'),
      mediaArtist: document.getElementById('media-artist'),
      mediaSessionTag: document.getElementById('media-session-tag'),
      musicDisc: document.getElementById('music-disc'),
      mediaSoundwave: document.getElementById('media-soundwave'),
      btnMediaPlay: document.getElementById('btn-media-play'),
      btnMediaNext: document.getElementById('btn-media-next'),
      btnMediaPrev: document.getElementById('btn-media-prev'),
      btnMediaMute: document.getElementById('btn-media-mute'),
      iconPlayPause: document.getElementById('icon-play-pause'),

      // Reminders
      calDayStr: document.getElementById('cal-day-str'),
      calDateStr: document.getElementById('cal-date-str'),
      btnAddReminderToggle: document.getElementById('btn-add-reminder-toggle'),
      addReminderForm: document.getElementById('add-reminder-form'),
      inputRemTitle: document.getElementById('input-rem-title'),
      inputRemDate: document.getElementById('input-rem-date'),
      inputRemTime: document.getElementById('input-rem-time'),
      inputRemCat: document.getElementById('input-rem-cat'),
      btnSaveReminder: document.getElementById('btn-save-reminder'),
      btnCancelReminder: document.getElementById('btn-cancel-reminder'),
      remindersList: document.getElementById('reminders-list'),
      filterChips: document.querySelectorAll('.filter-chip'),

      // Tools
      cpuVal: document.getElementById('cpu-val'),
      cpuBar: document.getElementById('cpu-bar'),
      ramVal: document.getElementById('ram-val'),
      ramBar: document.getElementById('ram-bar'),
      ramDetail: document.getElementById('ram-detail'),
      powerVal: document.getElementById('power-val'),
      powerBar: document.getElementById('power-bar'),
      btnToolLock: document.getElementById('btn-tool-lock'),
      btnToolTaskmgr: document.getElementById('btn-tool-taskmgr'),
      btnToolMute: document.getElementById('btn-tool-mute'),

      // Settings
      settingAlwaysOnTop: document.getElementById('setting-always-on-top'),
      settingReducedMotion: document.getElementById('setting-reduced-motion'),
      settingStartup: document.getElementById('setting-startup'),
      infoStoragePath: document.getElementById('info-storage-path'),
      btnQuitApp: document.getElementById('btn-quit-app'),

      // Backdrop & Themes
      livelyCanvas: document.getElementById('lively-canvas'),
      photoBackdrop: document.getElementById('photo-backdrop'),
      compactLivelyCanvas: document.getElementById('compact-lively-canvas'),
      compactPhotoBackdrop: document.getElementById('compact-photo-backdrop'),
      btnThemeDark: document.getElementById('btn-theme-dark'),
      btnThemeLight: document.getElementById('btn-theme-light'),
      currentThemeBadge: document.getElementById('current-theme-badge'),
      bgOptGlass: document.getElementById('bg-opt-glass'),
      bgOptLively: document.getElementById('bg-opt-lively'),
      bgOptPhoto: document.getElementById('bg-opt-photo'),
      photoConfigPanel: document.getElementById('photo-config-panel'),
      photoThumbs: document.querySelectorAll('.photo-thumb'),
      btnUploadPhoto: document.getElementById('btn-upload-photo'),
      inputCustomPhoto: document.getElementById('input-custom-photo'),
      btnClearPhoto: document.getElementById('btn-clear-photo'),

      // Dynamic Island Animation / Companion
      compactAnimSlot: document.getElementById('compact-anim-slot'),
      compactAnimImg: document.getElementById('compact-anim-img'),
      companionBtns: document.querySelectorAll('.companion-pill-btn'),

      // Modal
      confirmModal: document.getElementById('confirm-modal'),
      modalBtnCancel: document.getElementById('modal-btn-cancel'),
      modalBtnConfirm: document.getElementById('modal-btn-confirm'),

      // Software Updates
      updateBadgeStatus: document.getElementById('update-badge-status'),
      settingAutoCheckUpdates: document.getElementById('setting-auto-check-updates'),
      settingAutoDownloadUpdates: document.getElementById('setting-auto-download-updates'),
      updateStatusIcon: document.getElementById('update-status-icon'),
      updateStatusMsg: document.getElementById('update-status-msg'),
      updateStatusSub: document.getElementById('update-status-sub'),
      updateProgressBarWrap: document.getElementById('update-progress-bar-wrap'),
      updateProgressBar: document.getElementById('update-progress-bar'),
      btnCheckUpdates: document.getElementById('btn-check-updates'),
      btnDownloadUpdate: document.getElementById('btn-download-update'),
      btnInstallUpdate: document.getElementById('btn-install-update'),
      btnViewReleaseNotes: document.getElementById('btn-view-release-notes'),
      releaseNotesModal: document.getElementById('release-notes-modal'),
      modalNotesVersion: document.getElementById('modal-notes-version'),
      releaseNotesBody: document.getElementById('release-notes-body'),
      modalNotesClose: document.getElementById('modal-notes-close'),
      modalNotesInstall: document.getElementById('modal-notes-install')
    };
  }

  // --- Toast Notification ---
  let toastTimer = null;
  function showToast(message) {
    if (!elements.toast) return;
    elements.toast.textContent = message;
    elements.toast.classList.remove('hidden');
    clearTimeout(toastTimer);
    toastTimer = setTimeout(() => {
      elements.toast.classList.add('hidden');
    }, 2200);
  }

  // --- Lively Aurora Waves Background Canvas ---
  let livelyAnimId = null;
  let livelyStartTime = null;

  function initLivelyCanvas() {
    const dpr = Math.min(window.devicePixelRatio || 1, 2);
    if (elements.livelyCanvas && elements.capsuleExpanded) {
      const rect = elements.capsuleExpanded.getBoundingClientRect();
      elements.livelyCanvas.width = Math.round((rect.width || 480) * dpr);
      elements.livelyCanvas.height = Math.round((rect.height || 360) * dpr);
    }
    if (elements.compactLivelyCanvas && elements.capsuleCompact) {
      const rect = elements.capsuleCompact.getBoundingClientRect();
      elements.compactLivelyCanvas.width = Math.round((rect.width || 290) * dpr);
      elements.compactLivelyCanvas.height = Math.round((rect.height || 44) * dpr);
    }
  }

  function startLivelyAnimation() {
    if (livelyAnimId) return;
    initLivelyCanvas();
    livelyStartTime = performance.now();

    function renderFrame(now) {
      if (state.settings.backgroundType !== 'lively') {
        stopLivelyAnimation();
        return;
      }

      const canvas = state.isExpanded ? elements.livelyCanvas : elements.compactLivelyCanvas;
      if (!canvas) {
        livelyAnimId = requestAnimationFrame(renderFrame);
        return;
      }

      const ctx = canvas.getContext('2d');
      if (!ctx) return;

      const w = canvas.width;
      const h = canvas.height;
      if (w === 0 || h === 0) {
        initLivelyCanvas();
      }
      const t = (now - livelyStartTime) * 0.001;

      ctx.clearRect(0, 0, w, h);

      // Deep atmospheric background
      const baseGrad = ctx.createLinearGradient(0, 0, w, h);
      if (state.settings.themeMode === 'light') {
        baseGrad.addColorStop(0, '#e0f2fe');
        baseGrad.addColorStop(0.5, '#f0fdf4');
        baseGrad.addColorStop(1, '#f8fafc');
      } else {
        baseGrad.addColorStop(0, '#020617');
        baseGrad.addColorStop(0.5, '#070f22');
        baseGrad.addColorStop(1, '#020617');
      }
      ctx.fillStyle = baseGrad;
      ctx.fillRect(0, 0, w, h);

      const isExp = state.isExpanded;

      // Aurora Wave 1 (Cyan / Emerald Glow)
      drawAuroraWave(ctx, w, h, t * 0.8, {
        colorStart: state.settings.themeMode === 'light' ? 'rgba(6, 182, 212, 0.45)' : 'rgba(6, 182, 212, 0.55)',
        colorMid: state.settings.themeMode === 'light' ? 'rgba(16, 185, 129, 0.3)' : 'rgba(16, 185, 129, 0.4)',
        colorEnd: 'rgba(0, 0, 0, 0)',
        baseY: h * 0.52,
        amplitude: h * (isExp ? 0.24 : 0.35),
        freq: isExp ? 0.0035 : 0.006,
        speed: 1.1
      });

      // Aurora Wave 2 (Indigo / Purple Glow)
      drawAuroraWave(ctx, w, h, t * 0.6 + 2.0, {
        colorStart: state.settings.themeMode === 'light' ? 'rgba(168, 85, 247, 0.35)' : 'rgba(168, 85, 247, 0.45)',
        colorMid: state.settings.themeMode === 'light' ? 'rgba(59, 130, 246, 0.25)' : 'rgba(59, 130, 246, 0.35)',
        colorEnd: 'rgba(0, 0, 0, 0)',
        baseY: h * 0.44,
        amplitude: h * (isExp ? 0.2 : 0.3),
        freq: isExp ? 0.0045 : 0.007,
        speed: -0.85
      });

      // Aurora Wave 3 (Rose / Amber Ambient Accent)
      drawAuroraWave(ctx, w, h, t * 0.4 + 4.2, {
        colorStart: state.settings.themeMode === 'light' ? 'rgba(244, 63, 94, 0.25)' : 'rgba(244, 63, 94, 0.3)',
        colorMid: 'rgba(245, 158, 11, 0.18)',
        colorEnd: 'rgba(0, 0, 0, 0)',
        baseY: h * 0.65,
        amplitude: h * (isExp ? 0.16 : 0.25),
        freq: isExp ? 0.003 : 0.005,
        speed: 0.7
      });

      livelyAnimId = requestAnimationFrame(renderFrame);
    }

    livelyAnimId = requestAnimationFrame(renderFrame);
  }

  function drawAuroraWave(ctx, w, h, t, opt) {
    ctx.save();
    const grad = ctx.createLinearGradient(0, opt.baseY - opt.amplitude, 0, h);
    grad.addColorStop(0, opt.colorStart);
    grad.addColorStop(0.5, opt.colorMid);
    grad.addColorStop(1, opt.colorEnd);

    ctx.fillStyle = grad;
    ctx.beginPath();
    ctx.moveTo(0, h);

    const step = 10;
    for (let x = 0; x <= w + step; x += step) {
      const y = opt.baseY +
        Math.sin(x * opt.freq + t * opt.speed) * opt.amplitude * 0.7 +
        Math.cos(x * opt.freq * 0.6 + t * 0.5) * opt.amplitude * 0.3;
      if (x === 0) ctx.lineTo(x, y);
      else ctx.lineTo(x, y);
    }

    ctx.lineTo(w, h);
    ctx.closePath();
    ctx.fill();
    ctx.restore();
  }

  function stopLivelyAnimation() {
    if (livelyAnimId) {
      cancelAnimationFrame(livelyAnimId);
      livelyAnimId = null;
    }
  }

  // --- Themes & Appearance Management ---
  const PHOTO_PRESETS = {
    preset1: 'linear-gradient(135deg, #1e1b4b 0%, #4338ca 50%, #3b82f6 100%)',
    preset2: 'linear-gradient(135deg, #450a0a 0%, #991b1b 50%, #f59e0b 100%)',
    preset3: 'linear-gradient(135deg, #022c22 0%, #065f46 50%, #10b981 100%)',
    preset4: 'linear-gradient(135deg, #090d16 0%, #1e293b 50%, #334155 100%)',
    spiderverse: 'templates/spiderverse.gif',
    cat: 'templates/cat.gif',
    stitch: 'templates/stitch.gif'
  };

  function applyTheme(themeMode, backgroundType, customBackground) {
    const validThemes = ['dark', 'light'];
    const validBgs = ['glass', 'lively', 'photo'];

    const currentMode = validThemes.includes(themeMode) ? themeMode : 'dark';
    const currentBg = validBgs.includes(backgroundType) ? backgroundType : 'glass';
    const customBg = customBackground || 'preset1';

    state.settings.themeMode = currentMode;
    state.settings.backgroundType = currentBg;
    state.settings.customBackground = customBg;

    // 1. Update Body classes
    document.body.className = `theme-${currentMode} bg-${currentBg}`;
    if (state.settings.reducedMotion) {
      document.body.classList.add('reduced-motion');
    }

    // 2. Update Theme buttons active state
    if (elements.btnThemeDark) elements.btnThemeDark.classList.toggle('active', currentMode === 'dark');
    if (elements.btnThemeLight) elements.btnThemeLight.classList.toggle('active', currentMode === 'light');

    // 3. Update Backdrop Style buttons active state
    if (elements.bgOptGlass) elements.bgOptGlass.classList.toggle('active', currentBg === 'glass');
    if (elements.bgOptLively) elements.bgOptLively.classList.toggle('active', currentBg === 'lively');
    if (elements.bgOptPhoto) elements.bgOptPhoto.classList.toggle('active', currentBg === 'photo');

    // 4. Update Current Theme Badge
    if (elements.currentThemeBadge) {
      const modeLabel = currentMode === 'dark' ? 'Dark' : 'Light';
      const bgLabel = currentBg === 'glass' ? 'Glass Blur' : (currentBg === 'lively' ? 'Lively Motion' : 'Photo Image');
      elements.currentThemeBadge.textContent = `${modeLabel} • ${bgLabel}`;
    }

    // 5. Manage Backdrop layers
    if (currentBg === 'lively') {
      if (elements.photoBackdrop) {
        elements.photoBackdrop.classList.add('hidden');
        elements.photoBackdrop.style.background = 'none';
      }
      if (elements.compactPhotoBackdrop) {
        elements.compactPhotoBackdrop.classList.add('hidden');
        elements.compactPhotoBackdrop.style.background = 'none';
      }
      if (elements.photoConfigPanel) elements.photoConfigPanel.classList.add('hidden');
      if (elements.livelyCanvas) elements.livelyCanvas.classList.remove('hidden');
      if (elements.compactLivelyCanvas) elements.compactLivelyCanvas.classList.remove('hidden');
      startLivelyAnimation();
    } else if (currentBg === 'photo') {
      stopLivelyAnimation();
      if (elements.livelyCanvas) elements.livelyCanvas.classList.add('hidden');
      if (elements.compactLivelyCanvas) elements.compactLivelyCanvas.classList.add('hidden');
      if (elements.photoBackdrop) elements.photoBackdrop.classList.remove('hidden');
      if (elements.compactPhotoBackdrop) elements.compactPhotoBackdrop.classList.remove('hidden');
      if (elements.photoConfigPanel) elements.photoConfigPanel.classList.remove('hidden');

      // Apply background image or GIF animation or preset
      let bgStyle = PHOTO_PRESETS.preset1;
      if (customBg && (customBg.startsWith('data:image') || customBg.startsWith('http') || customBg.startsWith('file://'))) {
        bgStyle = `url("${customBg}") center/cover no-repeat`;
      } else if (customBg === 'spiderverse' || customBg === 'cat' || customBg === 'stitch' || customBg === 'vegeta' || customBg === 'sakura') {
        bgStyle = `#0a0d14 url("templates/${customBg}.gif") center/contain no-repeat`;
      } else if (PHOTO_PRESETS[customBg]) {
        bgStyle = PHOTO_PRESETS[customBg];
      }
      if (elements.photoBackdrop) elements.photoBackdrop.style.background = bgStyle;
      if (elements.compactPhotoBackdrop) elements.compactPhotoBackdrop.style.background = bgStyle;

      // Highlight active photo preset thumb
      if (elements.photoThumbs) {
        elements.photoThumbs.forEach(thumb => {
          thumb.classList.toggle('active', thumb.dataset.preset === customBg);
        });
      }
    } else {
      // Glass (frosted see-through background)
      stopLivelyAnimation();
      if (elements.livelyCanvas) elements.livelyCanvas.classList.add('hidden');
      if (elements.compactLivelyCanvas) elements.compactLivelyCanvas.classList.add('hidden');
      if (elements.photoBackdrop) {
        elements.photoBackdrop.classList.add('hidden');
        elements.photoBackdrop.style.background = 'none';
      }
      if (elements.compactPhotoBackdrop) {
        elements.compactPhotoBackdrop.classList.add('hidden');
        elements.compactPhotoBackdrop.style.background = 'none';
      }
      if (elements.photoConfigPanel) elements.photoConfigPanel.classList.add('hidden');
    }
  }

  async function setThemeMode(mode) {
    applyTheme(mode, state.settings.backgroundType, state.settings.customBackground);
    await callNative('save_settings', { settings: { themeMode: mode } });
    showToast(`Theme set to ${mode === 'dark' ? 'Dark' : 'Light'} Mode`);
  }

  async function setBackgroundType(bg) {
    applyTheme(state.settings.themeMode, bg, state.settings.customBackground);
    await callNative('save_settings', { settings: { backgroundType: bg } });
    const name = bg === 'glass' ? 'Glass Blur (See Wallpaper)' : (bg === 'lively' ? 'Lively Animated Motion' : 'Photo Image');
    showToast(`Backdrop set to ${name}`);
  }

  async function setCustomPhoto(val) {
    state.settings.customBackground = val;
    applyTheme(state.settings.themeMode, 'photo', val);
    await callNative('save_settings', { settings: { backgroundType: 'photo', customBackground: val } });
    showToast('Photo backdrop applied');
  }

  // --- Dynamic Island Animation Templates ---
  const ISLAND_ANIMATIONS = {
    spiderverse: 'templates/spiderverse.gif',
    cat: 'templates/cat.gif',
    stitch: 'templates/stitch.gif',
    vegeta: 'templates/vegeta.gif',
    sakura: 'templates/sakura.gif'
  };

  async function setIslandAnimation(anim, save = true) {
    const valid = ['spiderverse', 'cat', 'stitch', 'vegeta', 'sakura', 'none'];
    const chosen = valid.includes(anim) ? anim : 'spiderverse';
    state.settings.islandAnimation = chosen;

    if (elements.compactAnimSlot && elements.compactAnimImg) {
      if (chosen === 'none') {
        elements.compactAnimSlot.classList.add('hidden');
      } else {
        elements.compactAnimSlot.classList.remove('hidden');
        elements.compactAnimImg.src = ISLAND_ANIMATIONS[chosen] || ISLAND_ANIMATIONS.spiderverse;
        elements.compactAnimImg.alt = chosen;
      }
    }

    if (elements.companionBtns) {
      elements.companionBtns.forEach(btn => {
        btn.classList.toggle('active', btn.dataset.anim === chosen);
      });
    }

    if (save) {
      await callNative('save_settings', { settings: { islandAnimation: chosen } });
      const names = { spiderverse: 'Spider-Verse', cat: 'Cute Cat', stitch: 'Cute Stitch', vegeta: 'Vegeta Ultra Ego', sakura: 'Sakura Forest (Minecraft)', none: 'Disabled' };
      showToast(`Island Animation: ${names[chosen] || chosen}`);
    }
  }

  // --- Software Updates Controller ---
  let latestUpdateData = null;

  function handleUpdateStatusChange(info) {
    if (!info) return;
    latestUpdateData = info;

    const { state: updateState, currentVersion, latestVersion, releaseNotes, percent, message, error } = info;

    if (elements.updateStatusMsg) {
      elements.updateStatusMsg.textContent = message || (updateState === 'checking' ? 'Checking for updates...' : 'System status normal');
    }

    if (elements.updateProgressBarWrap && elements.updateProgressBar) {
      if (updateState === 'downloading') {
        elements.updateProgressBarWrap.classList.remove('hidden');
        elements.updateProgressBar.style.width = `${percent || 0}%`;
        if (elements.updateStatusSub) {
          elements.updateStatusSub.textContent = `Downloading update: ${percent || 0}%`;
        }
      } else {
        elements.updateProgressBarWrap.classList.add('hidden');
        if (elements.updateStatusSub) {
          elements.updateStatusSub.textContent = updateState === 'ready_to_install' ? 'Authenticode signature verified & ready' : 'Official GitHub Releases Channel';
        }
      }
    }

    if (elements.updateBadgeStatus) {
      if (updateState === 'checking') {
        elements.updateBadgeStatus.textContent = 'Checking...';
      } else if (updateState === 'available') {
        elements.updateBadgeStatus.textContent = `v${latestVersion} Available`;
      } else if (updateState === 'downloading') {
        elements.updateBadgeStatus.textContent = `Downloading (${percent || 0}%)`;
      } else if (updateState === 'ready_to_install') {
        elements.updateBadgeStatus.textContent = `v${latestVersion} Ready`;
      } else if (updateState === 'error') {
        elements.updateBadgeStatus.textContent = 'Check Failed';
      } else {
        elements.updateBadgeStatus.textContent = 'Up to date';
      }
    }

    if (elements.updateStatusIcon) {
      elements.updateStatusIcon.classList.toggle('spin', updateState === 'checking');
      elements.updateStatusIcon.classList.toggle('success', updateState === 'ready_to_install');
      elements.updateStatusIcon.classList.toggle('error', updateState === 'error');
    }

    if (elements.btnCheckUpdates) {
      elements.btnCheckUpdates.classList.toggle('hidden', updateState === 'checking' || updateState === 'downloading');
    }
    if (elements.btnDownloadUpdate) {
      elements.btnDownloadUpdate.classList.toggle('hidden', updateState !== 'available');
    }
    if (elements.btnInstallUpdate) {
      elements.btnInstallUpdate.classList.toggle('hidden', updateState !== 'ready_to_install');
    }
    if (elements.btnViewReleaseNotes) {
      elements.btnViewReleaseNotes.classList.toggle('hidden', !releaseNotes);
    }
    if (elements.modalNotesInstall) {
      elements.modalNotesInstall.classList.toggle('hidden', updateState !== 'ready_to_install');
    }

    if (updateState === 'ready_to_install') {
      showToast(`Update v${latestVersion} is verified and ready to install!`);
    } else if (updateState === 'error' && error) {
      showToast(error);
    }
  }

  // --- Window Expand & Collapse ---
  async function expandIsland() {
    if (state.isExpanded) return;
    state.isExpanded = true;

    // 1. Immediately switch DOM state to expanded so UI opens instantly
    if (elements.container) {
      elements.container.classList.remove('compact');
      elements.container.classList.add('expanded');
    }
    if (elements.capsuleCompact) {
      elements.capsuleCompact.setAttribute('aria-expanded', 'true');
    }

    const targetWidth = Math.max(480, state.settings.expandedWidth || 480);
    const targetHeight = Math.max(360, state.settings.expandedHeight || 360);

    // 2. Resize native window
    callNative('resize_window', {
      width: targetWidth,
      height: targetHeight
    });

    initLivelyCanvas();
    if (state.settings.backgroundType === 'lively') {
      startLivelyAnimation();
    }
  }

  async function collapseIsland() {
    if (!state.isExpanded) return;
    state.isExpanded = false;

    // 1. Immediately switch back to compact pill UI
    if (elements.container) {
      elements.container.classList.remove('expanded');
      elements.container.classList.add('compact');
    }
    if (elements.capsuleCompact) {
      elements.capsuleCompact.setAttribute('aria-expanded', 'false');
    }

    const targetWidth = Math.max(290, state.settings.compactWidth || 290);
    const targetHeight = Math.max(44, state.settings.compactHeight || 44);

    // 2. Resize native window back to compact size
    callNative('resize_window', {
      width: targetWidth,
      height: targetHeight
    });

    initLivelyCanvas();
    if (state.settings.backgroundType === 'lively') {
      startLivelyAnimation();
    }
  }

  // --- Tab Navigation ---
  function switchTab(tabName) {
    state.activeTab = tabName;
    elements.navTabs.forEach(tab => {
      const active = (tab.dataset.tab === tabName);
      tab.classList.toggle('active', active);
      tab.setAttribute('aria-selected', active ? 'true' : 'false');
    });

    elements.tabPanels.forEach(panel => {
      panel.classList.toggle('active', panel.id === `view-${tabName}`);
    });

    callNative('save_settings', { settings: { activeTab: tabName } });
  }

  // --- File List Management ---
  function formatBytes(bytes) {
    if (bytes === 0) return '0 B';
    const k = 1024;
    const sizes = ['B', 'KB', 'MB', 'GB', 'TB'];
    const i = Math.floor(Math.log(bytes) / Math.log(k));
    return parseFloat((bytes / Math.pow(k, i)).toFixed(1)) + ' ' + sizes[i];
  }

  function getFileIcon(ext, isDir) {
    if (isDir) {
      return '<svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="#38bdf8" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M22 19a2 2 0 0 1-2 2H4a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2h5l2 3h9a2 2 0 0 1 2 2z"></path></svg>';
    }
    const e = ext.toLowerCase();
    if (['.jpg', '.jpeg', '.png', '.gif', '.webp', '.svg', '.bmp', '.ico'].includes(e)) {
      return '<svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="#a78bfa" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><rect x="3" y="3" width="18" height="18" rx="2" ry="2"></rect><circle cx="8.5" cy="8.5" r="1.5"></circle><polyline points="21 15 16 10 5 21"></polyline></svg>';
    }
    if (['.mp3', '.wav', '.flac', '.m4a', '.aac', '.ogg', '.wma'].includes(e)) {
      return '<svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="#34d399" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M9 18V5l12-2v13"></path><circle cx="6" cy="18" r="3"></circle><circle cx="18" cy="16" r="3"></circle></svg>';
    }
    if (['.mp4', '.mkv', '.avi', '.mov', '.webm', '.wmv'].includes(e)) {
      return '<svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="#f43f5e" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><rect x="2" y="2" width="20" height="20" rx="2.18" ry="2.18"></rect><line x1="7" y1="2" x2="7" y2="22"></line><line x1="17" y1="2" x2="17" y2="22"></line><line x1="2" y1="12" x2="22" y2="12"></line><line x1="2" y1="7" x2="7" y2="7"></line><line x1="2" y1="17" x2="7" y2="17"></line><line x1="17" y1="17" x2="22" y2="17"></line><line x1="17" y1="7" x2="22" y2="7"></line></svg>';
    }
    if (['.zip', '.rar', '.7z', '.tar', '.gz', '.bz2', '.iso'].includes(e)) {
      return '<svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="#fbbf24" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><line x1="16.5" y1="9.4" x2="7.5" y2="4.21"></line><path d="M21 16V8a2 2 0 0 0-1-1.73l-7-4a2 2 0 0 0-2 0l-7 4A2 2 0 0 0 3 8v8a2 2 0 0 0 1 1.73l7 4a2 2 0 0 0 2 0l7-4A2 2 0 0 0 21 16z"></path><polyline points="3.27 6.96 12 12.01 20.73 6.96"></polyline><line x1="12" y1="22.08" x2="12" y2="12"></line></svg>';
    }
    if (['.pdf'].includes(e)) {
      return '<svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="#ef4444" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M14 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V8z"></path><polyline points="14 2 14 8 20 8"></polyline><line x1="16" y1="13" x2="8" y2="13"></line><line x1="16" y1="17" x2="8" y2="17"></line><polyline points="10 9 9 9 8 9"></polyline></svg>';
    }
    if (['.doc', '.docx', '.txt', '.rtf', '.md'].includes(e)) {
      return '<svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="#60a5fa" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M14 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V8z"></path><polyline points="14 2 14 8 20 8"></polyline><line x1="16" y1="13" x2="8" y2="13"></line><line x1="16" y1="17" x2="8" y2="17"></line><line x1="10" y1="9" x2="8" y2="9"></line></svg>';
    }
    if (['.cpp', '.h', '.c', '.hpp', '.js', '.ts', '.py', '.html', '.css', '.json', '.rs', '.go'].includes(e)) {
      return '<svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="#22d3ee" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><polyline points="16 18 22 12 16 6"></polyline><polyline points="8 6 2 12 8 18"></polyline></svg>';
    }
    if (['.exe', '.msi', '.bat', '.cmd', '.ps1'].includes(e)) {
      return '<svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="#f97316" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><rect x="4" y="4" width="16" height="16" rx="2" ry="2"></rect><rect x="9" y="9" width="6" height="6"></rect><line x1="9" y1="1" x2="9" y2="4"></line><line x1="15" y1="1" x2="15" y2="4"></line><line x1="9" y1="20" x2="9" y2="23"></line><line x1="15" y1="20" x2="15" y2="23"></line><line x1="20" y1="9" x2="23" y2="9"></line><line x1="20" y1="14" x2="23" y2="14"></line><line x1="1" y1="9" x2="4" y2="9"></line><line x1="1" y1="14" x2="4" y2="14"></line></svg>';
    }
    return '<svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="#94a3b8" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M13 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V9z"></path><polyline points="13 2 13 9 20 9"></polyline></svg>';
  }

  function renderFileList() {
    if (!elements.filesList) return;
    elements.filesList.innerHTML = '';
    
    const count = state.files.length;
    if (elements.filesCount) {
      elements.filesCount.textContent = `${count} item${count === 1 ? '' : 's'}`;
    }

    // Update compact badge & subtext
    if (elements.compactFileBadge && elements.compactBadgeCount) {
      if (count > 0) {
        elements.compactBadgeCount.textContent = count;
        elements.compactFileBadge.classList.remove('hidden');
      } else {
        elements.compactFileBadge.classList.add('hidden');
      }
    }
    updateCompactDisplay();

    if (count === 0) {
      elements.filesList.innerHTML = `
        <li class="empty-drop-state">
          <div class="empty-illustration">
            <svg width="36" height="36" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.6" stroke-linecap="round" stroke-linejoin="round"><path d="M22 19a2 2 0 0 1-2 2H4a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2h5l2 3h9a2 2 0 0 1 2 2z"></path></svg>
          </div>
          <div class="empty-title">No files dropped yet</div>
          <div class="empty-desc">Drag any file or directory from Windows File Explorer onto this capsule to inspect its path.</div>
        </li>
      `;
      return;
    }

    state.files.forEach((file) => {
      const li = document.createElement('li');
      li.className = 'file-card-item';

      const icon = getFileIcon(file.extension || '', file.isDirectory);
      const extText = file.isDirectory ? 'DIR' : (file.extension ? file.extension.replace('.', '').toUpperCase() : 'FILE');
      const sizeStr = file.isDirectory ? 'Directory' : formatBytes(file.size || 0);

      li.innerHTML = `
        <div class="file-card-left">
          <span class="file-type-icon">${icon}</span>
          <div class="file-meta-col">
            <div class="file-name-row">
              <span class="file-name-text" title="${escapeHtml(file.name)}">${escapeHtml(file.name)}</span>
              <span class="file-ext-badge">${escapeHtml(extText)}</span>
            </div>
            <span class="file-path-sub" title="${escapeHtml(file.path)}">${escapeHtml(file.path)} &bull; ${sizeStr}</span>
          </div>
        </div>
        <div class="file-card-right">
          <button class="btn-card-action btn-copy-path" data-path="${escapeHtml(file.path)}" title="Copy exact path to clipboard">
            <svg width="11" height="11" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><rect x="9" y="9" width="13" height="13" rx="2" ry="2"></rect><path d="M5 15H4a2 2 0 0 1-2-2V4a2 2 0 0 1 2-2h9a2 2 0 0 1 2 2v1"></path></svg>
            <span>Copy</span>
          </button>
          <button class="btn-card-action btn-open-folder" data-path="${escapeHtml(file.path)}" title="Open containing folder in File Explorer">
            <svg width="11" height="11" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M22 19a2 2 0 0 1-2 2H4a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2h5l2 3h9a2 2 0 0 1 2 2z"></path></svg>
            <span>Folder</span>
          </button>
        </div>
      `;

      elements.filesList.appendChild(li);
    });

    // Attach copy & open handlers
    elements.filesList.querySelectorAll('.btn-copy-path').forEach(btn => {
      btn.addEventListener('click', async (e) => {
        e.stopPropagation();
        const path = btn.dataset.path;
        const res = await callNative('copy_path', { path });
        if (res.success) {
          const origText = btn.innerHTML;
          btn.innerHTML = '<span>Copied!</span>';
          btn.classList.add('copied');
          showToast('Exact path copied to clipboard');
          setTimeout(() => {
            btn.innerHTML = origText;
            btn.classList.remove('copied');
          }, 1800);
        }
      });
    });

    elements.filesList.querySelectorAll('.btn-open-folder').forEach(btn => {
      btn.addEventListener('click', async (e) => {
        e.stopPropagation();
        const path = btn.dataset.path;
        const res = await callNative('open_folder', { path });
        if (!res.success) {
          showToast(res.error || 'Failed to open folder');
        }
      });
    });
  }

  function escapeHtml(str) {
    if (!str) return '';
    return String(str).replace(/&/g, '&amp;')
                      .replace(/</g, '&lt;')
                      .replace(/>/g, '&gt;')
                      .replace(/"/g, '&quot;')
                      .replace(/'/g, '&#039;');
  }

  // --- Reminders Management ---
  function renderReminders() {
    if (!elements.remindersList) return;
    elements.remindersList.innerHTML = '';
    
    let filtered = state.reminders;
    if (state.reminderFilter === 'pending') {
      filtered = state.reminders.filter(r => !r.completed);
    } else if (state.reminderFilter === 'completed') {
      filtered = state.reminders.filter(r => r.completed);
    }

    if (filtered.length === 0) {
      elements.remindersList.innerHTML = `
        <li class="empty-drop-state">
          <div class="empty-illustration">
            <svg width="36" height="36" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.6" stroke-linecap="round" stroke-linejoin="round"><rect x="3" y="4" width="18" height="18" rx="2" ry="2"></rect><line x1="16" y1="2" x2="16" y2="6"></line><line x1="8" y1="2" x2="8" y2="6"></line><line x1="3" y1="10" x2="21" y2="10"></line></svg>
          </div>
          <div class="empty-title">No reminders found</div>
          <div class="empty-desc">Create a new task to organize your day.</div>
        </li>
      `;
      return;
    }

    filtered.forEach(rem => {
      const li = document.createElement('li');
      li.className = `reminder-row ${rem.completed ? 'done' : ''}`;
      const timeStr = rem.time ? ` at ${rem.time}` : '';
      const dateStr = rem.date ? `${rem.date}` : '';

      li.innerHTML = `
        <div class="rem-check-col">
          <input type="checkbox" class="ios-checkbox rem-checkbox" data-id="${rem.id}" ${rem.completed ? 'checked' : ''} aria-label="Mark task completed">
          <div class="rem-text-wrap">
            <div class="rem-title-text" title="${escapeHtml(rem.title)}">${escapeHtml(rem.title)}</div>
            <div class="rem-meta-sub">
              <span class="file-ext-badge">[${escapeHtml(rem.category || 'General')}]</span>
              <span>${escapeHtml(dateStr)}${escapeHtml(timeStr)}</span>
            </div>
          </div>
        </div>
        <button class="btn-del-task" data-id="${rem.id}" title="Delete reminder" aria-label="Delete reminder">
          <svg width="13" height="13" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.2"><polyline points="3 6 5 6 21 6"></polyline><path d="M19 6v14a2 2 0 0 1-2 2H7a2 2 0 0 1-2-2V6m3 0V4a2 2 0 0 1 2-2h4a2 2 0 0 1 2 2v2"></path></svg>
        </button>
      `;
      elements.remindersList.appendChild(li);
    });

    // Checkbox toggles
    elements.remindersList.querySelectorAll('.rem-checkbox').forEach(cb => {
      cb.addEventListener('change', async () => {
        const id = cb.dataset.id;
        const target = state.reminders.find(r => r.id === id);
        if (target) {
          target.completed = cb.checked;
          await callNative('save_reminder', { reminder: target });
          renderReminders();
        }
      });
    });

    // Delete buttons
    elements.remindersList.querySelectorAll('.btn-del-task').forEach(btn => {
      btn.addEventListener('click', (e) => {
        e.stopPropagation();
        const id = btn.dataset.id;
        state.deleteTargetId = id;
        if (elements.confirmModal) {
          elements.confirmModal.classList.remove('hidden');
        }
      });
    });
  }

  // --- Live Metrics & Media ---
  async function updateSystemStats() {
    try {
      const res = await callNative('get_system_stats');
      if (res && res.success && res.stats) {
        const s = res.stats;
        const cpuPct = Math.min(100, Math.max(0, Math.round(s.cpuPercent)));
        const ramPct = Math.min(100, Math.max(0, Math.round(s.ramPercent)));

        if (elements.cpuVal) elements.cpuVal.textContent = `${cpuPct}%`;
        if (elements.cpuBar) elements.cpuBar.style.width = `${cpuPct}%`;

        if (elements.ramVal) elements.ramVal.textContent = `${ramPct}%`;
        if (elements.ramBar) elements.ramBar.style.width = `${ramPct}%`;
        if (elements.ramDetail) elements.ramDetail.textContent = `${s.ramUsedMb} / ${s.ramTotalMb} MB`;

        if (elements.powerVal && elements.powerBar) {
          if (s.hasBattery) {
            elements.powerVal.textContent = `${s.batteryPercent}% ${s.isCharging ? '(Charging)' : ''}`;
            elements.powerBar.style.width = `${Math.min(100, Math.max(0, s.batteryPercent))}%`;
          } else {
            elements.powerVal.textContent = s.isCharging ? 'AC Powered' : 'Desktop Power';
            elements.powerBar.style.width = '100%';
          }
        }
      }
    } catch (_) {}
  }

  function updateCompactDisplay() {
    const fileCount = (state.files && Array.isArray(state.files)) ? state.files.length : 0;
    const m = state.mediaStatus;
    const isPlaying = !!(m && m.isPlaying);
    const hasSession = !!(m && m.hasActiveSession);

    // Keep mini-equalizer strictly bound to actual sound playback
    if (elements.compactEqualizer) {
      elements.compactEqualizer.classList.toggle('active', isPlaying);
      if (isPlaying && m) {
        const label = m.title ? `${m.title} (${m.appName || 'Audio'})` : (m.appName || 'Audio Active');
        elements.compactEqualizer.setAttribute('title', `Audio Activity: ${label}`);
      } else {
        elements.compactEqualizer.setAttribute('title', 'Audio Activity (Idle)');
      }
    }

    // 1. If files dropped, files display takes priority
    if (fileCount > 0) {
      if (elements.compactCenterContent) elements.compactCenterContent.classList.remove('audio-active');
      if (elements.compactBrand) elements.compactBrand.textContent = 'Files';
      if (elements.compactSeparator) elements.compactSeparator.style.display = 'inline';
      if (elements.compactSubtext) {
        elements.compactSubtext.style.display = 'inline-block';
        elements.compactSubtext.classList.remove('marquee');
        elements.compactSubtext.textContent = `${fileCount} file${fileCount === 1 ? '' : 's'} ready`;
        elements.compactSubtext.removeAttribute('title');
      }
      return;
    }

    // 2. If actively playing music: Show Platform + Separator + Scrolling Track Title
    if (isPlaying && m) {
      if (elements.compactCenterContent) elements.compactCenterContent.classList.add('audio-active');
      if (elements.compactBrand) elements.compactBrand.textContent = m.appName || 'YouTube';
      if (elements.compactSeparator) elements.compactSeparator.style.display = 'inline';
      if (elements.compactSubtext) {
        elements.compactSubtext.style.display = 'inline-flex';
        const titleText = m.title || 'Playing Audio';
        const fullTrack = m.artist ? `${m.artist} - ${titleText}` : titleText;
        
        elements.compactSubtext.title = fullTrack;
        elements.compactSubtext.classList.add('marquee');
        elements.compactSubtext.innerHTML = `
          <span class="compact-marquee-content">
            <span>${escapeHtml(fullTrack)}</span>
            <span class="marquee-spacer">&bull;</span>
            <span>${escapeHtml(fullTrack)}</span>
            <span class="marquee-spacer">&bull;</span>
          </span>
        `;
      }
      return;
    }

    // 3. If music is stopped/paused but media app (e.g. YouTube/Spotify) is open:
    // Show ONLY platform, NO title, and audio activity not working
    if (hasSession && m && m.appName) {
      if (elements.compactCenterContent) elements.compactCenterContent.classList.remove('audio-active');
      if (elements.compactBrand) elements.compactBrand.textContent = m.appName;
      if (elements.compactSeparator) elements.compactSeparator.style.display = 'none';
      if (elements.compactSubtext) {
        elements.compactSubtext.style.display = 'none';
        elements.compactSubtext.classList.remove('marquee');
        elements.compactSubtext.textContent = '';
        elements.compactSubtext.removeAttribute('title');
      }
      return;
    }

    // 4. If not open YouTube / no media active (Default idle state):
    // Show ONLY platform (K2 Island), NO title, and audio activity not working
    if (elements.compactCenterContent) elements.compactCenterContent.classList.remove('audio-active');
    if (elements.compactBrand) elements.compactBrand.textContent = 'K2 Island';
    if (elements.compactSeparator) elements.compactSeparator.style.display = 'none';
    if (elements.compactSubtext) {
      elements.compactSubtext.style.display = 'none';
      elements.compactSubtext.classList.remove('marquee');
      elements.compactSubtext.textContent = '';
      elements.compactSubtext.removeAttribute('title');
    }
  }

  function setMediaPlaying(isPlaying, media) {
    if (elements.musicDisc) {
      elements.musicDisc.classList.toggle('playing', isPlaying);
    }
    if (elements.compactEqualizer) {
      elements.compactEqualizer.classList.toggle('active', isPlaying);
      if (isPlaying && media) {
        const label = media.title ? `${media.title} (${media.appName || 'Audio'})` : (media.appName || 'Audio Active');
        elements.compactEqualizer.setAttribute('title', `Audio Activity: ${label}`);
      } else {
        elements.compactEqualizer.setAttribute('title', 'Audio Activity (Idle)');
      }
    }
    if (elements.mediaSoundwave) {
      elements.mediaSoundwave.classList.toggle('active', isPlaying);
    }
    if (elements.iconPlayPause) {
      if (isPlaying) {
        elements.iconPlayPause.innerHTML = '<rect x="6" y="4" width="4" height="16" fill="currentColor"></rect><rect x="14" y="4" width="4" height="16" fill="currentColor"></rect>';
      } else {
        elements.iconPlayPause.innerHTML = '<polygon points="5 3 19 12 5 21 5 3" fill="currentColor"></polygon>';
      }
    }
    if (isPlaying && elements.mediaSessionTag && elements.mediaSessionTag.textContent === 'Audio Idle') {
      elements.mediaSessionTag.textContent = (media && media.appName) ? media.appName : 'Active';
    }
  }

  async function updateMediaStatus() {
    try {
      const res = await callNative('get_media_status');
      if (res && res.success && res.media) {
        const m = res.media;
        state.mediaStatus = m;
        const isPlaying = !!(m.isPlaying);

        if (m.hasActiveSession) {
          if (elements.mediaTitle) elements.mediaTitle.textContent = m.title || 'Audio Session Active';
          if (elements.mediaArtist) elements.mediaArtist.textContent = m.artist ? `${m.artist} (${m.appName || 'Audio'})` : (m.appName ? `Playing via ${m.appName}` : 'Playing Audio');
          if (elements.mediaSessionTag) elements.mediaSessionTag.textContent = m.appName || 'Active';
        } else {
          if (elements.mediaTitle) elements.mediaTitle.textContent = 'No Media Active';
          if (elements.mediaArtist) elements.mediaArtist.textContent = 'Spotify • YouTube • Windows Media Player • VLC';
          if (elements.mediaSessionTag) elements.mediaSessionTag.textContent = 'Audio Idle';
        }

        setMediaPlaying(isPlaying, m);
        updateCompactDisplay();

        if (m.isMuted !== undefined) {
          if (elements.btnMediaMute) elements.btnMediaMute.classList.toggle('active', m.isMuted);
          if (elements.btnToolMute) elements.btnToolMute.classList.toggle('active', m.isMuted);
        }
      }
    } catch (_) {}
  }

  // --- Attach All UI Event Listeners (Synchronous & Non-Blocking) ---
  function setupEventListeners() {
    // 1. Dropdown Chevron & Compact Pill Expand Click
    if (elements.compactChevronBtn) {
      elements.compactChevronBtn.addEventListener('click', (e) => {
        e.preventDefault();
        e.stopPropagation();
        expandIsland();
      });
      elements.compactChevronBtn.addEventListener('mousedown', (e) => {
        e.stopPropagation();
      });
      elements.compactChevronBtn.addEventListener('keydown', (e) => {
        if (e.key === 'Enter' || e.key === ' ') {
          e.preventDefault();
          e.stopPropagation();
          expandIsland();
        }
      });
    }

    if (elements.compactEqualizer) {
      elements.compactEqualizer.addEventListener('click', (e) => {
        e.stopPropagation();
        expandIsland();
        switchTab('media');
      });
      elements.compactEqualizer.addEventListener('mousedown', (e) => {
        e.stopPropagation();
      });
    }

    if (elements.capsuleCompact) {
      elements.capsuleCompact.addEventListener('click', (e) => {
        // Don't expand only if user clicked specifically on the drag handle
        if (e.target.closest('.drag-handle')) return;
        expandIsland();
      });

      elements.capsuleCompact.addEventListener('keydown', (e) => {
        if (e.key === 'Enter' || e.key === ' ') {
          e.preventDefault();
          expandIsland();
        }
      });
    }

    // 0. Prevent window drag from stealing clicks on buttons and inputs
    document.querySelectorAll('button, input, select, .compact-chevron, .hdr-btn, .dock-btn, .seg-btn, .pill-btn, .filter-chip, .shortcut-tile, .ios-checkbox, .ios-switch, .file-card-item, .reminder-row, .compact-anim-slot, .mini-equalizer').forEach(el => {
      el.addEventListener('mousedown', (e) => {
        e.stopPropagation();
      });
    });

    // 2. Collapse Button Click
    if (elements.btnCollapse) {
      elements.btnCollapse.addEventListener('click', (e) => {
        e.stopPropagation();
        collapseIsland();
      });
    }

    // 3. Window Dragging Handlers (Restricted to Drag Handle and Island Header only)
    const handleDrag = (e) => {
      if (e.button === 0 && (e.target.closest('.drag-handle') || e.target.closest('.island-header'))) {
        if (!e.target.closest('button, input, select, .compact-chevron, .hdr-btn, .dock-btn, .seg-btn, .pill-btn, .filter-chip, .shortcut-tile, .file-card-item, .reminder-row, .ios-checkbox, .ios-switch, .compact-anim-slot, .mini-equalizer')) {
          callNative('drag_window');
        }
      }
    };

    document.querySelectorAll('.drag-handle, .island-header').forEach(el => {
      el.addEventListener('mousedown', handleDrag);
    });

    // 4. Navigation Tabs
    elements.navTabs.forEach(tab => {
      tab.addEventListener('click', (e) => {
        e.stopPropagation();
        switchTab(tab.dataset.tab);
      });
    });

    // 5. Always on Top & Tray Controls
    if (elements.btnPinToggle) {
      elements.btnPinToggle.addEventListener('click', async (e) => {
        e.stopPropagation();
        const newVal = !state.settings.alwaysOnTop;
        state.settings.alwaysOnTop = newVal;
        elements.btnPinToggle.classList.toggle('pinned', newVal);
        if (elements.settingAlwaysOnTop) elements.settingAlwaysOnTop.checked = newVal;
        await callNative('set_always_on_top', { enabled: newVal });
        await callNative('save_settings', { settings: { alwaysOnTop: newVal } });
        showToast(newVal ? 'Capsule pinned Always On Top (Centered)' : 'Always On Top disabled');
      });
    }

    // 6. Files Actions
    if (elements.btnCopyAllPaths) {
      elements.btnCopyAllPaths.addEventListener('click', async (e) => {
        e.stopPropagation();
        if (state.files.length === 0) return;
        const allPaths = state.files.map(f => f.path).join('\r\n');
        const res = await callNative('copy_path', { path: allPaths });
        if (res.success) {
          showToast(`All ${state.files.length} paths copied to clipboard`);
        }
      });
    }

    if (elements.btnClearFiles) {
      elements.btnClearFiles.addEventListener('click', (e) => {
        e.stopPropagation();
        state.files = [];
        renderFileList();
        showToast('File list cleared');
      });
    }

    // 7. Media Controls (Optimistic & Responsive Feedback)
    if (elements.btnMediaPlay) {
      elements.btnMediaPlay.addEventListener('click', async (e) => {
        e.stopPropagation();
        const currentlyPlaying = elements.musicDisc && elements.musicDisc.classList.contains('playing');
        const nextState = !currentlyPlaying;
        setMediaPlaying(nextState);

        elements.btnMediaPlay.style.transform = 'scale(0.88)';
        setTimeout(() => { if (elements.btnMediaPlay) elements.btnMediaPlay.style.transform = ''; }, 140);

        await callNative('media_command', { action: 'playPause' });
        setTimeout(updateMediaStatus, 350);
      });
    }

    if (elements.btnMediaNext) {
      elements.btnMediaNext.addEventListener('click', async (e) => {
        e.stopPropagation();
        elements.btnMediaNext.style.transform = 'scale(0.88) translateX(2px)';
        setTimeout(() => { if (elements.btnMediaNext) elements.btnMediaNext.style.transform = ''; }, 140);
        await callNative('media_command', { action: 'next' });
        setTimeout(updateMediaStatus, 400);
      });
    }

    if (elements.btnMediaPrev) {
      elements.btnMediaPrev.addEventListener('click', async (e) => {
        e.stopPropagation();
        elements.btnMediaPrev.style.transform = 'scale(0.88) translateX(-2px)';
        setTimeout(() => { if (elements.btnMediaPrev) elements.btnMediaPrev.style.transform = ''; }, 140);
        await callNative('media_command', { action: 'prev' });
        setTimeout(updateMediaStatus, 400);
      });
    }

    if (elements.btnMediaMute) {
      elements.btnMediaMute.addEventListener('click', async (e) => {
        e.stopPropagation();
        elements.btnMediaMute.style.transform = 'scale(0.88)';
        setTimeout(() => { if (elements.btnMediaMute) elements.btnMediaMute.style.transform = ''; }, 140);
        const res = await callNative('media_command', { action: 'mute' });
        const isMuted = (res && res.isMuted !== undefined) ? res.isMuted : !elements.btnMediaMute.classList.contains('active');
        elements.btnMediaMute.classList.toggle('active', isMuted);
        if (elements.btnToolMute) elements.btnToolMute.classList.toggle('active', isMuted);
        showToast(isMuted ? 'Volume Muted' : 'Volume Unmuted');
      });
    }

    // 8. Quick Tools
    if (elements.btnToolLock) {
      elements.btnToolLock.addEventListener('click', (e) => {
        e.stopPropagation();
        callNative('quick_action', { action: 'lock_pc' });
      });
    }
    if (elements.btnToolTaskmgr) {
      elements.btnToolTaskmgr.addEventListener('click', (e) => {
        e.stopPropagation();
        callNative('quick_action', { action: 'task_manager' });
        showToast('Task Manager launched');
      });
    }
    if (elements.btnToolMute) {
      elements.btnToolMute.addEventListener('click', async (e) => {
        e.stopPropagation();
        const res = await callNative('media_command', { action: 'mute' });
        const isMuted = (res && res.isMuted !== undefined) ? res.isMuted : !elements.btnToolMute.classList.contains('active');
        elements.btnToolMute.classList.toggle('active', isMuted);
        if (elements.btnMediaMute) elements.btnMediaMute.classList.toggle('active', isMuted);
        showToast(isMuted ? 'Volume Muted' : 'Volume Unmuted');
      });
    }

    // 9. Reminders Form & Filter
    if (elements.btnAddReminderToggle) {
      elements.btnAddReminderToggle.addEventListener('click', () => {
        if (!elements.addReminderForm) return;
        elements.addReminderForm.classList.toggle('hidden');
        if (!elements.addReminderForm.classList.contains('hidden') && elements.inputRemTitle) {
          elements.inputRemTitle.focus();
        }
      });
    }

    if (elements.btnCancelReminder) {
      elements.btnCancelReminder.addEventListener('click', () => {
        if (elements.addReminderForm) elements.addReminderForm.classList.add('hidden');
        if (elements.inputRemTitle) elements.inputRemTitle.value = '';
      });
    }

    if (elements.btnSaveReminder) {
      elements.btnSaveReminder.addEventListener('click', async () => {
        const title = elements.inputRemTitle ? elements.inputRemTitle.value.trim() : '';
        if (!title) {
          if (elements.inputRemTitle) elements.inputRemTitle.focus();
          showToast('Reminder title is required');
          return;
        }

        const reminder = {
          title,
          date: elements.inputRemDate ? elements.inputRemDate.value : '',
          time: elements.inputRemTime ? elements.inputRemTime.value : '',
          category: elements.inputRemCat ? elements.inputRemCat.value : 'General',
          completed: false,
          createdAt: new Date().toISOString()
        };

        const res = await callNative('save_reminder', { reminder });
        if (res.success) {
          reminder.id = res.reminderId;
          state.reminders.push(reminder);
          renderReminders();
          if (elements.inputRemTitle) elements.inputRemTitle.value = '';
          if (elements.addReminderForm) elements.addReminderForm.classList.add('hidden');
          showToast('Reminder saved locally');
        }
      });
    }

    elements.filterChips.forEach(chip => {
      chip.addEventListener('click', () => {
        elements.filterChips.forEach(c => c.classList.remove('active'));
        chip.classList.add('active');
        state.reminderFilter = chip.dataset.filter;
        renderReminders();
      });
    });

    // Confirmation Modal
    if (elements.modalBtnCancel) {
      elements.modalBtnCancel.addEventListener('click', () => {
        if (elements.confirmModal) elements.confirmModal.classList.add('hidden');
        state.deleteTargetId = null;
      });
    }

    if (elements.modalBtnConfirm) {
      elements.modalBtnConfirm.addEventListener('click', async () => {
        if (!state.deleteTargetId) return;
        const id = state.deleteTargetId;
        if (elements.confirmModal) elements.confirmModal.classList.add('hidden');
        state.deleteTargetId = null;

        const res = await callNative('delete_reminder', { id, confirmed: true });
        if (res.success) {
          state.reminders = state.reminders.filter(r => r.id !== id);
          renderReminders();
          showToast('Reminder deleted');
        } else {
          showToast('Failed to delete reminder');
        }
      });
    }

    // 10. Settings Toggles
    if (elements.settingAlwaysOnTop) {
      elements.settingAlwaysOnTop.addEventListener('change', async (e) => {
        state.settings.alwaysOnTop = e.target.checked;
        if (elements.btnPinToggle) elements.btnPinToggle.classList.toggle('pinned', e.target.checked);
        await callNative('set_always_on_top', { enabled: e.target.checked });
        await callNative('save_settings', { settings: { alwaysOnTop: e.target.checked } });
      });
    }

    if (elements.settingReducedMotion) {
      elements.settingReducedMotion.addEventListener('change', async (e) => {
        state.settings.reducedMotion = e.target.checked;
        document.body.classList.toggle('reduced-motion', e.target.checked);
        await callNative('save_settings', { settings: { reducedMotion: e.target.checked } });
      });
    }

    if (elements.settingStartup) {
      elements.settingStartup.addEventListener('change', async (e) => {
        state.settings.launchAtStartup = e.target.checked;
        await callNative('save_settings', { settings: { launchAtStartup: e.target.checked } });
        showToast(e.target.checked ? 'Launch at startup enabled' : 'Launch at startup disabled');
      });
    }

    // Software Updates Listeners
    if (elements.settingAutoCheckUpdates) {
      elements.settingAutoCheckUpdates.addEventListener('change', async (e) => {
        state.settings.autoCheckUpdates = e.target.checked;
        await callNative('save_settings', { settings: { autoCheckUpdates: e.target.checked } });
        showToast(`Automatic update checks ${e.target.checked ? 'enabled' : 'disabled'}`);
      });
    }

    if (elements.settingAutoDownloadUpdates) {
      elements.settingAutoDownloadUpdates.addEventListener('change', async (e) => {
        state.settings.autoDownloadUpdates = e.target.checked;
        await callNative('save_settings', { settings: { autoDownloadUpdates: e.target.checked } });
        showToast(`Automatic download ${e.target.checked ? 'enabled' : 'disabled'}`);
      });
    }

    if (elements.btnCheckUpdates) {
      elements.btnCheckUpdates.addEventListener('click', async () => {
        elements.btnCheckUpdates.disabled = true;
        showToast('Checking GitHub Releases for updates...');
        const res = await callNative('check_updates', { manual: true });
        elements.btnCheckUpdates.disabled = false;
        if (res && res.status) {
          handleUpdateStatusChange(res.status);
        }
      });
    }

    if (elements.btnDownloadUpdate) {
      elements.btnDownloadUpdate.addEventListener('click', async () => {
        showToast('Starting background update download...');
        await callNative('download_update');
      });
    }

    if (elements.btnInstallUpdate) {
      elements.btnInstallUpdate.addEventListener('click', async () => {
        showToast('Launching verified update installer...');
        const res = await callNative('install_update', { silent: false });
        if (res && !res.success && res.error) {
          showToast(`Install failed: ${res.error}`);
        }
      });
    }

    if (elements.btnViewReleaseNotes) {
      elements.btnViewReleaseNotes.addEventListener('click', () => {
        if (!latestUpdateData) return;
        if (elements.modalNotesVersion) elements.modalNotesVersion.textContent = `v${latestUpdateData.latestVersion || ''}`;
        if (elements.releaseNotesBody) elements.releaseNotesBody.textContent = latestUpdateData.releaseNotes || 'No release notes provided.';
        if (elements.releaseNotesModal) elements.releaseNotesModal.classList.remove('hidden');
      });
    }

    if (elements.modalNotesClose) {
      elements.modalNotesClose.addEventListener('click', () => {
        if (elements.releaseNotesModal) elements.releaseNotesModal.classList.add('hidden');
      });
    }

    if (elements.modalNotesInstall) {
      elements.modalNotesInstall.addEventListener('click', async () => {
        if (elements.releaseNotesModal) elements.releaseNotesModal.classList.add('hidden');
        showToast('Launching verified update installer...');
        await callNative('install_update', { silent: false });
      });
    }

    if (elements.btnQuitApp) {
      elements.btnQuitApp.addEventListener('click', () => {
        callNative('quit');
      });
    }

    // 11. Theme & Appearance
    if (elements.btnThemeDark) {
      elements.btnThemeDark.addEventListener('click', () => setThemeMode('dark'));
    }
    if (elements.btnThemeLight) {
      elements.btnThemeLight.addEventListener('click', () => setThemeMode('light'));
    }

    if (elements.bgOptGlass) {
      elements.bgOptGlass.addEventListener('click', () => setBackgroundType('glass'));
    }
    if (elements.bgOptLively) {
      elements.bgOptLively.addEventListener('click', () => setBackgroundType('lively'));
    }
    if (elements.bgOptPhoto) {
      elements.bgOptPhoto.addEventListener('click', () => setBackgroundType('photo'));
    }

    if (elements.photoThumbs) {
      elements.photoThumbs.forEach(thumb => {
        thumb.addEventListener('click', () => {
          const preset = thumb.dataset.preset;
          setCustomPhoto(preset);
        });
      });
    }

    if (elements.btnUploadPhoto) {
      elements.btnUploadPhoto.addEventListener('click', async (e) => {
        e.preventDefault();
        e.stopPropagation();
        try {
          const res = await callNative('choose_image');
          if (res && res.success && res.dataUrl) {
            setCustomPhoto(res.dataUrl);
            showToast('Dynamic Island GIF/photo wallpaper applied!');
            return;
          }
          if (res && res.cancelled) {
            return;
          }
        } catch (_) {}

        // Fallback to HTML input file picker
        if (elements.inputCustomPhoto) {
          elements.inputCustomPhoto.click();
        }
      });
    }

    if (elements.inputCustomPhoto) {
      elements.inputCustomPhoto.addEventListener('change', (e) => {
        const file = e.target.files && e.target.files[0];
        if (!file) return;
        const isImageOrGif = (file.type && file.type.startsWith('image/')) ||
                             /\.(gif|png|jpe?g|webp|bmp|svg)$/i.test(file.name);
        if (!isImageOrGif) {
          showToast('Please select a valid image or GIF file');
          return;
        }
        showToast('Loading Dynamic Island wallpaper...');
        const reader = new FileReader();
        reader.onload = (ev) => {
          setCustomPhoto(ev.target.result);
          showToast('Dynamic Island wallpaper applied!');
        };
        reader.readAsDataURL(file);
      });
    }

    if (elements.btnClearPhoto) {
      elements.btnClearPhoto.addEventListener('click', () => {
        setCustomPhoto('preset1');
        if (elements.inputCustomPhoto) elements.inputCustomPhoto.value = '';
        showToast('Photo backdrop reset to default');
      });
    }

    // 12. Dynamic Island Animation / Companion Buttons
    if (elements.companionBtns) {
      elements.companionBtns.forEach(btn => {
        btn.addEventListener('click', () => {
          const anim = btn.dataset.anim;
          setIslandAnimation(anim, true);
        });
      });
    }

    if (elements.compactAnimSlot) {
      elements.compactAnimSlot.addEventListener('click', (e) => {
        e.stopPropagation();
        elements.compactAnimSlot.style.transform = 'scale(1.3) rotate(8deg)';
        setTimeout(() => {
          if (elements.compactAnimSlot) elements.compactAnimSlot.style.transform = '';
        }, 180);
      });
    }

    // Drag & drop visual feedback
    window.addEventListener('dragover', (e) => {
      e.preventDefault();
      if (elements.capsuleCompact) elements.capsuleCompact.classList.add('drag-over');
      if (elements.fileDropZone) elements.fileDropZone.classList.add('dragover');
    });

    window.addEventListener('dragleave', (e) => {
      e.preventDefault();
      if (elements.capsuleCompact) elements.capsuleCompact.classList.remove('drag-over');
      if (elements.fileDropZone) elements.fileDropZone.classList.remove('dragover');
    });

    window.addEventListener('drop', (e) => {
      e.preventDefault();
      if (elements.capsuleCompact) elements.capsuleCompact.classList.remove('drag-over');
      if (elements.fileDropZone) elements.fileDropZone.classList.remove('dragover');

      // HTML5 file drop fallback
      if (e.dataTransfer && e.dataTransfer.files && e.dataTransfer.files.length > 0) {
        const dropped = [];
        for (let i = 0; i < e.dataTransfer.files.length; i++) {
          const f = e.dataTransfer.files[i];
          const path = f.path || f.name;
          const ext = f.name.includes('.') ? f.name.substring(f.name.lastIndexOf('.')) : '';
          dropped.push({
            name: f.name,
            path: path,
            extension: ext,
            size: f.size || 0,
            isDirectory: false
          });
        }
        if (dropped.length > 0) {
          state.files = [...dropped, ...state.files];
          renderFileList();
          expandIsland();
          switchTab('files');
          showToast(`${dropped.length} file(s) received`);
        }
      }
    });
  }

  // --- Initial Setup & Data Fetching ---
  async function init() {
    initElements();
    setupEventListeners();

    // Immediately apply default glass theme to guarantee instant translucent render
    applyTheme(state.settings.themeMode || 'dark', state.settings.backgroundType || 'glass', state.settings.customBackground || 'preset1');
    setIslandAnimation(state.settings.islandAnimation || 'spiderverse', false);

    // 1. Calendar Header
    const now = new Date();
    if (elements.calDayStr) {
      elements.calDayStr.textContent = now.toLocaleDateString(undefined, { weekday: 'long' });
    }
    if (elements.calDateStr) {
      elements.calDateStr.textContent = now.toLocaleDateString(undefined, { month: 'short', day: 'numeric', year: 'numeric' });
    }
    if (elements.inputRemDate) {
      elements.inputRemDate.valueAsDate = now;
    }

    // 2. Load Settings asynchronously (without blocking UI)
    try {
      const res = await callNative('get_settings');
      if (res && res.success && res.settings) {
        state.settings = { ...state.settings, ...res.settings };
        if (elements.settingAlwaysOnTop) elements.settingAlwaysOnTop.checked = state.settings.alwaysOnTop;
        if (elements.settingReducedMotion) elements.settingReducedMotion.checked = state.settings.reducedMotion;
        if (elements.settingStartup) elements.settingStartup.checked = state.settings.launchAtStartup;
        if (elements.settingAutoCheckUpdates) elements.settingAutoCheckUpdates.checked = state.settings.autoCheckUpdates !== false;
        if (elements.settingAutoDownloadUpdates) elements.settingAutoDownloadUpdates.checked = state.settings.autoDownloadUpdates !== false;
        document.body.classList.toggle('reduced-motion', state.settings.reducedMotion);
        if (elements.btnPinToggle) {
          elements.btnPinToggle.classList.toggle('pinned', state.settings.alwaysOnTop);
        }
        if (state.settings.activeTab) {
          switchTab(state.settings.activeTab);
        }
        // Apply loaded theme & backdrop
        applyTheme(
          state.settings.themeMode || 'dark',
          state.settings.backgroundType || 'glass',
          state.settings.customBackground || 'preset1'
        );
        // Apply loaded Island Animation template
        setIslandAnimation(state.settings.islandAnimation || 'spiderverse', false);
      } else {
        applyTheme('dark', 'glass', 'preset1');
        setIslandAnimation('spiderverse', false);
      }
    } catch (_) {
      applyTheme('dark', 'glass', 'preset1');
      setIslandAnimation('spiderverse', false);
    }

    // 3. App Info
    try {
      const info = await callNative('get_app_info');
      if (info && info.success && info.dataDirectory && elements.infoStoragePath) {
        elements.infoStoragePath.textContent = info.dataDirectory;
      }
    } catch (_) {}

    // 4. Load Reminders
    try {
      const remRes = await callNative('get_reminders');
      if (remRes && remRes.success && Array.isArray(remRes.reminders)) {
        state.reminders = remRes.reminders;
        renderReminders();
      }
    } catch (_) {}

    // 5. Software Update Status
    try {
      const upRes = await callNative('get_update_status');
      if (upRes && upRes.success && upRes.status) {
        handleUpdateStatusChange(upRes.status);
      }
    } catch (_) {}

    // 5. Periodic stats updates
    setInterval(updateSystemStats, 2000);
    setInterval(updateMediaStatus, 800);
    updateSystemStats();
    updateMediaStatus();
  }

  // Run initialization when DOM is loaded
  if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', init);
  } else {
    init();
  }

})();
