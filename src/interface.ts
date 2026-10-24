export const icon = (name: string, className = ''): string => `<svg class="icon ${className}" viewBox="0 0 24 24" aria-hidden="true"><use href="#i-${name}"></use></svg>`

export function renderInterface(app: HTMLDivElement): void {
  app.innerHTML = `
  <svg class="svg-defs" xmlns="http://www.w3.org/2000/svg" aria-hidden="true"><defs>
    <symbol id="i-dunes" viewBox="0 0 24 24"><path d="M3 17 9 9l5 5 3-3 4 6M3 20h18M7 5h6M10 2v6"/><path d="m17 3 3 2-3 2-3-2 3-2Z"/></symbol>
    <symbol id="i-sun" viewBox="0 0 24 24"><path d="m12 6 4 2v5l-4 5-4-5V8l4-2Z"/><path d="M12 1v3m0 16v3M1 12h3m16 0h3M4 4l2 2m12 12 2 2M4 20l2-2M18 6l2-2"/></symbol>
    <symbol id="i-moon" viewBox="0 0 24 24"><path d="M17 3 9 5 4 12l3 7 8 2-4-5 1-7 5-6Z"/><path d="m18 12 1 2 2 1-2 1-1 2-1-2-2-1 2-1 1-2Z"/></symbol>
    <symbol id="i-heart" viewBox="0 0 24 24"><path d="m12 2 8 3v7l-8 10-8-10V5l8-3Z"/><path d="M7 12h3l2-4 2 7 2-3h2"/></symbol>
    <symbol id="i-bolt" viewBox="0 0 24 24"><path d="m14 2-9 11h6l-1 9 9-12h-6l1-8Z"/><path d="M3 5h4M2 9h3"/></symbol>
    <symbol id="i-drop" viewBox="0 0 24 24"><path d="m12 2 7 11-7 9-7-9 7-11Z"/><path d="m8 13 4 4 4-5M12 2v8"/></symbol>
    <symbol id="i-scrap" viewBox="0 0 24 24"><path d="m4 7 8-4 8 4-8 4-8-4Zm0 5 8 4 8-4M4 17l8 4 8-4"/><path d="M8 5.2 16 9M8 9l8-3.8"/></symbol>
    <symbol id="i-fiber" viewBox="0 0 24 24"><path d="M8 3c8 4 0 14 8 18M16 3C8 7 16 17 8 21M12 2v20"/><path d="M7 8h10M7 16h10"/></symbol>
    <symbol id="i-flask" viewBox="0 0 24 24"><path d="M9 2h6v4l3 4v9l-3 3H9l-3-3v-9l3-4V2Z"/><path d="M9 6h6M6 13h12M9 17h6"/></symbol>
    <symbol id="i-radio" viewBox="0 0 24 24"><path d="M12 8v14M7 22l5-14 5 14M5 22h14M8 17h8"/><path d="M6 3 2 7l4 4M18 3l4 4-4 4M12 2v3"/></symbol>
    <symbol id="i-sentry" viewBox="0 0 24 24"><path d="m5 5 7-3 7 3v7l-7 3-7-3V5Z"/><path d="m12 15-6 7m6-7 6 7m-6-7v7M19 8h4M8 8h5M8 11h3"/></symbol>
    <symbol id="i-bandage" viewBox="0 0 24 24"><path d="m7 3 14 14-4 4L3 7l4-4Z"/><path d="m8 9 3-3 5 5-3 3-5-5Z"/><path d="M11 7v6M8 10h6"/></symbol>
    <symbol id="i-compass" viewBox="0 0 24 24"><path d="m12 2 9 10-9 10-9-10 9-10Z"/><path d="m12 6 3 9-3-2-3 2 3-9Z"/><path d="M2 12h4m12 0h4"/></symbol>
    <symbol id="i-volume" viewBox="0 0 24 24"><path d="M3 9h4l5-5v16l-5-5H3V9Z"/><path d="M16 8v8M19 5v14M22 9v6"/></symbol>
    <symbol id="i-muted" viewBox="0 0 24 24"><path d="M3 9h4l5-5v16l-5-5H3V9Z"/><path d="m16 9 6 6m0-6-6 6"/></symbol>
    <symbol id="i-pause" viewBox="0 0 24 24"><path d="M5 4h5v16H5V4Zm9 0h5v16h-5V4Z"/><path d="M2 2h20v20H2V2Z" opacity=".35"/></symbol>
    <symbol id="i-play" viewBox="0 0 24 24"><path d="m6 3 15 9L6 21V3Z"/><path d="M9 8v8"/></symbol>
    <symbol id="i-arrow" viewBox="0 0 24 24"><path d="M3 12h16M13 5l7 7-7 7"/><path d="M3 8v8"/></symbol>
    <symbol id="i-book" viewBox="0 0 24 24"><path d="M5 2h14v20H5l-3-3V5l3-3Z"/><path d="M6 2v20M10 7h6M10 11h6M10 15h4"/></symbol>
    <symbol id="i-close" viewBox="0 0 24 24"><path d="M4 4l16 16M20 4 4 20"/><path d="M3 3h4M3 3v4M21 21h-4m4 0v-4"/></symbol>
    <symbol id="i-restart" viewBox="0 0 24 24"><path d="M5 8H1V4M2 8l4-5 7-1 7 4 2 8-4 6-8 2-6-4"/><path d="M8 12h8M12 8v8"/></symbol>
    <symbol id="i-home" viewBox="0 0 24 24"><path d="m2 20 10-18 10 18H2Z"/><path d="m8 20 4-8 4 8M6 20v2m12-2v2"/></symbol>
    <symbol id="i-skull" viewBox="0 0 24 24"><path d="m7 3 10 0 4 6-2 8-4 2v3H9v-3l-4-2-2-8 4-6Z"/><path d="m7 10 3 1-2 3-2-2m11-2-3 1 2 3 2-2M10 18h4"/></symbol>
    <symbol id="i-distance" viewBox="0 0 24 24"><path d="m12 2 8 6-2 9-6 5-6-5-2-9 8-6Z"/><path d="m12 7 3 3-3 3-3-3 3-3ZM12 13v9"/></symbol>
    <symbol id="i-spear" viewBox="0 0 24 24"><path d="m3 21 14-14M13 3l8-1-1 8-4-4-3-3ZM5 15l4 4M3 18l3 3"/></symbol>
    <symbol id="i-dash" viewBox="0 0 24 24"><path d="m3 5 7 7-7 7M10 5l7 7-7 7M18 8l3 4-3 4"/></symbol>
    <symbol id="i-target" viewBox="0 0 24 24"><path d="M8 3H3v5M16 3h5v5M8 21H3v-5M16 21h5v-5"/><path d="m12 7 5 5-5 5-5-5 5-5Z"/></symbol>
  </defs></svg>

  <main class="mirage-shell">
    <canvas id="game-canvas" aria-label="Открытый мир пустыни Миража"></canvas>
    <div class="screen-grade"></div><div id="damage-flash" class="damage-flash"></div>

    <section id="menu-screen" class="screen menu-screen">
      <div class="menu-grid" aria-hidden="true"></div>
      <header class="menu-header">
        <div class="brand"><span class="brand-mark">${icon('dunes')}</span><span><strong>MIRAGE</strong><small>MERIDIAN EXPEDITION</small></span></div>
        <div class="menu-system"><span><i></i>AUTONOMOUS BUILD</span><b>MR / 06</b><button class="icon-button" data-action="sound" aria-label="Выключить звук">${icon('volume')}</button></div>
      </header>
      <div class="menu-layout">
        <div class="hero">
          <div class="hero-code"><span>EXPEDITION PROTOCOL</span><b>001 — 006</b></div>
          <h1 data-text="МИРАЖ">МИРАЖ</h1>
          <div class="hero-subtitle"><i></i>ПЕПЕЛЬНЫЙ МЕРИДИАН</div>
          <p>Шесть глав. Четыре босса. Один маршрут через мёртвую пустыню.<br>Восстановите сеть и узнайте, кто отвечает из цитадели.</p>
          <div class="hero-actions">
            <button class="primary-button" data-action="start" disabled><span>ФОРМИРОВАНИЕ МИРА…</span>${icon('arrow')}<b>01</b></button>
            <button id="continue-button" class="secondary-button" data-action="continue" hidden>${icon('play')}<span>ПРОДОЛЖИТЬ</span><b>02</b></button>
            <button class="secondary-button guide-button" data-action="how">${icon('book')}<span>ПОЛЕВОЙ ГАЙД</span></button>
          </div>
          <div class="menu-controls"><span><kbd>WASD</kbd>ДВИЖЕНИЕ</span><span><kbd>МЫШЬ</kbd>СВОБОДНЫЙ ОБЗОР</span><span><kbd>ЛКМ / F</kbd>КОПЬЁ</span><span><kbd>E</kbd>ДЕЙСТВИЕ</span><span><kbd>SPACE</kbd>РЫВОК</span></div>
        </div>
        <aside class="briefing">
          <div class="briefing-top"><span>КАРТА ЭКСПЕДИЦИИ</span><b>06</b></div>
          <h2>МАРШРУТ<br><em>МЕРИДИАНА</em></h2>
          <ol>
            <li class="active"><span>01</span><div><strong>РАДИОСЛЕД</strong><small>Северный ретранслятор</small></div><i></i></li>
            <li><span>02</span><div><strong>МЁРТВЫЙ КАРАВАН</strong><small>Пепельный тракт</small></div><i></i></li>
            <li><span>03</span><div><strong>СУХОЙ КОЛОДЕЦ</strong><small>Соляные насосы</small></div><i></i></li>
            <li><span>04</span><div><strong>ПОГРЕБЁННЫЙ ГОРОД</strong><small>Архив Тар-Су</small></div><i></i></li>
            <li><span>05</span><div><strong>ГРОЗОВОЙ РАЗЛОМ</strong><small>Штормовое плато</small></div><i></i></li>
            <li><span>06</span><div><strong>ЦИТАДЕЛЬ</strong><small>Ядро Меридиана</small></div><i></i></li>
          </ol>
          <div class="briefing-stats"><div><strong>06</strong><span>РЕГИОНОВ</span></div><div><strong>04</strong><span>БОССА</span></div><div><strong>03</strong><span>АКТА</span></div></div>
        </aside>
      </div>
      <footer class="menu-footer"><span>MIRAGE TACTICAL SYSTEM / 1.2</span><span>ЛОКАЛЬНОЕ СОХРАНЕНИЕ</span><span class="desktop-only">BABYLON 3D · OFFLINE READY</span></footer>
    </section>

    <section id="hud" class="hud" hidden>
      <div class="aim-reticle" aria-hidden="true"><i></i><i></i><i></i><i></i><b></b></div>
      <div class="mission-hud">
        <div class="hud-kicker">${icon('radio')}<span id="chapter-value">ГЛАВА 1 / 6</span><b>LIVE</b></div>
        <div class="mission-content"><span class="panel-label">ТЕКУЩАЯ ЗАДАЧА</span><strong id="objective-title">Восстановите ретранслятор</strong><small id="objective-detail">Соберите ресурсы для ремонта</small>
          <div class="objective-progress"><i id="relay-progress"></i><b></b><b></b><b></b></div>
          <div class="mission-target">${icon('distance')}<span id="region-value">Северный ретранслятор</span><strong id="target-distance">0 м</strong></div>
        </div>
      </div>

      <div class="navigation-hud"><div class="nav-code">1P / NAV 77.04</div><div class="compass-line"><span>З</span><span>СЗ</span><strong id="heading-value">С 0°</strong><span>СВ</span><span>В</span></div><div class="world-state">${icon('sun', 'phase-icon')}<span id="day-value">ДЕНЬ 1</span><i></i><span id="temperature-value">18°C</span></div></div>

      <div id="boss-hud" class="boss-hud" hidden>
        <div class="boss-meta"><span>${icon('target')}ПРИОРИТЕТНАЯ ЦЕЛЬ</span><b id="boss-phase">ФАЗА 1 / 3</b></div>
        <div class="boss-name"><strong id="boss-name">АРХОНТ</strong><div id="boss-pips" class="boss-pips"><i></i><i></i><i></i></div></div>
        <div class="boss-track"><i id="boss-bar"></i></div>
      </div>

      <div class="inventory-hud">
        <div class="resource-rack">
          <div class="inventory-item scrap" title="Обломки"><span>${icon('scrap')}</span><div><small>МЕТАЛЛ</small><strong id="scrap-value">2</strong></div></div>
          <div class="inventory-item fiber" title="Волокно"><span>${icon('fiber')}</span><div><small>ВОЛОКНО</small><strong id="fiber-value">1</strong></div></div>
          <div class="inventory-item water" title="Фляги"><span>${icon('flask')}</span><div><small>ФЛЯГИ</small><strong id="water-value">2</strong></div></div>
        </div>
        <div class="hud-tools"><button class="icon-button" data-action="journal" aria-label="Журнал кампании (J)">${icon('book')}</button><button class="icon-button" data-action="sound" aria-label="Выключить звук">${icon('volume')}</button><button class="icon-button" data-action="pause" aria-label="Пауза">${icon('pause')}</button></div>
      </div>

      <div class="vitals-hud">
        <div class="vitals-title"><span>EXO / СОСТОЯНИЕ</span><b>01</b></div>
        <div class="vital health"><span class="vital-icon">${icon('heart')}</span><div class="vital-data"><div><small>ЗДОРОВЬЕ</small><strong id="health-value">100</strong></div><span class="vital-track"><i id="health-bar"></i></span></div></div>
        <div class="vital stamina"><span class="vital-icon">${icon('bolt')}</span><div class="vital-data"><div><small>ВЫНОСЛИВОСТЬ</small><strong id="stamina-value">100</strong></div><span class="vital-track"><i id="stamina-bar"></i></span></div></div>
        <div class="vital hydration"><span class="vital-icon">${icon('drop')}</span><div class="vital-data"><div><small>ГИДРАТАЦИЯ</small><strong id="hydration-value">100</strong></div><span class="vital-track"><i id="hydration-bar"></i></span></div></div>
        <small id="dodge-value">SPACE · РЫВОК ГОТОВ</small>
      </div>

      <div class="action-bar">
        <button data-recipe="sentry" class="action-slot" aria-keyshortcuts="1" aria-label="Турель: 8 обломков и 4 волокна"><kbd>1</kbd><span class="slot-icon">${icon('sentry')}</span><span class="slot-copy"><strong>SENTRY</strong><small>8 МЕТ · 4 ВОЛ</small></span><i class="slot-corner"></i></button>
        <button data-recipe="bandage" class="action-slot" aria-keyshortcuts="2" aria-label="Перевязка: 3 волокна"><kbd>2</kbd><span class="slot-icon">${icon('bandage')}</span><span class="slot-copy"><strong>ПЕРЕВЯЗКА</strong><small>3 ВОЛОКНА</small></span><i class="slot-corner"></i></button>
        <button data-recipe="drink" class="action-slot" aria-keyshortcuts="R" aria-label="Выпить воду из фляги"><kbd>R</kbd><span class="slot-icon">${icon('flask')}</span><span class="slot-copy"><strong>ГЛОТОК</strong><small>+48 ГИДРАТАЦИИ</small></span><i class="slot-corner"></i></button>
      </div>

      <div id="interaction-prompt" class="interaction-prompt" hidden><kbd>E</kbd><div><span>ВЗАИМОДЕЙСТВИЕ</span><strong id="prompt-title"></strong><small id="prompt-detail"></small></div>${icon('arrow')}</div>
      <div class="position-hud"><div class="position-coords"><span>POSITION</span><strong id="coordinates-value">6 · 8</strong></div><div><span>УСТРАНЕНО</span><strong id="kill-value">0 ЦЕЛЕЙ</strong></div><div class="gear-readout"><span>СНАРЯЖЕНИЕ</span><strong id="gear-value">КОПЬЁ I</strong></div></div>

      <div class="mobile-controls">
        <div id="joystick" class="joystick"><div class="joystick-ring"><i></i><b></b><div id="joystick-knob" class="joystick-knob">${icon('compass')}</div></div><span>MOVE</span></div>
        <div class="mobile-actions">
          <button data-mobile="dodge" aria-label="Рывок">${icon('dash')}<small>РЫВОК</small></button>
          <button data-mobile="drink" aria-label="Пить">${icon('flask')}<small>ПИТЬ</small></button>
          <button data-mobile="interact" class="interact" aria-label="Взаимодействовать">${icon('target')}<small>ДЕЙСТВИЕ</small></button>
          <button data-mobile="attack" class="attack" aria-label="Удар копьём">${icon('spear')}<small>ВЫПАД</small></button>
        </div>
      </div>
    </section>

    <div id="notifications" class="notifications" aria-live="polite"></div><div id="loot-feed" class="loot-feed" aria-live="polite"></div>
    <div id="announcement" class="announcement"><span id="announce-eyebrow"></span><h2 id="announce-title"></h2><p id="announce-detail"></p><i></i></div>

    <section id="pause-screen" class="screen overlay-screen" hidden><div class="overlay-card pause-card">
      <div class="overlay-code">SYSTEM / HOLD</div><div class="pause-heading">${icon('pause', 'overlay-symbol')}<div><div class="eyebrow">ЭКСПЕДИЦИЯ ПРИОСТАНОВЛЕНА</div><h2>ПАУЗА</h2></div></div><p>Симуляция остановлена. Прогресс текущей главы сохранён на контрольной точке.</p>
      <button class="primary-button wide" data-action="resume"><span>ВЕРНУТЬСЯ В ЭКСПЕДИЦИЮ</span>${icon('play')}</button><div class="split-buttons"><button class="secondary-button" data-action="restart">${icon('restart')}<span>КОНТРОЛЬНАЯ ТОЧКА</span></button><button class="secondary-button" data-action="menu">${icon('home')}<span>ГЛАВНОЕ МЕНЮ</span></button></div><button class="text-button" data-action="how">ОТКРЫТЬ ПОЛЕВОЙ ГАЙД</button>
    </div></section>

    <section id="story-screen" class="screen overlay-screen story-screen" hidden><div class="overlay-card story-card">
      <div class="story-rail"><span>ARCHIVE</span><strong id="story-number">01</strong><div id="story-progress"><i></i><i></i><i></i><i></i><i></i><i></i></div><b>MR-06</b></div>
      <div class="story-content"><div class="story-top"><div id="story-chapter" class="eyebrow">ГЛАВА 1 / 6</div><span>ВХОДЯЩАЯ ПЕРЕДАЧА</span></div><h2 id="story-title">Радиослед</h2><div class="speaker-line"><div id="story-avatar" class="speaker-avatar">А</div><div><small>СОБЕСЕДНИК</small><strong id="story-speaker">АЙРА</strong></div><i></i></div><p id="story-text" class="story-text"></p><button class="primary-button wide" data-action="continue-story"><span>ПРИНЯТЬ И ПРОДОЛЖИТЬ</span>${icon('arrow')}</button><small class="story-hint">J · ЖУРНАЛ ТЕКУЩЕЙ ГЛАВЫ</small></div>
    </div></section>

    <section id="end-screen" class="screen overlay-screen" hidden><div class="overlay-card end-card">
      <div class="overlay-code">EXPEDITION / REPORT</div><span id="end-symbol">${icon('radio', 'overlay-symbol')}</span><div id="end-eyebrow" class="eyebrow">ЭКСПЕДИЦИЯ ЗАВЕРШЕНА</div><h2 id="end-title">СИГНАЛ ПРИНЯТ</h2><p id="end-reason"></p><div class="run-stats"><div><span>ВРЕМЯ</span><strong id="stat-time">00:00</strong></div><div><span>ЦЕЛИ</span><strong id="stat-kills">0</strong></div><div><span>МАРШРУТ</span><strong id="stat-distance">0 м</strong></div></div><button class="primary-button wide" data-action="restart"><span>НОВАЯ ЭКСПЕДИЦИЯ</span>${icon('restart')}</button><button class="text-button" data-action="menu">ВЕРНУТЬСЯ В ГЛАВНОЕ МЕНЮ</button>
    </div></section>

    <section id="how-modal" class="screen overlay-screen how-modal" hidden><div class="overlay-card how-card">
      <button class="icon-button modal-close" data-action="close-how" aria-label="Закрыть">${icon('close')}</button><div class="overlay-code">FIELD MANUAL / MR-06</div><div class="eyebrow">БАЗА ЗНАНИЙ ЭКСПЕДИЦИИ</div><h2>ПОЛЕВОЙ ГАЙД</h2><div class="guide-grid">
        <article><span>01</span>${icon('drop')}<h3>ВОДА И ВЫЖИВАНИЕ</h3><p>Следите за голубой шкалой. <kbd>R</kbd> — пить, <kbd>E</kbd> у воды — пополнить фляги. Жара ускоряет расход.</p></article>
        <article><span>02</span>${icon('spear')}<h3>КОНТАКТНЫЙ БОЙ</h3><p><kbd>F</kbd> или короткий клик — выпад. <kbd>SPACE</kbd> / <kbd>Q</kbd> — рывок с короткой неуязвимостью.</p></article>
        <article><span>03</span>${icon('sentry')}<h3>СНАРЯЖЕНИЕ</h3><p><kbd>1</kbd> — sentry за 8 металла и 4 волокна. <kbd>2</kbd> — перевязка за 3 волокна.</p></article>
        <article><span>04</span>${icon('book')}<h3>КАМПАНИЯ</h3><p>Следуйте за маркером и расстоянием. <kbd>J</kbd> — журнал. Новая глава создаёт контрольную точку.</p></article>
      </div><div class="guide-controls"><span><kbd>WASD / СТРЕЛКИ</kbd>ДВИЖЕНИЕ</span><span><kbd>SHIFT</kbd>БЕГ</span><span><kbd>E</kbd>ДЕЙСТВИЕ</span><span><kbd>МЫШЬ</kbd>СВОБОДНЫЙ ОБЗОР</span><span><kbd>ЛКМ / F</kbd>УДАР</span><span><kbd>ESC</kbd>КУРСОР / ПАУЗА</span></div><button class="primary-button wide" data-action="close-how"><span>ЗАКРЫТЬ РУКОВОДСТВО</span>${icon('arrow')}</button>
    </div></section>
  </main>
`
}
