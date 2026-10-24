import {
  Bone,
  Color3,
  Engine,
  Matrix,
  Mesh,
  PBRMaterial,
  Scene,
  Space,
  TransformNode,
  Vector2,
  Vector3,
  VertexBuffer,
} from '@babylonjs/core'
import { ModelInstance, spawnBurst, spawnCombatEffect, spawnSlideDust, spawnTracer } from '../game/models'
import { Soundscape } from '../game/Soundscape'
import {
  CAMPAIGN_CHAPTERS,
  CAMPAIGN_LOCATIONS,
  CAMPAIGN_SAVE_KEY,
  ENEMY_ARCHETYPES,
  type CampaignChapter,
  type CampaignSave,
  type CampaignStep,
  type EnemyKind,
  type LocationId,
} from './Campaign'
import { CampaignModels } from './CampaignModels'
import { FirstPersonLegs } from './FirstPersonLegs'
import type { RelayVisual, SentryVisual } from './DesertModels'
import { DesertWorld, desertHeight, type DesertResource } from './DesertWorld'
import type { DesertCallbacks, DesertEndStats, DesertSnapshot, DesertState } from './types'

const DAY_DURATION = 180
const DODGE_DURATION = 0.68
const ATTACK_DURATION = 0.84
const SOLDIER_FORWARD_OFFSET = Math.PI

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
  return smoothstep(0, 0.28, progress) * (1 - smoothstep(0.58, 1, progress))
}

function attackWindup(progress: number): number {
  return smoothstep(0, 0.18, progress) * (1 - smoothstep(0.2, 0.36, progress))
}

function attackThrust(progress: number): number {
  return smoothstep(0.2, 0.43, progress) * (1 - smoothstep(0.58, 0.94, progress))
}

interface DesertEnemy {
  kind: EnemyKind
  model: ModelInstance
  health: number
  maxHealth: number
  speed: number
  damage: number
  attackCooldown: number
  specialCooldown: number
  hitTime: number
  phase: number
  ranged: boolean
  boss: boolean
  velocity: Vector3
}

interface ActiveSentry extends SentryVisual {
  cooldown: number
}

interface CampaignInteractable {
  id: string
  kind: 'salvage' | 'activate' | 'repair'
  root: TransformNode
  used: boolean
  label: string
}

interface GroundHazard {
  root: TransformNode
  position: Vector3
  radius: number
  time: number
  total: number
  damage: number
}

export class DesertGame {
  readonly sound = new Soundscape()
  readonly ready: Promise<void>
  private readonly engine: Engine
  private readonly scene: Scene
  private readonly world: DesertWorld
  private readonly campaignModels: CampaignModels
  private player!: ModelInstance
  private relay!: RelayVisual
  private weapon!: TransformNode
  private weaponHand: Bone | null = null
  private weaponHandAnchor: TransformNode | null = null
  private readonly actionBones = new Map<string, Bone>()
  private readonly fingerBones: { bone: Bone; thumb: boolean }[] = []
  private firstPersonUpperBody: Mesh | null = null
  private firstPersonArms: Mesh | null = null
  private firstPersonWeaponMeshes: Mesh[] = []
  private firstPersonLegs: FirstPersonLegs | null = null
  private readonly weaponRootInverse = Matrix.Identity()
  private readonly weaponHandWorld = new Vector3()
  private readonly fingerCurlAxis = Vector3.Forward()
  private playerContentBaseY = 0
  private readonly enemies: DesertEnemy[] = []
  private readonly sentries: ActiveSentry[] = []
  private readonly campaignObjects: CampaignInteractable[] = []
  private readonly hazards: GroundHazard[] = []
  private readonly landmarks = new Map<LocationId, TransformNode>()
  private waypoint!: TransformNode
  private activeBoss: DesertEnemy | null = null
  private readonly keys = new Set<string>()
  private readonly mobileMove = new Vector2(0, 0)
  private readonly moveDirection = new Vector3(0, 0, 1)
  private readonly dodgeDirection = new Vector3(0, 0, 1)
  private readonly cameraForward = new Vector3()
  private readonly cameraRight = new Vector3()
  private readonly cameraHead = new Vector3()
  private readonly velocity = new Vector3()
  private facingTarget = Math.PI
  private dodgePeakSpeed = 9
  private headBobPhase = 0
  private headBobWeight = 0
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
  private cameraYaw = 0.35
  private cameraPitch = 0.18
  private draggingPointer: number | null = null
  private dragX = 0
  private dragY = 0
  private snapshotTimer = 0

  private campaignChapter = 0
  private campaignStep = 0
  private campaignCount = 0
  private campaignTimer: number | null = null
  private campaignTransition = false
  private spearLevel = 1
  private sentryLevel = 1
  private armorLevel = 0

  constructor(private readonly canvas: HTMLCanvasElement, private readonly callbacks: DesertCallbacks) {
    this.engine = new Engine(canvas, true, {
      stencil: true,
      powerPreference: 'high-performance',
      doNotHandleContextLost: false,
    }, true)
    this.engine.setHardwareScalingLevel(1 / Math.min(window.devicePixelRatio || 1, 2))
    this.scene = new Scene(this.engine)
    this.scene.skipPointerMovePicking = true
    this.scene.constantlyUpdateMeshUnderPointer = false
    this.world = new DesertWorld(this.scene)
    this.campaignModels = new CampaignModels(this.scene, this.world.shadows, this.world.models)
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
      'LeftHand', 'RightHand',
    ]
    this.player.entries.skeletons.flatMap((skeleton) => skeleton.bones).forEach((bone) => {
      const name = poseBones.find((fragment) => bone.name.endsWith(fragment))
      if (name) this.actionBones.set(name, bone)
      const finger = bone.name.match(/Hand(Thumb|Index|Middle|Ring|Pinky)[123]$/)
      if (finger) this.fingerBones.push({ bone, thumb: finger[1] === 'Thumb' })
    })
    this.weapon = this.world.models.spear()
    this.weapon.parent = this.player.root
    this.weaponHand = this.player.entries.skeletons.flatMap((skeleton) => skeleton.bones)
      .find((bone) => bone.name.endsWith('RightHand')) ?? null
    this.weaponHandAnchor = this.player.content.getChildMeshes()
      .find((mesh) => this.weaponHand !== null && mesh.skeleton?.bones.includes(this.weaponHand)) ?? null
    this.splitFirstPersonBody()
    this.firstPersonLegs = new FirstPersonLegs(this.scene, this.player.root, this.world.shadows)
    this.scene.onAfterAnimationsObservable.add(() => {
      // The observable also fires while animations are paused. Never accumulate
      // additive bone rotations on the same frozen pose in story/pause screens.
      if (!this.scene.animationsEnabled) return
      this.applyActionPose()
      this.syncWeaponGrip()
    })
    this.relay = this.world.models.relay()
    this.relay.root.position.set(0, desertHeight(0, 0), 0)
    this.world.freezeStatic(this.relay.root)
    this.createLandmarks()
    this.waypoint = this.campaignModels.waypoint()
    this.waypoint.setEnabled(false)
    this.isReady = true
    this.resetGame(false)
    this.state = 'menu'
    this.emitSnapshot()
  }

  private splitFirstPersonBody(): void {
    const body = this.player.content.getChildMeshes().find((mesh): mesh is Mesh =>
      mesh instanceof Mesh && mesh.skeleton !== null && mesh.getTotalVertices() > 1000,
    )
    if (!body?.skeleton) return
    const boneIndices = body.getVerticesData(VertexBuffer.MatricesIndicesKind)
    const weights = body.getVerticesData(VertexBuffer.MatricesWeightsKind)
    const indices = body.getIndices()
    if (!boneIndices || !weights || !indices) return

    const categories = new Uint8Array(body.getTotalVertices())
    for (let vertex = 0; vertex < categories.length; vertex += 1) {
      let strongest = 0
      for (let influence = 1; influence < 4; influence += 1) {
        if (weights[vertex * 4 + influence] > weights[vertex * 4 + strongest]) strongest = influence
      }
      const bone = body.skeleton.bones[Math.floor(boneIndices[vertex * 4 + strongest])]
      const name = bone?.name ?? ''
      if (/(?:Arm|Hand|Finger|Thumb|Index|Middle|Ring|Pinky)/.test(name) && !/Shoulder/.test(name)) {
        categories[vertex] = 1
      } else if (/(?:UpLeg|Leg|Foot|Toe)/.test(name)) {
        categories[vertex] = 2
      }
    }
    const armIndices: number[] = []
    const upperIndices: number[] = []
    for (let triangle = 0; triangle < indices.length; triangle += 3) {
      let arms = false
      let legs = false
      for (let corner = 0; corner < 3; corner += 1) {
        const category = categories[indices[triangle + corner]]
        arms ||= category === 1
        legs ||= category === 2
      }
      if (arms) armIndices.push(indices[triangle], indices[triangle + 1], indices[triangle + 2])
      if (!arms && !legs) upperIndices.push(indices[triangle], indices[triangle + 1], indices[triangle + 2])
    }
    if (!armIndices.length || !upperIndices.length) return

    body.makeGeometryUnique()
    const arms = body.clone('survivor first-person arms', body.parent)
    arms.makeGeometryUnique().updateIndices(armIndices)
    body.updateIndices(upperIndices)
    this.firstPersonUpperBody = body
    this.firstPersonArms = arms
    this.firstPersonWeaponMeshes = this.weapon.getChildMeshes(false)
      .filter((mesh): mesh is Mesh => mesh instanceof Mesh)
    this.world.shadows.addShadowCaster(arms)
  }

  private createLandmarks(): void {
    const builders: Partial<Record<LocationId, () => TransformNode>> = {
      caravan: () => this.campaignModels.caravan(),
      wells: () => this.campaignModels.wellStation(),
      city: () => this.campaignModels.city(),
      storm: () => this.campaignModels.stormSpire(),
      citadel: () => this.campaignModels.citadel(),
    }
    for (const [id, build] of Object.entries(builders) as [LocationId, () => TransformNode][]) {
      const location = CAMPAIGN_LOCATIONS[id]
      const root = build()
      root.position.set(location.x, desertHeight(location.x, location.z), location.z)
      this.world.freezeStatic(root)
      root.setEnabled(false)
      this.landmarks.set(id, root)
    }
  }

  private syncWeaponGrip(): void {
    if (!this.weaponHand || !this.weaponHandAnchor) return
    this.player.root.computeWorldMatrix(true)
    this.weaponHandAnchor.computeWorldMatrix(true)
    const handNode = this.weaponHand.getTransformNode()
    if (handNode) {
      handNode.computeWorldMatrix(true)
      this.weaponHandWorld.copyFrom(handNode.getAbsolutePosition())
    } else {
      this.weaponHand.computeAbsoluteMatrices()
      this.weaponHand.getAbsolutePositionToRef(this.weaponHandAnchor, this.weaponHandWorld)
    }
    this.weaponRootInverse.copyFrom(this.player.root.getWorldMatrix())
    this.weaponRootInverse.invert()
    Vector3.TransformCoordinatesToRef(this.weaponHandWorld, this.weaponRootInverse, this.weapon.position)
  }

  private applyActionPose(): void {
    const headNode = this.actionBones.get('Head')?.getTransformNode()
    // Keep the animated body, but never put the camera inside the head/helmet.
    if (headNode) headNode.scaling.setAll(0.001)
    // At a steep downward angle chest and shoulder armour would otherwise fill
    // the view and occlude the feet. Arms and legs use separate skinned meshes.
    if (this.firstPersonUpperBody) this.firstPersonUpperBody.isVisible = this.cameraPitch < 1.02
    // Looking straight down should reveal the feet, not fill the screen with
    // shoulder armour. Fade the view-model smoothly before it reaches the
    // near plane; combat still gets a complete, readable attack pose.
    const handVisibility = this.attackTime >= 0 ? 1 : 1 - smoothstep(0.74, 1.08, this.cameraPitch)
    if (this.firstPersonArms) {
      this.firstPersonArms.visibility = handVisibility
      this.firstPersonArms.isVisible = handVisibility > 0.01
    }
    this.firstPersonWeaponMeshes.forEach((mesh) => {
      mesh.visibility = handVisibility
      mesh.isVisible = handVisibility > 0.01
    })
    this.poseFirstPersonLegs()
    const yaw = this.player.root.rotation.y
    const forward = new Vector3(Math.sin(yaw), 0, Math.cos(yaw))
    const right = Vector3.Cross(Vector3.Up(), forward).normalize()
    const rotate = (name: string, axis: Vector3, angle: number): void => {
      if (Math.abs(angle) <= 0.0001) return
      // WORLD-space bone rotations need the skinned mesh transform. Without it
      // Babylon uses the rig space, whose forward axis differs from Soldier's.
      const bone = this.actionBones.get(name)
      if (!bone) return
      const linked = bone.getTransformNode()
      if (linked) {
        // glTF animation drives these nodes; Skeleton.prepare() copies their
        // local transforms to the correct skeleton (Soldier contains two).
        linked.computeWorldMatrix(true)
        linked.rotate(axis, angle, Space.WORLD)
      } else {
        bone.rotate(axis, angle, Space.WORLD, this.weaponHandAnchor ?? undefined)
      }
    }
    if (this.dodgeTime > 0) {
      const progress = clamp01(1 - this.dodgeTime / DODGE_DURATION)
      const weight = dodgeEnvelope(progress)
      rotate('Spine1', right, 0.18 * weight)
      rotate('Spine2', right, 0.24 * weight)
      rotate('Head', right, -0.2 * weight)
      rotate('LeftArm', right, -0.35 * weight)
      rotate('RightArm', right, 0.28 * weight)
      this.poseSpearArms(0, 0, 1)
      return
    }
    if (this.attackTime >= 0) {
      const progress = clamp01(this.attackTime / ATTACK_DURATION)
      const windup = attackWindup(progress)
      const thrust = attackThrust(progress)
      rotate('RightArm', right, 0.55 - 0.32 * windup + 0.82 * thrust)
      rotate('RightForeArm', right, 0.3 - 0.42 * windup + 0.28 * thrust)
      rotate('LeftArm', right, 0.48 + 0.12 * windup + 0.22 * thrust)
      rotate('LeftForeArm', right, 0.28 + 0.22 * thrust)
      rotate('Spine1', right, 0.12 * thrust - 0.06 * windup)
      rotate('Spine2', Vector3.Up(), -0.22 * windup + 0.26 * thrust)
      rotate('Head', right, -0.08 * thrust)
      this.poseSpearArms(thrust, windup, 1)
      return
    }
    this.poseSpearArms(0, 0, 1)
  }

  private aimJointAt(startName: string, endName: string, target: Vector3, weight: number): void {
    const start = this.actionBones.get(startName)?.getTransformNode()
    const end = this.actionBones.get(endName)?.getTransformNode()
    if (!start || !end) return
    start.computeWorldMatrix(true)
    end.computeWorldMatrix(true)
    const current = end.getAbsolutePosition().subtract(start.getAbsolutePosition()).normalize()
    const desired = target.subtract(start.getAbsolutePosition()).normalize()
    const axis = Vector3.Cross(current, desired)
    const angle = Math.atan2(axis.length(), Math.max(-1, Math.min(1, Vector3.Dot(current, desired)))) * weight
    if (angle < 0.001 || axis.lengthSquared() < 0.000001) return
    start.rotate(axis.normalize(), angle, Space.WORLD)
  }

  private poseSpearArms(thrust: number, windup: number, weight: number): void {
    if (weight <= 0.0001) return
    const yaw = this.player.root.rotation.y
    const forward = new Vector3(Math.sin(yaw), 0, Math.cos(yaw))
    const right = Vector3.Cross(Vector3.Up(), forward).normalize()
    const point = (x: number, y: number, z: number): Vector3 => this.player.root.position
      .add(right.scale(x)).add(forward.scale(z)).add(new Vector3(0, y, 0))
    const reach = thrust * 0.34
    const pullback = windup * 0.24
    const pitchDrop = Math.sin(this.cameraPitch) * 0.06
    // Keep the view-model beyond the near-field perspective distortion. The
    // spear still reaches farther during a thrust, but idle hands do not fill
    // the bottom half of the screen.
    const rightHandY = 1.22 - pitchDrop + windup * 0.05
    const leftHandY = 1.24 - pitchDrop + windup * 0.025
    const rightHandZ = 1.02 + reach - pullback
    const leftHandZ = 1.08 + reach * 0.9 - pullback * 0.5
    this.aimJointAt('RightArm', 'RightForeArm', point(0.34, 1.2, 0.52 + reach * 0.2 - pullback * 0.4), weight)
    this.aimJointAt('RightForeArm', 'RightHand', point(0.24, rightHandY, rightHandZ), weight)
    this.aimJointAt('LeftArm', 'LeftForeArm', point(-0.34, 1.22, 0.56 + reach * 0.25 - pullback * 0.25), weight)
    this.aimJointAt('LeftForeArm', 'LeftHand', point(-0.16, leftHandY, leftHandZ), weight)
    // Soldier's finger chains point along local +Y. A positive local-Z bend
    // curls them toward the palm, giving both hands a stable spear grip while
    // the imported animation continues to drive the arms and wrists.
    this.fingerBones.forEach(({ bone, thumb }) => {
      bone.getTransformNode()?.rotate(this.fingerCurlAxis, (thumb ? 0.22 : 0.5) * weight, Space.LOCAL)
    })
  }

  private poseFirstPersonLegs(): void {
    const speed = Math.hypot(this.velocity.x, this.velocity.z)
    const dodgeProgress = clamp01(1 - this.dodgeTime / DODGE_DURATION)
    const dodgeWeight = this.dodgeTime > 0 ? dodgeEnvelope(dodgeProgress) : 0
    const dt = Math.min(this.engine.getDeltaTime() / 1000 || 0.016, 0.05)
    this.firstPersonLegs?.update(dt, speed, dodgeWeight, this.cameraPitch)
  }

  private maxHealth(): number {
    return 100 + this.armorLevel * 15
  }

  private readSave(): CampaignSave | null {
    try {
      const raw = localStorage.getItem(CAMPAIGN_SAVE_KEY)
      if (!raw) return null
      const save = JSON.parse(raw) as CampaignSave
      if (save.version !== 1 || !Number.isInteger(save.chapter) || save.chapter < 0 || save.chapter >= CAMPAIGN_CHAPTERS.length) return null
      return save
    } catch {
      return null
    }
  }

  hasCampaignSave(): boolean {
    return this.readSave() !== null
  }

  continueLabel(): string {
    const save = this.readSave()
    return save ? `ПРОДОЛЖИТЬ · ГЛАВА ${save.chapter + 1}` : 'ПРОДОЛЖИТЬ КАМПАНИЮ'
  }

  private writeSave(): void {
    const save: CampaignSave = {
      version: 1,
      chapter: this.campaignChapter,
      spearLevel: this.spearLevel,
      sentryLevel: this.sentryLevel,
      armorLevel: this.armorLevel,
      scrap: this.scrap,
      fiber: this.fiber,
      water: this.water,
      kills: this.kills,
      elapsed: this.elapsed,
      x: this.player.root.position.x,
      z: this.player.root.position.z,
    }
    try { localStorage.setItem(CAMPAIGN_SAVE_KEY, JSON.stringify(save)) } catch { /* Private browsing can disable storage. */ }
  }

  private restoreSave(save: CampaignSave): void {
    this.spearLevel = save.spearLevel
    this.sentryLevel = save.sentryLevel
    this.armorLevel = save.armorLevel
    this.resetGame()
    this.scrap = save.scrap
    this.fiber = save.fiber
    this.water = save.water
    this.kills = save.kills
    this.elapsed = save.elapsed
    this.health = this.maxHealth()
    this.player.root.position.set(save.x, desertHeight(save.x, save.z), save.z)
    this.campaignChapter = save.chapter
    this.state = 'playing'
    this.beginChapter(save.chapter)
  }

  start(): void {
    if (!this.isReady) return
    this.sound.unlock()
    try { localStorage.removeItem(CAMPAIGN_SAVE_KEY) } catch { /* Storage is optional. */ }
    this.spearLevel = 1
    this.sentryLevel = 1
    this.armorLevel = 0
    this.scene.animationsEnabled = true
    this.resetGame()
    this.campaignChapter = 0
    this.state = 'playing'
    this.beginChapter(0)
  }

  continueCampaign(): void {
    if (!this.isReady) return
    const save = this.readSave()
    if (!save) {
      this.start()
      return
    }
    this.sound.unlock()
    this.scene.animationsEnabled = true
    this.restoreSave(save)
  }

  restart(): void {
    if (this.state === 'victory') this.start()
    else this.continueCampaign()
  }

  journal(): void {
    if (this.state !== 'playing') return
    this.presentStory()
  }

  private presentStory(): void {
    const chapter = CAMPAIGN_CHAPTERS[this.campaignChapter]
    this.keys.clear()
    this.mobileMove.set(0, 0)
    this.state = 'story'
    this.releaseMouseLook()
    this.scene.animationsEnabled = false
    this.callbacks.onStory({
      act: chapter.act,
      title: chapter.title,
      speaker: chapter.speaker,
      text: chapter.story,
      chapter: this.campaignChapter + 1,
      totalChapters: CAMPAIGN_CHAPTERS.length,
    })
    this.callbacks.onState('story')
  }

  resumeStory(): void {
    if (this.state !== 'story') return
    this.scene.animationsEnabled = true
    this.state = 'playing'
    this.requestMouseLook()
    this.callbacks.onStory(null)
    this.callbacks.onState('playing')
    this.emitSnapshot()
  }

  toMenu(): void {
    if (!this.isReady) return
    this.releaseMouseLook()
    this.scene.animationsEnabled = true
    this.resetGame()
    this.state = 'menu'
    this.callbacks.onStory(null)
    this.callbacks.onState('menu')
  }

  pause(): void {
    if (this.state !== 'playing') return
    this.state = 'paused'
    this.releaseMouseLook()
    this.keys.clear()
    this.mobileMove.set(0, 0)
    this.draggingPointer = null
    this.scene.animationsEnabled = false
    this.callbacks.onState('paused')
  }

  resume(): void {
    if (this.state !== 'paused') return
    this.scene.animationsEnabled = true
    this.state = 'playing'
    this.requestMouseLook()
    this.callbacks.onState('playing')
  }

  togglePause(): void {
    if (this.state === 'playing') this.pause()
    else if (this.state === 'paused') this.resume()
    else if (this.state === 'story') this.resumeStory()
  }

  toggleSound(): boolean {
    this.sound.unlock()
    return this.sound.toggle()
  }

  setJoystick(x: number, y: number): void {
    this.mobileMove.set(x, y)
  }

  private clearCampaignObjects(): void {
    this.campaignObjects.forEach((object) => object.root.dispose(false, false))
    this.campaignObjects.length = 0
  }

  private clearHazards(): void {
    this.hazards.forEach((hazard) => hazard.root.dispose(false, false))
    this.hazards.length = 0
  }

  private clearEnemies(): void {
    this.enemies.forEach((enemy) => enemy.model.dispose())
    this.enemies.length = 0
    this.activeBoss = null
  }

  private resetGame(resetWorld = true): void {
    this.keys.clear()
    this.mobileMove.set(0, 0)
    this.draggingPointer = null
    this.clearEnemies()
    this.clearCampaignObjects()
    this.clearHazards()
    this.sentries.forEach((sentry) => sentry.root.dispose(false, false))
    this.sentries.length = 0
    this.health = this.maxHealth()
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
    this.velocity.setAll(0)
    this.facingTarget = Math.PI
    this.dodgePeakSpeed = 9
    this.headBobPhase = 0
    this.headBobWeight = 0
    this.cameraPitch = 0.18
    this.relayActive = false
    this.relay.setActive(false)
    this.campaignStep = 0
    this.campaignCount = 0
    this.campaignTimer = null
    this.campaignTransition = false
    this.waypoint?.setEnabled(false)
    this.player.root.position.set(6, desertHeight(6, 8), 8)
    this.player.root.rotation.y = Math.PI
    this.player.content.position.y = this.playerContentBaseY
    this.player.content.rotation.set(0, SOLDIER_FORWARD_OFFSET, 0)
    this.firstPersonLegs?.reset()
    this.player.play('Idle')
    if (resetWorld) this.world.reset(this.player.root.position)
    this.updateCamera(1)
    this.emitSnapshot()
  }

  private currentChapter(): CampaignChapter {
    return CAMPAIGN_CHAPTERS[this.campaignChapter]
  }

  private currentStep(): CampaignStep {
    return this.currentChapter().steps[this.campaignStep]
  }

  private currentLocation() {
    return CAMPAIGN_LOCATIONS[this.currentChapter().location]
  }

  private beginChapter(index: number): void {
    this.campaignChapter = index
    this.campaignStep = 0
    this.campaignCount = 0
    this.campaignTimer = null
    this.campaignTransition = false
    this.clearEnemies()
    this.clearHazards()
    this.beginStep()
    this.writeSave()
    const chapter = this.currentChapter()
    this.callbacks.onAnnounce(chapter.act, `ГЛАВА ${index + 1} · ${chapter.title.toUpperCase()}`, chapter.steps[0].detail)
    this.presentStory()
  }

  private beginStep(): void {
    this.clearCampaignObjects()
    this.campaignCount = 0
    this.campaignTimer = null
    this.campaignTransition = false
    const chapter = this.currentChapter()
    const step = this.currentStep()
    const location = this.currentLocation()
    this.waypoint.position.set(location.x, desertHeight(location.x, location.z), location.z)
    this.waypoint.setEnabled(true)
    if (step.kind === 'salvage' || step.kind === 'activate') {
      const count = step.count ?? 1
      for (let i = 0; i < count; i += 1) {
        const angle = i / count * Math.PI * 2 + 0.35
        const radius = count === 1 ? 3 : 5.5 + (i % 2) * 2
        const x = location.x + Math.cos(angle) * radius
        const z = location.z + Math.sin(angle) * radius
        const root = step.kind === 'salvage' ? this.campaignModels.cache() : this.campaignModels.console()
        root.position.set(x, desertHeight(x, z), z)
        this.campaignObjects.push({
          id: `${chapter.id}-${this.campaignStep}-${i}`,
          kind: step.kind,
          root,
          used: false,
          label: step.kind === 'salvage' ? 'Вскрыть контейнер' : 'Активировать устройство',
        })
      }
    } else if (step.kind === 'repair' && chapter.location !== 'relay') {
      const root = this.campaignModels.console()
      root.position.set(location.x + 3, desertHeight(location.x + 3, location.z), location.z)
      this.campaignObjects.push({ id: `${chapter.id}-repair`, kind: 'repair', root, used: false, label: step.title })
    } else if (step.kind === 'defend') {
      this.campaignTimer = step.duration ?? 40
      this.spawnTimer = 0.25
    } else if (step.kind === 'boss' && step.enemy) {
      const x = location.x + 7
      const z = location.z + 4
      this.spawnEnemy(step.enemy, new Vector3(x, desertHeight(x, z), z))
      this.spawnTimer = 5
      const archetype = ENEMY_ARCHETYPES[step.enemy]
      this.callbacks.onAnnounce(chapter.act, archetype.name, 'Босс меняет поведение на 66% и 33% здоровья.')
    }
    this.emitSnapshot()
  }

  private applyChapterReward(): void {
    switch (this.currentChapter().reward) {
      case 'spear2': this.spearLevel = Math.max(this.spearLevel, 2); break
      case 'sentry2': this.sentryLevel = Math.max(this.sentryLevel, 2); break
      case 'armor1': this.armorLevel = Math.max(this.armorLevel, 1); break
      case 'spear3': this.spearLevel = Math.max(this.spearLevel, 3); break
      case 'armor2':
        this.armorLevel = Math.max(this.armorLevel, 2)
        this.sentryLevel = Math.max(this.sentryLevel, 3)
        break
      case 'none': break
    }
    this.health = this.maxHealth()
    this.hydration = 100
    this.water = Math.min(5, this.water + 1)
  }

  private completeStep(): void {
    if (this.campaignTransition || this.state !== 'playing') return
    this.campaignTransition = true
    this.clearCampaignObjects()
    const chapter = this.currentChapter()
    if (this.campaignStep + 1 < chapter.steps.length) {
      this.campaignStep += 1
      this.beginStep()
      const step = this.currentStep()
      this.callbacks.onAnnounce(chapter.act, step.title.toUpperCase(), step.detail)
      this.emitSnapshot()
      return
    }
    this.applyChapterReward()
    this.callbacks.onNotify(chapter.rewardText, 'good')
    if (this.campaignChapter >= CAMPAIGN_CHAPTERS.length - 1) {
      try { localStorage.removeItem(CAMPAIGN_SAVE_KEY) } catch { /* Storage is optional. */ }
      this.finish(true, 'Архонт отключён. Айра передала координаты живым караванам — впервые за много лет это ваш собственный сигнал, а не голос Миража.')
      return
    }
    this.beginChapter(this.campaignChapter + 1)
  }

  private updateCampaign(dt: number): void {
    const step = this.currentStep()
    const location = this.currentLocation()
    const distance = Math.hypot(this.player.root.position.x - location.x, this.player.root.position.z - location.z)
    if (step.kind === 'travel' && distance < 25) {
      this.completeStep()
      return
    }
    if (step.kind === 'defend' && this.campaignTimer !== null) {
      if (distance < 31) this.campaignTimer = Math.max(0, this.campaignTimer - dt)
      if (this.campaignTimer <= 0) this.completeStep()
    }
  }

  private updateLandmarks(dt: number): void {
    for (const [id, root] of this.landmarks) {
      const location = CAMPAIGN_LOCATIONS[id]
      const distance = Math.hypot(this.player.root.position.x - location.x, this.player.root.position.z - location.z)
      root.setEnabled(distance < 175)
    }
    if (this.waypoint.isEnabled()) {
      this.waypoint.rotation.y += dt * 0.35
      const location = this.currentLocation()
      const distance = Math.hypot(this.player.root.position.x - location.x, this.player.root.position.z - location.z)
      this.waypoint.setEnabled(distance > 18 && distance < 650)
    }
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
      if (!this.attackHit && this.attackTime / ATTACK_DURATION >= 0.43) {
        this.attackHit = true
        this.performStrike()
      }
      if (this.attackTime >= ATTACK_DURATION) this.attackTime = -1
    }
    this.updatePlayer(dt)
    this.updateCamera(dt)
    this.world.updateSurface(dt, this.player.root.position, this.velocity, this.dodgeTime > 0)
    if (this.state !== 'playing') return
    this.updateCampaign(dt)
    if (this.state !== 'playing') return
    this.updateSpawning(dt)
    this.updateEnemies(dt)
    if (this.state !== 'playing') return
    this.updateHazards(dt)
    this.updateSentries(dt)
    this.updateLandmarks(dt)
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

  private horizontalLook(): Vector3 {
    return new Vector3(-Math.cos(this.cameraYaw), 0, -Math.sin(this.cameraYaw))
  }

  private updateCamera(dt: number): void {
    const speed = Math.hypot(this.velocity.x, this.velocity.z)
    const bobTarget = speed > 0.25 && this.dodgeTime <= 0 ? Math.min(1, speed / 4.45) : 0
    this.headBobWeight += (bobTarget - this.headBobWeight) * (1 - Math.exp(-dt * 8))
    this.headBobPhase += dt * (4.2 + speed * 1.05)
    const dodgeProgress = clamp01(1 - this.dodgeTime / DODGE_DURATION)
    const dodgeWeight = this.dodgeTime > 0 ? dodgeEnvelope(dodgeProgress) : 0
    const forward = this.cameraForward.set(-Math.cos(this.cameraYaw), 0, -Math.sin(this.cameraYaw))
    const right = Vector3.CrossToRef(Vector3.Up(), forward, this.cameraRight).normalize()
    const side = Math.sin(this.headBobPhase) * 0.012 * this.headBobWeight
    const vertical = Math.sin(this.headBobPhase * 2) * 0.012 * this.headBobWeight
    // Terrain height must be followed immediately. Smoothing this absolute
    // height made the camera lag inside the torso while climbing dunes.
    this.cameraHead.set(
      this.player.root.position.x + forward.x * 0.18 + right.x * side,
      this.player.root.position.y + 1.62 - dodgeWeight * 0.38 + vertical,
      this.player.root.position.z + forward.z * 0.18 + right.z * side,
    )
    this.world.updateFirstPersonCamera(this.cameraHead, this.cameraYaw, this.cameraPitch)
  }

  private updatePlayer(dt: number): void {
    const forwardInput = (this.keys.has('KeyW') || this.keys.has('ArrowUp') ? 1 : 0)
      - (this.keys.has('KeyS') || this.keys.has('ArrowDown') ? 1 : 0) + this.mobileMove.y
    const rightInput = (this.keys.has('KeyD') || this.keys.has('ArrowRight') ? 1 : 0)
      - (this.keys.has('KeyA') || this.keys.has('ArrowLeft') ? 1 : 0) + this.mobileMove.x
    const forward = this.horizontalLook()
    const right = Vector3.Cross(Vector3.Up(), forward).normalize()
    this.moveDirection.copyFrom(forward.scale(forwardInput).add(right.scale(rightInput)))
    const rawInputLength = Math.min(1, this.moveDirection.length())
    const inputAmount = rawInputLength > 0.05 ? rawInputLength : 0
    if (inputAmount > 0) this.moveDirection.normalize()
    else this.moveDirection.setAll(0)

    const sprinting = (this.keys.has('ShiftLeft') || this.keys.has('ShiftRight')) && inputAmount > 0.1 && this.stamina > 1 && this.dodgeTime <= 0
    if (sprinting) this.stamina = Math.max(0, this.stamina - dt * 17)
    else if (this.dodgeTime <= 0) this.stamina = Math.min(100, this.stamina + dt * (19 + this.armorLevel * 1.5))

    let desiredVelocity: Vector3
    if (this.dodgeTime > 0) {
      const progress = clamp01(1 - this.dodgeTime / DODGE_DURATION)
      const slideSpeed = 3.2 + (this.dodgePeakSpeed - 3.2) * (1 - smoothstep(0.08, 1, progress))
      desiredVelocity = this.dodgeDirection.scale(slideSpeed)
    } else {
      let targetSpeed = sprinting ? 7 : 4.45
      if (this.attackTime >= 0) targetSpeed *= 0.68
      desiredVelocity = this.moveDirection.scale(targetSpeed * inputAmount)
    }
    const reversing = this.velocity.lengthSquared() > 0.04 && Vector3.Dot(this.velocity, desiredVelocity) < 0
    const acceleration = this.dodgeTime > 0 ? 18 : reversing ? 34 : inputAmount > 0 ? (sprinting ? 10 : 15) : 24
    this.velocity.addInPlace(desiredVelocity.subtract(this.velocity).scale(1 - Math.exp(-dt * acceleration)))
    if (this.velocity.lengthSquared() < 0.000001) this.velocity.setAll(0)

    const before = this.player.root.position.clone()
    this.player.root.position.addInPlace(this.velocity.scale(dt))
    const planarSpeed = Math.hypot(this.velocity.x, this.velocity.z)
    let targetRotation = this.facingTarget
    if (this.dodgeTime > 0) targetRotation = Math.atan2(this.dodgeDirection.x, this.dodgeDirection.z)
    else if (this.attackTime < 0) {
      const look = this.horizontalLook()
      targetRotation = Math.atan2(look.x, look.z)
    }
    this.facingTarget = targetRotation
    let rotationDelta = targetRotation - this.player.root.rotation.y
    rotationDelta = Math.atan2(Math.sin(rotationDelta), Math.cos(rotationDelta))
    const rotationSpeed = this.attackTime >= 0 ? 16 : this.dodgeTime > 0 ? 8 : 11
    this.player.root.rotation.y += rotationDelta * (1 - Math.exp(-dt * rotationSpeed))

    if (this.dodgeTime > 0) this.player.play('Idle', 0.85, true, 0.18)
    else if (planarSpeed > 5.15) this.player.play('Run', Math.max(0.85, Math.min(1.24, planarSpeed / 6)), true, 0.3)
    else if (planarSpeed > 0.3) this.player.play('Walk', Math.max(0.72, Math.min(1.16, planarSpeed / 4.1)), true, 0.26)
    else this.player.play('Idle', 1, true, 0.32)

    if (this.attackTime >= 0) {
      const progress = clamp01(this.attackTime / ATTACK_DURATION)
      const lungeSpeed = 2.2 * smoothstep(0.12, 0.27, progress) * (1 - smoothstep(0.43, 0.6, progress))
      const lunge = new Vector3(Math.sin(this.player.root.rotation.y), 0, Math.cos(this.player.root.rotation.y))
      this.player.root.position.addInPlace(lunge.scale(dt * lungeSpeed))
    }
    this.resolvePlayerCollisions()
    this.player.root.position.y = desertHeight(this.player.root.position.x, this.player.root.position.z)
    this.distanceTravelled += distance2D(before, this.player.root.position)

    if (this.dodgeTime > 0) {
      const progress = clamp01(1 - this.dodgeTime / DODGE_DURATION)
      const weight = dodgeEnvelope(progress)
      this.weapon.rotation.x = -0.65 * weight
      this.weapon.rotation.z = -0.18 - 0.18 * weight
    } else if (this.attackTime >= 0) {
      const progress = clamp01(this.attackTime / ATTACK_DURATION)
      const windup = attackWindup(progress)
      const thrust = attackThrust(progress)
      const thrustTilt = 1.58 + this.cameraPitch * 0.45
      this.weapon.rotation.x = -0.3 * windup + thrustTilt * thrust
      this.weapon.rotation.z = -0.18 - 0.08 * thrust
    } else {
      const readyTilt = 0.85
      this.weapon.rotation.x += (readyTilt - this.weapon.rotation.x) * (1 - Math.exp(-dt * 14))
      this.weapon.rotation.z += (-0.18 - this.weapon.rotation.z) * (1 - Math.exp(-dt * 14))
    }
    const dodgeProgress = clamp01(1 - this.dodgeTime / DODGE_DURATION)
    const dodgeWeight = this.dodgeTime > 0 ? dodgeEnvelope(dodgeProgress) : 0
    this.player.content.position.y = this.playerContentBaseY - 0.34 * dodgeWeight
    this.player.content.rotation.set(0.24 * dodgeWeight, SOLDIER_FORWARD_OFFSET, -0.06 * dodgeWeight)

    const heat = Math.max(0, Math.sin(this.dayProgress() * Math.PI * 2))
    const stormMultiplier = this.campaignChapter === 4 ? 1.22 : 1
    this.hydration = Math.max(0, this.hydration - dt * (0.16 + heat * 0.24 + (sprinting ? 0.2 : 0)) * stormMultiplier)
    if (this.hydration <= 0) {
      this.health -= dt * 4
      if (this.health <= 0) this.finish(false, 'Обезвоживание. Пополняйте фляги у оазисов и колодцев, особенно перед дальним переходом.')
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
    this.invulnerableTime = 0.5
    const currentSpeed = Math.hypot(this.velocity.x, this.velocity.z)
    this.dodgePeakSpeed = Math.max(8.5, Math.min(12, currentSpeed + 3.5))
    if (this.moveDirection.lengthSquared() > 0.05) this.dodgeDirection.copyFrom(this.moveDirection).normalize()
    else if (currentSpeed > 0.2) this.dodgeDirection.copyFrom(this.velocity).normalize()
    else this.dodgeDirection.set(Math.sin(this.player.root.rotation.y), 0, Math.cos(this.player.root.rotation.y))
    this.facingTarget = Math.atan2(this.dodgeDirection.x, this.dodgeDirection.z)
    this.sound.play('slide')
    spawnSlideDust(this.scene, this.player.root.position.add(new Vector3(0, 0.12, 0)), this.dodgeDirection)
  }

  attack(): void {
    if (this.state !== 'playing' || this.attackCooldown > 0 || this.dodgeTime > 0) return
    this.attackCooldown = 0.88
    this.attackTime = 0
    this.attackHit = false
    const forward = this.horizontalLook()
    this.facingTarget = Math.atan2(forward.x, forward.z)
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
    const damage = 30 + this.spearLevel * 8
    targets.forEach((enemy) => this.damageEnemy(enemy, damage, this.player.root.position))
  }

  private updateBossPhase(enemy: DesertEnemy): void {
    if (!enemy.boss) return
    const ratio = enemy.health / enemy.maxHealth
    const phase = ratio > 0.66 ? 1 : ratio > 0.33 ? 2 : 3
    if (phase <= enemy.phase) return
    enemy.phase = phase
    enemy.specialCooldown = 0.35
    enemy.speed *= 1.12
    const archetype = ENEMY_ARCHETYPES[enemy.kind]
    this.callbacks.onAnnounce(`БОСС · ФАЗА ${phase}`, archetype.name, phase === 2 ? 'Контур нестабилен: противник вызывает подмогу.' : 'Последняя фаза: атаки ускорены, зоны поражения увеличены.')
    const addKind: EnemyKind = this.campaignChapter >= 4 ? 'brute' : this.campaignChapter >= 2 ? 'spitter' : 'runner'
    for (let i = 0; i < phase; i += 1) {
      const angle = i / phase * Math.PI * 2
      const position = enemy.model.root.position.add(new Vector3(Math.cos(angle) * 5, 0, Math.sin(angle) * 5))
      position.y = desertHeight(position.x, position.z)
      this.spawnEnemy(addKind, position)
    }
  }

  private damageEnemy(enemy: DesertEnemy, amount: number, source = this.player.root.position): void {
    if (!this.enemies.includes(enemy)) return
    enemy.health -= amount
    enemy.hitTime = 0.2
    this.sound.play('hit')
    const direction = enemy.model.root.position.subtract(source)
    spawnCombatEffect(this.scene, enemy.model.root.position.add(new Vector3(0, 0.55, 0)), direction, enemy.health <= 0)
    if (enemy.health > 0) {
      this.updateBossPhase(enemy)
      return
    }
    const wasBoss = enemy.boss
    enemy.model.dispose()
    this.enemies.splice(this.enemies.indexOf(enemy), 1)
    this.kills += 1
    const loot = wasBoss ? 6 : 1
    this.scrap += loot
    this.callbacks.onLoot(`+${loot} обломков`, 'scrap')
    if (wasBoss) {
      this.activeBoss = null
      this.completeStep()
    }
  }

  private updateSpawning(dt: number): void {
    this.spawnTimer -= dt
    const step = this.currentStep()
    const defending = step.kind === 'defend'
    const bossFight = step.kind === 'boss'
    const cap = defending ? 6 + this.campaignChapter : bossFight ? 3 + this.campaignChapter : Math.min(5, 2 + this.campaignChapter)
    if (this.spawnTimer > 0 || this.enemies.length >= cap) return
    this.spawnEnemy()
    this.spawnTimer = defending ? Math.max(1.8, 3.7 - this.campaignChapter * 0.22) + Math.random() * 1.4
      : bossFight ? 6 + Math.random() * 3 : 10 + Math.random() * 6
  }

  private spawnEnemy(requestedKind?: EnemyKind, requestedPosition?: Vector3): DesertEnemy {
    const pools: EnemyKind[][] = [
      ['stalker', 'runner'], ['runner', 'stalker'], ['runner', 'spitter', 'stalker'],
      ['spitter', 'brute', 'stalker'], ['spitter', 'brute', 'runner'], ['brute', 'spitter', 'runner'],
    ]
    const pool = pools[this.campaignChapter]
    const kind = requestedKind ?? pool[Math.floor(Math.random() * pool.length)]
    const archetype = ENEMY_ARCHETYPES[kind]
    const model = this.world.library.create('fox', archetype.scale, true, true)
    const colors: Record<EnemyKind, string> = {
      stalker: '#d99545', runner: '#e6b85f', spitter: '#8aab88', brute: '#6f6257',
      alpha: '#9d4c32', matriarch: '#567f73', warden: '#3e4a58', archon: '#654d78',
    }
    model.content.getChildMeshes().forEach((mesh) => {
      if (mesh.material instanceof PBRMaterial) {
        mesh.material.albedoColor = Color3.FromHexString(colors[kind])
        if (archetype.boss) {
          mesh.material.emissiveColor = Color3.FromHexString(colors[kind]).scale(0.16)
          mesh.material.emissiveIntensity = 0.65
        }
      }
    })
    if (kind === 'brute' || archetype.boss) {
      const accessory = this.campaignModels.enemyAccessory(archetype.scale, true, archetype.boss)
      accessory.parent = model.root
    }
    if (requestedPosition) {
      model.root.position.copyFrom(requestedPosition)
    } else {
      const angle = Math.random() * Math.PI * 2
      const distance = 20 + Math.random() * 9
      const x = this.player.root.position.x + Math.cos(angle) * distance
      const z = this.player.root.position.z + Math.sin(angle) * distance
      model.root.position.set(x, desertHeight(x, z), z)
    }
    model.play('Run', 1.1)
    const enemy: DesertEnemy = {
      kind, model, health: archetype.health, maxHealth: archetype.health,
      speed: archetype.speed, damage: archetype.damage, attackCooldown: 0.6,
      specialCooldown: 1.2 + Math.random(), hitTime: 0, phase: 1,
      ranged: archetype.ranged, boss: archetype.boss, velocity: new Vector3(),
    }
    this.enemies.push(enemy)
    if (enemy.boss) this.activeBoss = enemy
    return enemy
  }

  private createHazard(position: Vector3, radius: number, delay: number, damage: number): void {
    const root = this.campaignModels.warningZone(radius)
    const groundPosition = position.clone()
    groundPosition.y = desertHeight(groundPosition.x, groundPosition.z)
    root.position.copyFrom(groundPosition)
    this.hazards.push({ root, position: groundPosition, radius, time: delay, total: delay, damage })
  }

  private updateHazards(dt: number): void {
    for (let i = this.hazards.length - 1; i >= 0; i -= 1) {
      const hazard = this.hazards[i]
      hazard.time -= dt
      const pulse = 0.96 + Math.sin((hazard.total - hazard.time) * 18) * 0.04
      hazard.root.scaling.setAll(pulse)
      if (hazard.time > 0) continue
      if (distance2D(this.player.root.position, hazard.position) < hazard.radius) this.damagePlayer(hazard.damage)
      spawnBurst(this.scene, hazard.position.add(new Vector3(0, 0.15, 0)), Color3.FromHexString('#e56f43'), 18)
      hazard.root.dispose(false, false)
      this.hazards.splice(i, 1)
    }
  }

  private updateEnemies(dt: number): void {
    for (const enemy of [...this.enemies]) {
      enemy.attackCooldown = Math.max(0, enemy.attackCooldown - dt)
      enemy.specialCooldown = Math.max(0, enemy.specialCooldown - dt)
      enemy.hitTime = Math.max(0, enemy.hitTime - dt)
      const offset = this.player.root.position.subtract(enemy.model.root.position)
      offset.y = 0
      const distance = offset.length()
      const direction = distance > 0.001 ? offset.normalize() : new Vector3(0, 0, 1)
      const attackRange = enemy.boss ? 2.6 : enemy.kind === 'brute' ? 2.15 : 1.6
      let moving = distance > attackRange

      if (enemy.ranged && distance < 19 && enemy.specialCooldown <= 0) {
        const radius = enemy.boss ? 2.2 + enemy.phase * 0.45 : 1.45
        this.createHazard(this.player.root.position, radius, enemy.boss ? 1.15 : 0.85, enemy.damage)
        if (enemy.boss && enemy.phase >= 2) {
          const side = new Vector3(direction.z, 0, -direction.x).scale(2.8)
          this.createHazard(this.player.root.position.add(side), radius * 0.8, 1.3, enemy.damage)
        }
        enemy.specialCooldown = enemy.boss ? Math.max(2.1, 4.2 - enemy.phase * 0.45) : 3.1
        this.sound.play('swing')
      } else if (enemy.boss && !enemy.ranged && distance < 18 && enemy.specialCooldown <= 0) {
        this.createHazard(this.player.root.position, 2.3 + enemy.phase * 0.5, 1.05, enemy.damage + 3)
        enemy.specialCooldown = Math.max(2.2, 4.4 - enemy.phase * 0.5)
      }

      if (enemy.ranged && distance < 10 && distance > 5) moving = false
      if (enemy.ranged && distance <= 5) {
        direction.copyFrom(enemy.model.root.position.subtract(this.player.root.position))
        direction.y = 0
        direction.normalize()
        moving = true
      }
      const desiredVelocity = moving && enemy.hitTime <= 0 ? direction.scale(enemy.speed) : Vector3.Zero()
      const response = moving && enemy.hitTime <= 0 ? 7 : 11
      enemy.velocity.addInPlace(desiredVelocity.subtract(enemy.velocity).scale(1 - Math.exp(-dt * response)))
      const enemySpeed = Math.hypot(enemy.velocity.x, enemy.velocity.z)
      if (enemySpeed > 0.015) {
        enemy.model.root.position.addInPlace(enemy.velocity.scale(dt))
        const targetRotation = Math.atan2(enemy.velocity.x, enemy.velocity.z)
        let rotationDelta = targetRotation - enemy.model.root.rotation.y
        rotationDelta = Math.atan2(Math.sin(rotationDelta), Math.cos(rotationDelta))
        enemy.model.root.rotation.y += rotationDelta * (1 - Math.exp(-dt * 8))
      }
      if (enemySpeed > 0.15 && enemy.hitTime <= 0) {
        enemy.model.play('Run', Math.max(0.8, Math.min(1.5, enemySpeed / Math.max(1, enemy.speed) * 1.12)), true, 0.22)
      } else {
        enemy.model.play('Survey', 1, true, 0.28)
      }
      if (!moving && !enemy.ranged && distance <= attackRange + 0.25 && enemy.attackCooldown <= 0) {
        enemy.attackCooldown = enemy.kind === 'runner' ? 0.85 : enemy.kind === 'brute' ? 1.6 : 1.15
        this.damagePlayer(enemy.damage)
      } else if (!moving && enemy.ranged && distance <= 2 && enemy.attackCooldown <= 0) {
        enemy.attackCooldown = 1.4
        this.damagePlayer(Math.ceil(enemy.damage * 0.6))
      }
      enemy.model.root.position.y = desertHeight(enemy.model.root.position.x, enemy.model.root.position.z)
      const hitProgress = clamp01(1 - enemy.hitTime / 0.2)
      enemy.model.content.rotation.z = enemy.hitTime > 0 ? Math.sin(hitProgress * Math.PI) * 0.1 : 0
    }
  }

  private damagePlayer(amount: number): void {
    if (this.invulnerableTime > 0 || this.hurtCooldown > 0 || this.state !== 'playing') return
    this.hurtCooldown = 0.42
    const reduced = Math.max(1, Math.round(amount * (1 - this.armorLevel * 0.12)))
    this.health -= reduced
    this.callbacks.onDamage()
    this.sound.play('hurt')
    if (this.health <= 0) this.finish(false, 'Пустынные хищники остановили экспедицию. Контрольная точка текущей главы сохранена.')
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
    this.callbacks.onNotify(`Sentry MK-${this.sentryLevel} развёрнута. Радиус — ${17 + (this.sentryLevel - 1) * 2} метров.`, 'good')
    this.emitSnapshot()
  }

  bandage(): void {
    if (this.state !== 'playing') return
    if (this.health >= this.maxHealth()) {
      this.callbacks.onNotify('Ранений нет — волокно пока не нужно.', 'info')
      return
    }
    if (this.fiber < 3) {
      this.callbacks.onNotify('Для перевязки нужно 3 волокна.', 'danger')
      return
    }
    this.fiber -= 3
    this.health = Math.min(this.maxHealth(), this.health + 45)
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
    const range = 17 + (this.sentryLevel - 1) * 2
    const damage = 17 + (this.sentryLevel - 1) * 5
    for (const sentry of this.sentries) {
      sentry.cooldown = Math.max(0, sentry.cooldown - dt)
      const target = this.enemies.filter((enemy) => distance2D(enemy.model.root.position, sentry.root.position) < range)
        .sort((a, b) => distance2D(a.model.root.position, sentry.root.position) - distance2D(b.model.root.position, sentry.root.position))[0]
      if (!target) {
        sentry.head.rotation.y += (0 - sentry.head.rotation.y) * (1 - Math.exp(-dt * 5))
        continue
      }
      const direction = target.model.root.position.subtract(sentry.root.position)
      const targetYaw = Math.atan2(direction.x, direction.z) - sentry.root.rotation.y
      let yawDelta = targetYaw - sentry.head.rotation.y
      yawDelta = Math.atan2(Math.sin(yawDelta), Math.cos(yawDelta))
      sentry.head.rotation.y += yawDelta * (1 - Math.exp(-dt * 14))
      if (sentry.cooldown > 0) continue
      sentry.cooldown = Math.max(0.42, 0.58 - (this.sentryLevel - 1) * 0.06)
      sentry.root.computeWorldMatrix(true)
      const from = sentry.muzzle.getAbsolutePosition().clone()
      const to = target.model.root.position.add(new Vector3(0, 0.55, 0))
      spawnTracer(this.scene, from, to, Color3.FromHexString('#ffd18a'))
      this.sound.play('turret')
      this.damageEnemy(target, damage, sentry.root.position)
    }
  }

  private repairCost(step: CampaignStep): { scrap: number, fiber: number } {
    return { scrap: step.scrapCost ?? 0, fiber: step.fiberCost ?? 0 }
  }

  private activateRepair(): void {
    const step = this.currentStep()
    if (step.kind !== 'repair') return
    const cost = this.repairCost(step)
    if (this.scrap < cost.scrap || this.fiber < cost.fiber) {
      this.callbacks.onNotify(`Для ремонта: ${cost.scrap} обломков и ${cost.fiber} волокна. С вами: ${this.scrap} и ${this.fiber}.`, 'danger')
      return
    }
    this.scrap -= cost.scrap
    this.fiber -= cost.fiber
    this.relayActive = true
    this.relay.setActive(true)
    this.sound.play('core')
    this.completeStep()
  }

  interact(): void {
    if (this.state !== 'playing' || this.interactCooldown > 0) return
    this.interactCooldown = 0.25
    const campaignObject = this.nearestCampaignObject()
    if (campaignObject) {
      if (campaignObject.kind === 'repair') this.activateRepair()
      else {
        campaignObject.used = true
        if (campaignObject.kind === 'salvage') {
          campaignObject.root.setEnabled(false)
          this.scrap += 2
          this.callbacks.onLoot('+2 обломка · архив получен', 'scrap')
        } else {
          this.callbacks.onLoot('Устройство активировано', 'scrap')
          this.sound.play('core')
        }
        this.campaignCount += 1
        if (this.campaignCount >= (this.currentStep().count ?? 1)) this.completeStep()
      }
      this.emitSnapshot()
      return
    }
    if (this.nearRelay()) {
      if (this.currentStep().kind === 'repair' && this.currentChapter().location === 'relay') this.activateRepair()
      else this.callbacks.onNotify(this.relayActive ? 'Северный ретранслятор работает. Следующая цель отмечена в журнале.' : 'Ретранслятор пока не требует ремонта.', 'info')
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

  private nearestCampaignObject(): CampaignInteractable | undefined {
    let nearest: CampaignInteractable | undefined
    let best = 3.2
    for (const object of this.campaignObjects) {
      if (object.used) continue
      const distance = distance2D(object.root.position, this.player.root.position)
      if (distance < best) {
        nearest = object
        best = distance
      }
    }
    return nearest
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
    this.releaseMouseLook()
    this.keys.clear()
    this.mobileMove.set(0, 0)
    this.clearHazards()
    this.sound.play(victory ? 'win' : 'lose')
    const stats: DesertEndStats = { victory, reason, elapsed: this.elapsed, kills: this.kills, distance: this.distanceTravelled }
    this.callbacks.onState(this.state, stats)
    this.emitSnapshot()
  }

  private emitSnapshot(): void {
    if (!this.player || !this.relay || !this.currentChapter()) return
    const step = this.currentStep()
    const location = this.currentLocation()
    const distance = Math.hypot(this.player.root.position.x - location.x, this.player.root.position.z - location.z)
    const campaignObject = this.nearestCampaignObject()
    const nearRelay = this.nearRelay()
    const resource = this.nearestResource()
    let prompt = ''
    let promptDetail = ''
    if (campaignObject) {
      prompt = campaignObject.label
      promptDetail = campaignObject.kind === 'repair' ? 'E · запустить собственный маяк' : `${this.campaignCount + 1} / ${step.count ?? 1} · E взаимодействие`
    } else if (nearRelay && step.kind === 'repair' && this.currentChapter().location === 'relay') {
      const cost = this.repairCost(step)
      prompt = step.title
      promptDetail = `${cost.scrap} обломков · ${cost.fiber} волокна`
    } else if (resource) {
      prompt = resource.kind === 'scrap' ? 'Разобрать обломки' : resource.kind === 'fiber' ? 'Собрать волокно' : 'Наполнить фляги'
      promptDetail = resource.kind === 'water' ? 'Вода и гидратация восстановятся' : 'E · взаимодействие'
    }
    let progress = 0
    let target = 1
    if (step.kind === 'salvage' || step.kind === 'activate') {
      progress = this.campaignCount
      target = step.count ?? 1
    } else if (step.kind === 'defend') {
      target = step.duration ?? 40
      progress = target - (this.campaignTimer ?? target)
    } else if (step.kind === 'boss' && this.activeBoss) {
      target = this.activeBoss.maxHealth
      progress = target - this.activeBoss.health
    } else if (step.kind === 'travel') {
      target = 100
      progress = (1 - clamp01(distance / 400)) * 100
    }
    let detail = step.detail
    if (step.kind === 'defend') detail = `${Math.ceil(this.campaignTimer ?? 0)} с · ${distance < 31 ? 'сигнал стабилен' : 'вернитесь в зону'}`
    if (step.kind === 'salvage' || step.kind === 'activate') detail = `${this.campaignCount} / ${step.count ?? 1} · ${step.detail}`
    const forward = this.world.cameraForward()
    const dayProgress = this.dayProgress()
    const repair = step.kind === 'repair' ? this.repairCost(step) : { scrap: 0, fiber: 0 }
    const snapshot: DesertSnapshot = {
      state: this.state,
      health: this.health,
      maxHealth: this.maxHealth(),
      stamina: this.stamina,
      hydration: this.hydration,
      scrap: this.scrap,
      fiber: this.fiber,
      water: this.water,
      kills: this.kills,
      enemies: this.enemies.length,
      elapsed: this.elapsed,
      day: Math.floor(this.elapsed / DAY_DURATION) + 1,
      dayProgress,
      temperature: 18 + Math.max(0, Math.sin(dayProgress * Math.PI * 2)) * 27,
      heading: (Math.atan2(forward.x, forward.z) * 180 / Math.PI + 360) % 360,
      x: this.player.root.position.x,
      z: this.player.root.position.z,
      prompt,
      promptDetail,
      nearRelay,
      relayActive: this.relayActive,
      signalTime: this.campaignTimer ?? 0,
      canSentry: this.scrap >= 8 && this.fiber >= 4,
      canBandage: this.fiber >= 3 && this.health < this.maxHealth(),
      canRelay: this.scrap >= repair.scrap && this.fiber >= repair.fiber,
      dodgeCooldown: this.dodgeCooldown,
      campaign: {
        chapter: this.campaignChapter + 1,
        totalChapters: CAMPAIGN_CHAPTERS.length,
        act: this.currentChapter().act,
        chapterTitle: this.currentChapter().title,
        locationName: location.name,
        stepTitle: step.title,
        stepDetail: detail,
        progress,
        target,
        distance,
        targetX: location.x,
        targetZ: location.z,
        spearLevel: this.spearLevel,
        sentryLevel: this.sentryLevel,
        armorLevel: this.armorLevel,
        bossName: this.activeBoss ? ENEMY_ARCHETYPES[this.activeBoss.kind].name : '',
        bossHealth: this.activeBoss?.health ?? 0,
        bossMaxHealth: this.activeBoss?.maxHealth ?? 0,
        bossPhase: this.activeBoss?.phase ?? 0,
      },
    }
    this.callbacks.onSnapshot(snapshot)
  }

  private updateLook(dx: number, dy: number): void {
    this.cameraYaw -= dx * 0.0024
    this.cameraPitch = Math.max(-0.85, Math.min(1.42, this.cameraPitch + dy * 0.0024))
  }

  private requestMouseLook(): void {
    if (window.matchMedia('(pointer: coarse)').matches || document.pointerLockElement === this.canvas) return
    try {
      const request = this.canvas.requestPointerLock() as unknown as Promise<void> | undefined
      request?.catch(() => { /* Hover mouse look remains available without pointer lock. */ })
    } catch { /* Pointer lock is an enhancement, not a requirement for input. */ }
  }

  private releaseMouseLook(): void {
    if (document.pointerLockElement === this.canvas) document.exitPointerLock()
  }

  private bindInput(): void {
    window.addEventListener('keydown', (event) => {
      if (['Space', 'ArrowUp', 'ArrowDown', 'ArrowLeft', 'ArrowRight'].includes(event.code)) event.preventDefault()
      this.keys.add(event.code)
      if (event.repeat) return
      if (event.code === 'KeyP' || event.code === 'Escape') this.togglePause()
      if (event.code === 'KeyJ') this.journal()
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
    let pointerLockWasActive = false
    document.addEventListener('pointerlockchange', () => {
      if (document.pointerLockElement === this.canvas) {
        pointerLockWasActive = true
        return
      }
      const shouldPause = pointerLockWasActive && this.state === 'playing'
      pointerLockWasActive = false
      if (shouldPause) this.pause()
    })
    document.addEventListener('mousemove', (event) => {
      if (document.pointerLockElement === this.canvas && this.state === 'playing') {
        this.updateLook(event.movementX, event.movementY)
      }
    })
    this.canvas.addEventListener('pointerdown', (event) => {
      if (this.state !== 'playing') return
      if (event.pointerType === 'mouse') {
        if (event.button === 0) {
          this.requestMouseLook()
          this.attack()
        }
        return
      }
      this.draggingPointer = event.pointerId
      this.dragX = event.clientX
      this.dragY = event.clientY
      this.canvas.setPointerCapture(event.pointerId)
    })
    this.canvas.addEventListener('pointermove', (event) => {
      if (this.state !== 'playing') return
      if (event.pointerType === 'mouse') {
        // Fallback for browsers that deny pointer lock: no button is required.
        if (document.pointerLockElement !== this.canvas) this.updateLook(event.movementX, event.movementY)
        return
      }
      if (event.pointerId !== this.draggingPointer) return
      const dx = event.clientX - this.dragX
      const dy = event.clientY - this.dragY
      this.dragX = event.clientX
      this.dragY = event.clientY
      this.updateLook(dx * 2.1, dy * 1.65)
    })
    const endTouch = (event: PointerEvent): void => {
      if (event.pointerId === this.draggingPointer) this.draggingPointer = null
    }
    this.canvas.addEventListener('pointerup', endTouch)
    this.canvas.addEventListener('pointercancel', endTouch)
    this.canvas.addEventListener('lostpointercapture', endTouch)
    this.canvas.addEventListener('contextmenu', (event) => event.preventDefault())
  }
}
