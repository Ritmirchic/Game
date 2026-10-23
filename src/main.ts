import '@fontsource-variable/manrope'
import '@fontsource-variable/unbounded'
import './desert.css'
import { DesertGame } from './desert/DesertGame'
import type { DesertEndStats, DesertSnapshot, DesertState } from './desert/types'

const icon = (name: string, className = ''): string => `<svg class="icon ${className}" aria-hidden="true"><use href="#i-${name}"></use></svg>`
const formatTime = (seconds: number): string => {
  const rounded = Math.max(0, Math.floor(seconds))
  return `${String(Math.floor(rounded / 60)).padStart(2, '0')}:${String(rounded % 60).padStart(2, '0')}`
}

const app = document.querySelector<HTMLDivElement>('#app')
if (!app) throw new Error('App root not found')
app.innerHTML = `
  <svg class="svg-defs" xmlns="http://www.w3.org/2000/svg" aria-hidden="true"><defs>
    <symbol id="i-dunes" viewBox="0 0 24 24"><path d="M2 18c4-6 7-7 10-3s5 3 10-2M2 21h20M7 11l2-3 2 3"/><circle cx="17" cy="6" r="2.5"/></symbol>
    <symbol id="i-sun" viewBox="0 0 24 24"><circle cx="12" cy="12" r="3.5"/><path d="M12 2v2m0 16v2M2 12h2m16 0h2M5 5l1.5 1.5m11 11L19 19M5 19l1.5-1.5m11-11L19 5"/></symbol>
    <symbol id="i-moon" viewBox="0 0 24 24"><path d="M20 15.5A8.5 8.5 0 0 1 8.5 4 8.5 8.5 0 1 0 20 15.5Z"/></symbol>
    <symbol id="i-heart" viewBox="0 0 24 24"><path d="M12 20 4.2 12.5C-1 7.2 6.5 1.4 12 7c5.5-5.6 13 0 7.8 5.5L12 20Z"/></symbol>
    <symbol id="i-bolt" viewBox="0 0 24 24"><path d="m13 2-8 12h6l-1 8 9-12h-6V2Z"/></symbol>
    <symbol id="i-drop" viewBox="0 0 24 24"><path d="M12 2S5 10 5 15a7 7 0 0 0 14 0c0-5-7-13-7-13Z"/><path d="M9 16c.5 1.5 1.5 2 3 2"/></symbol>
    <symbol id="i-scrap" viewBox="0 0 24 24"><path d="m7 3 10 2 4 7-5 8-10-1-4-8L7 3Z"/><path d="m7 3 3 6-4 10m11-4 4 0M10 9l7 3-1 8"/></symbol>
    <symbol id="i-fiber" viewBox="0 0 24 24"><path d="M12 22V8m0 5-4-3m4 6 5-4M8 21h8"/><path d="M12 8c-5 0-5-6-2-6 2 0 2 3 2 6Zm0 0c5 0 5-6 2-6-2 0-2 3-2 6Z"/></symbol>
    <symbol id="i-flask" viewBox="0 0 24 24"><path d="M9 2h6M10 2v6l-5 9c-1 3 1 5 4 5h6c3 0 5-2 4-5l-5-9V2"/><path d="M7 15h10"/></symbol>
    <symbol id="i-radio" viewBox="0 0 24 24"><path d="M8 21h8M12 21V9m-4 4 4-8 4 8M5 21h14"/><path d="M5 7a10 10 0 0 1 14 0M8 10a6 6 0 0 1 8 0"/><circle cx="12" cy="13" r="1"/></symbol>
    <symbol id="i-sentry" viewBox="0 0 24 24"><path d="M12 11 5 22m7-11 7 11m-7-11v11M4 8h12v5H4zM16 10h6M8 8V5h5v3"/><circle cx="8" cy="10.5" r="1"/></symbol>
    <symbol id="i-bandage" viewBox="0 0 24 24"><path d="m5 19-2-2c-2-2-2-4 0-6L14 1c2-2 4-2 6 0l2 2c2 2 2 4 0 6L11 20c-2 2-4 2-6 0Z"/><path d="m8 12 4 4m0-8 4 4m-6-1 4 4"/></symbol>
    <symbol id="i-compass" viewBox="0 0 24 24"><circle cx="12" cy="12" r="9"/><path d="m15.5 8.5-2 5-5 2 2-5 5-2Z"/></symbol>
    <symbol id="i-volume" viewBox="0 0 24 24"><path d="M11 4 6 8H2v8h4l5 4V4Zm4 5a5 5 0 0 1 0 6m3-9a9 9 0 0 1 0 12"/></symbol>
    <symbol id="i-muted" viewBox="0 0 24 24"><path d="M11 4 6 8H2v8h4l5 4V4Zm5 5 6 6m0-6-6 6"/></symbol>
    <symbol id="i-pause" viewBox="0 0 24 24"><path d="M8 5v14M16 5v14"/></symbol>
    <symbol id="i-play" viewBox="0 0 24 24"><path d="m8 4 12 8-12 8V4Z"/></symbol>
    <symbol id="i-arrow" viewBox="0 0 24 24"><path d="M4 12h15m-5-5 5 5-5 5"/></symbol>
    <symbol id="i-book" viewBox="0 0 24 24"><path d="M4 20a3 3 0 0 1 3-3h13V2H7a3 3 0 0 0-3 3v15Zm0 0a3 3 0 0 0 3 3h13v-6"/></symbol>
    <symbol id="i-close" viewBox="0 0 24 24"><path d="m6 6 12 12M18 6 6 18"/></symbol>
    <symbol id="i-restart" viewBox="0 0 24 24"><path d="M3 10a9 9 0 1 1 2 8M3 3v7h7"/></symbol>
    <symbol id="i-home" viewBox="0 0 24 24"><path d="m3 11 9-9 9 9M5 10v11h14V10M9 21v-7h6v7"/></symbol>
    <symbol id="i-skull" viewBox="0 0 24 24"><path d="M5 15a8 8 0 1 1 14 0v4h-3v3h-3v-3h-2v3H8v-3H5v-4Z"/><circle cx="9" cy="11" r="1"/><circle cx="15" cy="11" r="1"/></symbol>
    <symbol id="i-distance" viewBox="0 0 24 24"><path d="M12 22s7-7 7-13A7 7 0 0 0 5 9c0 6 7 13 7 13Z"/><circle cx="12" cy="9" r="2"/></symbol>
  </defs></svg>

  <main class="mirage-shell">
    <canvas id="game-canvas" aria-label="Открытый мир пустыни Миража"></canvas>
    <div class="screen-grade"></div>
    <div id="damage-flash" class="damage-flash"></div>

    <section id="menu-screen" class="screen menu-screen">
      <header class="menu-header">
        <div class="brand">${icon('dunes')}<span><strong>MIRAGE</strong><small>OPEN-WORLD SURVIVAL</small></span></div>
        <button class="icon-button" data-action="sound" aria-label="Выключить звук">${icon('volume')}</button>
      </header>
      <div class="menu-layout">
        <div class="hero">
          <div class="eyebrow">ПУСТЫНЯ НЕ ЗАКАНЧИВАЕТСЯ НА ГОРИЗОНТЕ</div>
          <h1>МИРАЖ</h1>
          <p>Шесть глав истории Меридиана. Огромная пустыня.<br>Четыре многофазных босса и экспедиция с контрольными точками.</p>
          <div class="hero-actions">
            <button class="primary-button" data-action="start" disabled><span>ФОРМИРОВАНИЕ МИРА…</span>${icon('arrow')}</button>
            <button id="continue-button" class="secondary-button" data-action="continue" hidden>${icon('play')}<span>ПРОДОЛЖИТЬ</span></button>
            <button class="secondary-button" data-action="how">${icon('book')}<span>ПОЛЕВОЙ ГАЙД</span></button>
          </div>
          <div class="menu-controls"><span><kbd>WASD</kbd> движение</span><span><kbd>F / ЛКМ</kbd> выпад</span><span><kbd>E</kbd> действие</span><span><kbd>SPACE</kbd> слайд</span></div>
        </div>
        <aside class="briefing">
          <div class="briefing-index">CAMPAIGN / 01</div>
          <h2>ПЕПЕЛЬНЫЙ<br>МЕРИДИАН</h2>
          <ol>
            <li><span>01</span><div><strong>6 ГЛАВ · 6 РЕГИОНОВ</strong><small>От северной вышки до цитадели Меридиана</small></div></li>
            <li><span>02</span><div><strong>4 МНОГОФАЗНЫХ БОССА</strong><small>Телеграфируемые атаки, стража и новые тактики</small></div></li>
            <li><span>03</span><div><strong>РАЗВИТИЕ И СОХРАНЕНИЕ</strong><small>Улучшения снаряжения и контрольные точки глав</small></div></li>
          </ol>
        </aside>
      </div>
      <footer class="menu-footer"><span>БЕСШОВНАЯ ГЕНЕРАЦИЯ МИРА</span><span>ДЕНЬ / НОЧЬ</span><span class="desktop-only">Зажми мышь и веди для обзора</span></footer>
    </section>

    <section id="hud" class="hud" hidden>
      <div class="mission-hud">
        <div class="hud-kicker">${icon('radio')}<span id="chapter-value">ГЛАВА 1 / 6</span></div>
        <strong id="objective-title">Восстановите ретранслятор</strong>
        <div class="objective-progress"><i id="relay-progress"></i></div>
        <small id="objective-detail">Соберите ресурсы для ремонта</small>
        <div class="mission-target">${icon('distance')}<span id="region-value">Северный ретранслятор</span><b>·</b><span id="target-distance">0 м</span></div>
      </div>

      <div class="navigation-hud">
        <div class="compass-line"><span>З</span><span>СЗ</span><strong id="heading-value">С 0°</strong><span>СВ</span><span>В</span></div>
        <div class="world-state">${icon('sun', 'phase-icon')}<span id="day-value">ДЕНЬ 1</span><b></b><span id="temperature-value">18°C</span></div>
      </div>

      <div id="boss-hud" class="boss-hud" hidden>
        <div><strong id="boss-name">АРХОНТ</strong><span id="boss-phase">ФАЗА 1</span></div>
        <div class="boss-track"><i id="boss-bar"></i></div>
      </div>

      <div class="inventory-hud">
        <div class="inventory-item" title="Обломки">${icon('scrap')}<strong id="scrap-value">2</strong></div>
        <div class="inventory-item" title="Волокно">${icon('fiber')}<strong id="fiber-value">1</strong></div>
        <div class="inventory-item water" title="Фляги">${icon('flask')}<strong id="water-value">2</strong></div>
        <button class="icon-button" data-action="journal" aria-label="Журнал кампании (J)">${icon('book')}</button>
        <button class="icon-button" data-action="sound" aria-label="Выключить звук">${icon('volume')}</button>
        <button class="icon-button" data-action="pause" aria-label="Пауза">${icon('pause')}</button>
      </div>

      <div class="vitals-hud">
        <div class="vital health"><span>${icon('heart')}</span><div><i id="health-bar"></i></div><strong id="health-value">100</strong></div>
        <div class="vital stamina"><span>${icon('bolt')}</span><div><i id="stamina-bar"></i></div><strong id="stamina-value">100</strong></div>
        <div class="vital hydration"><span>${icon('drop')}</span><div><i id="hydration-bar"></i></div><strong id="hydration-value">100</strong></div>
        <small id="dodge-value">SPACE · СЛАЙД ГОТОВ</small>
      </div>

      <div class="action-bar">
        <button data-recipe="sentry" class="action-slot" aria-keyshortcuts="1" aria-label="Турель: 8 обломков и 4 волокна"><kbd>1</kbd>${icon('sentry')}<span><strong>ТУРЕЛЬ</strong><small>8 обл. · 4 вол.</small></span></button>
        <button data-recipe="bandage" class="action-slot" aria-keyshortcuts="2" aria-label="Перевязка: 3 волокна"><kbd>2</kbd>${icon('bandage')}<span><strong>ПЕРЕВЯЗКА</strong><small>3 волокна</small></span></button>
        <button data-recipe="drink" class="action-slot" aria-keyshortcuts="R" aria-label="Выпить воду из фляги"><kbd>R</kbd>${icon('flask')}<span><strong>ГЛОТОК</strong><small>+48 гидратации</small></span></button>
      </div>

      <div id="interaction-prompt" class="interaction-prompt" hidden><kbd>E</kbd><div><strong id="prompt-title"></strong><small id="prompt-detail"></small></div></div>
      <div class="position-hud">${icon('compass')}<span id="coordinates-value">6 · 8</span><b></b><span id="kill-value">0 ЦЕЛЕЙ</span><b></b><span id="gear-value">КОПЬЁ I</span></div>

      <div class="mobile-controls">
        <div id="joystick" class="joystick"><div class="joystick-ring"><div id="joystick-knob" class="joystick-knob"></div></div></div>
        <div class="mobile-actions">
          <button data-mobile="dodge" aria-label="Слайд">↯<small>СЛАЙД</small></button>
          <button data-mobile="drink" aria-label="Пить">${icon('flask')}<small>ПИТЬ</small></button>
          <button data-mobile="interact" class="interact" aria-label="Взаимодействовать">E<small>ДЕЙСТВИЕ</small></button>
          <button data-mobile="attack" class="attack" aria-label="Удар копьём">${icon('scrap')}<small>ВЫПАД</small></button>
        </div>
      </div>
    </section>

    <div id="notifications" class="notifications" aria-live="polite"></div>
    <div id="loot-feed" class="loot-feed" aria-live="polite"></div>
    <div id="announcement" class="announcement"><span id="announce-eyebrow"></span><h2 id="announce-title"></h2><p id="announce-detail"></p></div>

    <section id="pause-screen" class="screen overlay-screen" hidden><div class="overlay-card">
      ${icon('pause', 'overlay-symbol')}<div class="eyebrow">ЭКСПЕДИЦИЯ ПРИОСТАНОВЛЕНА</div><h2>ПАУЗА</h2><p>Ветер подождёт. Жажда — тоже.</p>
      <button class="primary-button wide" data-action="resume"><span>ПРОДОЛЖИТЬ</span>${icon('play')}</button>
      <div class="split-buttons"><button class="secondary-button" data-action="restart">${icon('restart')} С КОНТРОЛЬНОЙ ТОЧКИ</button><button class="secondary-button" data-action="menu">${icon('home')} В МЕНЮ</button></div>
      <button class="text-button" data-action="how">Полевой гайд</button>
    </div></section>

    <section id="story-screen" class="screen overlay-screen story-screen" hidden><div class="overlay-card story-card">
      <div id="story-chapter" class="eyebrow">ГЛАВА 1 / 6</div><h2 id="story-title">Радиослед</h2>
      <div id="story-speaker" class="story-speaker">АЙРА</div><p id="story-text" class="story-text"></p>
      <button class="primary-button wide" data-action="continue-story"><span>ПРОДОЛЖИТЬ ЭКСПЕДИЦИЮ</span>${icon('arrow')}</button>
      <small class="story-hint">J · открыть журнал текущей главы</small>
    </div></section>

    <section id="end-screen" class="screen overlay-screen" hidden><div class="overlay-card">
      <span id="end-symbol">${icon('radio', 'overlay-symbol')}</span><div id="end-eyebrow" class="eyebrow">ЭКСПЕДИЦИЯ ЗАВЕРШЕНА</div><h2 id="end-title">СИГНАЛ ПРИНЯТ</h2><p id="end-reason"></p>
      <div class="run-stats"><div><span>ВРЕМЯ</span><strong id="stat-time">00:00</strong></div><div><span>ЦЕЛИ</span><strong id="stat-kills">0</strong></div><div><span>ПУТЬ</span><strong id="stat-distance">0 м</strong></div></div>
      <button class="primary-button wide" data-action="restart"><span>НОВАЯ ЭКСПЕДИЦИЯ</span>${icon('restart')}</button><button class="text-button" data-action="menu">Вернуться в меню</button>
    </div></section>

    <section id="how-modal" class="screen overlay-screen how-modal" hidden><div class="overlay-card how-card">
      <button class="icon-button modal-close" data-action="close-how" aria-label="Закрыть">${icon('close')}</button><div class="eyebrow">ПОЛЕВОЙ ГАЙД / MIRAGE</div><h2>ВЫЖИВАНИЕ В ДЮНАХ</h2>
      <div class="guide-grid">
        <article>${icon('drop')}<h3>ВОДА</h3><p>Голубой индикатор — гидратация. Пейте флягу клавишей <kbd>R</kbd>, пополняйтесь <kbd>E</kbd> у оазисов и колодцев. При нуле воды здоровье падает.</p></article>
        <article>${icon('scrap')}<h3>КАМПАНИЯ</h3><p>Следуйте за световой отметкой и расстоянием до цели. <kbd>1</kbd> — sentry, <kbd>2</kbd> — перевязка. <kbd>J</kbd> открывает журнал главы. Прогресс сохраняется в начале каждой главы.</p></article>
        <article>${icon('sentry')}<h3>БОЙ</h3><p><kbd>F</kbd> или короткий клик — выпад с попаданием в момент контакта. <kbd>SPACE</kbd> / <kbd>Q</kbd> — слайд с короткой неуязвимостью.</p></article>
      </div>
      <div class="guide-controls"><span><kbd>WASD / СТРЕЛКИ</kbd> движение</span><span><kbd>SHIFT</kbd> бег</span><span><kbd>E</kbd> действие</span><span><kbd>R</kbd> пить</span><span><kbd>J</kbd> журнал</span><span><kbd>ЛКМ + ДВИЖЕНИЕ</kbd> обзор</span><span><kbd>КОЛЕСО</kbd> масштаб</span><span><kbd>P / ESC</kbd> пауза</span></div>
      <button class="primary-button wide" data-action="close-how"><span>ПОНЯТНО</span>${icon('arrow')}</button>
    </div></section>
  </main>
`

const byId = <T extends HTMLElement = HTMLElement>(id: string): T => {
  const element = document.getElementById(id)
  if (!element) throw new Error(`Element #${id} not found`)
  return element as T
}
const hud = byId('hud')
const menu = byId('menu-screen')
const pauseScreen = byId('pause-screen')
const endScreen = byId('end-screen')
const howModal = byId('how-modal')
const announcement = byId('announcement')
const notifications = byId('notifications')
const lootFeed = byId('loot-feed')
let announcementVersion = 0
let latestSnapshot: DesertSnapshot | null = null

function notify(message: string, tone: 'info' | 'good' | 'danger' = 'info'): void {
  const item = document.createElement('div')
  item.className = `notification ${tone}`
  item.innerHTML = '<i></i><span></span>'
  const text = item.querySelector('span')
  if (text) text.textContent = message
  notifications.append(item)
  while (notifications.children.length > 4) notifications.firstElementChild?.remove()
  window.setTimeout(() => item.classList.add('leaving'), 2900)
  window.setTimeout(() => item.remove(), 3300)
}

function showAnnouncement(eyebrow: string, title: string, detail: string): void {
  announcementVersion += 1
  const version = announcementVersion
  byId('announce-eyebrow').textContent = eyebrow
  byId('announce-title').textContent = title
  byId('announce-detail').textContent = detail
  announcement.classList.remove('visible')
  void announcement.offsetWidth
  announcement.classList.add('visible')
  window.setTimeout(() => { if (version === announcementVersion) announcement.classList.remove('visible') }, 3800)
}

function showLoot(message: string, kind: string): void {
  const item = document.createElement('div')
  item.className = `loot-pop ${kind}`
  item.textContent = message
  lootFeed.append(item)
  while (lootFeed.children.length > 3) lootFeed.firstElementChild?.remove()
  window.setTimeout(() => item.remove(), 1450)
}

function handleState(state: DesertState, stats?: DesertEndStats): void {
  menu.hidden = state !== 'menu'
  hud.hidden = state === 'menu'
  pauseScreen.hidden = state !== 'paused'
  byId('story-screen').hidden = state !== 'story'
  endScreen.hidden = state !== 'gameover' && state !== 'victory'
  if (stats) {
    byId('end-title').textContent = stats.victory ? 'МЕРИДИАН ОТКЛЮЧЁН' : 'ЭКСПЕДИЦИЯ ПОТЕРЯНА'
    byId('end-eyebrow').textContent = stats.victory ? 'КАМПАНИЯ ЗАВЕРШЕНА' : 'КОНТРОЛЬНАЯ ТОЧКА СОХРАНЕНА'
    byId('end-reason').textContent = stats.reason
    byId('end-symbol').innerHTML = icon(stats.victory ? 'radio' : 'skull', 'overlay-symbol')
    byId('stat-time').textContent = formatTime(stats.elapsed)
    byId('stat-kills').textContent = String(stats.kills)
    byId('stat-distance').textContent = `${Math.round(stats.distance)} м`
    const restartLabel = document.querySelector('#end-screen [data-action="restart"] span')
    if (restartLabel) restartLabel.textContent = stats.victory ? 'НОВАЯ КАМПАНИЯ' : 'ПОВТОРИТЬ ГЛАВУ'
  }
  if (state === 'menu') {
    updateContinueButton()
    announcement.classList.remove('visible')
    notifications.replaceChildren()
    lootFeed.replaceChildren()
  }
}

function updateSnapshot(snapshot: DesertSnapshot): void {
  latestSnapshot = snapshot
  for (const [id, value] of [['health', snapshot.health], ['stamina', snapshot.stamina], ['hydration', snapshot.hydration]] as const) {
    const percentage = id === 'health' ? value / snapshot.maxHealth * 100 : value
    byId(`${id}-bar`).style.width = `${Math.max(0, Math.min(100, percentage))}%`
    byId(`${id}-value`).textContent = String(Math.ceil(value))
    byId(`${id}-bar`).classList.toggle('critical', percentage <= 25)
  }
  byId('scrap-value').textContent = String(snapshot.scrap)
  byId('fiber-value').textContent = String(snapshot.fiber)
  byId('water-value').textContent = String(snapshot.water)
  byId('kill-value').textContent = `${snapshot.kills} ЦЕЛЕЙ`
  byId('coordinates-value').textContent = `${Math.round(snapshot.x)} · ${Math.round(snapshot.z)}`
  const directions = ['С', 'СВ', 'В', 'ЮВ', 'Ю', 'ЮЗ', 'З', 'СЗ']
  const cardinal = directions[Math.round(snapshot.heading / 45) % 8]
  byId('heading-value').textContent = `${cardinal} ${Math.round(snapshot.heading)}°`
  byId('day-value').textContent = `ДЕНЬ ${snapshot.day}`
  byId('temperature-value').textContent = `${Math.round(snapshot.temperature)}°C`
  document.querySelector('.phase-icon use')?.setAttribute('href', snapshot.temperature < 20 ? '#i-moon' : '#i-sun')
  const campaign = snapshot.campaign
  byId('chapter-value').textContent = `ГЛАВА ${campaign.chapter} / ${campaign.totalChapters} · ${campaign.act}`
  byId('objective-title').textContent = campaign.stepTitle
  byId('objective-detail').textContent = campaign.stepDetail
  byId('relay-progress').style.width = `${Math.max(0, Math.min(100, campaign.progress / Math.max(1, campaign.target) * 100))}%`
  byId('region-value').textContent = campaign.locationName
  byId('target-distance').textContent = `${Math.round(campaign.distance)} м`
  const roman = ['0', 'I', 'II', 'III']
  byId('gear-value').textContent = `КОПЬЁ ${roman[campaign.spearLevel]} · SENTRY ${roman[campaign.sentryLevel]} · БРОНЯ ${roman[campaign.armorLevel]}`
  const boss = byId('boss-hud')
  boss.hidden = campaign.bossMaxHealth <= 0
  if (campaign.bossMaxHealth > 0) {
    byId('boss-name').textContent = campaign.bossName
    byId('boss-phase').textContent = `ФАЗА ${campaign.bossPhase}`
    byId('boss-bar').style.width = `${Math.max(0, Math.min(100, campaign.bossHealth / campaign.bossMaxHealth * 100))}%`
  }
  byId('dodge-value').textContent = snapshot.dodgeCooldown > 0 ? `СЛАЙД · ${snapshot.dodgeCooldown.toFixed(1)} С` : 'SPACE · СЛАЙД ГОТОВ'
  const prompt = byId('interaction-prompt')
  prompt.hidden = snapshot.prompt.length === 0
  byId('prompt-title').textContent = snapshot.prompt
  byId('prompt-detail').textContent = snapshot.promptDetail
  document.querySelectorAll<HTMLButtonElement>('[data-recipe]').forEach((button) => {
    const recipe = button.dataset.recipe
    const disabled = recipe === 'sentry' ? !snapshot.canSentry : recipe === 'bandage' ? !snapshot.canBandage : snapshot.water <= 0 || snapshot.hydration >= 98
    button.classList.toggle('unavailable', disabled)
  })
}

function showStory(dialog: import('./desert/Campaign').StoryDialog | null): void {
  if (!dialog) return
  byId('story-chapter').textContent = `${dialog.act} · ГЛАВА ${dialog.chapter} / ${dialog.totalChapters}`
  byId('story-title').textContent = dialog.title
  byId('story-speaker').textContent = dialog.speaker
  byId('story-text').textContent = dialog.text
}

const game = new DesertGame(byId<HTMLCanvasElement>('game-canvas'), {
  onSnapshot: updateSnapshot,
  onState: handleState,
  onNotify: notify,
  onAnnounce: showAnnouncement,
  onLoot: showLoot,
  onStory: showStory,
  onDamage: () => {
    const flash = byId('damage-flash')
    flash.classList.remove('active')
    void flash.offsetWidth
    flash.classList.add('active')
  },
})
if (new URLSearchParams(location.search).has('debug')) {
  ;(window as unknown as { __MIRAGE_GAME__: DesertGame }).__MIRAGE_GAME__ = game
}

function updateContinueButton(): void {
  const button = byId<HTMLButtonElement>('continue-button')
  button.hidden = !game.hasCampaignSave()
  const label = button.querySelector('span')
  if (label) label.textContent = game.continueLabel()
}

void game.ready.then(() => {
  const start = document.querySelector<HTMLButtonElement>('[data-action="start"]')
  if (start) {
    start.disabled = false
    const label = start.querySelector('span')
    if (label) label.textContent = 'НОВАЯ КАМПАНИЯ'
  }
  updateContinueButton()
}).catch((error: unknown) => {
  console.error('Не удалось сформировать пустыню:', error)
  const label = document.querySelector('[data-action="start"] span')
  if (label) label.textContent = 'ОШИБКА WEBGL — ОБНОВИТЕ СТРАНИЦУ'
})

function updateSoundButtons(): void {
  document.querySelectorAll<HTMLButtonElement>('[data-action="sound"]').forEach((button) => {
    button.innerHTML = icon(game.sound.enabled ? 'volume' : 'muted')
    button.setAttribute('aria-label', game.sound.enabled ? 'Выключить звук' : 'Включить звук')
    button.classList.toggle('muted', !game.sound.enabled)
  })
}

function closeHow(): void { howModal.hidden = true }
const actions: Record<string, () => void> = {
  start: () => game.start(),
  continue: () => game.continueCampaign(),
  pause: () => game.pause(),
  resume: () => game.resume(),
  restart: () => game.restart(),
  'continue-story': () => game.resumeStory(),
  journal: () => game.journal(),
  menu: () => game.toMenu(),
  how: () => { howModal.hidden = false },
  'close-how': closeHow,
  sound: () => { game.toggleSound(); updateSoundButtons() },
}
document.querySelectorAll<HTMLButtonElement>('[data-action]').forEach((button) => {
  button.addEventListener('click', () => {
    const action = button.dataset.action
    if (action && actions[action]) {
      if (action !== 'sound') game.sound.play('click')
      actions[action]()
    }
  })
})
document.querySelectorAll<HTMLButtonElement>('[data-recipe]').forEach((button) => {
  button.addEventListener('click', () => {
    if (button.dataset.recipe === 'sentry') game.buildSentry()
    else if (button.dataset.recipe === 'bandage') game.bandage()
    else game.drink()
  })
})
howModal.addEventListener('click', (event) => { if (event.target === howModal) closeHow() })
document.addEventListener('keydown', (event) => {
  if (howModal.hidden) return
  event.stopImmediatePropagation()
  if (event.code === 'Escape') {
    event.preventDefault()
    closeHow()
  }
}, true)

const joystick = byId('joystick')
const joystickKnob = byId('joystick-knob')
let joystickPointer: number | null = null
function moveJoystick(event: PointerEvent): void {
  const bounds = joystick.getBoundingClientRect()
  const centerX = bounds.left + bounds.width / 2
  const centerY = bounds.top + bounds.height / 2
  const maxDistance = bounds.width * 0.3
  let dx = event.clientX - centerX
  let dy = event.clientY - centerY
  const length = Math.hypot(dx, dy)
  if (length > maxDistance) {
    dx = dx / length * maxDistance
    dy = dy / length * maxDistance
  }
  joystickKnob.style.transform = `translate(${dx}px, ${dy}px)`
  game.setJoystick(dx / maxDistance, -dy / maxDistance)
}
joystick.addEventListener('pointerdown', (event) => {
  event.preventDefault()
  event.stopPropagation()
  joystickPointer = event.pointerId
  joystick.setPointerCapture(event.pointerId)
  moveJoystick(event)
})
joystick.addEventListener('pointermove', (event) => { if (event.pointerId === joystickPointer) moveJoystick(event) })
const resetJoystick = (event: PointerEvent): void => {
  if (event.pointerId !== joystickPointer) return
  joystickPointer = null
  joystickKnob.style.transform = 'translate(0, 0)'
  game.setJoystick(0, 0)
}
joystick.addEventListener('pointerup', resetJoystick)
joystick.addEventListener('pointercancel', resetJoystick)
document.querySelectorAll<HTMLButtonElement>('[data-mobile]').forEach((button) => {
  button.addEventListener('pointerdown', (event) => {
    event.preventDefault()
    event.stopPropagation()
    if (button.dataset.mobile === 'attack') game.attack()
    else if (button.dataset.mobile === 'interact') game.interact()
    else if (button.dataset.mobile === 'dodge') game.dodge()
    else game.drink()
  })
})
updateSoundButtons()
if (latestSnapshot) updateSnapshot(latestSnapshot)
