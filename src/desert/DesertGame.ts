import {
  Bone,
  Color3,
  Engine,
  Matrix,
  Scene,
  Space,
  TransformNode,
  Vector2,
  Vector3,
} from '@babylonjs/core'
import {
  ModelInstance,
  spawnBurst,
  spawnSlideDust,
  spawnTracer,
} from '../game/models'
import { Soundscape } from '../game/Soundscape'
import type { RelayVisual, SentryVisual } from './DesertModels'
import { DesertWorld, desertHeight, type DesertResource } from './DesertWorld'
import type { DesertCallbacks, DesertEndStats, DesertSnapshot, DesertState } from './types'

const DAY_DURATION = 180
const SIGNAL_DURATION = 45
const DODGE_DURATION = 0.58
const ATTACK_DURATION = 0.72
const SOLDIER_FORWARD_OFFSET = Math.PI
const RELAY_SCRAP = 12
const RELAY_FIBER = 6

function clamp01(value: number): number {
  return Math.max(0, Math.min(1, value))
}

function smoothstep(edge0: number, edge1: number, value: number): number {
  const t = clamp01((value - edge0) / (edge1 - edge0))
  return t * t * (3 - 2 * t)
}

function distance2D(a: Vector3, b: Vector3): number {
  return Math.hypot(a.x - b.x, a.z - b.z)
}

function dodgeEnvelope(progress: number): number {
  return smoothstep(0, 0.14, progress) * (1 - smoothstep(0.7, 1, progress))
}

interface DesertEnemy {
  model: ModelInstance
  health: number
  speed: number
  attackCooldown: number
  hitTime: number
}

interface ActiveSentry extends SentryVisual {
  cooldown: number
}

export class DesertGame {
  readonly sound = new Soundscape()
  readonly ready: Promise<void>
  private readonly engine: Engine
  private readonly scene: Scene
  private readonly world: DesertWorld
  private player!: ModelInstance
  private relay!: RelayVisual
  private weapon!: TransformNode
  private weaponHand: Bone | null = null
  private weaponHandAnchor: TransformNode | null = null
  private readonly actionBones = new Map<string, Bone>()
  private readonly weaponRootInverse = Matrix.Identity()
  private readonly weaponHandWorld = new Vector3()
  private playerContentBaseY = 0
  private readonly enemies: DesertEnemy[] = []
  private readonly sentries: ActiveSentry[] = []
  private readonly keys = new Set<string>()
  private readonly mobileMove = new Vector2(0, 0)
  private readonly moveDirection = new Vector3(0, 0, 1)
  private readonly dodgeDirection = new Vector3(0, 0, 1)
  private isReady = false

  private state: DesertState = 'menu'
  private health = 100
  private stamina = 100
  private hydration = 100
  private scrap = 2
  private fiber = 1
  private water = 2
  private kills = 0
  private elapsed = 0
  private distanceTravelled = 0
  private spawnTimer = 8
  private hurtCooldown = 0
  private interactCooldown = 0
  private attackCooldown = 0
  private attackTime = -1
  private attackHit = false
  private dodgeCooldown = 0
  private dodgeTime = 0
  private invulnerableTime = 0
  private relayActive = false
  private signalTime: number | null = null
  private cameraYaw = 0.35
  private cameraPitch = 0.3
  private cameraDistance = 12
  private draggingPointer: number | null = null
  private dragX = 0
  private dragY = 0
  private dragDistance = 0
  private dragAttackCandidate = false
  private snapshotTimer = 0

  constructor(private readonly canvas: HTMLCanvasElement, private readonly callbacks: DesertCallbacks) {
    this.engine = new Engine(canvas, true, {
      stencil: true,
      powerPreference: 'high-performance',
      doNotHandleContextLost: false,
    }, true)
    this.engine.setHardwareScalingLevel(1 / Math.min(window.devicePixelRatio || 1, 1.5))
    this.scene = new Scene(this.engine)
    this.scene.skipPointerMovePicking = true
    this.scene.constantlyUpdateMeshUnderPointer = false
    this.world = new DesertWorld(this.scene)
    this.bindInput()
    this.ready = this.initialize()
    this.engine.runRenderLoop(() => this.frame())
    window.addEventListener('resize', () => this.engine.resize())
  }

  private async initialize(): Promise<void> {
    await this.world.load()
    this.player = this.world.library.create('soldier', 1.86)
    this.player.root.name = 'desert survivor'
    this.player.content.rotation.y = SOLDIER_FORWARD_OFFSET
    this.playerContentBaseY = this.player.content.position.y
    const poseBones = [
      'Spine1', 'Spine2', 'Head', 'LeftArm', 'RightArm', 'LeftForeArm', 'RightForeArm',
      'LeftUpLeg', 'LeftLeg', 'RightUpLeg', 'RightLeg',
    ]
    this.player.entries.skeletons.flatMap((skeleton) => skeleton.bones).forEach((bone) => {
      const name = poseBones.find((fragment) => bone.name.endsWith(fragment))
      if (name) this.actionBones.set(name, bone)
    })
    this.weapon = this.world.models.spear()
    this.weapon.parent = this.player.root
    this.weaponHand = this.player.entries.skeletons.flatMap((skeleton) => skeleton.bones)
      .find((bone) => bone.name.endsWith('RightHand')) ?? null
    this.weaponHandAnchor = this.player.content.getChildMeshes()
      .find((mesh) => this.weaponHand !== null && mesh.skeleton?.bones.includes(this.weaponHand)) ?? null
    this.scene.onAfterAnimationsObservable.add(() => {
      this.applyActionPose()
      this.syncWeaponGrip()
    })
    this.relay = this.world.models.relay()
    this.relay.root.position.set(0, desertHeight(0, 0), 0)
    this.isReady = true
    this.resetGame(false)
    this.state = 'menu'
    this.emitSnapshot()
  }

  private syncWeaponGrip(): void {
    if (!this.weaponHand || !this.weaponHandAnchor) return
    this.player.root.computeWorldMatrix(true)
    this.weaponHandAnchor.computeWorldMatrix(true)
    this.weaponHand.computeAbsoluteMatrices()
    this.weaponHand.getAbsolutePositionToRef(this.weaponHandAnchor, this.weaponHandWorld)
    this.weaponRootInverse.copyFrom(this.player.root.getWorldMatrix())
    this.weaponRootInverse.invert()
    Vector3.TransformCoordinatesToRef(this.weaponHandWorld, this.weaponRootInverse, this.weapon.position)
    if (this.attackTime >= 0) {
      const progress = clamp01(this.attackTime / ATTACK_DURATION)
      const thrust = smoothstep(0.25, 0.42, progress) * (1 - smoothstep(0.64, 1, progress))
      this.weapon.position.z += thrust * 0.72
    }
  }

  private applyActionPose(): void {
    const yaw = this.player.root.rotation.y
    const forward = new Vector3(Math.sin(yaw), 0, Math.cos(yaw))
    const right = Vector3.Cross(Vector3.Up(), forward).normalize()
    const rotate = (name: string, axis: Vector3, angle: number): void => {
      if (Math.abs(angle) > 0.0001) this.actionBones.get(name)?.rotate(axis, angle, Space.WORLD)
    }
    if (this.dodgeTime > 0) {
      const progress = clamp01(1 - this.dodgeTime / DODGE_DURATION)
      const weight = dodgeEnvelope(progress)
      rotate('Spine1', right, 0.22 * weight)
      rotate('Spine2', right, 0.3 * weight)
      rotate('Head', right, -0.3 * weight)
      rotate('LeftArm', right, -0.55 * weight)
      rotate('RightArm', right, 0.45 * weight)
      rotate('LeftUpLeg', right, -0.95 * weight)
      rotate('LeftLeg', right, 0.18 * weight)
      rotate('RightUpLeg', right, 0.15 * weight)
      rotate('RightLeg', right, 1.25 * weight)
      return
    }
    if (this.attackTime < 0) return
    const progress = clamp01(this.attackTime / ATTACK_DURATION)
    const windup = smoothstep(0, 0.25, progress) * (1 - smoothstep(0.3, 0.43, progress))
    const thrust = smoothstep(0.25, 0.42, progress) * (1 - smoothstep(0.64, 1, progress))
    rotate('RightArm', right, 0.48 * windup - 1.12 * thrust)
    rotate('RightForeArm', right, 0.32 * windup - 0.62 * thrust)
    rotate('LeftArm', right, -0.38 * thrust)
    rotate('LeftForeArm', right, -0.32 * thrust)
    rotate('Spine1', right, 0.28 * thrust - 0.08 * windup)
    rotate('Spine2', right, 0.16 * thrust)
    rotate('Spine2', Vector3.Up(), -0.2 * windup + 0.28 * thrust)
    rotate('Head', right, -0.12 * thrust)
  }

  start(): void {
    if (!this.isReady) return
    this.sound.unlock()
    this.scene.animationsEnabled = true
    this.resetGame()
    this.state = 'playing'
    this.callbacks.onState('playing')
    this.callbacks.onAnnounce('ЭКСПЕДИЦИЯ 01', 'ВОССТАНОВИТЕ СВЯЗЬ', 'Соберите 12 обломков и 6 волокна. Возвращайтесь к вышке и нажмите E.')
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

  private resetGame(resetWorld = true): void {
    this.keys.clear()
    this.mobileMove.set(0, 0)
    this.draggingPointer = null
    this.dragAttackCandidate = false
    this.enemies.forEach((enemy) => enemy.model.dispose())
    this.enemies.length = 0
    this.sentries.forEach((sentry) => sentry.root.dispose(false, false))
    this.sentries.length = 0
    this.health = 100
    this.stamina = 100
    this.hydration = 100
    this.scrap = 2
    this.fiber = 1
    this.water = 2
    this.kills = 0
    this.elapsed = 0
    this.distanceTravelled = 0
    this.spawnTimer = 8
    this.hurtCooldown = 0
    this.interactCooldown = 0
    this.attackCooldown = 0
    this.attackTime = -1
    this.attackHit = false
    this.dodgeCooldown = 0
    this.dodgeTime = 0
    this.invulnerableTime = 0
    this.relayActive = false
    this.signalTime = null
    this.relay.setActive(false)
    this.player.root.position.set(6, desertHeight(6, 8), 8)
    this.player.root.rotation.y = Math.PI
    this.player.content.position.y = this.playerContentBaseY
    this.player.content.rotation.set(0, SOLDIER_FORWARD_OFFSET, 0)
    this.player.play('Idle')
    if (resetWorld) this.world.reset(this.player.root.position)
    this.updateCamera(1)
    this.emitSnapshot()
  }

  private frame(): void {
    const dt = Math.min(this.engine.getDeltaTime() / 1000 || 0.016, 0.05)
    if (!this.isReady) {
      this.world.menuCamera(dt)
      this.world.update(dt, 0.12, Vector3.Zero())
      this.scene.render()
      return
    }
    if (this.state === 'playing') this.update(dt)
    else if (this.state === 'menu') {
      this.world.menuCamera(dt)
      this.world.update(dt, 0.12, this.player.root.position)
    }
    this.scene.render()
  }

  private update(dt: number): void {
    this.elapsed += dt
    this.attackCooldown = Math.max(0, this.attackCooldown - dt)
    this.interactCooldown = Math.max(0, this.interactCooldown - dt)
    this.hurtCooldown = Math.max(0, this.hurtCooldown - dt)
    this.dodgeCooldown = Math.max(0, this.dodgeCooldown - dt)
    this.dodgeTime = Math.max(0, this.dodgeTime - dt)
    this.invulnerableTime = Math.max(0, this.invulnerableTime - dt)
    if (this.attackTime >= 0) {
      this.attackTime += dt
      if (!this.attackHit && this.attackTime / ATTACK_DURATION >= 0.42) {
        this.attackHit = true
        this.performStrike()
      }
      if (this.attackTime >= ATTACK_DURATION) this.attackTime = -1
    }
    if (this.signalTime !== null) {
      this.signalTime = Math.max(0, this.signalTime - dt)
      if (this.signalTime <= 0) {
        this.finish(true, 'Сигнал принят. Караван нашёл вас в открытой пустыне.')
        return
      }
    }
    this.updateCamera(dt)
    this.updatePlayer(dt)
    this.updateSpawning(dt)
    this.updateEnemies(dt)
    if (this.state !== 'playing') return
    this.updateSentries(dt)
    this.world.update(dt, this.dayProgress(), this.player.root.position)
    this.snapshotTimer -= dt
    if (this.snapshotTimer <= 0) {
      this.snapshotTimer = 0.1
      this.emitSnapshot()
    }
  }

  private dayProgress(): number {
    return (this.elapsed / DAY_DURATION + 0.12) % 1
  }

  private updateCamera(dt: number): void {
    const target = this.player.root.position.add(new Vector3(0, 1.25, 0))
    this.world.updateCamera(dt, target, this.cameraYaw, this.cameraPitch, this.cameraDistance)
  }

  private updatePlayer(dt: number): void {
    const forwardInput = (this.keys.has('KeyW') || this.keys.has('ArrowUp') ? 1 : 0)
      - (this.keys.has('KeyS') || this.keys.has('ArrowDown') ? 1 : 0) + this.mobileMove.y
    const rightInput = (this.keys.has('KeyD') || this.keys.has('ArrowRight') ? 1 : 0)
      - (this.keys.has('KeyA') || this.keys.has('ArrowLeft') ? 1 : 0) + this.mobileMove.x
    const forward = this.world.camera.target.subtract(this.world.camera.position)
    forward.y = 0
    if (forward.lengthSquared() < 0.0001) forward.set(-Math.cos(this.cameraYaw), 0, -Math.sin(this.cameraYaw))
    else forward.normalize()
    const right = Vector3.Cross(Vector3.Up(), forward).normalize()
    this.moveDirection.copyFrom(forward.scale(forwardInput).add(right.scale(rightInput)))
    const inputLength = Math.min(1, this.moveDirection.length())
    if (inputLength > 0.05) this.moveDirection.normalize()

    const sprinting = (this.keys.has('ShiftLeft') || this.keys.has('ShiftRight')) && inputLength > 0.1 && this.stamina > 1 && this.dodgeTime <= 0
    let speed = sprinting ? 7 : 4.45
    if (this.attackTime >= 0) speed *= 0.68
    if (sprinting) this.stamina = Math.max(0, this.stamina - dt * 17)
    else if (this.dodgeTime <= 0) this.stamina = Math.min(100, this.stamina + dt * 19)
    if (this.dodgeTime > 0) {
      const progress = clamp01(1 - this.dodgeTime / DODGE_DURATION)
      speed = 4.45 + 11.2 * (1 - progress) ** 2
      this.moveDirection.copyFrom(this.dodgeDirection)
    }
    const before = this.player.root.position.clone()
    if (inputLength > 0.05 || this.dodgeTime > 0) {
      this.player.root.position.addInPlace(this.moveDirection.scale(speed * dt))
      const targetRotation = Math.atan2(this.moveDirection.x, this.moveDirection.z)
      let delta = targetRotation - this.player.root.rotation.y
      delta = Math.atan2(Math.sin(delta), Math.cos(delta))
      this.player.root.rotation.y += delta * Math.min(1, dt * 14)
      this.player.play(this.dodgeTime > 0 || this.attackTime >= 0 ? 'Idle' : sprinting ? 'Run' : 'Walk', sprinting ? 1.2 : 1.12)
    } else {
      this.player.play('Idle', 1)
    }
    if (this.attackTime >= 0) {
      const progress = clamp01(this.attackTime / ATTACK_DURATION)
      if (progress > 0.16 && progress < 0.46) {
        const lunge = new Vector3(Math.sin(this.player.root.rotation.y), 0, Math.cos(this.player.root.rotation.y))
        this.player.root.position.addInPlace(lunge.scale(dt * 2.1))
      }
    }
    this.resolvePlayerCollisions()
    this.player.root.position.y = desertHeight(this.player.root.position.x, this.player.root.position.z)
    this.distanceTravelled += distance2D(before, this.player.root.position)

    if (this.dodgeTime > 0) {
      const progress = clamp01(1 - this.dodgeTime / DODGE_DURATION)
      const weight = dodgeEnvelope(progress)
      this.weapon.rotation.x = -0.9 * weight
      this.weapon.rotation.z = -0.18 - 0.25 * weight
    } else if (this.attackTime >= 0) {
      const progress = clamp01(this.attackTime / ATTACK_DURATION)
      const windup = smoothstep(0, 0.25, progress) * (1 - smoothstep(0.3, 0.43, progress))
      const thrust = smoothstep(0.25, 0.42, progress) * (1 - smoothstep(0.64, 1, progress))
      this.weapon.rotation.x = -0.42 * windup + 2.05 * thrust
      this.weapon.rotation.z = -0.18 - 0.12 * thrust
    } else {
      this.weapon.rotation.x += (0 - this.weapon.rotation.x) * Math.min(1, dt * 18)
      this.weapon.rotation.z += (-0.18 - this.weapon.rotation.z) * Math.min(1, dt * 18)
    }
    const dodgeProgress = clamp01(1 - this.dodgeTime / DODGE_DURATION)
    const dodgeWeight = dodgeEnvelope(dodgeProgress)
    this.player.content.position.y = this.playerContentBaseY - 0.42 * dodgeWeight
    this.player.content.rotation.set(0.32 * dodgeWeight, SOLDIER_FORWARD_OFFSET, -0.1 * dodgeWeight)

    const heat = Math.max(0, Math.sin(this.dayProgress() * Math.PI * 2))
    this.hydration = Math.max(0, this.hydration - dt * (0.16 + heat * 0.24 + (sprinting ? 0.2 : 0)))
    if (this.hydration <= 0) {
      this.health -= dt * 4
      if (this.health <= 0) this.finish(false, 'Обезвоживание. Следите за запасом воды и ищите колодцы на карте.')
    }
  }

  private resolvePlayerCollisions(): void {
    const position = this.player.root.position
    this.pushOut(position, Vector3.Zero(), 2.05)
    for (const collider of this.world.colliders) {
      if (Math.abs(collider.x - position.x) > 2 || Math.abs(collider.z - position.z) > 2) continue
      this.pushOut(position, new Vector3(collider.x, 0, collider.z), collider.radius + 0.3)
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

  dodge(): void {
    if (this.state !== 'playing' || this.dodgeCooldown > 0 || this.stamina < 25 || this.attackTime >= 0) return
    this.stamina -= 25
    this.dodgeCooldown = 2.1
    this.dodgeTime = DODGE_DURATION
    this.invulnerableTime = 0.48
    if (this.moveDirection.lengthSquared() > 0.05) this.dodgeDirection.copyFrom(this.moveDirection).normalize()
    else this.dodgeDirection.set(Math.sin(this.player.root.rotation.y), 0, Math.cos(this.player.root.rotation.y))
    this.sound.play('slide')
    spawnSlideDust(this.scene, this.player.root.position.add(new Vector3(0, 0.12, 0)), this.dodgeDirection)
  }

  attack(): void {
    if (this.state !== 'playing' || this.attackCooldown > 0 || this.dodgeTime > 0) return
    this.attackCooldown = 0.78
    this.attackTime = 0
    this.attackHit = false
    const target = this.enemies.filter((enemy) => distance2D(enemy.model.root.position, this.player.root.position) < 3.6)
      .sort((a, b) => distance2D(a.model.root.position, this.player.root.position) - distance2D(b.model.root.position, this.player.root.position))[0]
    if (target) {
      const direction = target.model.root.position.subtract(this.player.root.position)
      this.player.root.rotation.y = Math.atan2(direction.x, direction.z)
    } else if (this.moveDirection.lengthSquared() > 0.05) {
      this.player.root.rotation.y = Math.atan2(this.moveDirection.x, this.moveDirection.z)
    } else {
      const forward = this.world.camera.target.subtract(this.world.camera.position)
      this.player.root.rotation.y = Math.atan2(forward.x, forward.z)
    }
    this.sound.play('swing')
  }

  private performStrike(): void {
    const forward = new Vector3(Math.sin(this.player.root.rotation.y), 0, Math.cos(this.player.root.rotation.y))
    const targets = this.enemies.filter((enemy) => {
      const offset = enemy.model.root.position.subtract(this.player.root.position)
      offset.y = 0
      const distance = offset.length()
      return distance < 3.25 && (distance < 1.15 || Vector3.Dot(offset.normalize(), forward) > 0.05)
    }).sort((a, b) => distance2D(a.model.root.position, this.player.root.position) - distance2D(b.model.root.position, this.player.root.position)).slice(0, 2)
    targets.forEach((enemy) => this.damageEnemy(enemy, 38))
  }

  private damageEnemy(enemy: DesertEnemy, amount: number): void {
    if (!this.enemies.includes(enemy)) return
    enemy.health -= amount
    enemy.hitTime = 0.2
    this.sound.play('hit')
    spawnBurst(this.scene, enemy.model.root.position.add(new Vector3(0, 0.55, 0)), Color3.FromHexString('#d7a06b'), 8)
    if (enemy.health > 0) return
    enemy.model.dispose()
    this.enemies.splice(this.enemies.indexOf(enemy), 1)
    this.kills += 1
    this.scrap += 1
    this.callbacks.onLoot('+1 обломок', 'scrap')
  }

  private updateSpawning(dt: number): void {
    this.spawnTimer -= dt
    const cap = this.signalTime !== null ? 10 : 5
    if (this.spawnTimer > 0 || this.enemies.length >= cap) return
    this.spawnEnemy()
    this.spawnTimer = this.signalTime !== null ? 3.2 + Math.random() * 1.8 : 9 + Math.random() * 5
  }

  private spawnEnemy(): void {
    const model = this.world.library.create('fox', 1.05)
    const angle = Math.random() * Math.PI * 2
    const distance = 20 + Math.random() * 9
    const x = this.player.root.position.x + Math.cos(angle) * distance
    const z = this.player.root.position.z + Math.sin(angle) * distance
    model.root.position.set(x, desertHeight(x, z), z)
    model.play('Run', 1.1)
    this.enemies.push({ model, health: 72, speed: 2 + Math.random() * 0.55, attackCooldown: 0.6, hitTime: 0 })
  }

  private updateEnemies(dt: number): void {
    for (const enemy of [...this.enemies]) {
      enemy.attackCooldown = Math.max(0, enemy.attackCooldown - dt)
      enemy.hitTime = Math.max(0, enemy.hitTime - dt)
      const offset = this.player.root.position.subtract(enemy.model.root.position)
      offset.y = 0
      const distance = offset.length()
      if (distance > 1.6 && enemy.hitTime <= 0) {
        offset.normalize()
        for (const other of this.enemies) {
          if (other === enemy) continue
          const separation = enemy.model.root.position.subtract(other.model.root.position)
          separation.y = 0
          const length = separation.length()
          if (length > 0.001 && length < 1.1) offset.addInPlace(separation.normalize().scale(0.65))
        }
        offset.normalize()
        enemy.model.root.position.addInPlace(offset.scale(enemy.speed * dt))
        enemy.model.root.rotation.y = Math.atan2(offset.x, offset.z)
        enemy.model.play('Run', 1.15)
      } else {
        enemy.model.play('Survey', 1)
        if (distance <= 1.85 && enemy.attackCooldown <= 0) {
          enemy.attackCooldown = 1.15
          this.damagePlayer(10)
        }
      }
      enemy.model.root.position.y = desertHeight(enemy.model.root.position.x, enemy.model.root.position.z)
      enemy.model.content.rotation.z = enemy.hitTime > 0 ? Math.sin(enemy.hitTime * 30) * 0.12 : 0
    }
  }

  private damagePlayer(amount: number): void {
    if (this.invulnerableTime > 0 || this.hurtCooldown > 0 || this.state !== 'playing') return
    this.hurtCooldown = 0.42
    this.health -= amount
    this.callbacks.onDamage()
    this.sound.play('hurt')
    if (this.health <= 0) this.finish(false, 'Пустынные хищники остановили экспедицию.')
  }

  buildSentry(): void {
    if (this.state !== 'playing') return
    if (this.sentries.length >= 3) {
      this.callbacks.onNotify('Одновременно можно развернуть только три турели.', 'info')
      return
    }
    if (this.scrap < 8 || this.fiber < 4) {
      this.callbacks.onNotify('Для турели нужно 8 обломков и 4 волокна.', 'danger')
      return
    }
    this.scrap -= 8
    this.fiber -= 4
    const forward = new Vector3(Math.sin(this.player.root.rotation.y), 0, Math.cos(this.player.root.rotation.y))
    const position = this.player.root.position.add(forward.scale(2.4))
    const sentry = this.world.models.sentry()
    sentry.root.position.set(position.x, desertHeight(position.x, position.z), position.z)
    sentry.root.rotation.y = this.player.root.rotation.y
    this.sentries.push({ ...sentry, cooldown: 0.2 })
    this.sound.play('craft')
    spawnBurst(this.scene, position.add(new Vector3(0, 0.5, 0)), Color3.FromHexString('#e0b57a'), 14)
    this.callbacks.onNotify('Турель развёрнута. Сектор обстрела — 17 метров.', 'good')
    this.emitSnapshot()
  }

  bandage(): void {
    if (this.state !== 'playing') return
    if (this.health >= 100) {
      this.callbacks.onNotify('Ранений нет — волокно пока не нужно.', 'info')
      return
    }
    if (this.fiber < 3) {
      this.callbacks.onNotify('Для перевязки нужно 3 волокна.', 'danger')
      return
    }
    this.fiber -= 3
    this.health = Math.min(100, this.health + 45)
    this.sound.play('craft')
    this.callbacks.onLoot('+45 здоровья', 'health')
    this.emitSnapshot()
  }

  drink(): void {
    if (this.state !== 'playing') return
    if (this.water <= 0) {
      this.callbacks.onNotify('Фляги пусты. Найдите оазис или колодец.', 'danger')
      return
    }
    if (this.hydration >= 98) {
      this.callbacks.onNotify('Пока не нужно пить.', 'info')
      return
    }
    this.water -= 1
    this.hydration = Math.min(100, this.hydration + 48)
    this.sound.play('crystal')
    this.callbacks.onLoot('+48 воды', 'water')
    this.emitSnapshot()
  }

  private updateSentries(dt: number): void {
    for (const sentry of this.sentries) {
      sentry.cooldown = Math.max(0, sentry.cooldown - dt)
      const target = this.enemies.filter((enemy) => distance2D(enemy.model.root.position, sentry.root.position) < 17)
        .sort((a, b) => distance2D(a.model.root.position, sentry.root.position) - distance2D(b.model.root.position, sentry.root.position))[0]
      if (!target) continue
      const direction = target.model.root.position.subtract(sentry.root.position)
      sentry.head.rotation.y = Math.atan2(direction.x, direction.z) - sentry.root.rotation.y
      if (sentry.cooldown > 0) continue
      sentry.cooldown = 0.58
      sentry.root.computeWorldMatrix(true)
      const from = sentry.muzzle.getAbsolutePosition().clone()
      const to = target.model.root.position.add(new Vector3(0, 0.55, 0))
      spawnTracer(this.scene, from, to, Color3.FromHexString('#ffd18a'))
      this.sound.play('turret')
      this.damageEnemy(target, 17)
    }
  }

  interact(): void {
    if (this.state !== 'playing' || this.interactCooldown > 0) return
    this.interactCooldown = 0.25
    if (this.nearRelay()) {
      if (this.relayActive) {
        this.callbacks.onNotify(`Сигнал уже идёт. До ответа: ${Math.ceil(this.signalTime ?? 0)} секунд.`, 'info')
      } else if (this.scrap < RELAY_SCRAP || this.fiber < RELAY_FIBER) {
        this.callbacks.onNotify(`Для ремонта: ${RELAY_SCRAP} обломков и ${RELAY_FIBER} волокна. С вами: ${this.scrap} и ${this.fiber}.`, 'danger')
      } else {
        this.scrap -= RELAY_SCRAP
        this.fiber -= RELAY_FIBER
        this.relayActive = true
        this.signalTime = SIGNAL_DURATION
        this.relay.setActive(true)
        this.spawnTimer = 0.2
        this.sound.play('core')
        this.callbacks.onAnnounce('РАДИОЭФИР ОТКРЫТ', 'ДЕРЖИТЕСЬ 45 СЕКУНД', 'Сигнал услышали не только спасатели. Используйте турели и копьё.')
      }
      this.emitSnapshot()
      return
    }
    const resource = this.nearestResource()
    if (!resource) return
    if (resource.kind === 'water') {
      this.water = Math.min(5, this.water + 2)
      this.hydration = 100
      this.sound.play('crystal')
      this.callbacks.onLoot('+2 фляги · вода восстановлена', 'water')
    } else {
      const amount = resource.kind === 'scrap' ? 3 : 2
      if (resource.kind === 'scrap') this.scrap += amount
      else this.fiber += amount
      this.world.consume(resource)
      this.sound.play(resource.kind === 'scrap' ? 'scrap' : 'wood')
      this.callbacks.onLoot(`+${amount} ${resource.kind === 'scrap' ? 'обломка' : 'волокна'}`, resource.kind)
    }
    this.emitSnapshot()
  }

  private nearRelay(): boolean {
    return distance2D(this.player.root.position, this.relay.root.position) < 4.3
  }

  private nearestResource(): DesertResource | undefined {
    let nearest: DesertResource | undefined
    let best = 2.9
    for (const resource of this.world.resources) {
      if (resource.amount <= 0) continue
      const distance = distance2D(resource.root.position, this.player.root.position)
      if (distance < best) {
        best = distance
        nearest = resource
      }
    }
    return nearest
  }

  private finish(victory: boolean, reason: string): void {
    if (this.state !== 'playing') return
    this.state = victory ? 'victory' : 'gameover'
    this.keys.clear()
    this.mobileMove.set(0, 0)
    this.sound.play(victory ? 'win' : 'lose')
    const stats: DesertEndStats = { victory, reason, elapsed: this.elapsed, kills: this.kills, distance: this.distanceTravelled }
    this.callbacks.onState(this.state, stats)
    this.emitSnapshot()
  }

  private emitSnapshot(): void {
    if (!this.player || !this.relay) return
    const nearRelay = this.nearRelay()
    const resource = this.nearestResource()
    let prompt = ''
    let promptDetail = ''
    if (nearRelay) {
      prompt = this.relayActive ? 'Сигнал активен' : 'Запустить вышку связи'
      promptDetail = this.relayActive ? `До ответа: ${Math.ceil(this.signalTime ?? 0)} с` : `${RELAY_SCRAP} обломков · ${RELAY_FIBER} волокна`
    } else if (resource) {
      prompt = resource.kind === 'scrap' ? 'Разобрать обломки' : resource.kind === 'fiber' ? 'Собрать волокно' : 'Наполнить фляги'
      promptDetail = resource.kind === 'water' ? 'Вода и гидратация восстановятся' : 'E · взаимодействие'
    }
    const forward = this.world.camera.target.subtract(this.world.camera.position)
    const progress = this.dayProgress()
    const snapshot: DesertSnapshot = {
      state: this.state,
      health: this.health,
      stamina: this.stamina,
      hydration: this.hydration,
      scrap: this.scrap,
      fiber: this.fiber,
      water: this.water,
      kills: this.kills,
      enemies: this.enemies.length,
      elapsed: this.elapsed,
      day: Math.floor(this.elapsed / DAY_DURATION) + 1,
      dayProgress: progress,
      temperature: 18 + Math.max(0, Math.sin(progress * Math.PI * 2)) * 27,
      heading: (Math.atan2(forward.x, forward.z) * 180 / Math.PI + 360) % 360,
      x: this.player.root.position.x,
      z: this.player.root.position.z,
      prompt,
      promptDetail,
      nearRelay,
      relayActive: this.relayActive,
      signalTime: this.signalTime ?? 0,
      canSentry: this.scrap >= 8 && this.fiber >= 4,
      canBandage: this.fiber >= 3 && this.health < 100,
      canRelay: this.scrap >= RELAY_SCRAP && this.fiber >= RELAY_FIBER,
      dodgeCooldown: this.dodgeCooldown,
    }
    this.callbacks.onSnapshot(snapshot)
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
      if (event.code === 'KeyR') this.drink()
      if (event.code === 'Digit1') this.buildSentry()
      if (event.code === 'Digit2') this.bandage()
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
