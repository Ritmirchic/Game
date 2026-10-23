import {
  Bone,
  Color3,
  Engine,
  Matrix,
  Mesh,
  MeshBuilder,
  PBRMaterial,
  Scene,
  StandardMaterial,
  TransformNode,
  Vector2,
  Vector3,
} from '@babylonjs/core'
import { Environment, islandHeight } from './Environment'
import {
  ModelInstance,
  ModelLibrary,
  createBarricade,
  createBeacon,
  createBroadleafTree,
  createCampfire,
  createCrystalCluster,
  createMaterialLibrary,
  createRock,
  createScrap,
  createTurret,
  setShadows,
  spawnBurst,
  spawnTracer,
  type BeaconVisual,
  type MaterialLibrary,
  type TurretVisual,
} from './models'
import { Soundscape } from './Soundscape'
import type {
  EndStats,
  GameCallbacks,
  GameState,
  Inventory,
  Phase,
  RecipeId,
  ResourceType,
} from './types'

interface ResourceNode {
  type: ResourceType
  root: TransformNode
  amount: number
  maxAmount: number
  baseScale: number
  hit: number
}

interface Enemy {
  kind: 'walker' | 'runner' | 'brute'
  model: ModelInstance
  health: number
  maxHealth: number
  speed: number
  damage: number
  attackCooldown: number
  aggro: number
  windup: number
  windupTotal: number
  targetKind: 'player' | 'beacon' | 'building'
  targetBuilding?: Building
  healthFill: Mesh
  healthBack: Mesh
  fireTick: number
}

interface Building {
  kind: 'barricade' | 'turret' | 'campfire'
  root: TransformNode
  health: number
  cooldown: number
  turret?: TurretVisual
  fuel?: number
  maxFuel?: number
  flames?: Mesh[]
  light?: import('@babylonjs/core').PointLight
  particles?: import('@babylonjs/core').ParticleSystem
}

const RECIPES: Record<RecipeId, Inventory> = {
  barricade: { wood: 5, scrap: 2, crystal: 0 },
  turret: { wood: 0, scrap: 7, crystal: 2 },
  campfire: { wood: 4, scrap: 1, crystal: 0 },
  repair: { wood: 3, scrap: 3, crystal: 0 },
  medkit: { wood: 0, scrap: 2, crystal: 1 },
}
const CORE_COST: Inventory = { wood: 0, scrap: 6, crystal: 4 }
// The Mixamo asset faces -Z, while gameplay uses +Z as forward.
const SOLDIER_FORWARD_OFFSET = Math.PI
const RESOURCE_NAMES: Record<ResourceType, string> = { wood: 'древесины', scrap: 'металла', crystal: 'осколка' }
const RESOURCE_COLORS: Record<ResourceType, Color3> = {
  wood: Color3.FromHexString('#c59a68'),
  scrap: Color3.FromHexString('#a9bec4'),
  crystal: Color3.FromHexString('#65f4df'),
}

function distance2D(a: Vector3, b: Vector3): number {
  return Math.hypot(a.x - b.x, a.z - b.z)
}

export class Game {
  readonly sound = new Soundscape()
  readonly ready: Promise<void>
  private readonly engine: Engine
  private readonly scene: Scene
  private readonly environment: Environment
  private readonly materials: MaterialLibrary
  private readonly models: ModelLibrary
  private readonly beacon: BeaconVisual
  private player!: ModelInstance
  private weapon!: TransformNode
  private weaponHand: Bone | null = null
  private weaponHandAnchor: TransformNode | null = null
  private readonly weaponRootInverse = Matrix.Identity()
  private readonly weaponHandWorld = new Vector3()
  private readonly resources: ResourceNode[] = []
  private readonly enemies: Enemy[] = []
  private readonly buildings: Building[] = []
  private readonly keys = new Set<string>()
  private readonly mobileMove = new Vector2(0, 0)
  private readonly moveDirection = new Vector3(0, 0, 1)
  private readonly dodgeDirection = new Vector3(0, 0, 1)
  private isReady = false

  private state: GameState = 'menu'
  private phase: Phase = 'day'
  private day = 1
  private phaseTime = 82
  private phaseDuration = 82
  private finalCharge: number | null = null
  private readonly finalChargeDuration = 40
  private health = 100
  private stamina = 100
  private warmth = 100
  private beaconHealth = 100
  private cores = 0
  private inventory: Inventory = { wood: 4, scrap: 2, crystal: 0 }
  private kills = 0
  private gathered = 0
  private elapsed = 0
  private spawnTimer = 0
  private attackCooldown = 0
  private attackAnimation = 0
  private interactCooldown = 0
  private hurtCooldown = 0
  private dodgeCooldown = 0
  private dodgeTime = 0
  private invulnerableTime = 0
  private cameraYaw = 0.15
  private cameraPitch = 0.43
  private cameraDistance = 11.8
  private cameraShake = 0
  private draggingPointer: number | null = null
  private dragX = 0
  private dragY = 0
  private dragDistance = 0
  private dragAttackCandidate = false
  private snapshotTimer = 0
  private worldTime = 0
  private visualNight = 0
  private freezeFeedback = 0

  constructor(
    private readonly canvas: HTMLCanvasElement,
    private readonly callbacks: GameCallbacks,
  ) {
    this.engine = new Engine(canvas, true, {
      stencil: true,
      powerPreference: 'high-performance',
      doNotHandleContextLost: false,
    }, true)
    this.engine.setHardwareScalingLevel(1 / Math.min(window.devicePixelRatio || 1, 1.5))
    this.scene = new Scene(this.engine)
    this.scene.skipPointerMovePicking = true
    this.scene.constantlyUpdateMeshUnderPointer = false
    this.environment = new Environment(this.scene)
    this.materials = createMaterialLibrary(this.scene)
    this.models = new ModelLibrary(this.scene, this.environment.shadows)
    this.beacon = createBeacon(this.scene, this.materials, this.environment.shadows)
    this.beacon.root.position.y = islandHeight(0, 0)
    this.environment.addReflections(this.beacon.root)
    this.weapon = new TransformNode('salvage spear pivot', this.scene)
    this.createWeapon()
    this.bindInput()
    window.addEventListener('resize', () => this.engine.resize())
    this.engine.runRenderLoop(() => this.frame())
    this.ready = this.initialize()
  }

  private async initialize(): Promise<void> {
    await this.models.load()
    this.player = this.models.create('soldier', 1.86)
    this.player.root.name = 'animated survivor'
    this.player.root.position.set(5, islandHeight(5, 7), 7)
    this.player.content.rotation.y = SOLDIER_FORWARD_OFFSET
    this.weapon.parent = this.player.root
    this.weaponHand = this.player.entries.skeletons
      .flatMap((skeleton) => skeleton.bones)
      .find((bone) => bone.name.endsWith('RightHand')) ?? null
    this.weaponHandAnchor = this.player.content.getChildMeshes()
      .find((mesh) => this.weaponHand !== null && mesh.skeleton?.bones.includes(this.weaponHand)) ?? null
    this.scene.onAfterAnimationsObservable.add(() => this.syncWeaponGrip())
    this.createWorld()
    this.isReady = true
    this.resetGame()
    this.state = 'menu'
    this.emitSnapshot()
  }

  private createWeapon(): void {
    const shaft = MeshBuilder.CreateCylinder('spear ashwood shaft', {
      diameter: 0.055, height: 1.45, tessellation: 10,
    }, this.scene)
    shaft.parent = this.weapon
    shaft.material = this.materials.wood
    const blade = MeshBuilder.CreateCylinder('forged spear head', {
      diameterTop: 0, diameterBottom: 0.24, height: 0.48, tessellation: 4,
    }, this.scene)
    blade.parent = this.weapon
    blade.position.y = 0.94
    blade.material = this.materials.steel
    for (let i = 0; i < 5; i += 1) {
      const binding = MeshBuilder.CreateTorus('spear binding', {
        diameter: 0.075, thickness: 0.015, tessellation: 8,
      }, this.scene)
      binding.parent = this.weapon
      binding.position.y = 0.57 + i * 0.045
      binding.material = this.materials.copper
    }
    this.weapon.position.set(0.3, 1, -0.18)
    this.weapon.rotation.z = -0.18
    setShadows(this.weapon, this.environment.shadows)
  }

  private syncWeaponGrip(): void {
    if (!this.weaponHand || !this.weaponHandAnchor) return
    this.player.root.computeWorldMatrix(true)
    this.weaponHandAnchor.computeWorldMatrix(true)
    this.weaponHand.computeAbsoluteMatrices()
    this.weaponHand.getAbsolutePositionToRef(this.weaponHandAnchor, this.weaponHandWorld)
    this.player.root.getWorldMatrix().invertToRef(this.weaponRootInverse)
    Vector3.TransformCoordinatesToRef(this.weaponHandWorld, this.weaponRootInverse, this.weapon.position)
  }

  private createWorld(): void {
    const treePositions = [
      [-8, -7], [-13, -3], [-16, 5], [-10, 12], [-3, 16], [6, 15], [13, 10],
      [16, 2], [12, -9], [5, -15], [-5, -16], [-17, -9],
    ]
    treePositions.forEach(([x, z], index) => {
      const root = createBroadleafTree(this.scene, this.materials, this.environment.shadows, 5.1 + (index % 3) * 0.55)
      root.position.set(x, islandHeight(x, z), z)
      root.rotation.y = index * 1.73
      this.environment.addReflections(root)
      this.resources.push({ type: 'wood', root, amount: 4, maxAmount: 4, baseScale: 1, hit: 0 })
    })

    const scrapPositions = [[7, 5], [10, -3], [2, -10], [-7, -11], [-12, 7], [1, 12], [17, -5]]
    scrapPositions.forEach(([x, z], index) => {
      const root = createScrap(this.scene, this.materials, this.environment.shadows)
      root.position.set(x, islandHeight(x, z), z)
      root.rotation.y = index * 0.91
      this.environment.addReflections(root)
      this.resources.push({ type: 'scrap', root, amount: 4, maxAmount: 4, baseScale: 1, hit: 0 })
    })

    const crystalPositions = [[-5, 7], [5, -6], [14, 5], [-14, -1], [7, 18], [-8, -18]]
    crystalPositions.forEach(([x, z], index) => {
      const root = createCrystalCluster(this.scene, this.materials, this.environment.shadows)
      root.position.set(x, islandHeight(x, z), z)
      root.rotation.y = index * 1.2
      this.environment.addReflections(root)
      this.resources.push({ type: 'crystal', root, amount: 3, maxAmount: 3, baseScale: 1, hit: 0 })
    })

    const decorations: Array<[number, number, number]> = [
      [-20, 3, 0.8], [-18, 13, 1.2], [-2, 21, 0.7], [17, 14, 1.1], [20, -8, 0.9],
      [9, -20, 1.25], [-13, -18, 0.75], [20, 4, 0.55], [-20, -12, 0.9], [1, -20, 0.65],
    ]
    decorations.forEach(([x, z, scale], index) => {
      const rock = createRock(this.scene, this.materials, this.environment.shadows, scale)
      rock.position.set(x, islandHeight(x, z) + scale * 0.2, z)
      rock.rotation.y = index * 1.37
      this.environment.waterMaterial.addToRenderList(rock)
    })

    const bushes = [[-6, 11], [9, 10], [15, -12], [-15, 10], [2, 18], [-18, -4], [11, 1], [-3, -11], [18, 8]]
    bushes.forEach(([x, z], index) => {
      if (index % 4 === 0) {
        const deadTree = this.models.create('deadTree', 3.4)
        deadTree.root.position.set(x, islandHeight(x, z), z)
        deadTree.root.rotation.y = index * 2.1
        this.environment.addReflections(deadTree.root)
      } else {
        const bush = createBroadleafTree(this.scene, this.materials, this.environment.shadows, 1.45, false)
        bush.position.set(x, islandHeight(x, z), z)
        bush.rotation.y = index * 2.1
        this.environment.addReflections(bush)
      }
    })
    this.environment.addReflections(this.player.root)
  }

  start(): void {
    if (!this.isReady) return
    this.sound.unlock()
    this.scene.animationsEnabled = true
    this.resetGame()
    this.state = 'playing'
    this.callbacks.onState('playing')
    this.callbacks.onAnnounce('СМЕНА 01', 'ДОБУДЬТЕ РЕСУРСЫ', 'Три ячейки зарядят маяк. Не забудьте про тепло и оборону.')
    this.emitSnapshot()
  }

  toMenu(): void {
    if (!this.isReady) return
    this.scene.animationsEnabled = true
    this.resetGame()
    this.state = 'menu'
    this.callbacks.onState('menu')
  }

  pause(): void {
    if (this.state !== 'playing') return
    this.state = 'paused'
    this.keys.clear()
    this.mobileMove.set(0, 0)
    this.draggingPointer = null
    this.dragAttackCandidate = false
    this.scene.animationsEnabled = false
    this.callbacks.onState('paused')
  }

  resume(): void {
    if (this.state !== 'paused') return
    this.scene.animationsEnabled = true
    this.state = 'playing'
    this.callbacks.onState('playing')
  }

  togglePause(): void {
    if (this.state === 'playing') this.pause()
    else if (this.state === 'paused') this.resume()
  }

  toggleSound(): boolean {
    this.sound.unlock()
    return this.sound.toggle()
  }

  setJoystick(x: number, y: number): void {
    this.mobileMove.set(x, y)
  }

  jump(): void {
    this.dodge()
  }

  dodge(): void {
    if (this.state !== 'playing' || this.dodgeCooldown > 0 || this.stamina < 25) return
    this.stamina -= 25
    this.dodgeCooldown = 2.1
    this.dodgeTime = 0.38
    this.invulnerableTime = 0.52
    if (this.moveDirection.lengthSquared() > 0.05) this.dodgeDirection.copyFrom(this.moveDirection).normalize()
    else this.dodgeDirection.set(Math.sin(this.player.root.rotation.y), 0, Math.cos(this.player.root.rotation.y))
    this.sound.play('swing')
    spawnBurst(this.scene, this.player.root.position.add(new Vector3(0, 0.25, 0)), Color3.FromHexString('#b5c7bd'), 9)
  }

  attack(): void {
    if (this.state !== 'playing' || this.attackCooldown > 0) return
    this.attackCooldown = 0.38
    this.attackAnimation = 0.32
    this.sound.play('swing')
    const forward = new Vector3(Math.sin(this.player.root.rotation.y), 0, Math.cos(this.player.root.rotation.y))
    const targets = this.enemies
      .filter((enemy) => {
        const offset = enemy.model.root.position.subtract(this.player.root.position)
        offset.y = 0
        const distance = offset.length()
        return distance < 3.35 && (distance < 1.25 || Vector3.Dot(offset.normalize(), forward) > 0.05)
      })
      .sort((a, b) => distance2D(a.model.root.position, this.player.root.position) - distance2D(b.model.root.position, this.player.root.position))
      .slice(0, 2)
    if (targets.length > 0) {
      const direction = targets[0].model.root.position.subtract(this.player.root.position)
      this.player.root.rotation.y = Math.atan2(direction.x, direction.z)
      targets.forEach((target) => this.damageEnemy(target, 32))
      this.sound.play('hit')
    }
  }

  interact(): void {
    if (this.state !== 'playing' || this.interactCooldown > 0) return
    this.interactCooldown = 0.25
    if (this.nearBeacon() && this.cores < 3) {
      this.installCore()
      return
    }
    const fire = this.nearestCampfire()
    if (fire) {
      if ((fire.fuel ?? 0) >= (fire.maxFuel ?? 120) - 4) {
        this.callbacks.onNotify('Костёр горит жарко — дрова пока не нужны.', 'info')
      } else if (this.inventory.wood < 2) {
        this.callbacks.onNotify('Для дозаправки нужно 2 древесины.', 'danger')
      } else {
        this.inventory.wood -= 2
        fire.fuel = Math.min(fire.maxFuel ?? 120, (fire.fuel ?? 0) + 55)
        fire.particles?.start()
        this.callbacks.onNotify('Костёр дозаправлен: +55 секунд огня.', 'good')
        this.sound.play('wood')
        spawnBurst(this.scene, fire.root.position.add(new Vector3(0, 0.7, 0)), Color3.FromHexString('#ff9d45'), 12)
      }
      this.emitSnapshot()
      return
    }
    const node = this.nearestResource()
    if (!node) return
    const amount = node.type === 'crystal' ? 1 : 2
    node.amount -= 1
    node.hit = 1
    this.inventory[node.type] += amount
    this.gathered += amount
    this.sound.play(node.type)
    this.callbacks.onLoot(`+${amount} ${RESOURCE_NAMES[node.type]}`, node.type)
    spawnBurst(this.scene, node.root.position.add(new Vector3(0, 1, 0)), RESOURCE_COLORS[node.type], 9)
    if (node.amount <= 0) node.root.setEnabled(false)
    this.emitSnapshot()
  }

  craft(recipe: RecipeId): void {
    if (this.state !== 'playing') return
    const cost = RECIPES[recipe]
    if (recipe === 'repair' && !this.nearBeacon()) {
      this.callbacks.onNotify('Ремонт возможен только рядом с маяком.', 'info')
      return
    }
    if (recipe === 'repair' && this.beaconHealth >= 100) {
      this.callbacks.onNotify('Маяк не нуждается в ремонте.', 'info')
      return
    }
    if (recipe === 'medkit' && this.health >= 100) {
      this.callbacks.onNotify('Здоровье уже полное.', 'info')
      return
    }
    if ((recipe === 'barricade' || recipe === 'turret' || recipe === 'campfire') && !this.canPlaceHere(recipe)) return
    if (!this.canAfford(cost)) {
      this.callbacks.onNotify(this.missingResources(cost), 'danger')
      return
    }
    this.pay(cost)

    if (recipe === 'barricade' || recipe === 'turret' || recipe === 'campfire') {
      const position = this.buildPosition()
      if (recipe === 'barricade') {
        const visual = createBarricade(this.scene, this.materials, this.environment.shadows)
        visual.root.position.copyFrom(position)
        visual.root.rotation.y = this.player.root.rotation.y
        this.buildings.push({ kind: recipe, root: visual.root, health: 110, cooldown: 0 })
        this.callbacks.onNotify('Баррикада установлена. Враги будут вынуждены её разрушить.', 'good')
      } else if (recipe === 'turret') {
        const turret = createTurret(this.scene, this.materials, this.environment.shadows)
        turret.root.position.copyFrom(position)
        turret.root.rotation.y = this.player.root.rotation.y
        this.buildings.push({ kind: recipe, root: turret.root, health: 85, cooldown: 0.2, turret })
        this.callbacks.onNotify('Турель настроена: автоматический обстрел до 11 метров.', 'good')
      } else {
        const fire = createCampfire(this.scene, this.materials, this.environment.shadows)
        fire.root.position.copyFrom(position)
        this.buildings.push({
          kind: recipe, root: fire.root, health: 65, cooldown: 0,
          fuel: 90, maxFuel: 120, flames: fire.flames, light: fire.light, particles: fire.fireParticles,
        })
        this.callbacks.onNotify('Костёр разожжён: тепло 90 секунд. E — дозаправить за 2 дерева.', 'good')
      }
      spawnBurst(this.scene, position.add(new Vector3(0, 0.7, 0)), Color3.FromHexString('#e3b778'), 15)
    } else if (recipe === 'repair') {
      this.beaconHealth = Math.min(100, this.beaconHealth + 35)
      this.callbacks.onNotify('Маяк отремонтирован: +35 прочности.', 'good')
      spawnBurst(this.scene, this.beacon.root.position.add(new Vector3(0, 1.2, 0)), Color3.FromHexString('#f2b56e'), 16)
    } else {
      this.health = Math.min(100, this.health + 45)
      this.callbacks.onLoot('+45 здоровья', 'health')
    }
    this.sound.play('craft')
    this.emitSnapshot()
  }

  private frame(): void {
    const dt = Math.min(this.engine.getDeltaTime() / 1000 || 0.016, 0.05)
    if (!this.isReady) {
      this.environment.menuCamera(dt)
      this.environment.update(dt, 0.04)
      this.scene.render()
      return
    }
    if (this.state === 'playing') this.update(dt)
    else if (this.state === 'menu') this.updateMenu(dt)
    this.scene.render()
  }

  private update(dt: number): void {
    this.worldTime += dt
    this.elapsed += dt
    this.attackCooldown = Math.max(0, this.attackCooldown - dt)
    this.attackAnimation = Math.max(0, this.attackAnimation - dt)
    this.interactCooldown = Math.max(0, this.interactCooldown - dt)
    this.hurtCooldown = Math.max(0, this.hurtCooldown - dt)
    this.dodgeCooldown = Math.max(0, this.dodgeCooldown - dt)
    this.dodgeTime = Math.max(0, this.dodgeTime - dt)
    this.invulnerableTime = Math.max(0, this.invulnerableTime - dt)
    this.cameraShake = Math.max(0, this.cameraShake - dt * 2.8)
    this.updatePhase(dt)
    if (this.state !== 'playing') return
    this.updatePlayer(dt)
    this.updateSpawning(dt)
    this.updateEnemies(dt)
    if (this.state !== 'playing') return
    this.updateBuildings(dt)
    this.updateResourceVisuals(dt)
    this.updateBeaconVisuals(dt)
    this.updateCamera(dt)
    const targetNight = this.finalCharge !== null ? 0.96 : this.phase === 'night' ? 0.9 : this.phaseTime < 9 ? (9 - this.phaseTime) / 12 : 0
    this.visualNight += (targetNight - this.visualNight) * Math.min(1, dt * 1.25)
    this.environment.update(dt, this.visualNight)
    this.snapshotTimer -= dt
    if (this.snapshotTimer <= 0) {
      this.snapshotTimer = 0.1
      this.emitSnapshot()
    }
  }

  private updateMenu(dt: number): void {
    this.worldTime += dt
    this.player.play('Idle', 0.75)
    this.weapon.rotation.x = 0
    this.environment.menuCamera(dt)
    this.environment.update(dt, 0.06)
    this.updateBeaconVisuals(dt)
  }

  private updatePhase(dt: number): void {
    if (this.finalCharge !== null) {
      this.finalCharge = Math.max(0, this.finalCharge - dt)
      this.phaseTime = this.finalCharge
      if (this.finalCharge <= 0) this.finish(true, 'Маяк пробил пепельный шторм. Спасательная баржа уже в пути.')
      return
    }
    this.phaseTime -= dt
    if (this.phaseTime > 0) return
    if (this.phase === 'day') this.startNight()
    else this.startDay()
  }

  private startNight(): void {
    this.phase = 'night'
    this.phaseDuration = 46 + Math.min(this.day * 2, 8)
    this.phaseTime = this.phaseDuration
    this.spawnTimer = 0.35
    this.sound.play('night')
    this.sound.setNight(true)
    this.callbacks.onAnnounce(`НОЧЬ ${String(this.day).padStart(2, '0')}`, 'ПЕПЕЛ ПРОСЫПАЕТСЯ', 'Следите за теплом. Атаки врагов можно переждать уклонением.')
    for (let i = 0; i < Math.min(1 + this.day, 3); i += 1) this.spawnEnemy()
  }

  private startDay(): void {
    this.day += 1
    this.phase = 'day'
    this.phaseDuration = 68
    this.phaseTime = this.phaseDuration
    this.health = Math.min(100, this.health + 18)
    this.warmth = Math.min(100, this.warmth + 35)
    this.sound.play('day')
    this.sound.setNight(false)
    for (const enemy of this.enemies) {
      spawnBurst(this.scene, enemy.model.root.position.add(new Vector3(0, 0.7, 0)), Color3.FromHexString('#8998a0'), 8)
      enemy.model.dispose()
    }
    this.enemies.length = 0
    for (const node of this.resources) {
      node.amount = node.maxAmount
      node.root.setEnabled(true)
    }
    this.callbacks.onAnnounce(`РАССВЕТ · ДЕНЬ ${this.day}`, 'ТИШИНА НАДОЛГО?', 'Ресурсы восстановлены. Дозаправьте костры и укрепите маяк.')
  }

  private updatePlayer(dt: number): void {
    const forwardInput = (this.keys.has('KeyW') || this.keys.has('ArrowUp') ? 1 : 0)
      - (this.keys.has('KeyS') || this.keys.has('ArrowDown') ? 1 : 0) + this.mobileMove.y
    const rightInput = (this.keys.has('KeyD') || this.keys.has('ArrowRight') ? 1 : 0)
      - (this.keys.has('KeyA') || this.keys.has('ArrowLeft') ? 1 : 0) + this.mobileMove.x
    const forward = new Vector3(-Math.sin(this.cameraYaw), 0, -Math.cos(this.cameraYaw))
    const right = new Vector3(Math.cos(this.cameraYaw), 0, -Math.sin(this.cameraYaw))
    this.moveDirection.copyFrom(forward.scale(forwardInput).add(right.scale(rightInput)))
    const inputLength = Math.min(1, this.moveDirection.length())
    if (inputLength > 0.05) this.moveDirection.normalize()

    const sprinting = (this.keys.has('ShiftLeft') || this.keys.has('ShiftRight')) && inputLength > 0.1 && this.stamina > 1 && this.dodgeTime <= 0
    let speed = sprinting ? 6.8 : 4.35
    if (sprinting) this.stamina = Math.max(0, this.stamina - dt * 17)
    else if (this.dodgeTime <= 0) this.stamina = Math.min(100, this.stamina + dt * (this.nearWarmth() ? 25 : 18))
    if (this.dodgeTime > 0) {
      speed = 13.5
      this.moveDirection.copyFrom(this.dodgeDirection)
    }

    if (inputLength > 0.05 || this.dodgeTime > 0) {
      this.player.root.position.addInPlace(this.moveDirection.scale(speed * dt))
      const targetRotation = Math.atan2(this.moveDirection.x, this.moveDirection.z)
      let delta = targetRotation - this.player.root.rotation.y
      delta = Math.atan2(Math.sin(delta), Math.cos(delta))
      this.player.root.rotation.y += delta * Math.min(1, dt * 14)
      this.player.play(this.dodgeTime > 0 || sprinting ? 'Run' : 'Walk', sprinting ? 1.2 : 1.15)
    } else {
      this.player.play('Idle', 1)
    }
    this.resolvePlayerCollisions()
    this.player.root.position.y = islandHeight(this.player.root.position.x, this.player.root.position.z)

    if (this.attackAnimation > 0) {
      const progress = 1 - this.attackAnimation / 0.32
      this.weapon.rotation.x = -Math.sin(progress * Math.PI) * 1.9
      this.weapon.rotation.z = -0.18 - Math.sin(progress * Math.PI) * 0.5
    } else {
      this.weapon.rotation.x += (0 - this.weapon.rotation.x) * Math.min(1, dt * 18)
      this.weapon.rotation.z += (-0.18 - this.weapon.rotation.z) * Math.min(1, dt * 18)
    }
    if (this.dodgeTime > 0) this.player.content.rotation.z = Math.sin((1 - this.dodgeTime / 0.38) * Math.PI) * -0.65
    else this.player.content.rotation.z += (0 - this.player.content.rotation.z) * Math.min(1, dt * 14)

    const warm = this.nearWarmth()
    if (warm) this.warmth = Math.min(100, this.warmth + dt * (this.nearLitCampfire() ? 12 : 4))
    else if (this.phase === 'day' && this.finalCharge === null) this.warmth = Math.min(100, this.warmth + dt * 2.2)
    else this.warmth = Math.max(0, this.warmth - dt * (this.finalCharge !== null ? 1.65 : 1.25))
    this.freezeFeedback -= dt
    if (this.warmth <= 0) {
      this.health -= dt * 4
      if (this.freezeFeedback <= 0) {
        this.freezeFeedback = 1
        this.callbacks.onDamage()
        this.callbacks.onNotify('Переохлаждение! Найдите костёр или вернитесь к маяку.', 'danger')
      }
      if (this.health <= 0) this.finish(false, 'Холод погасил последнюю искру. Ставьте костры и запасайтесь дровами.')
    }
  }

  private resolvePlayerCollisions(): void {
    const position = this.player.root.position
    const radius = Math.hypot(position.x, position.z)
    if (radius > 25.5) {
      position.x *= 25.5 / radius
      position.z *= 25.5 / radius
    }
    this.pushOut(position, this.beacon.root.position, 3.05)
    for (const building of this.buildings) {
      const collisionRadius = building.kind === 'barricade' ? 1.25 : 0.72
      this.pushOut(position, building.root.position, collisionRadius)
    }
    for (const node of this.resources) {
      if (node.type === 'wood' && node.amount > 0) this.pushOut(position, node.root.position, 0.58)
    }
  }

  private pushOut(position: Vector3, center: Vector3, radius: number): void {
    const dx = position.x - center.x
    const dz = position.z - center.z
    const distance = Math.hypot(dx, dz)
    if (distance > 0.001 && distance < radius) {
      position.x = center.x + dx / distance * radius
      position.z = center.z + dz / distance * radius
    }
  }

  private updateSpawning(dt: number): void {
    if (this.phase !== 'night') return
    this.spawnTimer -= dt
    const cap = this.finalCharge !== null ? 14 : Math.min(5 + this.day * 2, 11)
    if (this.spawnTimer <= 0 && this.enemies.length < cap) {
      this.spawnEnemy()
      const base = this.finalCharge !== null ? 1.35 : Math.max(2.15, 4 - this.day * 0.28)
      this.spawnTimer = base * (0.8 + Math.random() * 0.45)
    }
  }

  private spawnEnemy(): void {
    const roll = Math.random()
    const kind: Enemy['kind'] = (this.day >= 2 || this.finalCharge !== null) && roll > 0.8
      ? 'brute' : roll > 0.48 ? 'runner' : 'walker'
    const height = kind === 'brute' ? 1.45 : kind === 'runner' ? 0.88 : 1.08
    const model = this.models.create('fox', height, true, true)
    const angle = Math.random() * Math.PI * 2
    const radius = 22 + Math.random() * 1.3
    model.root.position.set(Math.sin(angle) * radius, 0, Math.cos(angle) * radius)
    model.root.position.y = islandHeight(model.root.position.x, model.root.position.z)
    model.root.rotation.y = angle + Math.PI
    const tint = kind === 'brute' ? Color3.FromHexString('#7a5145') : kind === 'runner' ? Color3.FromHexString('#566a78') : Color3.FromHexString('#616b67')
    model.content.getChildMeshes(false).forEach((mesh) => {
      const materials = mesh.material ? [mesh.material] : []
      materials.forEach((material) => {
        if (material instanceof PBRMaterial) {
          material.albedoColor = tint
          material.emissiveColor = kind === 'runner' ? new Color3(0.04, 0.09, 0.12) : new Color3(0.1, 0.035, 0.015)
          material.emissiveIntensity = 0.6
          material.roughness = 0.88
        }
      })
    })
    if (kind === 'brute') {
      const armour = MeshBuilder.CreateBox('brute salvage armour', { width: 0.95, height: 0.25, depth: 0.8 }, this.scene)
      armour.parent = model.root
      armour.position.y = 1.12
      armour.material = this.materials.rust
      setShadows(armour, this.environment.shadows)
    }
    const healthBack = MeshBuilder.CreatePlane('enemy health backing', { width: 1.15, height: 0.1 }, this.scene)
    healthBack.parent = model.root
    healthBack.position.y = height + 0.35
    healthBack.billboardMode = Mesh.BILLBOARDMODE_ALL
    const backMaterial = new StandardMaterial('enemy health dark', this.scene)
    backMaterial.emissiveColor = Color3.FromHexString('#11181b')
    backMaterial.disableLighting = true
    healthBack.material = backMaterial
    const healthFill = MeshBuilder.CreatePlane('enemy health fill', { width: 1.1, height: 0.065 }, this.scene)
    healthFill.parent = model.root
    healthFill.position.y = height + 0.35
    healthFill.billboardMode = Mesh.BILLBOARDMODE_ALL
    const fillMaterial = new StandardMaterial('enemy health red', this.scene)
    fillMaterial.emissiveColor = Color3.FromHexString('#df7057')
    fillMaterial.disableLighting = true
    healthFill.material = fillMaterial
    healthBack.isVisible = false
    healthFill.isVisible = false
    const stats = kind === 'brute'
      ? { health: 145, speed: 1.35, damage: 17 }
      : kind === 'runner' ? { health: 42, speed: 3.25, damage: 7 } : { health: 68, speed: 1.9, damage: 9 }
    this.enemies.push({
      kind, model, health: stats.health, maxHealth: stats.health, speed: stats.speed, damage: stats.damage,
      attackCooldown: 0.7 + Math.random() * 0.5, aggro: 0, windup: 0, windupTotal: 0,
      targetKind: 'beacon', healthFill, healthBack, fireTick: 0,
    })
    spawnBurst(this.scene, model.root.position.add(new Vector3(0, 0.6, 0)), Color3.FromHexString('#6d7780'), 10)
  }

  private updateEnemies(dt: number): void {
    for (let index = this.enemies.length - 1; index >= 0; index -= 1) {
      const enemy = this.enemies[index]
      if (!enemy) continue
      enemy.attackCooldown -= dt
      enemy.aggro = Math.max(0, enemy.aggro - dt)
      enemy.fireTick -= dt
      const position = enemy.model.root.position
      const playerDistance = distance2D(position, this.player.root.position)
      let targetKind: Enemy['targetKind'] = playerDistance < (enemy.kind === 'runner' ? 7 : 5.5) || enemy.aggro > 0 ? 'player' : 'beacon'
      let targetBuilding: Building | undefined
      let closestBuildingDistance = 3.3
      for (const building of this.buildings) {
        if (building.kind === 'campfire' && (building.fuel ?? 0) > 0) continue
        const distance = distance2D(position, building.root.position)
        if (distance < closestBuildingDistance) {
          closestBuildingDistance = distance
          targetBuilding = building
        }
      }
      if (targetBuilding) targetKind = 'building'
      enemy.targetKind = targetKind
      enemy.targetBuilding = targetBuilding
      const targetPosition = targetKind === 'player' ? this.player.root.position
        : targetKind === 'building' && targetBuilding ? targetBuilding.root.position : this.beacon.root.position
      const direction = targetPosition.subtract(position)
      direction.y = 0
      const distance = direction.length()
      if (distance > 0.001) direction.normalize()
      const stopDistance = targetKind === 'beacon' ? 2.8 : targetKind === 'building' ? 1.55 : 1.45

      if (enemy.windup > 0) {
        enemy.windup -= dt
        enemy.model.play('Survey', 1.4, true)
        enemy.model.content.rotation.x = -Math.sin((1 - enemy.windup / enemy.windupTotal) * Math.PI) * 0.28
        if (enemy.windup <= 0) {
          enemy.model.content.rotation.x = 0
          const stillClose = distance2D(position, targetPosition) < stopDistance + 0.85
          if (stillClose) {
            if (targetKind === 'player') this.damagePlayer(enemy.damage)
            else if (targetKind === 'building' && targetBuilding) this.damageBuilding(targetBuilding, enemy.damage)
            else this.damageBeacon(Math.ceil(enemy.damage * 0.48))
          }
          enemy.attackCooldown = enemy.kind === 'runner' ? 0.85 : enemy.kind === 'brute' ? 1.55 : 1.2
          if (this.state !== 'playing') return
        }
      } else if (distance > stopDistance) {
        let speed = enemy.speed
        if (enemy.kind === 'runner' && targetKind === 'player' && distance > 3 && distance < 7) speed *= 1.35
        const steering = direction.clone()
        for (const fire of this.buildings) {
          if (fire.kind !== 'campfire' || (fire.fuel ?? 0) <= 0) continue
          const fireDistance = distance2D(position, fire.root.position)
          if (fireDistance < 3.6 && fireDistance > 0.05) {
            const away = position.subtract(fire.root.position)
            away.y = 0
            steering.addInPlace(away.normalize().scale((3.6 - fireDistance) * 0.7))
          }
        }
        steering.normalize()
        position.addInPlace(steering.scale(speed * dt))
        enemy.model.root.rotation.y = Math.atan2(steering.x, steering.z)
        enemy.model.play('Run', Math.max(0.7, speed / 2.2))
      } else if (enemy.attackCooldown <= 0) {
        enemy.windupTotal = enemy.kind === 'brute' ? 0.62 : enemy.kind === 'runner' ? 0.24 : 0.4
        enemy.windup = enemy.windupTotal
      } else {
        enemy.model.play('Survey', 0.8)
      }

      for (const other of this.enemies) {
        if (other === enemy) continue
        const dx = position.x - other.model.root.position.x
        const dz = position.z - other.model.root.position.z
        const separation = Math.hypot(dx, dz)
        if (separation > 0.001 && separation < 0.85) {
          position.x += dx / separation * dt * 0.9
          position.z += dz / separation * dt * 0.9
        }
      }
      position.y = islandHeight(position.x, position.z)
      const ratio = Math.max(0, enemy.health / enemy.maxHealth)
      enemy.healthBack.isVisible = ratio < 1
      enemy.healthFill.isVisible = ratio < 1
      enemy.healthFill.scaling.x = ratio
      enemy.healthFill.position.x = -(1 - ratio) * 0.55
      if (enemy.fireTick <= 0) {
        const nearFire = this.buildings.some((building) => building.kind === 'campfire' && (building.fuel ?? 0) > 0
          && distance2D(position, building.root.position) < 2.4)
        if (nearFire) {
          enemy.fireTick = 0.5
          this.damageEnemy(enemy, 3, false)
        }
      }
    }
  }

  private damagePlayer(amount: number): void {
    if (this.hurtCooldown > 0 || this.invulnerableTime > 0 || this.state !== 'playing') return
    this.hurtCooldown = 0.25
    this.health = Math.max(0, this.health - amount)
    this.cameraShake = Math.min(0.35, this.cameraShake + 0.14)
    this.callbacks.onDamage()
    this.sound.play('hurt')
    spawnBurst(this.scene, this.player.root.position.add(new Vector3(0, 1, 0)), Color3.FromHexString('#c75e4b'), 8)
    if (this.health <= 0) this.finish(false, 'Пепельные прорвали оборону. Уклоняйтесь от замахов и используйте баррикады.')
  }

  private damageBeacon(amount: number): void {
    this.beaconHealth = Math.max(0, this.beaconHealth - amount)
    this.cameraShake = Math.min(0.3, this.cameraShake + 0.08)
    this.sound.play('hit')
    spawnBurst(this.scene, this.beacon.root.position.add(new Vector3(0, 1.3, 0)), Color3.FromHexString('#d69b61'), 8)
    if (this.beaconHealth <= 0) this.finish(false, 'Маяк разрушен. Без его света остров окончательно погрузился во тьму.')
  }

  private damageBuilding(building: Building, amount: number): void {
    building.health -= amount
    spawnBurst(this.scene, building.root.position.add(new Vector3(0, 0.7, 0)), Color3.FromHexString('#b58b5f'), 6)
    if (building.health > 0) return
    const index = this.buildings.indexOf(building)
    if (index >= 0) this.buildings.splice(index, 1)
    building.particles?.dispose()
    building.root.dispose()
    this.callbacks.onNotify(building.kind === 'barricade' ? 'Баррикада разрушена!' : building.kind === 'turret' ? 'Турель уничтожена!' : 'Костёр разрушен!', 'danger')
  }

  private damageEnemy(enemy: Enemy, amount: number, reward = true): void {
    if (!this.enemies.includes(enemy)) return
    enemy.health -= amount
    enemy.aggro = 5
    spawnBurst(this.scene, enemy.model.root.position.add(new Vector3(0, 0.65, 0)), Color3.FromHexString('#d28b64'), 6)
    if (enemy.health > 0) return
    const index = this.enemies.indexOf(enemy)
    if (index >= 0) this.enemies.splice(index, 1)
    spawnBurst(this.scene, enemy.model.root.position.add(new Vector3(0, 0.7, 0)), Color3.FromHexString('#778b8e'), 20)
    enemy.model.dispose()
    if (reward) {
      this.kills += 1
      const salvage = enemy.kind === 'brute' ? 2 : 1
      this.inventory.scrap += salvage
      this.callbacks.onLoot(`+${salvage} металла · трофей`, 'scrap')
      if (enemy.kind === 'brute') {
        this.inventory.crystal += 1
        this.callbacks.onLoot('+1 осколок · броня', 'crystal')
      }
    }
  }

  private updateBuildings(dt: number): void {
    for (const building of this.buildings) {
      building.cooldown -= dt
      if (building.kind === 'turret' && building.turret) {
        let target: Enemy | undefined
        let bestDistance = 11
        for (const enemy of this.enemies) {
          const distance = distance2D(building.root.position, enemy.model.root.position)
          if (distance < bestDistance) {
            bestDistance = distance
            target = enemy
          }
        }
        if (target) {
          const direction = target.model.root.position.subtract(building.root.position)
          building.turret.head.rotation.y = Math.atan2(direction.x, direction.z) - building.root.rotation.y
          if (building.cooldown <= 0) {
            building.cooldown = 0.58
            const from = building.turret.muzzle.getAbsolutePosition().clone()
            const to = target.model.root.position.add(new Vector3(0, 0.65, 0))
            spawnTracer(this.scene, from, to)
            spawnBurst(this.scene, from, Color3.FromHexString('#ffd37a'), 4)
            this.damageEnemy(target, 19)
            this.sound.play('turret')
          }
        }
      } else if (building.kind === 'campfire') {
        building.fuel = Math.max(0, (building.fuel ?? 0) - dt)
        const lit = (building.fuel ?? 0) > 0
        if (building.light) building.light.intensity = lit ? 30 + Math.sin(this.worldTime * 13) * 6 : 0
        building.flames?.forEach((flame, index) => {
          flame.isVisible = lit
          flame.scaling.y = 1.5 + Math.sin(this.worldTime * (8 + index) + index) * 0.45
          flame.scaling.x = 0.85 + Math.sin(this.worldTime * 11 + index * 2) * 0.15
        })
        if (!lit && building.particles) building.particles.stop()
      }
    }
  }

  private installCore(): void {
    if (!this.canAfford(CORE_COST)) {
      this.callbacks.onNotify(`Для ячейки: ${this.missingResources(CORE_COST).toLowerCase()}`, 'danger')
      return
    }
    this.pay(CORE_COST)
    this.cores += 1
    this.beaconHealth = Math.min(100, this.beaconHealth + 10)
    this.sound.play('core')
    this.callbacks.onAnnounce(`ЯЧЕЙКА ${this.cores} / 3`, 'ЭНЕРГИЯ ВОЗРАСТАЕТ', this.cores < 3 ? 'Соберите ресурсы для следующей ячейки.' : 'Полный заряд. Начинается финальный штурм!')
    spawnBurst(this.scene, this.beacon.root.position.add(new Vector3(0, 5.5, 0)), Color3.FromHexString('#8affe1'), 32)
    this.updateCoreSockets()
    if (this.cores === 3) {
      this.finalCharge = this.finalChargeDuration
      this.phase = 'night'
      this.phaseDuration = this.finalChargeDuration
      this.phaseTime = this.finalChargeDuration
      this.spawnTimer = 0.2
      this.sound.setNight(true)
      for (let i = 0; i < 4; i += 1) this.spawnEnemy()
    }
    this.emitSnapshot()
  }

  private updateResourceVisuals(dt: number): void {
    for (const node of this.resources) {
      node.hit = Math.max(0, node.hit - dt * 3.5)
      const pulse = Math.sin(node.hit * Math.PI) * 0.09
      node.root.scaling.setAll(node.baseScale + pulse)
      if (node.type === 'crystal') node.root.rotation.y += dt * 0.16
      if (node.type === 'wood') node.root.rotation.z = Math.sin(node.hit * 20) * node.hit * 0.025
    }
  }

  private updateBeaconVisuals(dt: number): void {
    const power = this.cores / 3
    this.beacon.rings.forEach((ring, index) => { ring.rotation.y += dt * (0.3 + index * 0.16) })
    this.beacon.lens.scaling.setAll(1 + Math.sin(this.worldTime * 3.5) * 0.035)
    this.beacon.light.intensity = 55 + power * 95 + Math.sin(this.worldTime * 4) * 5
    this.beacon.light.range = 18 + power * 15
    this.beacon.beam.isVisible = this.cores > 0
    const beamMaterial = this.beacon.beam.material
    if (beamMaterial) beamMaterial.alpha = this.finalCharge !== null || this.state === 'victory' ? 0.11 : 0.018 + power * 0.025
    this.beacon.beam.rotation.y += dt * 0.08
  }

  private updateCoreSockets(): void {
    this.beacon.socketLights.forEach((diode, index) => {
      diode.material = index < this.cores ? this.materials.lens : this.materials.darkSteel
    })
  }

  private updateCamera(dt: number): void {
    const target = this.player.root.position.add(new Vector3(0, 1.25, 0))
    this.environment.updateCamera(dt, target, this.cameraYaw, this.cameraPitch, this.cameraDistance, this.cameraShake)
  }

  private nearestResource(): ResourceNode | undefined {
    let nearest: ResourceNode | undefined
    let best = 2.8
    for (const node of this.resources) {
      if (node.amount <= 0) continue
      const distance = distance2D(node.root.position, this.player.root.position)
      if (distance < best) {
        best = distance
        nearest = node
      }
    }
    return nearest
  }

  private nearestCampfire(): Building | undefined {
    let nearest: Building | undefined
    let best = 2.65
    for (const building of this.buildings) {
      if (building.kind !== 'campfire') continue
      const distance = distance2D(building.root.position, this.player.root.position)
      if (distance < best) {
        best = distance
        nearest = building
      }
    }
    return nearest
  }

  private nearLitCampfire(): boolean {
    return this.buildings.some((building) => building.kind === 'campfire' && (building.fuel ?? 0) > 0
      && distance2D(building.root.position, this.player.root.position) < 4.6)
  }

  private nearWarmth(): boolean {
    return this.nearLitCampfire() || distance2D(this.player.root.position, this.beacon.root.position) < 4.8
  }

  private nearBeacon(): boolean {
    return distance2D(this.player.root.position, this.beacon.root.position) < 5.1
  }

  private buildPosition(): Vector3 {
    const direction = new Vector3(Math.sin(this.player.root.rotation.y), 0, Math.cos(this.player.root.rotation.y))
    const result = this.player.root.position.add(direction.scale(3.1))
    result.y = islandHeight(result.x, result.z)
    return result
  }

  private canPlaceHere(recipe: 'barricade' | 'turret' | 'campfire'): boolean {
    const limits = { barricade: 10, turret: 5, campfire: 6 }
    if (this.buildings.filter((building) => building.kind === recipe).length >= limits[recipe]) {
      this.callbacks.onNotify('Достигнут предел таких построек на эту смену.', 'info')
      return false
    }
    const position = this.buildPosition()
    if (Math.hypot(position.x, position.z) > 24.5 || distance2D(position, this.beacon.root.position) < 3.7) {
      this.callbacks.onNotify('Здесь нельзя строить: нужна сухая ровная площадка вне маяка.', 'info')
      return false
    }
    if (this.buildings.some((building) => distance2D(position, building.root.position) < 2)) {
      this.callbacks.onNotify('Слишком близко к другой постройке.', 'info')
      return false
    }
    return true
  }

  private canAfford(cost: Inventory): boolean {
    return this.inventory.wood >= cost.wood && this.inventory.scrap >= cost.scrap && this.inventory.crystal >= cost.crystal
  }

  private pay(cost: Inventory): void {
    this.inventory.wood -= cost.wood
    this.inventory.scrap -= cost.scrap
    this.inventory.crystal -= cost.crystal
  }

  private missingResources(cost: Inventory): string {
    const missing: string[] = []
    if (this.inventory.wood < cost.wood) missing.push(`${cost.wood - this.inventory.wood} дерева`)
    if (this.inventory.scrap < cost.scrap) missing.push(`${cost.scrap - this.inventory.scrap} металла`)
    if (this.inventory.crystal < cost.crystal) missing.push(`${cost.crystal - this.inventory.crystal} осколков`)
    return `Не хватает: ${missing.join(', ')}.`
  }

  private getPrompt(): { title: string; detail: string } {
    if (this.nearBeacon() && this.cores < 3) {
      return this.canAfford(CORE_COST)
        ? { title: 'Установить энергоячейку', detail: '6 металла + 4 осколка' }
        : { title: 'Ячейка не собрана', detail: 'Нужно 6 металла и 4 осколка' }
    }
    const fire = this.nearestCampfire()
    if (fire) return { title: (fire.fuel ?? 0) > 0 ? 'Дозаправить костёр' : 'Снова разжечь костёр', detail: `2 дерева · огонь ${Math.ceil(fire.fuel ?? 0)} сек.` }
    const node = this.nearestResource()
    if (node) {
      const labels: Record<ResourceType, string> = { wood: 'Собрать древесину', scrap: 'Разобрать механизм', crystal: 'Добыть осколок' }
      return { title: labels[node.type], detail: `Осталось зарядов: ${node.amount}` }
    }
    if (this.nearBeacon()) return { title: 'Маяк полностью заряжен', detail: 'Держитесь рядом и защищайте свет' }
    return { title: '', detail: '' }
  }

  private emitSnapshot(): void {
    const prompt = this.getPrompt()
    this.callbacks.onSnapshot({
      state: this.state,
      health: this.health,
      stamina: this.stamina,
      warmth: this.warmth,
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
        campfire: this.canAfford(RECIPES.campfire),
        repair: this.canAfford(RECIPES.repair),
        medkit: this.canAfford(RECIPES.medkit),
      },
      nearBeacon: this.nearBeacon(),
      dodgeCooldown: this.dodgeCooldown,
      campfires: this.buildings.filter((building) => building.kind === 'campfire' && (building.fuel ?? 0) > 0).length,
    })
  }

  private finish(victory: boolean, reason: string): void {
    if (this.state !== 'playing') return
    this.state = victory ? 'victory' : 'gameover'
    this.keys.clear()
    this.mobileMove.set(0, 0)
    if (victory) {
      this.beacon.beam.isVisible = true
      this.sound.play('win')
    } else {
      this.sound.play('lose')
    }
    const stats: EndStats = {
      victory, reason, kills: this.kills, gathered: this.gathered,
      elapsed: this.elapsed, day: this.day, cores: this.cores,
    }
    this.callbacks.onState(this.state, stats)
    this.emitSnapshot()
  }

  private resetGame(): void {
    this.keys.clear()
    this.mobileMove.set(0, 0)
    this.draggingPointer = null
    this.dragAttackCandidate = false
    for (const enemy of this.enemies) enemy.model.dispose()
    this.enemies.length = 0
    for (const building of this.buildings) {
      building.particles?.dispose()
      building.root.dispose()
    }
    this.buildings.length = 0
    this.phase = 'day'
    this.day = 1
    this.phaseDuration = 82
    this.phaseTime = 82
    this.finalCharge = null
    this.health = 100
    this.stamina = 100
    this.warmth = 100
    this.beaconHealth = 100
    this.cores = 0
    this.inventory = { wood: 4, scrap: 2, crystal: 0 }
    this.kills = 0
    this.gathered = 0
    this.elapsed = 0
    this.spawnTimer = 0
    this.attackCooldown = 0
    this.attackAnimation = 0
    this.interactCooldown = 0
    this.hurtCooldown = 0
    this.dodgeCooldown = 0
    this.dodgeTime = 0
    this.invulnerableTime = 0
    this.visualNight = 0
    this.player.root.position.set(4.5, islandHeight(4.5, 6.5), 6.5)
    this.player.root.rotation.y = Math.PI
    this.player.root.setEnabled(true)
    this.player.content.rotation.set(0, SOLDIER_FORWARD_OFFSET, 0)
    this.player.play('Idle')
    for (const node of this.resources) {
      node.amount = node.maxAmount
      node.hit = 0
      node.root.rotation.z = 0
      node.root.scaling.setAll(node.baseScale)
      node.root.setEnabled(true)
    }
    this.updateCoreSockets()
    this.beacon.beam.isVisible = false
    this.sound.setNight(false)
  }

  private bindInput(): void {
    window.addEventListener('keydown', (event) => {
      if (['Space', 'ArrowUp', 'ArrowDown', 'ArrowLeft', 'ArrowRight'].includes(event.code)) event.preventDefault()
      this.keys.add(event.code)
      if (event.repeat) return
      if (event.code === 'KeyP' || event.code === 'Escape') this.togglePause()
      if (this.state !== 'playing') return
      if (event.code === 'KeyE') this.interact()
      if (event.code === 'KeyF') this.attack()
      if (event.code === 'Space' || event.code === 'KeyQ') this.dodge()
      if (event.code === 'Digit1') this.craft('barricade')
      if (event.code === 'Digit2') this.craft('turret')
      if (event.code === 'Digit3') this.craft('campfire')
      if (event.code === 'Digit4') this.craft('repair')
      if (event.code === 'Digit5') this.craft('medkit')
    })
    window.addEventListener('keyup', (event) => this.keys.delete(event.code))
    window.addEventListener('blur', () => {
      this.keys.clear()
      if (this.state === 'playing') this.pause()
    })
    this.canvas.addEventListener('pointerdown', (event) => {
      if (this.state !== 'playing') return
      this.draggingPointer = event.pointerId
      this.dragX = event.clientX
      this.dragY = event.clientY
      this.dragDistance = 0
      this.dragAttackCandidate = event.button === 0 && event.pointerType === 'mouse'
      this.canvas.setPointerCapture(event.pointerId)
    })
    this.canvas.addEventListener('pointermove', (event) => {
      if (event.pointerId !== this.draggingPointer || this.state !== 'playing') return
      const dx = event.clientX - this.dragX
      const dy = event.clientY - this.dragY
      this.dragDistance += Math.abs(dx) + Math.abs(dy)
      this.dragX = event.clientX
      this.dragY = event.clientY
      this.cameraYaw -= dx * 0.005
      this.cameraPitch = Math.max(0.12, Math.min(1.05, this.cameraPitch + dy * 0.004))
    })
    const endDrag = (event: PointerEvent, allowAttack: boolean): void => {
      if (event.pointerId !== this.draggingPointer) return
      if (allowAttack && this.dragAttackCandidate && this.dragDistance < 6 && this.state === 'playing') this.attack()
      this.draggingPointer = null
      this.dragAttackCandidate = false
    }
    this.canvas.addEventListener('pointerup', (event) => endDrag(event, true))
    this.canvas.addEventListener('pointercancel', (event) => endDrag(event, false))
    this.canvas.addEventListener('lostpointercapture', (event) => endDrag(event, false))
    this.canvas.addEventListener('wheel', (event) => {
      event.preventDefault()
      this.cameraDistance = Math.max(6.5, Math.min(18, this.cameraDistance + event.deltaY * 0.01))
    }, { passive: false })
    this.canvas.addEventListener('contextmenu', (event) => event.preventDefault())
  }
}
