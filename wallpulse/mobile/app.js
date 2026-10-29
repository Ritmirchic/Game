const samples = [
  { id: 'aurora', title: 'Полярное сияние', subtitle: 'Сияние над фьордом', type: 'photo', duration: 'SVG', art: 'aurora', favourite: true },
  { id: 'moon', title: 'Тихая орбита', subtitle: 'Ночная сторона луны', type: 'photo', duration: 'SVG', art: 'moon' },
  { id: 'neon', title: 'Неоновый ритм', subtitle: 'Город после дождя', type: 'photo', duration: 'SVG', art: 'neon' },
  { id: 'ocean', title: 'Глубина', subtitle: 'Течение на закате', type: 'photo', duration: 'SVG', art: 'ocean' },
  { id: 'ember', title: 'Тёплый вечер', subtitle: 'Последний свет', type: 'photo', duration: 'SVG', art: 'ember' },
  { id: 'cloud', title: 'Облачный берег', subtitle: 'Море тумана', type: 'photo', duration: 'SVG', art: 'cloud' },
  { id: 'sakura', title: 'Сакура', subtitle: 'Весенний ветер', type: 'photo', duration: 'SVG', art: 'sakura' },
];
const typeNames = { video: 'ВИДЕО', photo: 'ФОТО', gif: 'GIF' };
const $ = (selector, root = document) => root.querySelector(selector);
const $$ = (selector, root = document) => [...root.querySelectorAll(selector)];
const dbPromise = openDatabase();
let imported = [];
let activeFilter = 'all';
let activeTab = 'library';
let currentItem = null;
let toastTimer;
let deferredInstallPrompt = null;
let previewObjectUrl = null;
const settings = Object.assign({ fit: 'cover', sound: false, dataSaver: false }, loadSettings());

function openDatabase() {
  return new Promise((resolve) => {
    if (!('indexedDB' in window)) return resolve(null);
    const request = indexedDB.open('wallpulse-pocket', 1);
    request.onupgradeneeded = () => {
      if (!request.result.objectStoreNames.contains('wallpapers')) request.result.createObjectStore('wallpapers', { keyPath: 'id' });
    };
    request.onsuccess = () => resolve(request.result);
    request.onerror = () => resolve(null);
  });
}
function loadSettings() {
  try { return JSON.parse(localStorage.getItem('wallpulse-pocket-settings') || '{}'); } catch { return {}; }
}
function saveSettings() {
  localStorage.setItem('wallpulse-pocket-settings', JSON.stringify(settings));
}
function allItems() { return [...samples, ...imported]; }
function getItem(id) { return allItems().find(item => item.id === id); }
function labelFor(item) { return item.type === 'video' ? 'Видео' : item.type === 'gif' ? 'GIF' : 'Фото'; }
function esc(value = '') { return String(value).replace(/[&<>"']/g, ch => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' })[ch]); }

async function loadImports() {
  const db = await dbPromise;
  if (!db) return;
  const tx = db.transaction('wallpapers', 'readonly');
  const request = tx.objectStore('wallpapers').getAll();
  request.onsuccess = () => {
    imported = (request.result || []).map(row => ({ ...row, url: row.blob && row.type !== 'video' ? URL.createObjectURL(row.blob) : null, art: 'uploaded', favourite: !!row.favourite }));
    render();
  };
}
async function persistImport(item) {
  const db = await dbPromise;
  if (!db) { toast('Браузер не поддерживает локальное хранилище'); return; }
  const tx = db.transaction('wallpapers', 'readwrite');
  const storedItem = { ...item };
  delete storedItem.url;
  tx.objectStore('wallpapers').put(storedItem);
  tx.oncomplete = () => { imported.unshift({ ...item, art: 'uploaded' }); render(); toast('Обои добавлены в вашу библиотеку'); };
  tx.onerror = () => toast('Не удалось сохранить файл. Попробуйте ещё раз.');
}
function countLabel(n) { const mod100 = n % 100; const mod10 = n % 10; const word = mod100 >= 11 && mod100 <= 14 ? 'вариантов' : mod10 === 1 ? 'вариант' : mod10 >= 2 && mod10 <= 4 ? 'варианта' : 'вариантов'; return `${n} ${word}`; }
function cardMarkup(item) {
  const type = typeNames[item.type] || 'ФАЙЛ';
  const source = item.url && item.type !== 'video' ? `<span class="wall-art art-uploaded" style="background-image:url('${esc(item.url)}')"></span>` : `<span class="wall-art art-${esc(item.art)}"></span>`;
  return `<article class="wall-card" data-wallpaper="${esc(item.id)}" aria-label="Открыть ${esc(item.title)}">${source}<span class="wall-type">${type}</span><button class="wall-heart ${item.favourite ? 'liked' : ''}" data-favorite="${esc(item.id)}" aria-label="${item.favourite ? 'Убрать из избранного' : 'В избранное'}">${item.favourite ? '♥' : '♡'}</button><span class="wall-meta"><strong>${esc(item.title)}</strong><small>${esc(item.subtitle || labelFor(item))} · ${esc(item.duration || labelFor(item))}</small></span></article>`;
}
function render() {
  const items = allItems();
  $('#wallpaper-count').textContent = countLabel(items.length);
  const libraryItems = items.filter(item => activeFilter === 'all' || item.type === activeFilter);
  $('#wallpaper-grid').innerHTML = libraryItems.map(cardMarkup).join('');
  const favourites = items.filter(item => item.favourite);
  $('#favorites-grid').innerHTML = favourites.map(cardMarkup).join('');
  $('#favorites-empty').hidden = favourites.length > 0;
}
function setTab(tab) {
  activeTab = tab;
  $$('.page').forEach(page => page.classList.toggle('active', page.id === `${tab}-page`));
  $$('.nav-item').forEach(button => {
    const selected = button.dataset.tab === tab;
    button.classList.toggle('active', selected);
    if (selected) button.setAttribute('aria-current', 'page'); else button.removeAttribute('aria-current');
  });
  window.scrollTo({ top: 0, behavior: 'smooth' });
}
function toast(message) {
  const element = $('#toast');
  element.textContent = message;
  element.classList.add('show');
  clearTimeout(toastTimer);
  toastTimer = setTimeout(() => element.classList.remove('show'), 2600);
}
function toggleFavourite(id) {
  const item = getItem(id);
  if (!item) return;
  item.favourite = !item.favourite;
  if (item.id.startsWith('upload-')) {
    dbPromise.then(db => {
      if (!db) return;
      const tx = db.transaction('wallpapers', 'readwrite');
      tx.objectStore('wallpapers').put({ ...item, url: undefined });
    });
  }
  render();
  if (currentItem?.id === id) updatePreviewFavorite();
  toast(item.favourite ? 'Добавлено в избранное' : 'Убрано из избранного');
}
function updatePreviewFavorite() {
  const button = $('#favorite-toggle');
  button.classList.toggle('liked', !!currentItem?.favourite);
  button.textContent = currentItem?.favourite ? '♥' : '♡';
  button.setAttribute('aria-label', currentItem?.favourite ? 'Убрать из избранного' : 'В избранное');
}
function openPreview(item) {
  if (!item) return;
  currentItem = item;
  const media = $('#preview-media');
  $('#preview-stage').style.setProperty('--backdrop-image', 'none');
  media.replaceChildren();
  if (previewObjectUrl) { URL.revokeObjectURL(previewObjectUrl); previewObjectUrl = null; }
  if (item.blob) {
    previewObjectUrl = URL.createObjectURL(item.blob);
    if (item.type === 'video') {
      const video = document.createElement('video');
      video.src = previewObjectUrl; video.autoplay = !settings.dataSaver; video.loop = true; video.playsInline = true; video.muted = !settings.sound; video.controls = settings.dataSaver; video.preload = settings.dataSaver ? 'none' : 'metadata';
      media.append(video);
      $('#preview-stage').style.setProperty('--backdrop-image', `url(\"${previewObjectUrl}\")`);
    } else {
      const image = document.createElement('img'); image.src = previewObjectUrl; image.alt = item.title; media.append(image);
      $('#preview-stage').style.setProperty('--backdrop-image', `url(\"${previewObjectUrl}\")`);
    }
  } else {
    const art = document.createElement('div'); art.className = `preview-art art-${item.art}`; media.append(art);
  }
  $('#preview-title').textContent = item.title;
  $('#preview-subtitle').textContent = item.subtitle || `${labelFor(item)} · ${item.name || ''}`;
  $('#save-wallpaper').innerHTML = item.type === 'video' || item.type === 'gif' ? 'Установить живые обои <span>✦</span>' : 'Сохранить обои <span>↓</span>';
  $('#preview-stage').dataset.fit = settings.fit;
  $('#preview-stage').style.setProperty('--preview-fit', settings.fit === 'contain' || settings.fit === 'blur' ? 'contain' : 'cover');
  $('#sound-toggle').textContent = settings.sound ? '◗' : '◖';
  $('#sound-toggle').setAttribute('aria-label', settings.sound ? 'Выключить звук' : 'Включить звук');
  updatePreviewFavorite();
  const dialog = $('#preview-dialog');
  if (!dialog.open) dialog.showModal();
}
function closePreview() {
  const video = $('#preview-media video');
  if (video) video.pause();
  $('#preview-dialog').close();
  currentItem = null;
  if (previewObjectUrl) { URL.revokeObjectURL(previewObjectUrl); previewObjectUrl = null; }
}
async function chooseFiles(fileList) {
  const files = [...fileList];
  for (const file of files) {
    if (!file.type.startsWith('image/') && !file.type.startsWith('video/')) { toast(`${file.name}: формат не поддерживается`); continue; }
    const ext = file.name.split('.').pop()?.toLowerCase();
    const type = file.type === 'image/gif' || ext === 'gif' ? 'gif' : file.type.startsWith('video/') || ['mp4', 'm4v', 'mov', 'webm', 'mkv'].includes(ext) ? 'video' : 'photo';
    const mimeType = type === 'gif' ? 'image/gif' : type === 'video' ? (file.type.startsWith('video/') ? file.type : ext === 'webm' ? 'video/webm' : 'video/mp4') : file.type || 'image/jpeg';
    const item = { id: `upload-${crypto.randomUUID?.() || `${Date.now()}-${Math.random().toString(16).slice(2)}`}`, title: file.name.replace(/\.[^.]+$/, '').slice(0, 48) || 'Мои обои', subtitle: 'Добавлено с телефона', name: file.name, type, mimeType, duration: type === 'photo' ? 'Фото' : 'Новое', art: 'uploaded', blob: file, url: type === 'video' ? null : URL.createObjectURL(file), favourite: false, createdAt: Date.now() };
    await persistImport(item);
  }
}
async function downloadCurrent() {
  if (!currentItem) return;
  let blob = currentItem.blob;
  let fileName = currentItem.name || `${currentItem.title.replace(/[^\p{L}\p{N}-]+/gu, '-')}.${currentItem.type === 'photo' ? 'jpg' : currentItem.type === 'gif' ? 'gif' : 'mp4'}`;
  if (!blob) {
    blob = new Blob([demoWallpaperSvg(currentItem)], { type: 'image/svg+xml' });
    fileName = `wallpulse-${currentItem.art}.svg`;
    toast('Демо-сцена сохранена как изображение');
  }
  if (!blob) { toast('Сначала добавьте файл в библиотеку, чтобы сохранить его'); return; }
  // The native Android wrapper saves through MediaStore and can open the
  // system wallpaper picker. Browsers continue through Web Share/download.
  if (blob.type === 'image/svg+xml' && currentItem?.art) {
    try {
      const bitmap = await createImageBitmap(blob);
      const canvas = document.createElement('canvas'); canvas.width = bitmap.width || 1080; canvas.height = bitmap.height || 1920;
      canvas.getContext('2d').drawImage(bitmap, 0, 0, canvas.width, canvas.height); bitmap.close();
      blob = await new Promise(resolve => canvas.toBlob(resolve, 'image/jpeg', .94));
      fileName = `wallpulse-${currentItem.art}.jpg`;
    } catch { /* retain the original SVG for browsers that can save it */ }
  }
  if (window.AndroidWallPulse?.beginSave) {
    const bridge = window.AndroidWallPulse;
    const mime = currentItem.mimeType || blob.type || 'application/octet-stream';
    const button = $('#save-wallpaper');
    button.disabled = true;
    toast('Сохраняем файл на устройство…');
    try {
      if (!bridge.beginSave(fileName, currentItem.mimeType || mime)) { toast('Не удалось начать сохранение'); return; }
      const chunkSize = 512 * 1024;
      for (let offset = 0; offset < blob.size; offset += chunkSize) {
        const bytes = new Uint8Array(await blob.slice(offset, Math.min(offset + chunkSize, blob.size)).arrayBuffer());
        let binary = '';
        for (let i = 0; i < bytes.length; i += 0x8000) binary += String.fromCharCode(...bytes.subarray(i, i + 0x8000));
        if (!bridge.writeChunk(btoa(binary))) { bridge.cancelSave(); toast('Не удалось записать файл'); return; }
      }
      const saved = bridge.finishSave();
      if (saved) toast(currentItem.type === 'video' || currentItem.type === 'gif' ? 'Открылся предпросмотр живых обоев — нажмите «Установить»' : 'Открылся системный выбор обоев');
    } catch (error) {
      bridge.cancelSave();
      toast('Не удалось сохранить файл');
    } finally { button.disabled = false; }
    return;
  }
  const mime = currentItem.mimeType || blob.type || 'application/octet-stream';
  const file = new File([blob], fileName, { type: mime });
  try {
    if (navigator.canShare?.({ files: [file] }) && navigator.share) {
      await navigator.share({ files: [file], title: currentItem.title });
      toast('Файл готов — сохраните его на устройстве');
      return;
    }
  } catch (error) { if (error.name === 'AbortError') return; }
  const url = URL.createObjectURL(blob);
  const link = document.createElement('a'); link.href = url; link.download = fileName; link.click();
  setTimeout(() => URL.revokeObjectURL(url), 1200);
  toast('Файл сохранён. Назначьте его в настройках телефона.');
}
function demoWallpaperSvg(item) {
  const palettes = {
    aurora: ['#102329', '#236b59', '#80ffbf'], moon: ['#17213d', '#51677b', '#f0dfb9'],
    neon: ['#17142f', '#51245a', '#ff75dc'], ocean: ['#0b2233', '#145562', '#6debd2'],
    ember: ['#26202a', '#8b4935', '#ff9b52'], cloud: ['#354b50', '#91aaa5', '#e8e5d9'],
    sakura: ['#251f3a', '#794c69', '#ff9fbd'],
  };
  const [top, bottom, glow] = palettes[item.art] || palettes.aurora;
  const accent = item.art === 'moon'
    ? '<circle cx="760" cy="520" r="142" fill="#f0dfb9"/><circle cx="712" cy="480" r="190" fill="#f0dfb9" opacity=".12"/>'
    : item.art === 'neon'
      ? '<path d="M220 1150H860M180 1270H900M120 1400H960M60 1540H1020" stroke="#ff75dc" stroke-opacity=".45" stroke-width="4"/><path d="M540 1040 90 1900M540 1040 330 1900M540 1040 750 1900M540 1040 990 1900" stroke="#ff75dc" stroke-opacity=".32" stroke-width="3"/>'
      : item.art === 'sakura'
        ? '<g fill="#ffb1cb" opacity=".8"><circle cx="180" cy="480" r="22"/><circle cx="820" cy="710" r="28"/><circle cx="360" cy="920" r="17"/><circle cx="910" cy="1120" r="20"/><circle cx="250" cy="1320" r="26"/></g>'
        : '';
  const glowShape = item.art === 'ocean'
    ? '<path d="M0 1120 Q260 1010 540 1120T1080 1120M0 1260Q260 1150 540 1260t540 0M0 1430q270-110 540 0t540 0" fill="none" stroke="#6debd2" stroke-opacity=".32" stroke-width="12"/>'
    : `<ellipse cx="390" cy="520" rx="570" ry="770" fill="url(#a)"/>`;
  const hills = item.art === 'cloud'
    ? '<path d="M0 1180 Q160 960 360 1150 Q610 970 820 1190 Q970 1030 1080 1140V1920H0Z" fill="#263b3f" opacity=".7"/>'
    : '<path d="m0 1320 210-180 180 100 160-265 170 300 160-215 200 130v730H0z" fill="#101a19" opacity=".92"/>';
  return `<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 1080 1920"><defs><linearGradient id="g" x2=".8" y2="1"><stop stop-color="${top}"/><stop offset=".62" stop-color="${bottom}"/><stop offset="1" stop-color="#0c1718"/></linearGradient><radialGradient id="a"><stop stop-color="${glow}" stop-opacity=".78"/><stop offset="1" stop-color="${glow}" stop-opacity="0"/></radialGradient></defs><path fill="url(#g)" d="M0 0h1080v1920H0z"/>${glowShape}${accent}${hills}</svg>`;
}
function toggleFit() {
  const order = ['cover', 'contain', 'blur'];
  settings.fit = order[(order.indexOf(settings.fit) + 1) % order.length];
  saveSettings();
  $('#preview-stage').dataset.fit = settings.fit;
  $('#preview-stage').style.setProperty('--preview-fit', settings.fit === 'contain' || settings.fit === 'blur' ? 'contain' : 'cover');
  toast(settings.fit === 'contain' ? 'Показать целиком' : settings.fit === 'blur' ? 'Мягкий фон' : 'Заполнить экран');
}
function setSound(enabled) {
  settings.sound = enabled;
  saveSettings();
  const video = $('#preview-media video'); if (video) video.muted = !enabled;
  $('#sound-toggle').textContent = enabled ? '◗' : '◖';
  $('#sound-toggle').setAttribute('aria-label', enabled ? 'Выключить звук' : 'Включить звук');
}

$('#main-content').addEventListener('click', event => {
  const fav = event.target.closest('[data-favorite]');
  if (fav) { event.stopPropagation(); toggleFavourite(fav.dataset.favorite); return; }
  const tab = event.target.closest('[data-tab]');
  if (tab) { setTab(tab.dataset.tab); return; }
  const importButton = event.target.closest('[data-action="import"]');
  if (importButton) { $('#file-input').click(); return; }
  const card = event.target.closest('[data-wallpaper]');
  if (card) openPreview(getItem(card.dataset.wallpaper));
});
$$('.bottom-nav .nav-item, .text-button, .empty-state [data-tab]').forEach(button => button.addEventListener('click', () => setTab(button.dataset.tab)));
$$('.filter-chip').forEach(button => button.addEventListener('click', () => {
  activeFilter = button.dataset.filter;
  $$('.filter-chip').forEach(chip => chip.classList.toggle('selected', chip === button));
  render();
}));
$('#file-input').addEventListener('change', event => { chooseFiles(event.target.files); event.target.value = ''; });
$('#preview-dialog').addEventListener('click', event => { if (event.target === $('#preview-dialog')) closePreview(); });
$('#preview-dialog').addEventListener('cancel', event => { event.preventDefault(); closePreview(); });
$('#preview-dialog').addEventListener('click', event => {
  const action = event.target.closest('[data-action]')?.dataset.action;
  if (action === 'close-preview') closePreview();
});
$('#favorite-toggle').addEventListener('click', () => currentItem && toggleFavourite(currentItem.id));
$('#fit-toggle').addEventListener('click', toggleFit);
$('#sound-toggle').addEventListener('click', () => setSound(!settings.sound));
$('#share-button').addEventListener('click', downloadCurrent);
$('#save-wallpaper').addEventListener('click', downloadCurrent);
$('#fit-setting').value = settings.fit;
$('#fit-setting').addEventListener('change', event => { settings.fit = event.target.value; saveSettings(); });
$('#sound-setting').checked = settings.sound;
$('#sound-setting').addEventListener('change', event => { setSound(event.target.checked); });
$('#data-setting').checked = settings.dataSaver;
$('#data-setting').addEventListener('change', event => { settings.dataSaver = event.target.checked; saveSettings(); toast(settings.dataSaver ? 'Экономия трафика включена' : 'Экономия трафика выключена'); });
$('#profile-button').addEventListener('click', () => $('#help-dialog').showModal());
$$('[data-action="close-help"]').forEach(button => button.addEventListener('click', () => $('#help-dialog').close()));
$('#help-dialog').addEventListener('click', event => { if (event.target === $('#help-dialog') || event.target.closest('[data-action="close-help"]')) $('#help-dialog').close(); });

window.addEventListener('beforeinstallprompt', event => {
  event.preventDefault(); deferredInstallPrompt = event;
  $('#install-button').hidden = false; $('#install-settings').hidden = false;
});
async function installApp() {
  if (!deferredInstallPrompt) { toast('Откройте меню браузера и выберите «Добавить на главный экран»'); return; }
  deferredInstallPrompt.prompt();
  await deferredInstallPrompt.userChoice;
  deferredInstallPrompt = null;
  $('#install-button').hidden = true; $('#install-settings').hidden = true;
}
$('#install-button').addEventListener('click', installApp);
$('#install-settings').addEventListener('click', installApp);
if ('serviceWorker' in navigator && location.protocol !== 'file:' && !window.AndroidWallPulse) {
  window.addEventListener('load', () => navigator.serviceWorker.register('./sw.js').catch(() => {}));
}
window.addEventListener('online', () => toast('Подключение восстановлено'));

render();
loadImports();
if (!localStorage.getItem('wallpulse-pocket-welcomed')) {
  setTimeout(() => { if (!$('#preview-dialog').open) $('#help-dialog').showModal(); }, 650);
  localStorage.setItem('wallpulse-pocket-welcomed', '1');
}
