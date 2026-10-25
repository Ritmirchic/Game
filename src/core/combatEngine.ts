import { ENEMIES, GAME_MODES, MODULE_LIST, MODULES, RELIC_LIST, RELICS, WARDENS } from './content';
import { makeUid } from './circuitEngine';
import {
  ElementType,
  EnemyId,
  EnemyInstance,
  GameModeId,
  RelicId,
  ShopOffer,
  TurretFireCommand,
  WardenId,
  WeatherId,
} from './types';

export interface Point2D {
  x: number;
  y: number;
}

export const CORE_POS: Point2D = { x: 642, y: 355 };
export const RIFT_POS: Point2D = { x: 1142, y: 358 };

export const LANE_WAYPOINTS: Record<0 | 1 | 2, Point2D[]> = {
  0: [
    { x: 1142, y: 338 },
    { x: 1052, y: 186 },
    { x: 920, y: 168 },
    { x: 785, y: 196 },
    { x: 694, y: 256 },
    { x: 642, y: 350 },
  ],
  1: [
    { x: 1130, y: 358 },
    { x: 985, y: 352 },
    { x: 825, y: 356 },
    { x: 642, y: 355 },
  ],
  2: [
    { x: 1142, y: 378 },
    { x: 1052, y: 528 },
    { x: 920, y: 544 },
    { x: 785, y: 516 },
    { x: 694, y: 454 },
    { x: 642, y: 360 },
  ],
};

export function sampleLanePosition(lane: 0 | 1 | 2, progress: number): Point2D {
  const pts = LANE_WAYPOINTS[lane];
  const clamped = Math.max(0, Math.min(1, progress));
  // Compute cumulative lengths
  const segLengths: number[] = [];
  let total = 0;
  for (let i = 0; i < pts.length - 1; i++) {
    const dx = pts[i + 1].x - pts[i].x;
    const dy = pts[i + 1].y - pts[i].y;
    const len = Math.hypot(dx, dy);
    segLengths.push(len);
    total += len;
  }
  const targetDist = clamped * total;
  let acc = 0;
  for (let i = 0; i < segLengths.length; i++) {
    const seg = segLengths[i];
    if (acc + seg >= targetDist || i === segLengths.length - 1) {
      const t = seg > 0 ? (targetDist - acc) / seg : 0;
      return {
        x: pts[i].x + (pts[i + 1].x - pts[i].x) * t,
        y: pts[i].y + (pts[i + 1].y - pts[i].y) * t,
      };
    }
    acc += seg;
  }
  return pts[pts.length - 1];
}

export interface SpawnScheduleItem {
  delay: number;
  enemyId: EnemyId;
  lane: 0 | 1 | 2;
}

export interface VisualHitEvent {
  kind: 'projectile' | 'beam' | 'chain' | 'aoe' | 'heal' | 'shatter' | 'meteor';
  fromX: number;
  fromY: number;
  toX: number;
  toY: number;
  color: number;
  damage: number;
  isCrit: boolean;
  spriteKey?: string;
  radius?: number;
}

export interface LootDropEvent {
  x: number;
  y: number;
  gold: number;
  starlight: number;
}

export interface CombatTickEvents {
  visuals: VisualHitEvent[];
  lootDrops: LootDropEvent[];
  slainIds: EnemyId[];
  coreHitDamage: number;
  phoenixTriggered: boolean;
  highestHit: number;
  relicsTriggered: RelicId[];
}

export class CombatEngine {
  public enemies: EnemyInstance[] = [];
  public spawnQueue: SpawnScheduleItem[] = [];
  public waveElapsed = 0;
  public priorityEnemyUid: string | null = null;
  public coreHp = 100;
  public maxCoreHp = 100;
  public coreShield = 0;
  public phoenixUsed = false;
  public killsCounterForVamp = 0;

  public buildWaveSchedule(wave: number, modeId: GameModeId): SpawnScheduleItem[] {
    const schedule: SpawnScheduleItem[] = [];
    const swarmMult = modeId === 'challenge_swarm' ? 1.6 : 1.0;
    const baseCount = Math.round((7 + wave * 2.6) * swarmMult);
    const interval = Math.max(0.28, (1.05 - wave * 0.032) / swarmMult);

    const pool: EnemyId[] = ['skitterling'];
    if (wave >= 2) pool.push('sprinter');
    if (wave >= 3) pool.push('bulwark');
    if (wave >= 4) pool.push('wraith');
    if (wave >= 6) pool.push('brood', 'acolyte');
    if (wave >= 8) pool.push('sentinel');

    for (let i = 0; i < baseCount; i++) {
      const lane = (i % 3) as 0 | 1 | 2;
      const pick = pool[(i * 7 + wave * 3) % pool.length];
      schedule.push({
        delay: 0.4 + i * interval,
        enemyId: pick,
        lane,
      });
    }

    // Boss every 5th wave!
    if (wave % 5 === 0) {
      let bossId: EnemyId = 'boss_colossus';
      if (wave === 10 || (wave > 15 && wave % 15 === 10)) bossId = 'boss_weaver';
      else if (wave >= 15 && wave % 15 === 0) bossId = 'boss_archon';
      schedule.push({
        delay: Math.min(6.5, 2.5 + baseCount * interval * 0.35),
        enemyId: bossId,
        lane: 1,
      });
    }

    return schedule.sort((a, b) => a.delay - b.delay);
  }

  public spawnEnemy(
    defId: EnemyId,
    lane: 0 | 1 | 2,
    wave: number,
    modeId: GameModeId,
    initialProgress = 0
  ): EnemyInstance {
    const def = ENEMIES[defId];
    // Smooth exponential scaling with wave
    const hpWaveScale =
      wave <= 15
        ? 1 + (wave - 1) * 0.24 + Math.pow(Math.max(0, wave - 4), 1.45) * 0.08
        : (1 + 14 * 0.24 + Math.pow(11, 1.45) * 0.08) * Math.pow(1.22, wave - 15);
    const modeHpMult =
      modeId === 'challenge_swarm' ? 0.8 : modeId === 'challenge_mirror' ? 1.25 : 1.0;

    const maxHp = Math.round(def.baseHp * hpWaveScale * modeHpMult);
    const maxShield = Math.round(def.shield * hpWaveScale);
    const pos = sampleLanePosition(lane, initialProgress);

    const inst: EnemyInstance = {
      uid: makeUid('en'),
      defId,
      isBoss: def.isBoss,
      lane,
      progress: initialProgress,
      x: pos.x,
      y: pos.y,
      maxHp,
      hp: maxHp,
      maxShield,
      shield: maxShield,
      armor: def.armor + Math.floor((wave - 1) / 4),
      baseSpeed: def.speed,
      goldDrop: def.goldDrop,
      coreDamage: def.coreDamage,
      burnDps: 0,
      burnTimer: 0,
      chillStacks: 0,
      chillTimer: 0,
      freezeTimer: 0,
      shockTimer: 0,
      venomStacks: 0,
      venomTimer: 0,
      phaseTimer: 3.2,
      isPhased: false,
      specialCooldown: 4.5,
      enraged: false,
    };
    this.enemies.push(inst);
    return inst;
  }

  public startWave(wave: number, modeId: GameModeId, waveStartShield: number): void {
    this.waveElapsed = 0;
    this.coreShield = Math.max(this.coreShield, waveStartShield);
    this.spawnQueue = this.buildWaveSchedule(wave, modeId);
  }

  public isWaveComplete(): boolean {
    return this.spawnQueue.length === 0 && this.enemies.length === 0;
  }

  public selectTarget(preferHighestHp = false): EnemyInstance | null {
    const living = this.enemies.filter((e) => e.hp > 0);
    if (living.length === 0) return null;
    if (this.priorityEnemyUid) {
      const prio = living.find((e) => e.uid === this.priorityEnemyUid);
      if (prio) return prio;
    }
    if (preferHighestHp) {
      return living.reduce((best, cur) => (cur.hp + cur.shield > best.hp + best.shield ? cur : best));
    }
    // Closest to Sunspire Core (highest progress)
    return living.reduce((best, cur) => (cur.progress > best.progress ? cur : best));
  }

  public applyDamage(
    enemy: EnemyInstance,
    rawDamage: number,
    elements: ElementType[],
    isCrit: boolean,
    context: {
      wardenId: WardenId;
      weatherId: WeatherId;
      relics: RelicId[];
    },
    events: CombatTickEvents
  ): number {
    if (enemy.hp <= 0) return 0;

    let dmg = rawDamage;

    // Elemental weather & warden amplification
    if (elements.includes('pyro')) {
      if (context.weatherId === 'solar_flare') dmg *= 1.4;
      if (context.wardenId === 'lyra') dmg *= 1.15;
    }

    // Priority target focus mark
    if (enemy.uid === this.priorityEnemyUid) {
      dmg *= 1.2;
    }

    // Phase Wraith dodge reduction unless chilled/frozen
    if (enemy.isPhased && enemy.chillStacks === 0 && enemy.freezeTimer <= 0) {
      dmg *= 0.5;
    }

    // Frozen bonus
    if (enemy.freezeTimer > 0 && context.relics.includes('relic_shatter_core')) {
      dmg *= 1.5;
      if (!events.relicsTriggered.includes('relic_shatter_core')) {
        events.relicsTriggered.push('relic_shatter_core');
      }
    }

    // Executioner Crest
    if (
      context.relics.includes('relic_exec_sigil') &&
      (enemy.isBoss || enemy.hp / enemy.maxHp <= 0.35)
    ) {
      dmg *= 1.4;
      if (!events.relicsTriggered.includes('relic_exec_sigil')) {
        events.relicsTriggered.push('relic_exec_sigil');
      }
    }

    if (isCrit) {
      const critMult = context.relics.includes('relic_monocle') ? 2.25 : 1.85;
      dmg *= critMult;
    }

    // Apply Elemental Statuses
    const statusDurBonus = context.relics.includes('relic_alembic') ? 2.0 : 0;
    const dotMult = context.relics.includes('relic_alembic') ? 1.45 : 1.0;

    if (elements.includes('venom')) {
      enemy.venomStacks = Math.min(12, enemy.venomStacks + 1);
      enemy.venomTimer = 5.0 + statusDurBonus;
    }
    if (elements.includes('pyro')) {
      const burnAdd = dmg * 0.22 * dotMult * (context.wardenId === 'lyra' ? 1.35 : 1.0);
      enemy.burnDps = Math.max(enemy.burnDps, burnAdd);
      enemy.burnTimer = 4.0 + statusDurBonus;
    }
    if (elements.includes('cryo')) {
      enemy.chillStacks += 1;
      enemy.chillTimer = 4.0;
      if (enemy.chillStacks >= 4 && enemy.freezeTimer <= 0) {
        enemy.chillStacks = 0;
        enemy.freezeTimer = enemy.isBoss ? 1.1 : 2.0;
        // Cryo Shatter burst!
        const shatterMult = context.weatherId === 'cryo_nebula' ? 1.5 : 1.0;
        dmg += rawDamage * 0.55 * shatterMult;
        events.visuals.push({
          kind: 'shatter',
          fromX: enemy.x,
          fromY: enemy.y,
          toX: enemy.x,
          toY: enemy.y,
          color: 0x7ef9ff,
          damage: Math.round(rawDamage * 0.55 * shatterMult),
          isCrit: false,
          radius: 46,
        });
      }
    }
    if (elements.includes('volt')) {
      enemy.shockTimer = 3.5;
    }

    // Shield absorbs first (Volt deals x2 to shield!)
    if (enemy.shield > 0) {
      const shieldMult = elements.includes('volt') ? 2.0 : 1.0;
      const effectiveShieldHit = dmg * shieldMult;
      if (effectiveShieldHit <= enemy.shield) {
        enemy.shield -= effectiveShieldHit;
        const dealt = Math.max(1, Math.round(effectiveShieldHit));
        events.highestHit = Math.max(events.highestHit, dealt);
        return dealt;
      } else {
        const leftoverRatio = 1 - enemy.shield / effectiveShieldHit;
        enemy.shield = 0;
        dmg *= leftoverRatio;
      }
    }

    // Armor mitigation reduced by Venom stacks
    const shredPerStack = context.wardenId === 'nyx' ? 4 : 2;
    const effectiveArmor = Math.max(0, enemy.armor - enemy.venomStacks * shredPerStack);
    const finalHpDmg = Math.max(1, Math.round(dmg - effectiveArmor));
    enemy.hp -= finalHpDmg;
    events.highestHit = Math.max(events.highestHit, finalHpDmg);

    return finalHpDmg;
  }

  public executeTurretFire(
    cmd: TurretFireCommand,
    origin: Point2D,
    context: {
      wardenId: WardenId;
      weatherId: WeatherId;
      relics: RelicId[];
      turretDamageScale: number;
    },
    events: CombatTickEvents
  ): number {
    if (this.enemies.length === 0) return 0;

    const def = MODULES[cmd.defId];
    const tIdx = cmd.tier - 1;
    let baseDmg = def.baseDamage[tIdx] * context.turretDamageScale * cmd.powerMult;
    if (cmd.defId === 'turret_beam') {
      // Sunbeam scales an extra +35% from high multipliers
      baseDmg *= 1 + (cmd.powerMult - 1) * 0.35;
    }

    const elems = new Set<ElementType>(cmd.elements);
    if (def.element) elems.add(def.element);
    const elemArr = Array.from(elems);

    const rollCrit = () => Math.random() < cmd.critChance;
    let totalDealt = 0;

    if (cmd.defId === 'turret_blaze') {
      const target = this.selectTarget(false);
      if (!target) return 0;
      const blastRadius = (cmd.tier === 1 ? 68 : cmd.tier === 2 ? 84 : 104) * (context.wardenId === 'lyra' ? 1.2 : 1.0);
      const isCrit = rollCrit();
      events.visuals.push({
        kind: 'projectile',
        fromX: origin.x,
        fromY: origin.y,
        toX: target.x,
        toY: target.y,
        color: 0xff6e27,
        damage: 0,
        isCrit,
        spriteKey: 'proj_blaze',
        radius: blastRadius,
      });
      for (const en of this.enemies) {
        const dist = Math.hypot(en.x - target.x, en.y - target.y);
        if (dist <= blastRadius) {
          const falloff = dist < 16 ? 1.0 : 0.72;
          const d = this.applyDamage(en, baseDmg * falloff, elemArr, isCrit, context, events);
          totalDealt += d;
          events.visuals.push({
            kind: 'aoe',
            fromX: target.x,
            fromY: target.y,
            toX: en.x,
            toY: en.y,
            color: 0xff8c42,
            damage: d,
            isCrit,
            radius: blastRadius,
          });
        }
      }
    } else if (cmd.defId === 'turret_arc') {
      const primary = this.selectTarget(false);
      if (!primary) return 0;
      let maxJumps =
        (cmd.tier === 1 ? 3 : cmd.tier === 2 ? 4 : 6) +
        (context.wardenId === 'kael' ? 1 : 0) +
        (context.relics.includes('relic_storm_coil') ? 2 : 0);

      const hitSet = new Set<string>();
      let cur: EnemyInstance | null = primary;
      let prevPos = origin;

      while (cur && maxJumps > 0) {
        hitSet.add(cur.uid);
        const isCrit = rollCrit();
        const d = this.applyDamage(cur, baseDmg, elemArr, isCrit, context, events);
        totalDealt += d;
        events.visuals.push({
          kind: 'chain',
          fromX: prevPos.x,
          fromY: prevPos.y,
          toX: cur.x,
          toY: cur.y,
          color: 0x6ef2ff,
          damage: d,
          isCrit,
        });
        prevPos = { x: cur.x, y: cur.y };
        baseDmg *= 0.86;
        maxJumps--;

        // Find nearest unhit enemy within 180px
        let nearest: EnemyInstance | null = null;
        let bestDist = 185;
        for (const cand of this.enemies) {
          if (cand.hp <= 0 || hitSet.has(cand.uid)) continue;
          const dist = Math.hypot(cand.x - prevPos.x, cand.y - prevPos.y);
          if (dist < bestDist) {
            bestDist = dist;
            nearest = cand;
          }
        }
        cur = nearest;
      }
    } else if (cmd.defId === 'turret_frost') {
      const primary = this.selectTarget(false);
      if (!primary) return 0;
      const pierceCap = cmd.tier === 1 ? 3 : cmd.tier === 2 ? 4 : 6;
      const sorted = [...this.enemies]
        .filter((e) => e.hp > 0 && Math.abs(e.y - primary.y) < 58)
        .sort((a, b) => b.progress - a.progress)
        .slice(0, pierceCap);
      const targets = sorted.length > 0 ? sorted : [primary];

      for (const t of targets) {
        const isCrit = rollCrit();
        const d = this.applyDamage(t, baseDmg, elemArr, isCrit, context, events);
        totalDealt += d;
        events.visuals.push({
          kind: 'projectile',
          fromX: origin.x,
          fromY: origin.y,
          toX: t.x,
          toY: t.y,
          color: 0x80ffea,
          damage: d,
          isCrit,
          spriteKey: 'proj_frost',
        });
      }
    } else if (cmd.defId === 'turret_venom') {
      const darts = cmd.tier === 1 ? 3 : cmd.tier === 2 ? 4 : 6;
      const living = this.enemies.filter((e) => e.hp > 0);
      if (living.length === 0) return 0;
      for (let i = 0; i < darts; i++) {
        const t = living[i % living.length];
        const isCrit = rollCrit();
        const d = this.applyDamage(t, baseDmg, elemArr, isCrit, context, events);
        totalDealt += d;
        events.visuals.push({
          kind: 'projectile',
          fromX: origin.x,
          fromY: origin.y,
          toX: t.x,
          toY: t.y,
          color: 0x69ff7a,
          damage: d,
          isCrit,
          spriteKey: 'proj_venom',
        });
      }
    } else if (cmd.defId === 'turret_beam') {
      const bossOrHeavy = this.selectTarget(true);
      if (!bossOrHeavy) return 0;
      const isCrit = rollCrit();
      const d = this.applyDamage(bossOrHeavy, baseDmg, elemArr, isCrit, context, events);
      totalDealt += d;
      events.visuals.push({
        kind: 'beam',
        fromX: origin.x,
        fromY: origin.y,
        toX: bossOrHeavy.x,
        toY: bossOrHeavy.y,
        color: 0xffe066,
        damage: d,
        isCrit,
      });
    } else if (cmd.defId === 'turret_mortar') {
      const target = this.selectTarget(false);
      if (!target) return 0;
      const pullRadius = cmd.tier === 1 ? 85 : cmd.tier === 2 ? 105 : 130;
      const isCrit = rollCrit();
      events.visuals.push({
        kind: 'projectile',
        fromX: origin.x,
        fromY: origin.y,
        toX: target.x,
        toY: target.y,
        color: 0xc77dff,
        damage: 0,
        isCrit,
        spriteKey: 'proj_mortar',
        radius: pullRadius,
      });
      for (const en of this.enemies) {
        const dist = Math.hypot(en.x - target.x, en.y - target.y);
        if (dist <= pullRadius) {
          if (!en.isBoss) {
            en.progress = Math.max(0, en.progress - 0.035);
          }
          const d = this.applyDamage(en, baseDmg * 0.9, elemArr, isCrit, context, events);
          totalDealt += d;
          events.visuals.push({
            kind: 'aoe',
            fromX: target.x,
            fromY: target.y,
            toX: en.x,
            toY: en.y,
            color: 0xd982ff,
            damage: d,
            isCrit,
            radius: pullRadius,
          });
        }
      }
    }

    // Bonus Volt chain if infused on a non-arc turret!
    if (cmd.defId !== 'turret_arc' && elems.has('volt') && this.enemies.length > 1) {
      const extraJumps = 1 + (context.relics.includes('relic_storm_coil') ? 2 : 0);
      const living = this.enemies.filter((e) => e.hp > 0).slice(0, extraJumps);
      for (const e of living) {
        const arcDmg = this.applyDamage(e, baseDmg * 0.35, ['volt'], false, context, events);
        totalDealt += arcDmg;
        events.visuals.push({
          kind: 'chain',
          fromX: CORE_POS.x,
          fromY: CORE_POS.y,
          toX: e.x,
          toY: e.y,
          color: 0x7ef9ff,
          damage: arcDmg,
          isCrit: false,
        });
      }
    }

    return totalDealt;
  }

  public stepCombat(
    dt: number,
    wave: number,
    modeId: GameModeId,
    context: {
      wardenId: WardenId;
      weatherId: WeatherId;
      relics: RelicId[];
      bountyScale: number;
    }
  ): CombatTickEvents {
    const events: CombatTickEvents = {
      visuals: [],
      lootDrops: [],
      slainIds: [],
      coreHitDamage: 0,
      phoenixTriggered: false,
      highestHit: 0,
      relicsTriggered: [],
    };

    this.waveElapsed += dt;

    // 1. Spawn scheduled enemies
    while (this.spawnQueue.length > 0 && this.spawnQueue[0].delay <= this.waveElapsed) {
      const item = this.spawnQueue.shift()!;
      this.spawnEnemy(item.enemyId, item.lane, wave, modeId);
    }

    // 2. Update living enemies
    const newlySpawnedChildren: Array<{ id: EnemyId; lane: 0 | 1 | 2; prog: number }> = [];

    for (const en of this.enemies) {
      if (en.hp <= 0) continue;

      // Tick DoTs
      if (en.burnTimer > 0) {
        en.burnTimer -= dt;
        en.hp -= en.burnDps * dt;
      }
      if (en.venomTimer > 0) {
        en.venomTimer -= dt;
        const venomDps = en.venomStacks * 6.5 * (context.relics.includes('relic_alembic') ? 1.45 : 1.0);
        en.hp -= venomDps * dt;
      } else {
        en.venomStacks = 0;
      }

      if (en.chillTimer > 0) {
        en.chillTimer -= dt;
      } else {
        en.chillStacks = 0;
      }

      if (en.shockTimer > 0) en.shockTimer -= dt;

      // Special Enemy / Boss AI
      if (en.defId === 'wraith') {
        en.phaseTimer -= dt;
        if (en.phaseTimer <= 0) {
          en.isPhased = !en.isPhased;
          en.phaseTimer = en.isPhased ? 1.1 : 3.2;
        }
      } else if (en.defId === 'acolyte') {
        en.specialCooldown -= dt;
        if (en.specialCooldown <= 0) {
          en.specialCooldown = 4.2;
          for (const ally of this.enemies) {
            if (ally.hp > 0 && Math.hypot(ally.x - en.x, ally.y - en.y) <= 130) {
              const heal = Math.round(ally.maxHp * 0.08);
              ally.hp = Math.min(ally.maxHp, ally.hp + heal);
            }
          }
          events.visuals.push({
            kind: 'heal',
            fromX: en.x,
            fromY: en.y,
            toX: en.x,
            toY: en.y,
            color: 0x59ffc4,
            damage: 0,
            isCrit: false,
            radius: 110,
          });
        }
      } else if (en.defId === 'boss_colossus') {
        en.specialCooldown -= dt;
        if (en.specialCooldown <= 0) {
          en.specialCooldown = 6.5;
          newlySpawnedChildren.push(
            { id: 'skitterling', lane: 0, prog: Math.max(0, en.progress - 0.06) },
            { id: 'skitterling', lane: 2, prog: Math.max(0, en.progress - 0.06) }
          );
        }
      } else if (en.defId === 'boss_weaver') {
        en.specialCooldown -= dt;
        if (en.specialCooldown <= 0) {
          en.specialCooldown = 5.5;
          en.lane = (((en.lane + 1) % 3) as 0 | 1 | 2);
          en.shield = Math.min(en.maxShield, en.shield + Math.round(en.maxShield * 0.2));
        }
      } else if (en.defId === 'boss_archon') {
        if (!en.enraged && en.hp <= en.maxHp * 0.5) {
          en.enraged = true;
          en.shield = Math.round(en.maxShield * 0.5);
          newlySpawnedChildren.push(
            { id: 'sentinel', lane: 0, prog: 0.15 },
            { id: 'sentinel', lane: 2, prog: 0.15 }
          );
        }
      }

      // Movement
      if (en.freezeTimer > 0) {
        en.freezeTimer -= dt;
      } else {
        let spd = en.baseSpeed;
        if (en.defId === 'sprinter' && en.hp <= en.maxHp * 0.5) spd *= 1.45;
        if (en.enraged) spd *= 1.25;
        if (en.chillStacks > 0) spd *= Math.max(0.45, 1 - en.chillStacks * 0.14);
        if (context.weatherId === 'cryo_nebula') spd *= 0.85;
        if (context.weatherId === 'blood_eclipse') spd *= 1.2;

        en.progress += spd * dt;
        const pos = sampleLanePosition(en.lane, en.progress);
        en.x = pos.x;
        en.y = pos.y;
      }
    }

    // 3. Process deaths & Core hits
    const survivors: EnemyInstance[] = [];
    for (const en of this.enemies) {
      if (en.hp <= 0) {
        events.slainIds.push(en.defId);
        const weatherGoldMult = context.weatherId === 'blood_eclipse' ? 1.45 : 1.0;
        const gold = Math.max(1, Math.round(en.goldDrop * context.bountyScale * weatherGoldMult));
        const starlight = en.isBoss
          ? Math.round((8 + wave * 0.8) * GAME_MODES[modeId].starlightMult)
          : 0;
        events.lootDrops.push({
          x: en.x,
          y: en.y,
          gold,
          starlight,
        });

        // Brood split
        if (en.defId === 'brood') {
          for (let m = 0; m < 3; m++) {
            newlySpawnedChildren.push({
              id: 'mite',
              lane: en.lane,
              prog: Math.max(0, en.progress - m * 0.03),
            });
          }
        }

        // Glacial Shattercore death shard AoE
        if (en.freezeTimer > 0 && context.relics.includes('relic_shatter_core')) {
          events.visuals.push({
            kind: 'shatter',
            fromX: en.x,
            fromY: en.y,
            toX: en.x,
            toY: en.y,
            color: 0x9efcff,
            damage: 45,
            isCrit: false,
            radius: 75,
          });
          for (const near of this.enemies) {
            if (near.hp > 0 && Math.hypot(near.x - en.x, near.y - en.y) <= 75) {
              near.hp -= 45;
            }
          }
        }

        // Sanguine Conduit heal
        if (context.relics.includes('relic_vampiric')) {
          this.killsCounterForVamp++;
          if (this.killsCounterForVamp >= 18) {
            this.killsCounterForVamp = 0;
            if (this.coreHp < this.maxCoreHp) {
              this.coreHp = Math.min(this.maxCoreHp, this.coreHp + 2);
            } else {
              this.coreShield += 2;
            }
            if (!events.relicsTriggered.includes('relic_vampiric')) {
              events.relicsTriggered.push('relic_vampiric');
            }
          }
        }
        continue;
      }

      if (en.progress >= 1.0) {
        // Reached Sunspire Core!
        let incoming = en.coreDamage;
        if (this.coreShield > 0) {
          const absorbed = Math.min(this.coreShield, incoming);
          this.coreShield -= absorbed;
          incoming -= absorbed;
        }
        if (incoming > 0) {
          this.coreHp -= incoming;
          events.coreHitDamage += incoming;
        }

        // Phoenix Feather check
        if (this.coreHp <= 0 && !this.phoenixUsed && context.relics.includes('relic_phoenix')) {
          this.phoenixUsed = true;
          this.coreHp = Math.max(10, Math.round(this.maxCoreHp * 0.5));
          events.phoenixTriggered = true;
          events.relicsTriggered.push('relic_phoenix');
          for (const other of this.enemies) {
            if (!other.isBoss) other.hp = 0;
          }
        }
        continue;
      }

      survivors.push(en);
    }

    this.enemies = survivors;

    for (const child of newlySpawnedChildren) {
      this.spawnEnemy(child.id, child.lane, wave, modeId, child.prog);
    }

    if (this.coreHp < 0) this.coreHp = 0;
    return events;
  }
}

export function rollShopOffers(
  wave: number,
  ownedRelics: RelicId[],
  maxRelics: number,
  rarityBonusChance: number,
  previousOffers?: ShopOffer[]
): ShopOffer[] {
  const offers: ShopOffer[] = [];
  const availableRelics = RELIC_LIST.filter((r) => !ownedRelics.includes(r.id));

  for (let i = 0; i < 4; i++) {
    const prev = previousOffers?.[i];
    if (prev && prev.locked && !prev.purchased) {
      offers.push({ ...prev, slotIndex: i });
      continue;
    }

    // Slot 3 offers a Relic if relic capacity allows
    const offerRelic = i === 3 && ownedRelics.length < maxRelics && availableRelics.length > 0;
    if (offerRelic) {
      const roll = Math.random() + rarityBonusChance;
      const targetRarity = roll > 0.86 ? 'legendary' : roll > 0.5 ? 'rare' : 'common';
      const candidates = availableRelics.filter((r) => r.rarity === targetRarity);
      const pool = candidates.length > 0 ? candidates : availableRelics;
      const picked = pool[Math.floor(Math.random() * pool.length)];
      const idx = availableRelics.indexOf(picked);
      if (idx >= 0) availableRelics.splice(idx, 1);

      offers.push({
        slotIndex: i,
        kind: 'relic',
        relicId: picked.id,
        cost: RELICS[picked.id].cost,
        locked: false,
        purchased: false,
      });
    } else {
      const roll = Math.random() + rarityBonusChance + wave * 0.015;
      const wantRare = roll > 0.68;
      const pool = MODULE_LIST.filter((m) => (wantRare ? m.rarity === 'rare' : m.rarity === 'common'));
      const chosen = (pool.length > 0 ? pool : MODULE_LIST)[
        Math.floor(Math.random() * (pool.length > 0 ? pool.length : MODULE_LIST.length))
      ];
      offers.push({
        slotIndex: i,
        kind: 'module',
        moduleId: chosen.id,
        cost: chosen.cost,
        locked: false,
        purchased: false,
      });
    }
  }

  return offers;
}

export function getRerollCost(rerollCount: number, wardenId: WardenId, freeFirstReroll: boolean): number {
  if (rerollCount === 0 && freeFirstReroll) return 0;
  const raw = 6 + rerollCount * 4;
  return wardenId === 'nyx' ? Math.max(2, Math.round(raw * WARDENS.nyx.emitterIntervalScale * 0.6)) : raw;
}
