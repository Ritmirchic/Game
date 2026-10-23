import * as THREE from 'three';
import { Environment } from './Environment';
import { Soundscape } from './Soundscape';
import {
  createBarricade,
  createBeacon,
  createEnemy,
  createPlayer,
  createResource,
  createTurret,
  type BeaconRig,
  type CharacterRig,
  type EnemyRig,
  type TurretRig,
} from './models';
import type {
  EndStats,
  GameCallbacks,
  GameState,
  Inventory,
  Phase,
  RecipeId,
  ResourceType,
} from './types';

interface ResourceNode {
  type: ResourceType;
  root: THREE.Group;
  amount: number;
  maxAmount: number;
  baseScale: number;
  currentScale: number;
  hit: number;
}

interface Enemy {
  kind: 'walker' | 'runner' | 'brute';
  rig: EnemyRig;
  health: number;
  maxHealth: number;
  speed: number;
  damage: number;
  attackCooldown: number;
  walkTime: number;
  aggro: number;
}

interface Building {
  kind: 'barricade' | 'turret';
  root: THREE.Group;
  health: number;
  cooldown: number;
  turret?: TurretRig;
}

interface Effect {
  object: THREE.Object3D;
  life: number;
  maxLife: number;
  velocity?: THREE.Vector3;
  gravity?: number;
  grow?: boolean;
  disposeGeometry?: boolean;
}

const RECIPES: Record<RecipeId, Inventory> = {
  barricade: { wood: 5, scrap: 2, crystal: 0 },
  turret: { wood: 0, scrap: 7, crystal: 2 },
  repair: { wood: 3, scrap: 3, crystal: 0 },
  medkit: { wood: 0, scrap: 2, crystal: 1 },
};
const CORE_COST: Inventory = { wood: 0, scrap: 6, crystal: 4 };
const RESOURCE_NAMES: Record<ResourceType, string> = { wood: 'древесины', scrap: 'металла', crystal: 'осколка' };

export class Game {
  readonly sound = new Soundscape();
  private readonly renderer: THREE.WebGLRenderer;
  private readonly scene = new THREE.Scene();
  private readonly camera = new THREE.PerspectiveCamera(54, 1, 0.1, 180);
  private readonly environment: Environment;
  private readonly player: CharacterRig;
  private readonly beacon: BeaconRig;
  private readonly resources: ResourceNode[] = [];
  private readonly enemies: Enemy[] = [];
  private readonly buildings: Building[] = [];
  private readonly effects: Effect[] = [];
  private readonly keys = new Set<string>();
  private readonly particleGeometry = new THREE.BoxGeometry(0.11, 0.11, 0.11);
  private readonly mobileMove = new THREE.Vector2();
  private readonly tempA = new THREE.Vector3();
  private readonly tempB = new THREE.Vector3();

  private state: GameState = 'menu';
  private phase: Phase = 'day';
  private day = 1;
  private phaseTime = 75;
  private phaseDuration = 75;
  private finalCharge: number | null = null;
  private readonly finalChargeDuration = 40;
  private health = 100;
  private stamina = 100;
  private beaconHealth = 100;
  private cores = 0;
  private inventory: Inventory = { wood: 2, scrap: 1, crystal: 0 };
  private kills = 0;
  private gathered = 0;
  private elapsed = 0;
  private spawnTimer = 0;
  private attackCooldown = 0;
  private attackAnimation = 0;
  private interactCooldown = 0;
  private hurtCooldown = 0;
  private verticalVelocity = 0;
  private grounded = true;
  private walkCycle = 0;
  private cameraYaw = 0;
  private cameraPitch = 0.46;
  private cameraDistance = 11.5;
  private cameraShake = 0;
  private draggingPointer: number | null = null;
  private dragX = 0;
  private dragY = 0;
  private snapshotTimer = 0;
  private worldTime = 0;
  private menuTime = 0;
  private lastFrame = 0;

  constructor(
    private readonly canvas: HTMLCanvasElement,
    private readonly callbacks: GameCallbacks,
  ) {
    this.renderer = new THREE.WebGLRenderer({ canvas, antialias: true, powerPreference: 'high-performance' });
    this.renderer.setPixelRatio(Math.min(window.devicePixelRatio, 1.8));
    this.renderer.shadowMap.enabled = true;
    this.renderer.shadowMap.type = THREE.PCFSoftShadowMap;
    this.renderer.outputColorSpace = THREE.SRGBColorSpace;
    this.renderer.toneMapping = THREE.ACESFilmicToneMapping;
    this.renderer.toneMappingExposure = 1.05;

    this.environment = new Environment(this.scene);
    this.beacon = createBeacon();
    this.scene.add(this.beacon.root);
    this.player = createPlayer();
    this.scene.add(this.player.root);
    this.createResources();
    this.bindInput();
    this.resize();
    window.addEventListener('resize', () => this.resize());
    this.resetGame();
    this.state = 'menu';
    this.renderer.setAnimationLoop((time) => this.frame(time));
  }

  start(): void {
    this.sound.unlock();
    this.resetGame();
    this.state = 'playing';
    this.callbacks.onState('playing');
    this.callbacks.onAnnounce('СМЕНА 01', 'ДОБУДЬТЕ РЕСУРСЫ', 'До заката есть время. Зарядите маяк тремя ячейками.');
    this.emitSnapshot();
  }

  toMenu(): void {
    this.resetGame();
    this.state = 'menu';
    this.callbacks.onState('menu');
  }

  pause(): void {
    if (this.state !== 'playing') return;
    this.state = 'paused';
    this.keys.clear();
    this.mobileMove.set(0, 0);
    this.callbacks.onState('paused');
  }

  resume(): void {
    if (this.state !== 'paused') return;
    this.state = 'playing';
    this.callbacks.onState('playing');
  }

  togglePause(): void {
    if (this.state === 'playing') this.pause();
    else if (this.state === 'paused') this.resume();
  }

  toggleSound(): boolean {
    this.sound.unlock();
    return this.sound.toggle();
  }

  setJoystick(x: number, y: number): void {
    this.mobileMove.set(x, y);
  }

  jump(): void {
    if (this.state === 'playing' && this.grounded) {
      this.verticalVelocity = 6.2;
      this.grounded = false;
    }
  }

  attack(): void {
    if (this.state !== 'playing' || this.attackCooldown > 0) return;
    this.attackCooldown = 0.4;
    this.attackAnimation = 0.3;
    this.sound.play('swing');
    this.spawnSlash();

    const targets = this.enemies
      .filter((enemy) => this.horizontalDistance(enemy.rig.root.position, this.player.root.position) < 3.25)
      .sort((a, b) => this.horizontalDistance(a.rig.root.position, this.player.root.position) - this.horizontalDistance(b.rig.root.position, this.player.root.position))
      .slice(0, 2);
    if (targets.length > 0) {
      const direction = this.tempA.copy(targets[0].rig.root.position).sub(this.player.root.position);
      this.player.root.rotation.y = Math.atan2(direction.x, direction.z);
      let connected = false;
      for (const target of targets) {
        if (this.enemies.includes(target)) {
          this.damageEnemy(target, 26);
          connected = true;
        }
      }
      if (connected) this.sound.play('hit');
    }
  }

  interact(): void {
    if (this.state !== 'playing' || this.interactCooldown > 0) return;
    this.interactCooldown = 0.28;
    if (this.nearBeacon() && this.cores < 3) {
      this.installCore();
      return;
    }
    const node = this.nearestResource();
    if (!node) return;
    const amount = node.type === 'crystal' ? 1 : 2;
    node.amount -= 1;
    node.hit = 1;
    this.inventory[node.type] += amount;
    this.gathered += amount;
    this.sound.play(node.type);
    this.callbacks.onLoot(`+${amount} ${RESOURCE_NAMES[node.type]}`, node.type);
    const colors: Record<ResourceType, number> = { wood: 0xb58b5f, scrap: 0xa7b4ae, crystal: 0x70e4cd };
    this.spawnParticles(node.root.position.clone().add(new THREE.Vector3(0, 1, 0)), colors[node.type], 7);
    this.emitSnapshot();
  }

  craft(recipe: RecipeId): void {
    if (this.state !== 'playing') return;
    const cost = RECIPES[recipe];
    if (recipe === 'repair' && !this.nearBeacon()) {
      this.callbacks.onNotify('Ремонт возможен только рядом с маяком.', 'info');
      return;
    }
    if (recipe === 'repair' && this.beaconHealth >= 100) {
      this.callbacks.onNotify('Маяк не нуждается в ремонте.', 'info');
      return;
    }
    if (recipe === 'medkit' && this.health >= 100) {
      this.callbacks.onNotify('Здоровье уже полное.', 'info');
      return;
    }
    if ((recipe === 'barricade' || recipe === 'turret') && !this.canPlaceHere(recipe)) return;
    if (!this.canAfford(cost)) {
      this.callbacks.onNotify(this.missingResources(cost), 'danger');
      return;
    }
    this.pay(cost);

    if (recipe === 'barricade' || recipe === 'turret') {
      const position = this.buildPosition();
      const rotation = this.player.root.rotation.y;
      if (recipe === 'barricade') {
        const root = createBarricade();
        root.position.copy(position);
        root.rotation.y = rotation;
        this.scene.add(root);
        this.buildings.push({ kind: recipe, root, health: 85, cooldown: 0 });
        this.callbacks.onNotify('Баррикада установлена. Пепельные будут атаковать её.', 'good');
      } else {
        const turret = createTurret();
        turret.root.position.copy(position);
        turret.root.rotation.y = rotation;
        this.scene.add(turret.root);
        this.buildings.push({ kind: recipe, root: turret.root, health: 75, cooldown: 0.2, turret });
        this.callbacks.onNotify('Турель настроена. Дальность обстрела — 10 метров.', 'good');
      }
      this.spawnParticles(position.clone().add(new THREE.Vector3(0, 0.8, 0)), 0xe3b778, 12);
    } else if (recipe === 'repair') {
      this.beaconHealth = Math.min(100, this.beaconHealth + 35);
      this.callbacks.onNotify('Маяк отремонтирован: +35 прочности.', 'good');
      this.spawnParticles(new THREE.Vector3(0, 1.2, 0), 0xf2b56e, 14);
    } else {
      this.health = Math.min(100, this.health + 45);
      this.callbacks.onLoot('+45 здоровья', 'health');
    }
    this.sound.play('craft');
    this.emitSnapshot();
  }

  private frame(timestamp: number): void {
    const dt = this.lastFrame === 0 ? 0.016 : Math.min((timestamp - this.lastFrame) / 1000, 0.05);
    this.lastFrame = timestamp;
    const worldDt = this.state === 'paused' ? 0 : dt;
    this.worldTime += worldDt;

    if (this.state === 'playing') this.update(dt);
    else if (this.state === 'menu') this.updateMenu(dt);
    if (this.state !== 'paused') {
      this.updateResourceVisuals(dt);
      this.updateBeaconVisuals(dt);
      this.updateEffects(dt);
    }
    this.environment.update(worldDt, this.worldTime, this.phase, this.state !== 'paused');
    this.updateCamera(dt);
    this.renderer.render(this.scene, this.camera);
  }

  private update(dt: number): void {
    this.elapsed += dt;
    this.attackCooldown = Math.max(0, this.attackCooldown - dt);
    this.attackAnimation = Math.max(0, this.attackAnimation - dt);
    this.interactCooldown = Math.max(0, this.interactCooldown - dt);
    this.hurtCooldown = Math.max(0, this.hurtCooldown - dt);
    this.cameraShake = Math.max(0, this.cameraShake - dt * 1.8);
    this.updatePhase(dt);
    if (this.state !== 'playing') return;
    this.updatePlayer(dt);
    this.updateSpawning(dt);
    this.updateEnemies(dt);
    if (this.state !== 'playing') return;
    this.updateBuildings(dt);
    if (this.keys.has('KeyE') && this.interactCooldown <= 0) this.interact();
    this.snapshotTimer -= dt;
    if (this.snapshotTimer <= 0) {
      this.snapshotTimer = 0.1;
      this.emitSnapshot();
    }
  }

  private updateMenu(dt: number): void {
    this.menuTime += dt;
    this.walkCycle += dt * 0.8;
    this.player.leftArm.rotation.x = Math.sin(this.walkCycle) * 0.035;
    this.player.rightArm.rotation.x = -Math.sin(this.walkCycle) * 0.035;
    this.player.head.rotation.y = Math.sin(this.menuTime * 0.45) * 0.12;
  }

  private updatePhase(dt: number): void {
    if (this.finalCharge !== null) {
      this.finalCharge = Math.max(0, this.finalCharge - dt);
      this.phaseTime = this.finalCharge;
      if (this.finalCharge <= 0) {
        this.finish(true, 'Маяк пробил пепельный шторм. Спасательная баржа уже в пути.');
      }
      return;
    }
    this.phaseTime -= dt;
    if (this.phaseTime > 0) return;
    if (this.phase === 'day') this.startNight();
    else this.startDay();
  }

  private startNight(): void {
    this.phase = 'night';
    this.phaseDuration = 44 + Math.min(this.day * 2, 8);
    this.phaseTime = this.phaseDuration;
    this.spawnTimer = 0.4;
    this.sound.play('night');
    this.sound.setNight(true);
    this.callbacks.onAnnounce(`НОЧЬ ${String(this.day).padStart(2, '0')}`, 'ПЕПЕЛ ПРОСЫПАЕТСЯ', 'Не дайте им разрушить маяк.');
    for (let i = 0; i < Math.min(1 + this.day, 3); i += 1) this.spawnEnemy();
  }

  private startDay(): void {
    this.day += 1;
    this.phase = 'day';
    this.phaseDuration = 62;
    this.phaseTime = this.phaseDuration;
    this.health = Math.min(100, this.health + 18);
    this.sound.play('day');
    this.sound.setNight(false);
    for (const enemy of this.enemies) {
      this.spawnParticles(enemy.rig.root.position.clone().add(new THREE.Vector3(0, 1, 0)), 0x8998a0, 5);
      this.scene.remove(enemy.rig.root);
    }
    this.enemies.length = 0;
    for (const node of this.resources) {
      node.amount = node.maxAmount;
      node.root.visible = true;
    }
    this.callbacks.onAnnounce(`РАССВЕТ · ДЕНЬ ${this.day}`, 'ТИШИНА НАДОЛГО?', 'Ресурсы восстановлены. Укрепите маяк до следующей ночи.');
  }

  private updatePlayer(dt: number): void {
    const forwardInput = (this.keys.has('KeyW') || this.keys.has('ArrowUp') ? 1 : 0)
      - (this.keys.has('KeyS') || this.keys.has('ArrowDown') ? 1 : 0) + this.mobileMove.y;
    const rightInput = (this.keys.has('KeyD') || this.keys.has('ArrowRight') ? 1 : 0)
      - (this.keys.has('KeyA') || this.keys.has('ArrowLeft') ? 1 : 0) + this.mobileMove.x;
    const forward = this.tempA.set(-Math.sin(this.cameraYaw), 0, -Math.cos(this.cameraYaw));
    const right = this.tempB.set(Math.cos(this.cameraYaw), 0, -Math.sin(this.cameraYaw));
    const movement = new THREE.Vector3().addScaledVector(forward, forwardInput).addScaledVector(right, rightInput);
    const isMoving = movement.lengthSq() > 0.04;
    if (isMoving) movement.normalize();
    const sprinting = isMoving && (this.keys.has('ShiftLeft') || this.keys.has('ShiftRight')) && this.stamina > 1;
    const speed = sprinting ? 7.1 : 4.65;
    if (sprinting) this.stamina = Math.max(0, this.stamina - dt * 25);
    else this.stamina = Math.min(100, this.stamina + dt * (isMoving ? 13 : 20));

    if (isMoving) {
      this.player.root.position.addScaledVector(movement, speed * dt);
      const radius = Math.hypot(this.player.root.position.x, this.player.root.position.z);
      if (radius > 22.2) {
        this.player.root.position.x *= 22.2 / radius;
        this.player.root.position.z *= 22.2 / radius;
      }
      this.resolvePlayerCollisions();
      const targetRotation = Math.atan2(movement.x, movement.z);
      const difference = Math.atan2(Math.sin(targetRotation - this.player.root.rotation.y), Math.cos(targetRotation - this.player.root.rotation.y));
      this.player.root.rotation.y += difference * Math.min(1, dt * 13);
      this.walkCycle += dt * (sprinting ? 12 : 8.5);
    }

    if (!this.grounded) {
      this.verticalVelocity -= 16 * dt;
      this.player.root.position.y += this.verticalVelocity * dt;
      if (this.player.root.position.y <= 0) {
        this.player.root.position.y = 0;
        this.verticalVelocity = 0;
        this.grounded = true;
      }
    }

    const swing = isMoving && this.grounded ? Math.sin(this.walkCycle) * (sprinting ? 0.82 : 0.58) : 0;
    this.player.leftLeg.rotation.x = THREE.MathUtils.damp(this.player.leftLeg.rotation.x, swing, 14, dt);
    this.player.rightLeg.rotation.x = THREE.MathUtils.damp(this.player.rightLeg.rotation.x, -swing, 14, dt);
    this.player.leftArm.rotation.x = THREE.MathUtils.damp(this.player.leftArm.rotation.x, -swing * 0.75, 12, dt);
    if (this.attackAnimation > 0) {
      const progress = 1 - this.attackAnimation / 0.3;
      this.player.rightArm.rotation.x = -Math.sin(progress * Math.PI) * 2.25;
      this.player.rightArm.rotation.z = -Math.sin(progress * Math.PI) * 0.45;
    } else {
      this.player.rightArm.rotation.x = THREE.MathUtils.damp(this.player.rightArm.rotation.x, swing * 0.75, 14, dt);
      this.player.rightArm.rotation.z = THREE.MathUtils.damp(this.player.rightArm.rotation.z, 0, 14, dt);
    }
    if (!this.grounded) {
      this.player.leftLeg.rotation.x = -0.35;
      this.player.rightLeg.rotation.x = 0.35;
    }
    if (this.phase === 'day' && this.nearBeacon() && this.health < 100) this.health = Math.min(100, this.health + dt * 1.5);
  }

  private resolvePlayerCollisions(): void {
    const position = this.player.root.position;
    this.pushOut(position, this.beacon.root.position, 2.35);
    for (const node of this.resources) {
      if (node.amount <= 0) continue;
      this.pushOut(position, node.root.position, node.type === 'wood' ? 0.82 : 0.58);
    }
    for (const building of this.buildings) this.pushOut(position, building.root.position, building.kind === 'barricade' ? 1.15 : 0.7);
  }

  private pushOut(position: THREE.Vector3, center: THREE.Vector3, radius: number): void {
    const dx = position.x - center.x;
    const dz = position.z - center.z;
    const distance = Math.hypot(dx, dz);
    if (distance >= radius) return;
    if (distance < 0.001) {
      position.x += radius;
      return;
    }
    position.x = center.x + (dx / distance) * radius;
    position.z = center.z + (dz / distance) * radius;
  }

  private updateSpawning(dt: number): void {
    if (this.phase !== 'night') return;
    this.spawnTimer -= dt;
    const cap = this.finalCharge !== null ? 14 : Math.min(5 + this.day * 2, 11);
    if (this.spawnTimer <= 0 && this.enemies.length < cap) {
      this.spawnEnemy();
      const base = this.finalCharge !== null ? 1.45 : Math.max(2.25, 4.1 - this.day * 0.3);
      this.spawnTimer = base * (0.8 + Math.random() * 0.45);
    }
  }

  private spawnEnemy(): void {
    const roll = Math.random();
    const kind: Enemy['kind'] = (this.day >= 2 || this.finalCharge !== null) && roll > 0.8
      ? 'brute'
      : roll > 0.48 ? 'runner' : 'walker';
    const rig = createEnemy(kind);
    const angle = Math.random() * Math.PI * 2;
    const radius = 21.2 + Math.random() * 0.8;
    rig.root.position.set(Math.sin(angle) * radius, 0, Math.cos(angle) * radius);
    this.scene.add(rig.root);
    const stats = kind === 'brute'
      ? { health: 120, speed: 1.25, damage: 14 }
      : kind === 'runner'
        ? { health: 30, speed: 3.05, damage: 6 }
        : { health: 52, speed: 1.85, damage: 8 };
    this.enemies.push({
      kind,
      rig,
      health: stats.health,
      maxHealth: stats.health,
      speed: stats.speed,
      damage: stats.damage,
      attackCooldown: 0.6 + Math.random() * 0.5,
      walkTime: Math.random() * 10,
      aggro: 0,
    });
    this.spawnParticles(rig.root.position.clone().add(new THREE.Vector3(0, 0.8, 0)), 0x6d7780, 8);
  }

  private updateEnemies(dt: number): void {
    for (let index = this.enemies.length - 1; index >= 0; index -= 1) {
      const enemy = this.enemies[index];
      if (!enemy) continue;
      enemy.attackCooldown -= dt;
      enemy.aggro = Math.max(0, enemy.aggro - dt);
      enemy.walkTime += dt * enemy.speed * 4;
      const enemyPosition = enemy.rig.root.position;
      const playerDistance = this.horizontalDistance(enemyPosition, this.player.root.position);
      let targetKind: 'player' | 'beacon' | 'building' = playerDistance < 5.5 || enemy.aggro > 0 ? 'player' : 'beacon';
      let targetBuilding: Building | undefined;
      let closestBuildingDistance = 4.1;
      for (const building of this.buildings) {
        const distance = this.horizontalDistance(enemyPosition, building.root.position);
        if (distance < closestBuildingDistance) {
          closestBuildingDistance = distance;
          targetBuilding = building;
        }
      }
      if (targetBuilding) targetKind = 'building';
      const targetPosition = targetKind === 'player'
        ? this.player.root.position
        : targetKind === 'building' && targetBuilding
          ? targetBuilding.root.position
          : this.beacon.root.position;
      const direction = this.tempA.copy(targetPosition).sub(enemyPosition);
      direction.y = 0;
      const distance = direction.length();
      const stopDistance = targetKind === 'beacon' ? 2.25 : targetKind === 'building' ? 1.55 : 1.3;
      if (distance > stopDistance) {
        direction.normalize();
        enemyPosition.addScaledVector(direction, enemy.speed * dt);
        enemy.rig.root.rotation.y = Math.atan2(direction.x, direction.z);
        const swing = Math.sin(enemy.walkTime) * 0.65;
        enemy.rig.leftLeg.rotation.x = swing;
        enemy.rig.rightLeg.rotation.x = -swing;
        enemy.rig.leftArm.rotation.x = -0.65 - swing * 0.35;
        enemy.rig.rightArm.rotation.x = -0.65 + swing * 0.35;
      } else if (enemy.attackCooldown <= 0) {
        enemy.attackCooldown = enemy.kind === 'runner' ? 0.85 : 1.15;
        enemy.rig.rightArm.rotation.x = -2.1;
        if (targetKind === 'player') this.damagePlayer(enemy.damage);
        else if (targetKind === 'building' && targetBuilding) this.damageBuilding(targetBuilding, enemy.damage);
        else this.damageBeacon(Math.ceil(enemy.damage * 0.45));
        if (this.state !== 'playing') return;
      }

      for (const other of this.enemies) {
        if (other === enemy) continue;
        const dx = enemyPosition.x - other.rig.root.position.x;
        const dz = enemyPosition.z - other.rig.root.position.z;
        const separation = Math.hypot(dx, dz);
        if (separation > 0.001 && separation < 0.72) {
          enemyPosition.x += (dx / separation) * dt * 0.7;
          enemyPosition.z += (dz / separation) * dt * 0.7;
        }
      }
      const ratio = Math.max(0, enemy.health / enemy.maxHealth);
      enemy.rig.healthBar.visible = ratio < 1;
      enemy.rig.healthFill.scale.x = ratio;
      enemy.rig.healthFill.position.x = -(1 - ratio) * 0.5;
      enemy.rig.healthBar.quaternion.copy(enemy.rig.root.quaternion).invert().multiply(this.camera.quaternion);
    }
  }

  private damagePlayer(amount: number): void {
    if (this.hurtCooldown > 0 || this.state !== 'playing') return;
    this.hurtCooldown = 0.25;
    this.health = Math.max(0, this.health - amount);
    this.cameraShake = 0.4;
    this.sound.play('hurt');
    this.callbacks.onDamage();
    this.spawnParticles(this.player.root.position.clone().add(new THREE.Vector3(0, 1.2, 0)), 0xe98667, 7);
    if (this.health <= 0) this.finish(false, 'Выживший погиб. Маяк остался один против пепельного шторма.');
  }

  private damageBeacon(amount: number): void {
    this.beaconHealth = Math.max(0, this.beaconHealth - amount);
    this.spawnParticles(new THREE.Vector3((Math.random() - 0.5) * 2, 0.8, (Math.random() - 0.5) * 2), 0xb87955, 4);
    if (this.beaconHealth <= 25 && this.beaconHealth + amount > 25) this.callbacks.onNotify('Маяк на грани разрушения! Нужен ремонт.', 'danger');
    if (this.beaconHealth <= 0) this.finish(false, 'Маяк разрушен. Его свет больше не укажет путь спасателям.');
  }

  private damageBuilding(building: Building, amount: number): void {
    building.health -= amount;
    if (building.health > 0) return;
    this.scene.remove(building.root);
    const index = this.buildings.indexOf(building);
    if (index >= 0) this.buildings.splice(index, 1);
    this.spawnParticles(building.root.position.clone().add(new THREE.Vector3(0, 0.8, 0)), 0x967554, 13);
    this.callbacks.onNotify(building.kind === 'turret' ? 'Турель разрушена!' : 'Баррикада разрушена!', 'danger');
  }

  private damageEnemy(enemy: Enemy, amount: number): void {
    enemy.health -= amount;
    enemy.aggro = 4;
    enemy.rig.healthBar.visible = true;
    this.spawnParticles(enemy.rig.root.position.clone().add(new THREE.Vector3(0, 1.35, 0)), 0xe59569, 5);
    if (enemy.health > 0) return;
    const index = this.enemies.indexOf(enemy);
    if (index >= 0) this.enemies.splice(index, 1);
    this.scene.remove(enemy.rig.root);
    this.kills += 1;
    if (Math.random() < 0.28) {
      this.inventory.scrap += 1;
      this.callbacks.onLoot('+1 металла с Пепельного', 'scrap');
    }
  }

  private updateBuildings(dt: number): void {
    for (const building of this.buildings) {
      if (building.kind !== 'turret' || !building.turret) continue;
      building.cooldown -= dt;
      let target: Enemy | undefined;
      let closest = 10.5;
      for (const enemy of this.enemies) {
        const distance = this.horizontalDistance(building.root.position, enemy.rig.root.position);
        if (distance < closest) {
          closest = distance;
          target = enemy;
        }
      }
      if (!target) continue;
      const direction = this.tempA.copy(target.rig.root.position).sub(building.root.position);
      building.turret.head.rotation.y = Math.atan2(direction.x, direction.z) - building.root.rotation.y;
      if (building.cooldown > 0) continue;
      building.cooldown = 0.72;
      const muzzle = building.turret.muzzle.getWorldPosition(new THREE.Vector3());
      const targetPosition = target.rig.root.position.clone().add(new THREE.Vector3(0, 1.2, 0));
      this.spawnTracer(muzzle, targetPosition);
      this.damageEnemy(target, 12);
      this.sound.play('turret');
    }
  }

  private installCore(): void {
    if (!this.canAfford(CORE_COST)) {
      this.callbacks.onNotify(this.missingResources(CORE_COST), 'danger');
      return;
    }
    this.pay(CORE_COST);
    this.cores += 1;
    this.beaconHealth = Math.min(100, this.beaconHealth + 12);
    this.updateCoreSockets();
    this.sound.play('core');
    this.spawnParticles(new THREE.Vector3(0, 2, 0), 0x8ff2d8, 24);
    if (this.cores < 3) {
      this.callbacks.onAnnounce('ЭНЕРГОЯЧЕЙКА УСТАНОВЛЕНА', `${this.cores} / 3`, this.cores === 2 ? 'Ещё одна — и маяк начнёт финальную зарядку.' : 'Свет стал ярче. Продолжайте добычу.');
    } else {
      this.finalCharge = this.finalChargeDuration;
      this.phase = 'night';
      this.phaseDuration = this.finalChargeDuration;
      this.phaseTime = this.finalChargeDuration;
      this.spawnTimer = 0.3;
      this.sound.setNight(true);
      this.callbacks.onAnnounce('ФИНАЛЬНАЯ ЗАРЯДКА', '40 СЕКУНД', 'Шторм идёт на остров. Защитите маяк любой ценой!');
      for (let i = 0; i < 4; i += 1) this.spawnEnemy();
    }
    this.emitSnapshot();
  }

  private updateResourceVisuals(dt: number): void {
    for (const node of this.resources) {
      node.hit = Math.max(0, node.hit - dt * 4.5);
      const remainingScale = 0.72 + 0.28 * (node.amount / node.maxAmount);
      const target = node.amount > 0 ? node.baseScale * remainingScale : 0.001;
      node.currentScale = THREE.MathUtils.damp(node.currentScale, target, node.amount > 0 ? 8 : 11, dt);
      const bump = 1 + Math.sin(node.hit * Math.PI) * 0.08;
      node.root.scale.setScalar(node.currentScale * bump);
      node.root.rotation.z = Math.sin(node.hit * 18) * node.hit * 0.035;
      if (node.amount <= 0 && node.currentScale < 0.025) node.root.visible = false;
      if (node.amount > 0 && !node.root.visible) node.root.visible = true;
    }
  }

  private updateBeaconVisuals(dt: number): void {
    const time = this.worldTime;
    this.beacon.rings[0].rotation.z += dt * (0.35 + this.cores * 0.2);
    this.beacon.rings[1].rotation.z -= dt * (0.25 + this.cores * 0.22);
    this.beacon.glow.scale.setScalar(1 + Math.sin(time * 3.2) * 0.07 + this.cores * 0.04);
    this.beacon.glow.material.emissiveIntensity = 2.1 + this.cores * 1.15 + Math.sin(time * 4) * 0.2;
    this.beacon.light.intensity = (this.phase === 'night' ? 75 : 28) + this.cores * 42;
    const charging = this.finalCharge !== null;
    this.beacon.beam.material.opacity = charging
      ? 0.24 + Math.sin(time * 5) * 0.035
      : 0.018 + this.cores * 0.025;
    this.beacon.beam.rotation.y += dt * 0.08;
  }

  private updateCoreSockets(): void {
    this.beacon.coreSockets.forEach((socket, index) => {
      const active = index < this.cores;
      socket.material.color.setHex(active ? 0x83d9c5 : 0x334c50);
      socket.material.emissive.setHex(active ? 0x35bda4 : 0x152a2b);
      socket.material.emissiveIntensity = active ? 2 : 0.3;
    });
  }

  private updateCamera(dt: number): void {
    if (this.state === 'menu') {
      const angle = 0.65 + this.menuTime * 0.045;
      const desired = new THREE.Vector3(Math.sin(angle) * 27, 12.5, Math.cos(angle) * 27);
      this.camera.position.lerp(desired, 1 - Math.exp(-dt * 2));
      this.camera.lookAt(-3.2, 2.8, 0);
      return;
    }
    const target = this.player.root.position.clone().add(new THREE.Vector3(0, 1.45, 0));
    const horizontal = Math.cos(this.cameraPitch) * this.cameraDistance;
    const desired = new THREE.Vector3(
      target.x + Math.sin(this.cameraYaw) * horizontal,
      target.y + Math.sin(this.cameraPitch) * this.cameraDistance,
      target.z + Math.cos(this.cameraYaw) * horizontal,
    );
    this.camera.position.lerp(desired, 1 - Math.exp(-dt * 10));
    if (this.cameraShake > 0) {
      this.camera.position.x += (Math.random() - 0.5) * this.cameraShake * 0.35;
      this.camera.position.y += (Math.random() - 0.5) * this.cameraShake * 0.25;
    }
    this.camera.lookAt(target);
  }

  private updateEffects(dt: number): void {
    for (let index = this.effects.length - 1; index >= 0; index -= 1) {
      const effect = this.effects[index];
      effect.life -= dt;
      if (effect.velocity) {
        if (effect.gravity) effect.velocity.y -= effect.gravity * dt;
        effect.object.position.addScaledVector(effect.velocity, dt);
        effect.object.rotation.x += dt * 4;
        effect.object.rotation.z += dt * 3;
      }
      if (effect.grow) effect.object.scale.multiplyScalar(1 + dt * 4);
      const opacity = Math.max(0, effect.life / effect.maxLife);
      effect.object.traverse((child) => {
        if (child instanceof THREE.Mesh || child instanceof THREE.Line) {
          const material = child.material as THREE.Material & { opacity: number };
          if (material.transparent) material.opacity = opacity;
        }
      });
      if (effect.life > 0) continue;
      this.scene.remove(effect.object);
      effect.object.traverse((child) => {
        if (child instanceof THREE.Mesh || child instanceof THREE.Line) {
          (child.material as THREE.Material).dispose();
          if (effect.disposeGeometry) child.geometry.dispose();
        }
      });
      this.effects.splice(index, 1);
    }
  }

  private spawnParticles(position: THREE.Vector3, color: number, count: number): void {
    for (let i = 0; i < count; i += 1) {
      const material = new THREE.MeshBasicMaterial({ color, transparent: true });
      const particle = new THREE.Mesh(this.particleGeometry, material);
      particle.position.copy(position);
      particle.scale.setScalar(0.65 + Math.random() * 1.25);
      this.scene.add(particle);
      const life = 0.45 + Math.random() * 0.45;
      this.effects.push({
        object: particle,
        life,
        maxLife: life,
        velocity: new THREE.Vector3((Math.random() - 0.5) * 3.2, 1.2 + Math.random() * 2.7, (Math.random() - 0.5) * 3.2),
        gravity: 5.5,
      });
    }
  }

  private spawnSlash(): void {
    const material = new THREE.MeshBasicMaterial({ color: 0xf4d39b, transparent: true, opacity: 0.85, depthWrite: false });
    const slash = new THREE.Mesh(new THREE.TorusGeometry(1.55, 0.035, 4, 24, Math.PI * 1.25), material);
    slash.position.copy(this.player.root.position).add(new THREE.Vector3(0, 1.05, 0));
    slash.rotation.x = Math.PI / 2;
    slash.rotation.z = -this.player.root.rotation.y - 0.5;
    this.scene.add(slash);
    this.effects.push({ object: slash, life: 0.2, maxLife: 0.2, grow: true, disposeGeometry: true });
  }

  private spawnTracer(from: THREE.Vector3, to: THREE.Vector3): void {
    const geometry = new THREE.BufferGeometry().setFromPoints([from, to]);
    const material = new THREE.LineBasicMaterial({ color: 0x9af4d8, transparent: true, opacity: 0.9 });
    const line = new THREE.Line(geometry, material);
    this.scene.add(line);
    this.effects.push({ object: line, life: 0.12, maxLife: 0.12, disposeGeometry: true });
  }

  private createResources(): void {
    const placements: Array<[ResourceType, number, number]> = [
      ['wood', -10, -7], ['wood', -15, 2], ['wood', -12, 10], ['wood', -5, 16],
      ['wood', 5, 17], ['wood', 13, 11], ['wood', 16, 3], ['wood', 13, -8],
      ['wood', 6, -15], ['wood', -4, -17], ['wood', -14, -12], ['wood', -18, 7],
      ['scrap', 9, 5], ['scrap', 11, -2], ['scrap', 7, -10], ['scrap', -7, -11],
      ['scrap', -11, 5], ['scrap', -7, 12], ['scrap', 3, 13], ['scrap', 17, -3],
      ['scrap', -17, -4], ['scrap', 2, -9],
      ['crystal', -3, 7], ['crystal', 4, 7], ['crystal', -8, 1], ['crystal', 8, 1],
      ['crystal', -2, -13], ['crystal', 14, 6], ['crystal', -13, 14], ['crystal', 10, -14],
    ];
    placements.forEach(([type, x, z], index) => {
      const root = createResource(type, (index * 0.618) % 1);
      const baseScale = type === 'wood' ? 0.85 + (index % 4) * 0.08 : 0.9 + (index % 3) * 0.08;
      root.position.set(x, 0, z);
      root.rotation.y = index * 1.71;
      root.scale.setScalar(baseScale);
      this.scene.add(root);
      const maxAmount = type === 'crystal' ? 3 : 4;
      this.resources.push({ type, root, amount: maxAmount, maxAmount, baseScale, currentScale: baseScale, hit: 0 });
    });
  }

  private resetGame(): void {
    for (const enemy of this.enemies) this.scene.remove(enemy.rig.root);
    for (const building of this.buildings) this.scene.remove(building.root);
    for (const effect of this.effects) this.scene.remove(effect.object);
    this.enemies.length = 0;
    this.buildings.length = 0;
    this.effects.length = 0;
    this.phase = 'day';
    this.day = 1;
    this.phaseTime = 75;
    this.phaseDuration = 75;
    this.finalCharge = null;
    this.health = 100;
    this.stamina = 100;
    this.beaconHealth = 100;
    this.cores = 0;
    this.inventory = { wood: 2, scrap: 1, crystal: 0 };
    this.kills = 0;
    this.gathered = 0;
    this.elapsed = 0;
    this.spawnTimer = 0;
    this.attackCooldown = 0;
    this.attackAnimation = 0;
    this.interactCooldown = 0;
    this.hurtCooldown = 0;
    this.verticalVelocity = 0;
    this.grounded = true;
    this.cameraYaw = 0;
    this.cameraPitch = 0.46;
    this.cameraDistance = 11.5;
    this.keys.clear();
    this.mobileMove.set(0, 0);
    this.player.root.position.set(0, 0, 9);
    this.player.root.rotation.set(0, Math.PI, 0);
    this.player.root.visible = true;
    for (const node of this.resources) {
      node.amount = node.maxAmount;
      node.currentScale = node.baseScale;
      node.root.scale.setScalar(node.baseScale);
      node.root.visible = true;
      node.hit = 0;
    }
    this.updateCoreSockets();
    this.sound.setNight(false);
  }

  private nearestResource(): ResourceNode | undefined {
    let closest: ResourceNode | undefined;
    let distance = 3;
    for (const node of this.resources) {
      if (node.amount <= 0) continue;
      const current = this.horizontalDistance(this.player.root.position, node.root.position);
      if (current < distance) {
        distance = current;
        closest = node;
      }
    }
    return closest;
  }

  private nearBeacon(): boolean {
    return this.horizontalDistance(this.player.root.position, this.beacon.root.position) < 4.4;
  }

  private canPlaceHere(recipe: 'barricade' | 'turret'): boolean {
    const sameKind = this.buildings.filter((building) => building.kind === recipe).length;
    const limit = recipe === 'turret' ? 3 : 6;
    if (sameKind >= limit) {
      this.callbacks.onNotify(`Можно установить не больше ${limit} таких построек.`, 'info');
      return false;
    }
    const position = this.buildPosition();
    if (Math.hypot(position.x, position.z) > 21.5 || this.horizontalDistance(position, this.beacon.root.position) < 3.2) {
      this.callbacks.onNotify('Здесь недостаточно места для постройки.', 'info');
      return false;
    }
    if (this.buildings.some((building) => this.horizontalDistance(position, building.root.position) < 2)) {
      this.callbacks.onNotify('Другая постройка мешает установке.', 'info');
      return false;
    }
    if (this.resources.some((node) => node.amount > 0 && this.horizontalDistance(position, node.root.position) < 1.25)) {
      this.callbacks.onNotify('Ресурс мешает установке. Отойдите на свободное место.', 'info');
      return false;
    }
    return true;
  }

  private buildPosition(): THREE.Vector3 {
    const rotation = this.player.root.rotation.y;
    return this.player.root.position.clone().add(new THREE.Vector3(Math.sin(rotation) * 2.6, 0, Math.cos(rotation) * 2.6)).setY(0);
  }

  private canAfford(cost: Inventory): boolean {
    return this.inventory.wood >= cost.wood && this.inventory.scrap >= cost.scrap && this.inventory.crystal >= cost.crystal;
  }

  private pay(cost: Inventory): void {
    this.inventory.wood -= cost.wood;
    this.inventory.scrap -= cost.scrap;
    this.inventory.crystal -= cost.crystal;
  }

  private missingResources(cost: Inventory): string {
    const missing: string[] = [];
    if (this.inventory.wood < cost.wood) missing.push(`${cost.wood - this.inventory.wood} древесины`);
    if (this.inventory.scrap < cost.scrap) missing.push(`${cost.scrap - this.inventory.scrap} металла`);
    if (this.inventory.crystal < cost.crystal) missing.push(`${cost.crystal - this.inventory.crystal} осколков`);
    return `Не хватает: ${missing.join(', ')}.`;
  }

  private getPrompt(): { title: string; detail: string } {
    if (this.nearBeacon()) {
      if (this.cores < 3) return { title: 'Установить энергоячейку', detail: 'E · 6 металла + 4 осколка' };
      return { title: this.finalCharge !== null ? 'Идёт зарядка маяка' : 'Маяк заряжен', detail: 'Оставайтесь рядом и защищайте его' };
    }
    const node = this.nearestResource();
    if (node) {
      const verbs: Record<ResourceType, string> = { wood: 'Срубить дерево', scrap: 'Собрать металл', crystal: 'Добыть осколок' };
      return { title: verbs[node.type], detail: `E · осталось: ${node.amount}` };
    }
    return { title: '', detail: '' };
  }

  private emitSnapshot(): void {
    const prompt = this.getPrompt();
    this.callbacks.onSnapshot({
      state: this.state,
      health: this.health,
      stamina: this.stamina,
      beaconHealth: this.beaconHealth,
      inventory: { ...this.inventory },
      cores: this.cores,
      day: this.day,
      phase: this.phase,
      phaseTime: Math.max(0, this.phaseTime),
      phaseDuration: this.phaseDuration,
      finalCharge: this.finalCharge,
      finalChargeDuration: this.finalChargeDuration,
      prompt: prompt.title,
      promptDetail: prompt.detail,
      enemies: this.enemies.length,
      kills: this.kills,
      gathered: this.gathered,
      elapsed: this.elapsed,
      canAfford: {
        barricade: this.canAfford(RECIPES.barricade),
        turret: this.canAfford(RECIPES.turret),
        repair: this.canAfford(RECIPES.repair),
        medkit: this.canAfford(RECIPES.medkit),
      },
      nearBeacon: this.nearBeacon(),
    });
  }

  private finish(victory: boolean, reason: string): void {
    if (this.state !== 'playing') return;
    this.state = victory ? 'victory' : 'gameover';
    this.keys.clear();
    this.mobileMove.set(0, 0);
    this.sound.play(victory ? 'win' : 'lose');
    if (victory) {
      for (const enemy of this.enemies) {
        this.spawnParticles(enemy.rig.root.position.clone().add(new THREE.Vector3(0, 1, 0)), 0xf5c57d, 7);
        this.scene.remove(enemy.rig.root);
      }
      this.enemies.length = 0;
      this.beacon.beam.material.opacity = 0.5;
      this.spawnParticles(new THREE.Vector3(0, 5.7, 0), 0xffd38f, 38);
    }
    const stats: EndStats = {
      victory,
      reason,
      kills: this.kills,
      gathered: this.gathered,
      elapsed: this.elapsed,
      day: this.day,
      cores: this.cores,
    };
    this.callbacks.onState(this.state, stats);
    this.emitSnapshot();
  }

  private horizontalDistance(a: THREE.Vector3, b: THREE.Vector3): number {
    return Math.hypot(a.x - b.x, a.z - b.z);
  }

  private bindInput(): void {
    window.addEventListener('keydown', (event) => {
      if (event.code === 'KeyP' && (this.state === 'playing' || this.state === 'paused')) {
        event.preventDefault();
        this.togglePause();
        return;
      }
      if (this.state !== 'playing') return;
      if (['Space', 'ArrowUp', 'ArrowDown', 'ArrowLeft', 'ArrowRight'].includes(event.code)) event.preventDefault();
      this.keys.add(event.code);
      if (event.repeat) return;
      if (event.code === 'Space') this.jump();
      else if (event.code === 'KeyF') this.attack();
      else if (event.code === 'KeyE') this.interact();
      else if (event.code === 'Digit1') this.craft('barricade');
      else if (event.code === 'Digit2') this.craft('turret');
      else if (event.code === 'Digit3') this.craft('repair');
      else if (event.code === 'Digit4') this.craft('medkit');
    });
    window.addEventListener('keyup', (event) => this.keys.delete(event.code));
    window.addEventListener('blur', () => {
      this.keys.clear();
      this.mobileMove.set(0, 0);
    });

    this.canvas.addEventListener('pointerdown', (event) => {
      if (this.state !== 'playing' || this.draggingPointer !== null) return;
      this.draggingPointer = event.pointerId;
      this.dragX = event.clientX;
      this.dragY = event.clientY;
      this.canvas.setPointerCapture(event.pointerId);
      if (event.pointerType === 'mouse' && event.button === 0) this.attack();
    });
    this.canvas.addEventListener('pointermove', (event) => {
      if (this.draggingPointer !== event.pointerId || this.state !== 'playing') return;
      const dx = event.clientX - this.dragX;
      const dy = event.clientY - this.dragY;
      this.dragX = event.clientX;
      this.dragY = event.clientY;
      this.cameraYaw -= dx * (event.pointerType === 'touch' ? 0.007 : 0.005);
      this.cameraPitch = THREE.MathUtils.clamp(this.cameraPitch + dy * 0.004, 0.16, 1.02);
    });
    const endDrag = (event: PointerEvent) => {
      if (this.draggingPointer === event.pointerId) this.draggingPointer = null;
    };
    this.canvas.addEventListener('pointerup', endDrag);
    this.canvas.addEventListener('pointercancel', endDrag);
    this.canvas.addEventListener('wheel', (event) => {
      if (this.state !== 'playing') return;
      event.preventDefault();
      this.cameraDistance = THREE.MathUtils.clamp(this.cameraDistance + event.deltaY * 0.008, 7.5, 16);
    }, { passive: false });
    this.canvas.addEventListener('contextmenu', (event) => event.preventDefault());
  }

  private resize(): void {
    const width = this.canvas.clientWidth || window.innerWidth;
    const height = this.canvas.clientHeight || window.innerHeight;
    this.renderer.setSize(width, height, false);
    this.camera.aspect = width / Math.max(height, 1);
    this.camera.updateProjectionMatrix();
  }
}
