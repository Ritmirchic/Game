import '@fontsource-variable/manrope'
import '@fontsource-variable/unbounded'
import './desert.css'
import { DesertGame } from './desert/DesertGame'
import { icon, renderInterface } from './interface'
import type { DesertEndStats, DesertSnapshot, DesertState, PerspectiveMode } from './desert/types'

const formatTime = (seconds: number): string => {
  const rounded = Math.max(0, Math.floor(seconds))
  return `${String(Math.floor(rounded / 60)).padStart(2, '0')}:${String(rounded % 60).padStart(2, '0')}`
}

const app = document.querySelector<HTMLDivElement>('#app')
if (!app) throw new Error('App root not found')
renderInterface(app)

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
  updatePerspectiveUI(snapshot.perspective)
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
  hud.classList.toggle('boss-active', campaign.bossMaxHealth > 0)
  if (campaign.bossMaxHealth > 0) {
    byId('boss-name').textContent = campaign.bossName
    byId('boss-phase').textContent = `ФАЗА ${campaign.bossPhase} / 3`
    byId('boss-bar').style.width = `${Math.max(0, Math.min(100, campaign.bossHealth / campaign.bossMaxHealth * 100))}%`
    Array.from(byId('boss-pips').children).forEach((pip, index) => pip.classList.toggle('active', index < campaign.bossPhase))
  }
  byId('dodge-value').textContent = snapshot.dodgeCooldown > 0 ? `РЫВОК · ${snapshot.dodgeCooldown.toFixed(1)} С` : 'SPACE · РЫВОК ГОТОВ'
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
  byId('story-number').textContent = String(dialog.chapter).padStart(2, '0')
  byId('story-avatar').textContent = dialog.speaker.trim().charAt(0)
  Array.from(byId('story-progress').children).forEach((pip, index) => {
    pip.classList.toggle('done', index < dialog.chapter - 1)
    pip.classList.toggle('active', index === dialog.chapter - 1)
  })
  byId('story-title').textContent = dialog.title
  byId('story-speaker').textContent = dialog.speaker
  byId('story-text').textContent = dialog.text
}

function updatePerspectiveUI(mode: PerspectiveMode): void {
  byId('navigation-mode').textContent = `${mode === 'first' ? '1P' : '3P'} / NAV 77.04`
  document.querySelectorAll<HTMLButtonElement>('[data-perspective]').forEach((button) => {
    const active = button.dataset.perspective === mode
    button.classList.toggle('active', active)
    button.setAttribute('aria-pressed', String(active))
  })
  const button = byId<HTMLButtonElement>('perspective-button')
  if (button.dataset.mode !== mode) {
    button.dataset.mode = mode
    button.innerHTML = icon(mode === 'first' ? 'third' : 'eye')
  }
  button.setAttribute('aria-label', mode === 'first' ? 'Переключить на вид от третьего лица (V)' : 'Переключить на вид от первого лица (V)')
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
  updatePerspectiveUI(game.getPerspective())
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
  perspective: () => game.togglePerspective(),
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
document.querySelectorAll<HTMLButtonElement>('[data-perspective]').forEach((button) => {
  button.addEventListener('click', () => {
    const mode = button.dataset.perspective
    if (mode !== 'first' && mode !== 'third') return
    game.sound.play('click')
    game.setPerspective(mode)
    updatePerspectiveUI(mode)
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
