import fs from 'node:fs';
import path from 'node:path';
import { Resvg } from '@resvg/resvg-js';

const SPRITE_DIR = path.resolve('public/assets/sprites');
const UI_DIR = path.resolve('public/assets/ui');
fs.mkdirSync(SPRITE_DIR, { recursive: true });
fs.mkdirSync(UI_DIR, { recursive: true });

function saveSvgAsPng(outPath, width, height, innerSvg, viewBox = `0 0 ${width} ${height}`) {
  const svg = `<svg xmlns="http://www.w3.org/2000/svg" width="${width}" height="${height}" viewBox="${viewBox}">
  <defs>
    <linearGradient id="brass" x1="0" y1="0" x2="1" y2="1">
      <stop offset="0%" stop-color="#f6dc96"/>
      <stop offset="45%" stop-color="#c4943f"/>
      <stop offset="100%" stop-color="#694818"/>
    </linearGradient>
    <linearGradient id="obsidian" x1="0" y1="0" x2="0" y2="1">
      <stop offset="0%" stop-color="#283358"/>
      <stop offset="55%" stop-color="#141b33"/>
      <stop offset="100%" stop-color="#090d1b"/>
    </linearGradient>
    <linearGradient id="silver" x1="0" y1="0" x2="1" y2="1">
      <stop offset="0%" stop-color="#eef6ff"/>
      <stop offset="50%" stop-color="#90a7c9"/>
      <stop offset="100%" stop-color="#415270"/>
    </linearGradient>
    <radialGradient id="solarGlow" cx="50%" cy="50%" r="50%">
      <stop offset="0%" stop-color="#fff8d6" stop-opacity="1"/>
      <stop offset="45%" stop-color="#ffb838" stop-opacity="0.9"/>
      <stop offset="100%" stop-color="#ff6a00" stop-opacity="0"/>
    </radialGradient>
    <radialGradient id="cyanGlow" cx="50%" cy="50%" r="50%">
      <stop offset="0%" stop-color="#e8ffff" stop-opacity="1"/>
      <stop offset="45%" stop-color="#3ce8ff" stop-opacity="0.88"/>
      <stop offset="100%" stop-color="#0a58ff" stop-opacity="0"/>
    </radialGradient>
    <radialGradient id="voidGlow" cx="50%" cy="50%" r="50%">
      <stop offset="0%" stop-color="#f6d8ff" stop-opacity="1"/>
      <stop offset="45%" stop-color="#b942ff" stop-opacity="0.85"/>
      <stop offset="100%" stop-color="#360568" stop-opacity="0"/>
    </radialGradient>
  </defs>
  ${innerSvg}
</svg>`;
  const resvg = new Resvg(svg, {
    fitTo: { mode: 'width', value: width },
    background: 'rgba(0,0,0,0)',
  });
  const pngBuffer = resvg.render().asPng();
  fs.writeFileSync(outPath, pngBuffer);
}

// ============================================================================
// 1. GRID TILES (84x84)
// ============================================================================
function buildTile(name, rimColor, runeColor, fillTop, fillBot, extra = '') {
  saveSvgAsPng(
    path.join(SPRITE_DIR, `tile_${name}.png`),
    84,
    84,
    `
    <defs>
      <linearGradient id="tf" x1="0" y1="0" x2="0" y2="1">
        <stop offset="0%" stop-color="${fillTop}"/>
        <stop offset="100%" stop-color="${fillBot}"/>
      </linearGradient>
    </defs>
    <rect x="4" y="4" width="76" height="76" rx="11" fill="url(#tf)" stroke="${rimColor}" stroke-width="2.5"/>
    <rect x="9" y="9" width="66" height="66" rx="7" fill="none" stroke="${runeColor}" stroke-opacity="0.38" stroke-width="1.2"/>
    <path d="M10 20 L20 10 M64 10 L74 20 M10 64 L20 74 M64 74 L74 64" stroke="${rimColor}" stroke-opacity="0.65" stroke-width="1.6"/>
    <circle cx="42" cy="42" r="16" fill="none" stroke="${runeColor}" stroke-opacity="0.28" stroke-width="1.2" stroke-dasharray="4 3"/>
    ${extra}
    `
  );
}

buildTile('normal', '#6e5832', '#6485c4', '#18213d', '#0d1327');
buildTile(
  'radiant',
  '#f0b94b',
  '#ffe184',
  '#2d2415',
  '#17120c',
  `<circle cx="42" cy="42" r="24" fill="url(#solarGlow)" opacity="0.45"/>
   <polygon points="42,18 47,37 66,42 47,47 42,66 37,47 18,42 37,37" fill="#ffd269" fill-opacity="0.45" stroke="#ffe79e" stroke-width="1.2"/>`
);
buildTile(
  'haste',
  '#38d8ff',
  '#96f2ff',
  '#132b3d',
  '#0a1826',
  `<circle cx="42" cy="42" r="24" fill="url(#cyanGlow)" opacity="0.42"/>
   <path d="M26 48 L42 26 L58 48 M26 58 L42 36 L58 58" fill="none" stroke="#6ef2ff" stroke-width="2.4" stroke-linecap="round"/>`
);
buildTile(
  'prismatic',
  '#d96bff',
  '#ff9ef5',
  '#29173b',
  '#140b24',
  `<circle cx="42" cy="42" r="25" fill="url(#voidGlow)" opacity="0.48"/>
   <polygon points="42,18 62,42 42,66 22,42" fill="none" stroke="#7dffea" stroke-width="2"/>
   <polygon points="42,25 56,42 42,59 28,42" fill="#e580ff" fill-opacity="0.32" stroke="#ffd269" stroke-width="1.4"/>`
);
buildTile(
  'locked',
  '#414a66',
  '#5a6589',
  '#101424',
  '#080b14',
  `<path d="M14 14 L70 70 M70 14 L14 70" stroke="#39425e" stroke-width="2.5"/>
   <rect x="29" y="38" width="26" height="20" rx="4" fill="#27304a" stroke="#8595bd" stroke-width="2"/>
   <path d="M34 38 V31 A8 8 0 0 1 50 31 V38" fill="none" stroke="#8595bd" stroke-width="2.5"/>
   <circle cx="42" cy="48" r="3" fill="#ffd269"/>`
);
buildTile(
  'hover',
  '#ffe58f',
  '#ffffff',
  '#25335c',
  '#162040',
  `<rect x="6" y="6" width="72" height="72" rx="10" fill="none" stroke="#fff3b8" stroke-width="2.5"/>`
);

// ============================================================================
// 2. CIRCUIT MODULES (96x96) — 6 TURRETS + 8 PRISMS/MODIFIERS + EMITTER + CORE
// ============================================================================
function moduleBase(accent, ringColor = 'url(#brass)') {
  return `
    <circle cx="48" cy="48" r="40" fill="url(#obsidian)" stroke="${ringColor}" stroke-width="3.5"/>
    <circle cx="48" cy="48" r="33" fill="none" stroke="${accent}" stroke-opacity="0.45" stroke-width="1.5" stroke-dasharray="6 4"/>
    <polygon points="48,5 53,12 43,12" fill="#f3d283"/>
    <polygon points="91,48 84,53 84,43" fill="#f3d283"/>
    <polygon points="48,91 53,84 43,84" fill="#f3d283"/>
    <polygon points="5,48 12,53 12,43" fill="#f3d283"/>
  `;
}

const MODULES_SVG = {
  mod_turret_blaze: `
    ${moduleBase('#ff6e27')}
    <circle cx="48" cy="48" r="26" fill="url(#solarGlow)" opacity="0.6"/>
    <rect x="46" y="36" width="34" height="24" rx="5" fill="url(#brass)" stroke="#ffdf8e" stroke-width="1.8"/>
    <rect x="66" y="39" width="16" height="18" rx="3" fill="#ff5500" stroke="#ffd27d" stroke-width="1.5"/>
    <polygon points="48,22 65,48 48,74 26,58 26,38" fill="#a82810" stroke="#ffb35c" stroke-width="2.2"/>
    <path d="M36 48 C40 34, 54 36, 58 48 C54 60, 40 62, 36 48 Z" fill="#ffdf73"/>
    <circle cx="46" cy="48" r="6" fill="#ffffff"/>
  `,
  mod_turret_arc: `
    ${moduleBase('#47c8ff')}
    <circle cx="48" cy="48" r="28" fill="url(#cyanGlow)" opacity="0.65"/>
    <polygon points="48,16 60,36 78,48 60,60 48,80 36,60 18,48 36,36" fill="#183d6e" stroke="#6eeeff" stroke-width="2.2"/>
    <circle cx="48" cy="48" r="15" fill="#10274a" stroke="url(#brass)" stroke-width="3"/>
    <path d="M44 28 L38 47 L49 47 L43 68 L60 44 L48 44 Z" fill="#bffaff" stroke="#3be2ff" stroke-width="1.5"/>
  `,
  mod_turret_frost: `
    ${moduleBase('#70f3ff')}
    <circle cx="48" cy="48" r="25" fill="url(#cyanGlow)" opacity="0.5"/>
    <polygon points="20,48 44,32 84,48 44,64" fill="#3198cc" stroke="#d9ffff" stroke-width="2.2"/>
    <polygon points="34,48 50,39 78,48 50,57" fill="#aefbff"/>
    <polygon points="44,20 52,40 44,48 36,40" fill="#5ac8f5" stroke="#d9ffff" stroke-width="1.5"/>
    <polygon points="44,76 52,56 44,48 36,56" fill="#5ac8f5" stroke="#d9ffff" stroke-width="1.5"/>
    <circle cx="44" cy="48" r="6" fill="#ffffff"/>
  `,
  mod_turret_venom: `
    ${moduleBase('#5dff7a')}
    <defs>
      <radialGradient id="vg" cx="50%" cy="50%" r="50%">
        <stop offset="0%" stop-color="#e3ff9e"/>
        <stop offset="55%" stop-color="#2ee656"/>
        <stop offset="100%" stop-color="#0b4d22" stop-opacity="0"/>
      </radialGradient>
    </defs>
    <circle cx="48" cy="48" r="27" fill="url(#vg)" opacity="0.7"/>
    <polygon points="52,32 82,39 60,46" fill="#39d85e" stroke="#d8ffab" stroke-width="1.6"/>
    <polygon points="54,43 86,48 54,53" fill="#67ff78" stroke="#ffffff" stroke-width="1.6"/>
    <polygon points="52,64 82,57 60,50" fill="#39d85e" stroke="#d8ffab" stroke-width="1.6"/>
    <circle cx="44" cy="48" r="16" fill="#143b22" stroke="url(#brass)" stroke-width="3"/>
    <circle cx="44" cy="48" r="8" fill="#8cff66"/>
  `,
  mod_turret_beam: `
    ${moduleBase('#ffd452')}
    <circle cx="48" cy="48" r="29" fill="url(#solarGlow)" opacity="0.72"/>
    <polygon points="28,26 68,36 84,48 68,60 28,70 36,48" fill="url(#brass)" stroke="#fff4bd" stroke-width="2"/>
    <ellipse cx="52" cy="48" rx="14" ry="10" fill="#ff9e1b" stroke="#fff8d6" stroke-width="2"/>
    <line x1="52" y1="48" x2="88" y2="48" stroke="#fffbe0" stroke-width="5" stroke-linecap="round"/>
    <circle cx="50" cy="48" r="6" fill="#ffffff"/>
  `,
  mod_turret_mortar: `
    ${moduleBase('#c66bff')}
    <circle cx="48" cy="48" r="27" fill="url(#voidGlow)" opacity="0.7"/>
    <circle cx="48" cy="48" r="23" fill="#26133d" stroke="url(#brass)" stroke-width="3.5"/>
    <circle cx="48" cy="48" r="14" fill="#120721" stroke="#d982ff" stroke-width="2.2"/>
    <circle cx="48" cy="48" r="7" fill="#f3b8ff"/>
    <path d="M28 28 L38 38 M68 28 L58 38 M28 68 L38 58 M68 68 L58 58" stroke="#f5d782" stroke-width="3" stroke-linecap="round"/>
  `,
  // PRISMS & RELAYS (note default orientation faces East = 0 deg)
  mod_splitter: `
    ${moduleBase('#59f0ff', 'url(#silver)')}
    <circle cx="48" cy="48" r="26" fill="url(#cyanGlow)" opacity="0.55"/>
    <!-- Incoming West arrow, splitting North & South -->
    <path d="M14 48 H44 M48 44 V14 M48 52 V82" stroke="#9efaff" stroke-width="4" stroke-linecap="round"/>
    <polygon points="48,10 41,22 55,22" fill="#ffffff"/>
    <polygon points="48,86 41,74 55,74" fill="#ffffff"/>
    <polygon points="48,26 68,48 48,70 28,48" fill="#2cc9eb" fill-opacity="0.85" stroke="#ffffff" stroke-width="2.4"/>
    <circle cx="48" cy="48" r="6" fill="#ffffff"/>
  `,
  mod_tri_prism: `
    ${moduleBase('#ff8af2', 'url(#brass)')}
    <circle cx="48" cy="48" r="28" fill="url(#voidGlow)" opacity="0.6"/>
    <path d="M14 48 H82 M48 14 V82" stroke="#ffd6ff" stroke-width="3.6" stroke-linecap="round"/>
    <polygon points="48,10 42,21 54,21" fill="#fff2ab"/>
    <polygon points="48,86 42,75 54,75" fill="#fff2ab"/>
    <polygon points="86,48 75,42 75,54" fill="#fff2ab"/>
    <polygon points="48,24 72,48 48,72 24,48" fill="#c74dff" fill-opacity="0.82" stroke="#fff4bd" stroke-width="2.4"/>
    <polygon points="48,34 62,48 48,62 34,48" fill="#7df9ff" fill-opacity="0.75"/>
  `,
  mod_mirror: `
    ${moduleBase('#ffe07d', 'url(#silver)')}
    <!-- Default rotation 0: reflects incoming pulse 90deg clockwise (West->East ray bends South, or standard diagonal slash mirror) -->
    <!-- Slash "\" mirror: reflects East-moving ray -> South, North-moving ray -> West, etc. Let's make rotation 0 clearly show path West->Center->South AND diagonal crystal mirror! -->
    <path d="M14 48 H48 V82" fill="none" stroke="#7ef9ff" stroke-width="4" stroke-linecap="round" stroke-dasharray="4 2"/>
    <polygon points="48,86 41,74 55,74" fill="#7ef9ff"/>
    <g transform="rotate(45 48 48)">
      <rect x="18" y="41" width="60" height="14" rx="5" fill="url(#silver)" stroke="#fff4bd" stroke-width="2.2"/>
      <rect x="24" y="45" width="48" height="6" rx="3" fill="#b8fbff"/>
    </g>
  `,
  mod_amp_lens: `
    ${moduleBase('#ffbe3b')}
    <circle cx="48" cy="48" r="28" fill="url(#solarGlow)" opacity="0.68"/>
    <path d="M14 48 H82" stroke="#fff3b0" stroke-width="4.5" stroke-linecap="round"/>
    <polygon points="86,48 73,40 73,56" fill="#fff6c7"/>
    <ellipse cx="48" cy="48" rx="14" ry="28" fill="#ff9d1c" fill-opacity="0.78" stroke="#fff5c0" stroke-width="2.6"/>
    <ellipse cx="48" cy="48" rx="7" ry="20" fill="#fff7d1" fill-opacity="0.85"/>
  `,
  mod_pyro_lens: `
    ${moduleBase('#ff5722')}
    <circle cx="48" cy="48" r="28" fill="url(#solarGlow)" opacity="0.72"/>
    <path d="M48 18 C66 32, 68 56, 48 76 C28 56, 30 32, 48 18 Z" fill="#ff4500" stroke="#ffd166" stroke-width="2.4"/>
    <path d="M48 30 C58 40, 58 55, 48 66 C38 55, 38 40, 48 30 Z" fill="#ffd166"/>
    <circle cx="48" cy="50" r="6" fill="#ffffff"/>
  `,
  mod_cryo_lens: `
    ${moduleBase('#4deeea')}
    <circle cx="48" cy="48" r="27" fill="url(#cyanGlow)" opacity="0.65"/>
    <g stroke="#d4ffff" stroke-width="3" stroke-linecap="round">
      <line x1="48" y1="20" x2="48" y2="76"/>
      <line x1="24" y1="34" x2="72" y2="62"/>
      <line x1="24" y1="62" x2="72" y2="34"/>
    </g>
    <polygon points="48,32 62,40 62,56 48,64 34,56 34,40" fill="#29b6f6" fill-opacity="0.78" stroke="#ffffff" stroke-width="2"/>
  `,
  mod_volt_lens: `
    ${moduleBase('#8c6bff')}
    <circle cx="48" cy="48" r="27" fill="url(#voidGlow)" opacity="0.65"/>
    <circle cx="48" cy="48" r="20" fill="none" stroke="#70e4ff" stroke-width="2.5"/>
    <polygon points="52,20 32,50 47,50 42,76 66,44 50,44" fill="#ffe96b" stroke="#ffffff" stroke-width="1.8"/>
  `,
  mod_midas_core: `
    ${moduleBase('#ffd700')}
    <circle cx="48" cy="48" r="28" fill="url(#solarGlow)" opacity="0.75"/>
    <polygon points="48,20 72,34 72,62 48,76 24,62 24,34" fill="url(#brass)" stroke="#fff5b8" stroke-width="2.5"/>
    <circle cx="48" cy="48" r="15" fill="#f7b731" stroke="#fff7cc" stroke-width="2"/>
    <polygon points="48,37 54,48 48,59 42,48" fill="#fffbe6"/>
  `,
};

for (const [key, svg] of Object.entries(MODULES_SVG)) {
  saveSvgAsPng(path.join(SPRITE_DIR, `${key}.png`), 96, 96, svg);
}

// Emitter, Core, Void Rift
saveSvgAsPng(
  path.join(SPRITE_DIR, 'emitter_solar.png'),
  96,
  96,
  `
  <circle cx="44" cy="48" r="36" fill="url(#obsidian)" stroke="url(#brass)" stroke-width="4"/>
  <circle cx="44" cy="48" r="28" fill="url(#solarGlow)" opacity="0.85"/>
  <polygon points="44,18 54,38 78,48 54,58 44,78 34,58 14,48 34,38" fill="#ffbe3b" stroke="#fff7cf" stroke-width="2.2"/>
  <polygon points="66,34 92,48 66,62" fill="#ffe694" stroke="#b87e22" stroke-width="2"/>
  <circle cx="44" cy="48" r="10" fill="#ffffff"/>
  `
);

saveSvgAsPng(
  path.join(SPRITE_DIR, 'bastion_core.png'),
  128,
  128,
  `
  <circle cx="64" cy="64" r="56" fill="url(#solarGlow)" opacity="0.55"/>
  <polygon points="64,8 80,26 106,22 102,48 120,64 102,80 106,106 80,102 64,120 48,102 22,106 26,80 8,64 26,48 22,22 48,26" fill="url(#obsidian)" stroke="url(#brass)" stroke-width="4"/>
  <circle cx="64" cy="64" r="36" fill="#16254c" stroke="#7ef9ff" stroke-width="2.5"/>
  <polygon points="64,22 76,52 106,64 76,76 64,106 52,76 22,64 52,52" fill="#46dcff" fill-opacity="0.85" stroke="#ffffff" stroke-width="2.2"/>
  <polygon points="64,36 72,56 92,64 72,72 64,92 56,72 36,64 56,56" fill="#ffd86b"/>
  <circle cx="64" cy="64" r="10" fill="#ffffff"/>
  `
);

saveSvgAsPng(
  path.join(SPRITE_DIR, 'void_rift.png'),
  128,
  128,
  `
  <circle cx="64" cy="64" r="58" fill="url(#voidGlow)" opacity="0.85"/>
  <ellipse cx="64" cy="64" rx="44" ry="54" fill="#18052e" stroke="#c85cff" stroke-width="3.5"/>
  <ellipse cx="64" cy="64" rx="28" ry="38" fill="#090114" stroke="#75e6ff" stroke-width="2" stroke-dasharray="8 5"/>
  <path d="M64 16 C88 34, 92 94, 64 112 C36 94, 40 34, 64 16 Z" fill="none" stroke="#f0a8ff" stroke-width="2.2"/>
  `
);

// ============================================================================
// 3. MULTI-FRAME ANIMATED ENEMY & BOSS SPRITESHEETS (3 frames each!)
// ============================================================================
function buildEnemySheet(filename, frameSize, frameFn) {
  const totalW = frameSize * 3;
  let inner = '';
  for (let f = 0; f < 3; f++) {
    inner += `<g transform="translate(${f * frameSize}, 0)">${frameFn(f, frameSize)}</g>`;
  }
  saveSvgAsPng(path.join(SPRITE_DIR, filename), totalW, frameSize, inner);
}

// Standard Enemies (80x80 per frame -> 240x80)
buildEnemySheet('enemy_skitterling.png', 80, (f) => {
  const legShift = (f - 1) * 5;
  return `
    <g stroke="#b84dff" stroke-width="3" stroke-linecap="round">
      <line x1="34" y1="30" x2="${22 + legShift}" y2="14"/>
      <line x1="46" y1="30" x2="${56 - legShift}" y2="14"/>
      <line x1="32" y1="50" x2="${22 - legShift}" y2="66"/>
      <line x1="48" y1="50" x2="${56 + legShift}" y2="66"/>
    </g>
    <ellipse cx="42" cy="40" rx="20" ry="15" fill="#2a1142" stroke="#d57dff" stroke-width="2.4"/>
    <polygon points="16,40 30,29 34,40 30,51" fill="#5e1d8c" stroke="#ff9ef5" stroke-width="1.8"/>
    <circle cx="25" cy="36" r="3" fill="#ff3b6b"/>
    <circle cx="25" cy="44" r="3" fill="#ff3b6b"/>
    <circle cx="48" cy="40" r="${5 + f}" fill="#c94dff" fill-opacity="0.7"/>
  `;
});

buildEnemySheet('enemy_bulwark.png', 80, (f) => {
  const bob = f === 1 ? -2 : f === 2 ? 2 : 0;
  return `
    <g transform="translate(0,${bob})">
      <polygon points="18,24 44,14 66,26 66,54 44,66 18,56" fill="#23283d" stroke="#96a8cf" stroke-width="3"/>
      <polygon points="14,22 28,18 28,62 14,58" fill="#465378" stroke="#ffd269" stroke-width="2.2"/>
      <path d="M34 26 L52 40 L34 54" fill="none" stroke="#ff6b35" stroke-width="3"/>
      <circle cx="38" cy="40" r="${6 + f}" fill="#ff7838"/>
      <circle cx="24" cy="34" r="2.8" fill="#ffe66d"/>
      <circle cx="24" cy="46" r="2.8" fill="#ffe66d"/>
    </g>
  `;
});

buildEnemySheet('enemy_wraith.png', 80, (f) => {
  const wave = (f - 1) * 4;
  return `
    <circle cx="38" cy="40" r="26" fill="url(#cyanGlow)" opacity="${0.35 + f * 0.1}"/>
    <path d="M18 40 C22 20, 48 18, 66 ${32 + wave} C56 40, 62 46, 68 ${50 - wave} C46 62, 22 60, 18 40 Z" fill="#173854" fill-opacity="0.88" stroke="#7afcff" stroke-width="2.2"/>
    <circle cx="28" cy="36" r="3.2" fill="#ffffff"/>
    <circle cx="28" cy="45" r="3.2" fill="#ffffff"/>
    <polygon points="10,40 20,35 18,45" fill="#9efcff"/>
  `;
});

buildEnemySheet('enemy_brood.png', 80, (f) => {
  const pulse = f * 1.5;
  return `
    <ellipse cx="44" cy="40" rx="${23 + pulse * 0.5}" ry="${19 + pulse * 0.5}" fill="#2b1738" stroke="#a6ff5e" stroke-width="2.4"/>
    <circle cx="48" cy="32" r="${5 + pulse * 0.4}" fill="#8aff47"/>
    <circle cx="54" cy="44" r="${6 - pulse * 0.3}" fill="#6ee838"/>
    <circle cx="42" cy="49" r="4.5" fill="#b4ff70"/>
    <polygon points="16,40 30,30 30,50" fill="#462259" stroke="#d4ff8a" stroke-width="2"/>
    <circle cx="24" cy="36" r="2.8" fill="#d9ff66"/>
    <circle cx="24" cy="44" r="2.8" fill="#d9ff66"/>
  `;
});

buildEnemySheet('enemy_mite.png', 80, (f) => {
  const w = (f - 1) * 3;
  return `
    <ellipse cx="40" cy="${40 + w * 0.4}" rx="13" ry="10" fill="#2c451d" stroke="#a6ff5e" stroke-width="2"/>
    <polygon points="22,40 32,34 32,46" fill="#7dff4d"/>
    <circle cx="38" cy="40" r="4" fill="#dfff9e"/>
  `;
});

buildEnemySheet('enemy_acolyte.png', 80, (f) => {
  const rot = f * 30;
  return `
    <g transform="rotate(${rot} 40 40)">
      <circle cx="40" cy="40" r="25" fill="none" stroke="#47ffb2" stroke-width="2" stroke-dasharray="7 5"/>
    </g>
    <polygon points="18,40 38,22 60,30 54,40 60,50 38,58" fill="#173b34" stroke="#6bffca" stroke-width="2.2"/>
    <circle cx="36" cy="40" r="7" fill="#59ffc4"/>
    <path d="M32 40 H40 M36 36 V44" stroke="#0b2922" stroke-width="2.4"/>
  `;
});

buildEnemySheet('enemy_sprinter.png', 80, (f) => {
  const stretch = f === 1 ? 4 : 0;
  return `
    <polygon points="${10 - stretch},40 34,24 68,32 54,40 68,48 34,56" fill="#4a1228" stroke="#ff5c8a" stroke-width="2.2"/>
    <polygon points="22,40 44,32 56,40 44,48" fill="#ff2e63"/>
    <circle cx="24" cy="37" r="2.5" fill="#fff275"/>
    <circle cx="24" cy="43" r="2.5" fill="#fff275"/>
  `;
});

buildEnemySheet('enemy_sentinel.png', 80, (f) => {
  const rot = f * 20;
  return `
    <g transform="rotate(${rot} 40 40)">
      <polygon points="40,12 64,26 64,54 40,68 16,54 16,26" fill="none" stroke="#66d9ff" stroke-width="2.5"/>
    </g>
    <polygon points="22,40 40,22 58,40 40,58" fill="#232c4d" stroke="url(#brass)" stroke-width="2.6"/>
    <circle cx="40" cy="40" r="8" fill="#6be8ff"/>
    <circle cx="40" cy="40" r="3.5" fill="#ffffff"/>
  `;
});

// Bosses (120x120 per frame -> 360x120)
buildEnemySheet('boss_colossus.png', 120, (f) => {
  const swing = (f - 1) * 5;
  return `
    <circle cx="60" cy="60" r="48" fill="url(#voidGlow)" opacity="0.55"/>
    <polygon points="18,${28 + swing} 44,14 54,32 26,42" fill="#47205c" stroke="#f5b841" stroke-width="2.8"/>
    <polygon points="18,${92 - swing} 44,106 54,88 26,78" fill="#47205c" stroke="#f5b841" stroke-width="2.8"/>
    <polygon points="26,40 58,20 96,34 102,60 96,86 58,100 26,80" fill="#231433" stroke="url(#brass)" stroke-width="4"/>
    <polygon points="42,42 74,34 86,60 74,86 42,78" fill="#541c73" stroke="#e27aff" stroke-width="2.4"/>
    <circle cx="56" cy="60" r="${11 + f}" fill="#ff4d79"/>
    <circle cx="36" cy="48" r="4" fill="#ffe66d"/>
    <circle cx="36" cy="72" r="4" fill="#ffe66d"/>
  `;
});

buildEnemySheet('boss_weaver.png', 120, (f) => {
  const wing = (f - 1) * 6;
  return `
    <circle cx="60" cy="60" r="50" fill="url(#cyanGlow)" opacity="0.45"/>
    <polygon points="46,46 84,${10 + wing} 106,30 68,52" fill="#233d6e" stroke="#73f7ff" stroke-width="2.5"/>
    <polygon points="46,74 84,${110 - wing} 106,90 68,68" fill="#233d6e" stroke="#73f7ff" stroke-width="2.5"/>
    <polygon points="16,60 44,36 86,46 96,60 86,74 44,84" fill="#2c1447" stroke="#df80ff" stroke-width="3.2"/>
    <circle cx="54" cy="60" r="12" fill="#63f5ff"/>
    <polygon points="54,48 64,60 54,72 44,60" fill="#ffffff"/>
  `;
});

buildEnemySheet('boss_archon.png', 120, (f) => {
  const rot = f * 24;
  return `
    <circle cx="60" cy="60" r="54" fill="url(#voidGlow)" opacity="0.75"/>
    <g transform="rotate(${rot} 60 60)">
      <circle cx="60" cy="60" r="44" fill="none" stroke="url(#brass)" stroke-width="3.5" stroke-dasharray="16 8"/>
      <polygon points="60,10 66,22 54,22" fill="#ffd86b"/>
      <polygon points="60,110 66,98 54,98" fill="#ffd86b"/>
    </g>
    <polygon points="14,60 42,26 84,30 104,60 84,90 42,94" fill="#180b2e" stroke="#ffd86b" stroke-width="3.6"/>
    <polygon points="30,60 54,38 80,60 54,82" fill="#8b2be2" stroke="#ff9ef5" stroke-width="2.5"/>
    <circle cx="54" cy="60" r="${12 + f}" fill="#ffe985"/>
    <circle cx="54" cy="60" r="6" fill="#18082b"/>
  `;
});

// ============================================================================
// 4. 18 RELIC MEDALLIONS (72x72)
// ============================================================================
const RELIC_SPECS = [
  ['relic_prism_crown', '#ffd452', '#52eeff', `<polygon points="18,48 22,26 36,36 46,20 54,36 56,48" fill="url(#brass)" stroke="#fff7c2" stroke-width="2"/><circle cx="36" cy="30" r="4" fill="#63f5ff"/>`],
  ['relic_pendulum', '#56e4ff', '#ffd452', `<line x1="36" y1="14" x2="36" y2="44" stroke="#fff3b0" stroke-width="3"/><polygon points="36,36 48,48 36,60 24,48" fill="#46dcff" stroke="#ffffff" stroke-width="2"/>`],
  ['relic_glass_lens', '#ff7af5', '#7afcff', `<circle cx="36" cy="36" r="18" fill="#4ce8ff" fill-opacity="0.55" stroke="#ffffff" stroke-width="2.5"/><line x1="25" y1="25" x2="47" y2="47" stroke="#ffffff" stroke-width="2.5"/>`],
  ['relic_alembic', '#66ff8c', '#c66bff', `<path d="M30 18 H42 V28 L52 48 A10 10 0 0 1 42 56 H30 A10 10 0 0 1 20 48 L30 28 Z" fill="#2bd968" stroke="#e4ffbd" stroke-width="2"/>`],
  ['relic_storm_coil', '#4cc9f0', '#ffe66d', `<path d="M26 22 H46 L30 36 H46 L28 54" fill="none" stroke="#ffe66d" stroke-width="4" stroke-linecap="round" stroke-linejoin="round"/>`],
  ['relic_shatter_core', '#80ffea', '#4895ef', `<polygon points="36,14 52,28 46,54 26,54 20,28" fill="#5ce1e6" stroke="#ffffff" stroke-width="2"/><path d="M28 26 L42 42 L32 50" fill="none" stroke="#0d2238" stroke-width="2.2"/>`],
  ['relic_solar_heart', '#ff9f1c', '#ff4040', `<polygon points="36,16 54,34 36,58 18,34" fill="#ff5e1a" stroke="#fff0a6" stroke-width="2.2"/><circle cx="36" cy="36" r="7" fill="#fff7cc"/>`],
  ['relic_abacus', '#ffd166', '#f4a261', `<rect x="20" y="20" width="32" height="32" rx="4" fill="none" stroke="url(#brass)" stroke-width="3"/><line x1="20" y1="30" x2="52" y2="30" stroke="#ffd166" stroke-width="2"/><line x1="20" y1="42" x2="52" y2="42" stroke="#ffd166" stroke-width="2"/><circle cx="30" cy="30" r="4" fill="#fff3b0"/><circle cx="42" cy="42" r="4" fill="#fff3b0"/>`],
  ['relic_void_mirror', '#b5179e', '#7209b7', `<ellipse cx="36" cy="36" rx="14" ry="20" fill="#2b0945" stroke="#f788ff" stroke-width="3"/><ellipse cx="36" cy="36" rx="7" ry="12" fill="#b942ff"/>`],
  ['relic_twin_dynamo', '#ffbe0b', '#3a86ff', `<circle cx="28" cy="36" r="11" fill="#ff9f1c" stroke="#ffffff" stroke-width="2"/><circle cx="44" cy="36" r="11" fill="#3ec8ff" stroke="#ffffff" stroke-width="2"/>`],
  ['relic_exec_sigil', '#ef233c', '#ffd166', `<polygon points="36,14 44,34 56,36 42,44 36,60 30,44 16,36 28,34" fill="#e63946" stroke="#ffd166" stroke-width="2"/>`],
  ['relic_compass', '#4cc9f0', '#f7b731', `<circle cx="36" cy="36" r="18" fill="#14294a" stroke="url(#brass)" stroke-width="2.5"/><polygon points="36,18 42,36 36,54 30,36" fill="#6ef2ff"/><polygon points="36,18 42,36 30,36" fill="#ff595e"/>`],
  ['relic_phoenix', '#ff6b35', '#ffd166', `<path d="M22 52 C24 30, 38 18, 52 18 C48 34, 42 46, 22 52 Z" fill="#ff7b29" stroke="#fff3b0" stroke-width="2"/>`],
  ['relic_capacitor', '#72efdd', '#5390d9', `<rect x="24" y="20" width="24" height="32" rx="5" fill="#1e3d6b" stroke="#80ffdb" stroke-width="2.4"/><line x1="28" y1="30" x2="44" y2="30" stroke="#80ffdb" stroke-width="3"/><line x1="28" y1="40" x2="44" y2="40" stroke="#80ffdb" stroke-width="3"/>`],
  ['relic_trinity', '#ffbe0b', '#8338ec', `<circle cx="36" cy="24" r="8" fill="#ff595e"/><circle cx="26" cy="42" r="8" fill="#4cc9f0"/><circle cx="46" cy="42" r="8" fill="#80ed99"/>`],
  ['relic_monocle', '#ffd166', '#90e0ef', `<circle cx="36" cy="34" r="15" fill="#48cae4" fill-opacity="0.45" stroke="url(#brass)" stroke-width="3.2"/><path d="M48 44 Q52 54 44 58" fill="none" stroke="#ffd166" stroke-width="2.2"/>`],
  ['relic_vampiric', '#d90429', '#ff758f', `<polygon points="36,16 50,36 36,56 22,36" fill="#a4133c" stroke="#ffccd5" stroke-width="2"/><circle cx="36" cy="36" r="5" fill="#ff4d6d"/>`],
  ['relic_archon_shard', '#c77dff', '#ffe66d', `<polygon points="36,12 52,32 44,58 28,58 20,32" fill="#7b2cbf" stroke="#ffe66d" stroke-width="2.4"/><polygon points="36,20 44,34 36,50 28,34" fill="#e0aaff"/>`],
];

for (const [name, c1, c2, shape] of RELIC_SPECS) {
  saveSvgAsPng(
    path.join(SPRITE_DIR, `${name}.png`),
    72,
    72,
    `
    <circle cx="36" cy="36" r="32" fill="url(#obsidian)" stroke="url(#brass)" stroke-width="3"/>
    <circle cx="36" cy="36" r="26" fill="none" stroke="${c1}" stroke-opacity="0.6" stroke-width="1.5"/>
    <circle cx="36" cy="36" r="20" fill="${c2}" fill-opacity="0.18"/>
    ${shape}
    `
  );
}

// ============================================================================
// 5. PROJECTILES, LOOT & VFX SPRITES
// ============================================================================
saveSvgAsPng(
  path.join(SPRITE_DIR, 'proj_pulse.png'),
  48,
  48,
  `<circle cx="24" cy="24" r="22" fill="url(#solarGlow)"/>
   <polygon points="24,6 30,18 42,24 30,30 24,42 18,30 6,24 18,18" fill="#fff4bd"/>
   <circle cx="24" cy="24" r="7" fill="#ffffff"/>`
);

saveSvgAsPng(
  path.join(SPRITE_DIR, 'proj_blaze.png'),
  40,
  40,
  `<circle cx="20" cy="20" r="18" fill="url(#solarGlow)"/>
   <path d="M8 20 C14 10, 28 10, 36 20 C28 30, 14 30, 8 20 Z" fill="#ff5400" stroke="#ffe169" stroke-width="2"/>
   <circle cx="24" cy="20" r="5" fill="#ffffff"/>`
);

saveSvgAsPng(
  path.join(SPRITE_DIR, 'proj_frost.png'),
  40,
  40,
  `<polygon points="4,20 20,11 38,20 20,29" fill="#5ce1e6" stroke="#ffffff" stroke-width="2"/>
   <polygon points="14,20 24,15 34,20 24,25" fill="#ffffff"/>`
);

saveSvgAsPng(
  path.join(SPRITE_DIR, 'proj_venom.png'),
  36,
  36,
  `<polygon points="4,18 18,10 34,18 18,26" fill="#38e54d" stroke="#d8ff9c" stroke-width="1.8"/>`
);

saveSvgAsPng(
  path.join(SPRITE_DIR, 'proj_mortar.png'),
  44,
  44,
  `<circle cx="22" cy="22" r="18" fill="url(#voidGlow)"/>
   <circle cx="22" cy="22" r="10" fill="#5a189a" stroke="#f3b8ff" stroke-width="2.2"/>
   <circle cx="22" cy="22" r="4" fill="#ffffff"/>`
);

saveSvgAsPng(
  path.join(SPRITE_DIR, 'proj_arcane.png'),
  36,
  36,
  `<circle cx="18" cy="18" r="15" fill="url(#cyanGlow)"/>
   <polygon points="8,18 18,10 30,18 18,26" fill="#ffffff"/>`
);

saveSvgAsPng(
  path.join(SPRITE_DIR, 'vfx_spark.png'),
  32,
  32,
  `<polygon points="16,2 20,12 30,16 20,20 16,30 12,20 2,16 12,12" fill="#ffffff"/>`
);

saveSvgAsPng(
  path.join(SPRITE_DIR, 'vfx_ring.png'),
  64,
  64,
  `<circle cx="32" cy="32" r="28" fill="none" stroke="#ffffff" stroke-width="4"/>`
);

saveSvgAsPng(
  path.join(SPRITE_DIR, 'loot_gold.png'),
  32,
  32,
  `<circle cx="16" cy="16" r="12" fill="url(#brass)" stroke="#fff6bf" stroke-width="2"/>
   <polygon points="16,8 21,16 16,24 11,16" fill="#fff4b8"/>`
);

saveSvgAsPng(
  path.join(SPRITE_DIR, 'loot_starlight.png'),
  32,
  32,
  `<polygon points="16,2 20,12 30,16 20,20 16,30 12,20 2,16 12,12" fill="#8ef9ff" stroke="#ffffff" stroke-width="1.6"/>`
);

saveSvgAsPng(
  path.join(SPRITE_DIR, 'reticle_target.png'),
  64,
  64,
  `<circle cx="32" cy="32" r="24" fill="none" stroke="#ff5964" stroke-width="2.5" stroke-dasharray="8 6"/>
   <path d="M32 2 V12 M32 52 V62 M2 32 H12 M52 32 H62" stroke="#ffe66d" stroke-width="3" stroke-linecap="round"/>`
);

// ============================================================================
// 6. UI CREST, BUTTONS & CUSTOM ICONS
// ============================================================================
saveSvgAsPng(
  path.join(UI_DIR, 'logo_crest.png'),
  160,
  160,
  `
  <circle cx="80" cy="80" r="74" fill="url(#solarGlow)" opacity="0.45"/>
  <circle cx="80" cy="80" r="64" fill="url(#obsidian)" stroke="url(#brass)" stroke-width="4.5"/>
  <circle cx="80" cy="80" r="52" fill="none" stroke="#62ecff" stroke-width="1.8" stroke-dasharray="10 6"/>
  <polygon points="80,14 96,58 142,80 96,102 80,146 64,102 18,80 64,58" fill="url(#brass)" stroke="#fff6c4" stroke-width="2.5"/>
  <polygon points="80,32 94,66 128,80 94,94 80,128 66,94 32,80 66,66" fill="#2cc9eb" stroke="#ffffff" stroke-width="2"/>
  <circle cx="80" cy="80" r="14" fill="#fffbe6"/>
  `
);

function buildButton(name, topC, botC, borderC, glowC) {
  saveSvgAsPng(
    path.join(UI_DIR, `${name}.png`),
    260,
    56,
    `
    <defs>
      <linearGradient id="bg" x1="0" y1="0" x2="0" y2="1">
        <stop offset="0%" stop-color="${topC}"/>
        <stop offset="100%" stop-color="${botC}"/>
      </linearGradient>
    </defs>
    <rect x="3" y="3" width="254" height="50" rx="10" fill="url(#bg)" stroke="${borderC}" stroke-width="2.5"/>
    <rect x="8" y="8" width="244" height="40" rx="7" fill="none" stroke="${glowC}" stroke-opacity="0.4" stroke-width="1.2"/>
    <polygon points="12,28 19,21 26,28 19,35" fill="${borderC}"/>
    <polygon points="248,28 241,21 234,28 241,35" fill="${borderC}"/>
    `
  );
}

buildButton('btn_primary', '#3d2c14', '#1d1408', '#f5c869', '#ffeaa7');
buildButton('btn_primary_hover', '#5c4119', '#2e200b', '#fff0a6', '#ffffff');
buildButton('btn_secondary', '#1d2847', '#0e1529', '#6e8cc7', '#9ec5ff');
buildButton('btn_secondary_hover', '#2b3c6b', '#162242', '#a8ccff', '#ffffff');

const UI_ICONS = {
  icon_gold: `<circle cx="20" cy="20" r="15" fill="url(#brass)" stroke="#fff6bf" stroke-width="2.2"/><polygon points="20,9 26,20 20,31 14,20" fill="#fff7cc"/>`,
  icon_starlight: `<polygon points="20,3 25,15 37,20 25,25 20,37 15,25 3,20 15,15" fill="#6ef2ff" stroke="#ffffff" stroke-width="1.8"/>`,
  icon_heart: `<path d="M20 34 L7 21 A8 8 0 0 1 20 11 A8 8 0 0 1 33 21 Z" fill="#ff4d6d" stroke="#ffd6e0" stroke-width="2"/>`,
  icon_shield: `<path d="M20 4 L34 10 V21 C34 29 27 34 20 37 C13 34 6 29 6 21 V10 Z" fill="#3a86ff" stroke="#caf0f8" stroke-width="2"/>`,
  icon_sword: `<path d="M8 32 L29 11 M23 9 H31 V17 M6 26 L14 34" stroke="#ffd166" stroke-width="3.2" stroke-linecap="round"/>`,
  icon_reroll: `<path d="M10 18 A11 11 0 0 1 30 14 M30 22 A11 11 0 0 1 10 26" fill="none" stroke="#7ef9ff" stroke-width="3" stroke-linecap="round"/><polygon points="32,16 32,8 24,13" fill="#7ef9ff"/><polygon points="8,24 8,32 16,27" fill="#7ef9ff"/>`,
  icon_lock: `<rect x="10" y="18" width="20" height="16" rx="3" fill="url(#brass)" stroke="#fff4bd" stroke-width="2"/><path d="M14 18 V13 A6 6 0 0 1 26 13 V18" fill="none" stroke="#fff4bd" stroke-width="2.5"/>`,
  icon_unlock: `<rect x="10" y="18" width="20" height="16" rx="3" fill="#2c3e66" stroke="#9ec5ff" stroke-width="2"/><path d="M14 18 V12 A6 6 0 0 1 26 12" fill="none" stroke="#9ec5ff" stroke-width="2.5"/>`,
  icon_sell: `<circle cx="20" cy="20" r="15" fill="#4f1d2c" stroke="#ff758f" stroke-width="2.2"/><path d="M20 10 V30 M15 15 H23 A3 3 0 0 1 23 20 H17 A3 3 0 0 0 17 25 H25" fill="none" stroke="#ffd166" stroke-width="2.4" stroke-linecap="round"/>`,
  icon_rotate: `<path d="M20 7 A13 13 0 1 1 8 23" fill="none" stroke="#ffd166" stroke-width="3" stroke-linecap="round"/><polygon points="20,2 20,12 28,7" fill="#ffd166"/>`,
  icon_sound_on: `<polygon points="7,15 15,15 23,8 23,32 15,25 7,25" fill="#ffd166"/><path d="M27 14 A8 8 0 0 1 27 26 M31 10 A13 13 0 0 1 31 30" fill="none" stroke="#7ef9ff" stroke-width="2.5" stroke-linecap="round"/>`,
  icon_sound_off: `<polygon points="7,15 15,15 23,8 23,32 15,25 7,25" fill="#7a859e"/><path d="M27 15 L35 25 M35 15 L27 25" stroke="#ff5964" stroke-width="2.8" stroke-linecap="round"/>`,
  icon_pause: `<rect x="11" y="9" width="6" height="22" rx="2" fill="#ffd166"/><rect x="23" y="9" width="6" height="22" rx="2" fill="#ffd166"/>`,
  icon_trophy: `<path d="M12 8 H28 V17 A8 8 0 0 1 12 17 Z" fill="url(#brass)" stroke="#fff4bd" stroke-width="2"/><path d="M20 25 V32 M13 33 H27" stroke="#ffd166" stroke-width="3" stroke-linecap="round"/>`,
  icon_codex: `<rect x="8" y="6" width="24" height="28" rx="3" fill="#25365e" stroke="url(#brass)" stroke-width="2.4"/><line x1="14" y1="14" x2="26" y2="14" stroke="#7ef9ff" stroke-width="2"/><line x1="14" y1="20" x2="26" y2="20" stroke="#7ef9ff" stroke-width="2"/><line x1="14" y1="26" x2="22" y2="26" stroke="#ffd166" stroke-width="2"/>`,
  icon_tree: `<circle cx="20" cy="9" r="5" fill="#6ef2ff"/><circle cx="10" cy="28" r="5" fill="#ffd166"/><circle cx="30" cy="28" r="5" fill="#df80ff"/><path d="M20 14 V21 L12 25 M20 21 L28 25" fill="none" stroke="#ffffff" stroke-width="2.2"/>`,
  icon_settings: `<circle cx="20" cy="20" r="11" fill="none" stroke="url(#brass)" stroke-width="4" stroke-dasharray="5 3"/><circle cx="20" cy="20" r="5" fill="#7ef9ff"/>`,
  icon_weather_clear: `<circle cx="20" cy="20" r="9" fill="#ffd166"/><circle cx="20" cy="20" r="14" fill="none" stroke="#7ef9ff" stroke-width="2" stroke-dasharray="4 4"/>`,
  icon_weather_solar: `<circle cx="20" cy="20" r="10" fill="#ff6b00" stroke="#ffe66d" stroke-width="2.5"/><path d="M20 3 V8 M20 32 V37 M3 20 H8 M32 20 H37" stroke="#ffd166" stroke-width="2.5" stroke-linecap="round"/>`,
  icon_weather_storm: `<polygon points="22,4 10,22 20,22 16,36 30,18 20,18" fill="#5ce1e6" stroke="#ffffff" stroke-width="1.5"/>`,
  icon_weather_cryo: `<path d="M20 5 V35 M7 12 L33 28 M7 28 L33 12" stroke="#9efcff" stroke-width="2.6" stroke-linecap="round"/>`,
  icon_weather_eclipse: `<circle cx="20" cy="20" r="13" fill="#240046" stroke="#ff4d6d" stroke-width="3"/><path d="M24 9 A11 11 0 0 0 24 31 A12 12 0 1 1 24 9 Z" fill="#ff4d6d"/>`,
  ability_surge: `<circle cx="20" cy="20" r="16" fill="#2c1d0d" stroke="#ffd166" stroke-width="2.2"/><polygon points="20,6 25,16 35,20 25,24 20,34 15,24 5,20 15,16" fill="#ffd166"/>`,
  ability_nova: `<circle cx="20" cy="20" r="16" fill="#2d1220" stroke="#ff6b6b" stroke-width="2.2"/><circle cx="20" cy="20" r="10" fill="none" stroke="#ffd166" stroke-width="2.5"/><circle cx="20" cy="20" r="4" fill="#ffffff"/>`,
  ability_ult_lyra: `<circle cx="20" cy="20" r="16" fill="#3b150b" stroke="#ff7b29" stroke-width="2.2"/><path d="M26 8 L14 24 H22 L14 34 L30 18 H22 Z" fill="#ffd166"/>`,
  ability_ult_vex: `<circle cx="20" cy="20" r="16" fill="#0d253b" stroke="#5ce1e6" stroke-width="2.2"/><circle cx="20" cy="20" r="11" fill="none" stroke="#9efcff" stroke-width="2"/><path d="M20 12 V20 L26 24" fill="none" stroke="#ffffff" stroke-width="2.5" stroke-linecap="round"/>`,
  ability_ult_kael: `<circle cx="20" cy="20" r="16" fill="#1b1440" stroke="#9b72ff" stroke-width="2.2"/><path d="M12 10 L20 20 L28 10 M20 20 V34" fill="none" stroke="#7ef9ff" stroke-width="3" stroke-linecap="round"/>`,
  ability_ult_nyx: `<circle cx="20" cy="20" r="16" fill="#2a0b38" stroke="#e066ff" stroke-width="2.2"/><circle cx="20" cy="20" r="8" fill="#10002b" stroke="#70ffba" stroke-width="2.5"/>`,
};

for (const [name, svg] of Object.entries(UI_ICONS)) {
  saveSvgAsPng(path.join(UI_DIR, `${name}.png`), 40, 40, svg);
}

console.log(
  `Generated ${fs.readdirSync(SPRITE_DIR).length} sprite assets and ${fs.readdirSync(UI_DIR).length} UI assets.`
);
