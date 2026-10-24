import {
  ArcRotateCamera,
  Color3,
  Color4,
  CubeTextureCreateFromPrefilteredData,
  DefaultRenderingPipeline,
  DirectionalLight,
  DynamicTexture,
  FreeCamera,
  HemisphericLight,
  ImageProcessingConfiguration,
  MeshBuilder,
  ParticleSystem,
  Scene,
  ShadowGenerator,
  Texture,
  TransformNode,
  Vector3,
  VertexData,
} from '@babylonjs/core'
import { SkyMaterial, PBRCustomMaterial } from '@babylonjs/materials'
import { assetUrl } from '../game/assets'
import { ModelLibrary, createRadialTexture } from '../game/models'
import { DesertModels } from './DesertModels'
import { DesertSurfaceEffects } from './SurfaceEffects'
import { installSandShader } from './DesertShaders'
import { DesertDressing } from './DesertDressing'
import type { ResourceKind } from './types'

const CHUNK_SIZE = 64
const CHUNK_SUBDIVISIONS = 24

function clamp01(value: number): number {
  return Math.max(0, Math.min(1, value))
}

function smoothstep(edge0: number, edge1: number, value: number): number {
  const t = clamp01((value - edge0) / (edge1 - edge0))
  return t * t * (3 - 2 * t)
}

function hash(x: number, z: number, seed = 0): number {
  const value = Math.sin(x * 127.1 + z * 311.7 + seed * 73.31) * 43758.5453123
  return value - Math.floor(value)
}

function noise(x: number, z: number): number {
  const ix = Math.floor(x)
  const iz = Math.floor(z)
  const fx = x - ix
  const fz = z - iz
  const ux = fx * fx * (3 - 2 * fx)
  const uz = fz * fz * (3 - 2 * fz)
  const a = hash(ix, iz)
  const b = hash(ix + 1, iz)
  const c = hash(ix, iz + 1)
  const d = hash(ix + 1, iz + 1)
  return (a * (1 - ux) + b * ux) * (1 - uz) + (c * (1 - ux) + d * ux) * uz
}

export function desertHeight(x: number, z: number): number {
  const broad = (noise(x * 0.006 + 30, z * 0.006 - 18) - 0.48) * 22
  const ridge = Math.sin(x * 0.025 + noise(x * 0.009, z * 0.009) * 5) * 2.2
  const cross = Math.sin(z * 0.018 + x * 0.004) * 1.2
  const detail = (noise(x * 0.12, z * 0.12) - 0.5) * 0.3
  const dune = Math.sin(x * 0.052 + z * 0.018 + noise(x * 0.012, z * 0.012) * 2.6) * 1.5
    + Math.sin(z * 0.075 - x * 0.012) * 0.55
  const flatten = smoothstep(18, 68, Math.hypot(x, z))
  const duneMask = smoothstep(20, 52, Math.hypot(x, z))
  const oasis = smoothstep(5.3, 9.5, Math.hypot(x - 22, z + 17))
  return ((broad + ridge + cross + detail) * flatten + dune * duneMask) * oasis - 0.16 * (1 - oasis)
}

export interface DesertResource {
  id: string
  kind: ResourceKind
  root: TransformNode
  amount: number
  collider?: DesertCollider
}

export interface DesertCollider {
  x: number
  z: number
  radius: number
}

interface DesertChunk {
  root: TransformNode
  resources: DesertResource[]
  colliders: DesertCollider[]
}

function tiledTexture(scene: Scene, name: 'sand.jpg' | 'rock-normal.png', repeat: number): Texture {
  const texture = new Texture(assetUrl(name), scene)
  texture.uScale = repeat
  texture.vScale = repeat
  texture.wrapU = Texture.WRAP_ADDRESSMODE
  texture.wrapV = Texture.WRAP_ADDRESSMODE
  texture.anisotropicFilteringLevel = 16
  return texture
}

function sandNormalTexture(scene: Scene, size: number): DynamicTexture {
  const texture = new DynamicTexture('wind rippled sand micro normal', size, scene, true)
  const ctx = texture.getContext() as CanvasRenderingContext2D
  const image = ctx.createImageData(size, size)
  for (let y = 0; y < size; y += 1) {
    for (let x = 0; x < size; x += 1) {
      const u = x / size * Math.PI * 10
      const v = y / size * Math.PI * 10
      const grainX = (hash(x * 0.71, y * 0.71, 4) - 0.5) * 0.28
      const grainY = (hash(x * 0.71, y * 0.71, 9) - 0.5) * 0.28
      const dx = Math.cos(u + Math.sin(v * 0.43) * 1.4) * 0.22 + grainX
      const dy = Math.cos(v * 0.72 + Math.sin(u * 0.31)) * 0.1 + grainY
      const index = (y * size + x) * 4
      image.data[index] = Math.max(0, Math.min(255, 128 - dx * 110))
      image.data[index + 1] = Math.max(0, Math.min(255, 128 - dy * 110))
      image.data[index + 2] = 238
      image.data[index + 3] = 255
    }
  }
  ctx.putImageData(image, 0, 0)
  texture.update()
  texture.wrapU = Texture.WRAP_ADDRESSMODE
  texture.wrapV = Texture.WRAP_ADDRESSMODE
  texture.uScale = 18
  texture.vScale = 18
  texture.anisotropicFilteringLevel = 16
  return texture
}

export class DesertWorld {
  readonly camera: ArcRotateCamera
  readonly firstPersonCamera: FreeCamera
  readonly shadows: ShadowGenerator
  readonly models: DesertModels
  readonly library: ModelLibrary
  readonly resources = new Set<DesertResource>()
  readonly colliders = new Set<DesertCollider>()
  readonly highQuality: boolean
  private readonly sun: DirectionalLight
  private readonly hemi: HemisphericLight
  private readonly skyMaterial: SkyMaterial
  private readonly groundMaterial: PBRCustomMaterial
  private readonly dressing: DesertDressing
  private readonly chunks = new Map<string, DesertChunk>()
  private readonly depleted = new Set<string>()
  private readonly persistentRoots: TransformNode[] = []
  private readonly surface: DesertSurfaceEffects
  private readonly dust: ParticleSystem
  private currentChunkX = Number.NaN
  private currentChunkZ = Number.NaN
  private elapsed = 0
  private loaded = false
  private readonly dustEmitter = new Vector3()
  private readonly firstPersonDirection = new Vector3()
  private readonly firstPersonTarget = new Vector3()
  private readonly nightFogColor = new Color3(0.055, 0.085, 0.13)
  private readonly dayFogColor = new Color3(0.46, 0.6, 0.68)
  private readonly nightSunColor = Color3.FromHexString('#8298b5')
  private readonly daySunColor = Color3.FromHexString('#ffdfac')

  constructor(private scene: Scene) {
    this.highQuality = !window.matchMedia('(pointer: coarse)').matches
      && new URLSearchParams(window.location.search).get('quality') !== 'low'
    scene.clearColor = new Color4(0.46, 0.6, 0.68, 1)
    scene.ambientColor = new Color3(0.2, 0.18, 0.14)
    scene.fogMode = Scene.FOGMODE_EXP2
    scene.fogColor = new Color3(0.46, 0.6, 0.68)
    scene.fogDensity = 0.0048
    scene.environmentTexture = CubeTextureCreateFromPrefilteredData(assetUrl('outdoor.env'), scene, '.env')
    scene.environmentIntensity = 0.7
    scene.imageProcessingConfiguration.toneMappingEnabled = true
    scene.imageProcessingConfiguration.toneMappingType = ImageProcessingConfiguration.TONEMAPPING_ACES
    scene.imageProcessingConfiguration.exposure = 1.08
    scene.imageProcessingConfiguration.contrast = 1.22

    this.camera = new ArcRotateCamera('desert follower', 0.2, 1.05, 12, new Vector3(0, 1.4, 0), scene)
    this.camera.minZ = 0.1
    this.camera.maxZ = 1000
    this.camera.lowerBetaLimit = 0.28
    this.camera.upperBetaLimit = 1.48
    this.camera.fov = 0.82
    this.camera.panningSensibility = 0
    this.firstPersonCamera = new FreeCamera('survivor first person view', new Vector3(0, 1.6, 0), scene)
    this.firstPersonCamera.minZ = 0.04
    this.firstPersonCamera.maxZ = 1000
    this.firstPersonCamera.fov = 1.1
    this.firstPersonCamera.rotation.set(0, 0, 0)

    this.hemi = new HemisphericLight('desert skylight', new Vector3(0.15, 1, 0.1), scene)
    this.hemi.diffuse = Color3.FromHexString('#d9e2dc')
    this.hemi.groundColor = Color3.FromHexString('#70523b')
    this.hemi.intensity = 0.55
    this.sun = new DirectionalLight('desert sun', new Vector3(-0.55, -1, -0.35), scene)
    this.sun.position = new Vector3(35, 55, 25)
    this.sun.diffuse = Color3.FromHexString('#ffdfac')
    this.sun.specular = Color3.FromHexString('#ffe8c5')
    this.sun.intensity = 1.35
    this.sun.shadowMinZ = 1
    this.sun.shadowMaxZ = 90
    this.shadows = new ShadowGenerator(this.highQuality ? 4096 : 1024, this.sun)
    this.shadows.usePercentageCloserFiltering = true
    this.shadows.filteringQuality = this.highQuality ? ShadowGenerator.QUALITY_HIGH : ShadowGenerator.QUALITY_MEDIUM
    this.shadows.bias = 0.0004
    this.shadows.normalBias = 0.03
    this.shadows.darkness = 0.34
    this.shadows.setTransparencyShadow(false)

    this.groundMaterial = new PBRCustomMaterial('wind carved desert sand · GLSL', scene)
    installSandShader(this.groundMaterial)
    this.groundMaterial.albedoColor = new Color3(0.98, 0.93, 0.82)
    this.groundMaterial.albedoTexture = tiledTexture(scene, 'sand.jpg', 18)
    this.groundMaterial.bumpTexture = sandNormalTexture(scene, this.highQuality ? 512 : 256)
    this.groundMaterial.bumpTexture.level = 0.28
    this.groundMaterial.roughness = 0.94
    this.groundMaterial.metallic = 0
    this.groundMaterial.environmentIntensity = 0.4

    this.skyMaterial = new SkyMaterial('desert physical sky', scene)
    this.skyMaterial.useSunPosition = true
    this.skyMaterial.sunPosition = this.sun.position.clone()
    this.skyMaterial.turbidity = 2.4
    this.skyMaterial.rayleigh = 2.4
    this.skyMaterial.mieCoefficient = 0.004
    this.skyMaterial.mieDirectionalG = 0.86
    this.skyMaterial.cloudiness = 0.18
    this.skyMaterial.dithering = true
    this.skyMaterial.backFaceCulling = false
    const sky = MeshBuilder.CreateBox('desert sky dome', { size: 900 }, scene)
    sky.material = this.skyMaterial
    sky.infiniteDistance = true
    sky.applyFog = false
    sky.isPickable = false

    const pipeline = new DefaultRenderingPipeline('desert cinematic pipeline', true, scene, [this.camera, this.firstPersonCamera])
    pipeline.samples = this.highQuality ? 4 : 1
    pipeline.fxaaEnabled = true
    pipeline.sharpenEnabled = this.highQuality
    pipeline.sharpen.edgeAmount = 0.22
    pipeline.sharpen.colorAmount = 1
    pipeline.bloomEnabled = true
    pipeline.bloomThreshold = 0.82
    pipeline.bloomWeight = 0.28
    pipeline.bloomKernel = 48
    pipeline.bloomScale = 0.5
    pipeline.imageProcessingEnabled = true

    this.library = new ModelLibrary(scene, this.shadows)
    this.models = new DesertModels(scene, this.shadows)
    this.dressing = new DesertDressing(scene, this.models, this.highQuality)
    this.surface = new DesertSurfaceEffects(scene, this.highQuality)
    this.dust = new ParticleSystem('wind borne sand', this.highQuality ? 500 : 240, scene)
    this.dust.particleTexture = createRadialTexture(scene, 'sand mote', 'rgba(239,216,170,.5)', 'rgba(220,196,150,0)')
    this.dustEmitter.set(0, 2, 0)
    this.dust.emitter = this.dustEmitter
    this.dust.minEmitBox = new Vector3(-35, -2, -35)
    this.dust.maxEmitBox = new Vector3(35, 9, 35)
    this.dust.color1 = new Color4(0.78, 0.68, 0.48, 0.18)
    this.dust.color2 = new Color4(0.62, 0.54, 0.39, 0.1)
    this.dust.colorDead = new Color4(0.6, 0.52, 0.38, 0)
    this.dust.minSize = 0.025
    this.dust.maxSize = 0.16
    this.dust.minLifeTime = 3
    this.dust.maxLifeTime = 7
    this.dust.emitRate = this.highQuality ? 70 : 35
    this.dust.direction1 = new Vector3(2.5, 0.04, 0.7)
    this.dust.direction2 = new Vector3(5.5, 0.35, 1.5)
    this.dust.minEmitPower = 0.4
    this.dust.maxEmitPower = 1.1
    this.dust.blendMode = ParticleSystem.BLENDMODE_STANDARD
    this.dust.start()
  }

  async load(): Promise<void> {
    await this.library.load()
    this.createStarterArea()
    this.updateStreaming(new Vector3(6, 0, 8), true)
    this.loaded = true
  }

  private createStarterArea(): void {
    const oasis = this.models.oasis(5.5)
    oasis.position.set(22, desertHeight(22, -17), -17)
    this.addResource('water', 22, -17, 'starter-oasis', oasis)
    const oasisGreen = new TransformNode('oasis salt tolerant planting', this.scene)
    for (let i = 0; i < 8; i += 1) {
      const angle = i / 8 * Math.PI * 2 + 0.18
      const radius = 6.45 + (i % 2) * 0.35
      const x = 22 + Math.cos(angle) * radius
      const z = -17 + Math.sin(angle) * radius
      const plant = this.models.agave(0.42 + (i % 3) * 0.08)
      plant.parent = oasisGreen
      plant.position.set(x, desertHeight(x, z) + 0.02, z)
      plant.rotation.y = angle + i * 0.4
    }
    this.persistentRoots.push(oasisGreen)
    this.addResource('scrap', 4.5, 4, 'starter-scrap-a')
    this.addResource('scrap', -6, -3.5, 'starter-scrap-b')
    this.addResource('scrap', 10, -6, 'starter-scrap-c')
    this.addResource('scrap', -10, 8, 'starter-scrap-d')
    this.addResource('fiber', -4.5, 6.5, 'starter-fiber-a')
    this.addResource('fiber', 13, 4, 'starter-fiber-b')
    this.addResource('fiber', -12, -8, 'starter-fiber-c')
    const deadTree = this.library.create('deadTree', 4.2)
    deadTree.root.position.set(-16, desertHeight(-16, 14), 14)
    deadTree.root.rotation.y = 0.7
    this.persistentRoots.push(deadTree.root)
    this.colliders.add({ x: -16, z: 14, radius: 0.65 })
    this.persistentRoots.push(this.dressing.createStarter(desertHeight))
    this.persistentRoots.forEach((root) => this.freezeStatic(root))
  }

  /** Freeze transforms only after a static assembly has reached its final pose. */
  freezeStatic(root: TransformNode): void {
    root.computeWorldMatrix(true)
    root.getChildMeshes(false).forEach((mesh) => {
      mesh.computeWorldMatrix(true)
      mesh.isPickable = false
      mesh.freezeWorldMatrix()
    })
    root.freezeWorldMatrix()
  }

  addResource(kind: ResourceKind, x: number, z: number, id: string, existingRoot?: TransformNode, chunk?: DesertChunk): DesertResource {
    const root = existingRoot ?? (kind === 'scrap' ? this.models.salvage() : kind === 'fiber' ? this.models.cactus(2.1 + hash(x, z) * 0.8) : this.models.well())
    root.position.set(x, desertHeight(x, z), z)
    root.rotation.y = hash(x, z, 4) * Math.PI * 2
    const resource: DesertResource = { id, kind, root, amount: 1 }
    this.resources.add(resource)
    if (!chunk) this.persistentRoots.push(root)
    const radius = kind === 'fiber' ? 0.42 : kind === 'water' && !existingRoot ? 1.02 : 0
    if (radius > 0) {
      const collider = { x, z, radius }
      resource.collider = collider
      this.colliders.add(collider)
      chunk?.colliders.push(collider)
    }
    return resource
  }

  reset(playerPosition: Vector3): void {
    for (const key of [...this.chunks.keys()]) this.removeChunk(key)
    this.persistentRoots.forEach((root) => root.dispose(false, false))
    this.persistentRoots.length = 0
    this.resources.clear()
    this.colliders.clear()
    this.depleted.clear()
    this.surface.reset()
    this.currentChunkX = Number.NaN
    this.currentChunkZ = Number.NaN
    this.createStarterArea()
    this.updateStreaming(playerPosition, true)
  }

  consume(resource: DesertResource): void {
    resource.amount = 0
    resource.root.setEnabled(false)
    if (resource.collider) this.colliders.delete(resource.collider)
    this.depleted.add(resource.id)
    this.resources.delete(resource)
  }

  private createChunk(cx: number, cz: number): DesertChunk {
    const key = `${cx}:${cz}`
    const root = new TransformNode(`desert chunk ${key}`, this.scene)
    const ground = MeshBuilder.CreateGround(`streamed dunes ${key}`, {
      width: CHUNK_SIZE, height: CHUNK_SIZE, subdivisions: this.highQuality ? 48 : CHUNK_SUBDIVISIONS, updatable: true,
    }, this.scene)
    ground.parent = root
    ground.position.set(cx * CHUNK_SIZE, 0, cz * CHUNK_SIZE)
    const positions = ground.getVerticesData('position')
    const indices = ground.getIndices()
    if (positions && indices) {
      for (let i = 0; i < positions.length; i += 3) {
        positions[i + 1] = desertHeight(positions[i] + cx * CHUNK_SIZE, positions[i + 2] + cz * CHUNK_SIZE)
      }
      const normals: number[] = []
      VertexData.ComputeNormals(positions, indices, normals)
      ground.updateVerticesData('position', positions)
      ground.updateVerticesData('normal', normals)
    }
    ground.material = this.groundMaterial
    ground.receiveShadows = true
    ground.isPickable = false
    const chunk: DesertChunk = { root, resources: [], colliders: [] }
    const objectCount = this.highQuality ? 8 : 5
    for (let i = 0; i < objectCount; i += 1) {
      const x = (cx + hash(cx, cz, i * 5 + 1) - 0.5) * CHUNK_SIZE
      const z = (cz + hash(cx, cz, i * 5 + 2) - 0.5) * CHUNK_SIZE
      if (Math.hypot(x, z) < 30) continue
      const selector = hash(cx, cz, i * 5 + 3)
      const id = `${key}:${i}`
      if (this.depleted.has(id)) continue
      if (selector < 0.16) {
        const rock = this.models.rock(0.45 + hash(x, z) * 0.8)
        rock.parent = root
        rock.position.set(x, desertHeight(x, z), z)
        rock.rotation.y = hash(x, z, 8) * Math.PI * 2
        const collider = { x, z, radius: 0.55 }
        this.colliders.add(collider)
        chunk.colliders.push(collider)
      } else if (selector < 0.32) {
        const resource = this.addResource('fiber', x, z, id, undefined, chunk)
        resource.root.parent = root
        chunk.resources.push(resource)
      } else if (selector < 0.43) {
        const resource = this.addResource('scrap', x, z, id, undefined, chunk)
        resource.root.parent = root
        chunk.resources.push(resource)
      } else if (selector < 0.53) {
        const agave = this.models.agave(0.75 + hash(x, z) * 0.65)
        agave.parent = root
        agave.position.set(x, desertHeight(x, z), z)
        agave.rotation.y = hash(x, z, 9) * Math.PI * 2
      } else if (selector < 0.61) {
        const tree = this.library.create('deadTree', 3.2 + hash(x, z) * 2)
        tree.root.parent = root
        tree.root.position.set(x, desertHeight(x, z), z)
        tree.root.rotation.y = hash(x, z, 10) * Math.PI * 2
      } else if (selector < 0.67) {
        const ruin = this.models.ruins()
        ruin.parent = root
        ruin.position.set(x, desertHeight(x, z), z)
        ruin.rotation.y = hash(x, z, 11) * Math.PI * 2
        const collider = { x, z, radius: 1.5 }
        this.colliders.add(collider)
        chunk.colliders.push(collider)
      } else if (selector < 0.7) {
        const resource = this.addResource('water', x, z, id, undefined, chunk)
        resource.root.parent = root
        chunk.resources.push(resource)
      } else {
        const rock = this.models.rock(0.35 + hash(x, z) * 0.55)
        rock.parent = root
        rock.position.set(x, desertHeight(x, z), z)
      }
    }
    this.dressing.populateChunk(root, cx, cz, desertHeight)
    this.freezeStatic(root)
    this.chunks.set(key, chunk)
    return chunk
  }

  private removeChunk(key: string): void {
    const chunk = this.chunks.get(key)
    if (!chunk) return
    chunk.resources.forEach((resource) => this.resources.delete(resource))
    chunk.colliders.forEach((collider) => this.colliders.delete(collider))
    chunk.root.dispose(false, false)
    this.chunks.delete(key)
  }

  updateStreaming(position: Vector3, force = false): void {
    const cx = Math.round(position.x / CHUNK_SIZE)
    const cz = Math.round(position.z / CHUNK_SIZE)
    if (!force && cx === this.currentChunkX && cz === this.currentChunkZ) return
    this.currentChunkX = cx
    this.currentChunkZ = cz
    const radius = this.highQuality ? 3 : 2
    const required = new Set<string>()
    for (let x = cx - radius; x <= cx + radius; x += 1) {
      for (let z = cz - radius; z <= cz + radius; z += 1) {
        const key = `${x}:${z}`
        required.add(key)
        if (!this.chunks.has(key)) this.createChunk(x, z)
      }
    }
    for (const key of this.chunks.keys()) {
      if (!required.has(key)) this.removeChunk(key)
    }
  }

  updateFirstPersonCamera(head: Vector3, yaw: number, pitch: number): void {
    this.scene.activeCamera = this.firstPersonCamera
    const aspect = this.scene.getEngine().getRenderWidth() / Math.max(1, this.scene.getEngine().getRenderHeight())
    const narrow = clamp01((1 - aspect) / 0.55)
    this.firstPersonCamera.fov = 1.1 + narrow * 0.48
    const horizontal = Math.cos(pitch)
    this.firstPersonDirection.set(-Math.cos(yaw) * horizontal, -Math.sin(pitch), -Math.sin(yaw) * horizontal).normalize()
    this.firstPersonCamera.position.copyFrom(head)
    this.firstPersonTarget.copyFrom(head).addInPlace(this.firstPersonDirection)
    this.firstPersonCamera.setTarget(this.firstPersonTarget)
    this.firstPersonCamera.getViewMatrix(true)
  }

  cameraForward(): Vector3 {
    if (this.scene.activeCamera === this.firstPersonCamera) return this.firstPersonCamera.getDirection(Vector3.Forward())
    return this.camera.target.subtract(this.camera.position)
  }

  menuCamera(dt: number): void {
    this.scene.activeCamera = this.camera
    this.elapsed += dt
    this.camera.alpha = 0.55 + this.elapsed * 0.025
    this.camera.beta = 1.08
    this.camera.radius = 25
    this.camera.target.set(0, 3.2, 0)
  }

  updateSurface(dt: number, position: Vector3, velocity: Vector3, sliding: boolean): void {
    this.surface.update(dt, position, velocity, sliding)
  }

  update(dt: number, dayProgress: number, playerPosition: Vector3): void {
    this.models.update(dt)
    const waterNormal = this.models.water.bumpTexture
    if (waterNormal instanceof Texture) {
      waterNormal.uOffset = (waterNormal.uOffset + dt * 0.018) % 1
      waterNormal.vOffset = (waterNormal.vOffset + dt * 0.011) % 1
    }
    if (this.loaded) this.updateStreaming(playerPosition)
    this.dustEmitter.set(playerPosition.x, playerPosition.y + 2, playerPosition.z)
    const angle = dayProgress * Math.PI * 2
    const elevation = Math.sin(angle)
    const daylight = clamp01(elevation * 1.5 + 0.18)
    Color3.LerpToRef(this.nightFogColor, this.dayFogColor, daylight, this.scene.fogColor)
    this.scene.clearColor.set(this.scene.fogColor.r, this.scene.fogColor.g, this.scene.fogColor.b, 1)
    this.scene.fogDensity = 0.0048 + (1 - daylight) * 0.002
    this.sun.position.set(Math.cos(angle) * 65, Math.max(5, elevation * 72), 28)
    this.sun.direction.set(-Math.cos(angle) * 0.7, -Math.max(0.35, elevation), -0.35).normalize()
    this.sun.intensity = 0.08 + daylight * 1.28
    Color3.LerpToRef(this.nightSunColor, this.daySunColor, daylight, this.sun.diffuse)
    this.hemi.intensity = 0.16 + daylight * 0.4
    this.skyMaterial.sunPosition = this.sun.position
    this.skyMaterial.luminance = 0.22 + daylight * 0.78
    this.skyMaterial.rayleigh = 0.65 + daylight * 1.75
    this.scene.environmentIntensity = 0.28 + daylight * 0.44
  }
}
