import {
  AssetContainer,
  Color3,
  DynamicTexture,
  InstantiatedEntries,
  LoadAssetContainerAsync,
  Mesh,
  MeshBuilder,
  PBRMaterial,
  ParticleSystem,
  PointLight,
  Quaternion,
  Scene,
  ShadowGenerator,
  StandardMaterial,
  Texture,
  TransformNode,
  Vector3,
  type AnimationGroup,
  type Node,
  type Observer,
} from '@babylonjs/core'
import '@babylonjs/loaders/glTF'
import { assetUrl, type AssetName } from './assets'

export interface MaterialLibrary {
  steel: PBRMaterial
  darkSteel: PBRMaterial
  rust: PBRMaterial
  copper: PBRMaterial
  concrete: PBRMaterial
  rock: PBRMaterial
  wood: PBRMaterial
  bark: PBRMaterial
  foliage: PBRMaterial
  glass: PBRMaterial
  lens: PBRMaterial
  crystal: PBRMaterial
  amber: PBRMaterial
}

function texture(scene: Scene, name: AssetName, repeat = 1): Texture {
  const result = new Texture(assetUrl(name), scene)
  result.uScale = repeat
  result.vScale = repeat
  result.anisotropicFilteringLevel = 8
  return result
}

function pbr(scene: Scene, name: string, color: Color3, metallic: number, roughness: number): PBRMaterial {
  const material = new PBRMaterial(name, scene)
  material.albedoColor = color
  material.metallic = metallic
  material.roughness = roughness
  material.environmentIntensity = 0.9
  return material
}

function metalAlbedo(scene: Scene): DynamicTexture {
  const map = new DynamicTexture('brushed steel albedo', 512, scene, true)
  const ctx = map.getContext() as CanvasRenderingContext2D
  const gradient = ctx.createLinearGradient(0, 0, 512, 512)
  gradient.addColorStop(0, '#718088')
  gradient.addColorStop(0.45, '#a2adb0')
  gradient.addColorStop(1, '#526068')
  ctx.fillStyle = gradient
  ctx.fillRect(0, 0, 512, 512)
  for (let i = 0; i < 12000; i += 1) {
    const shade = 70 + Math.floor(Math.random() * 100)
    ctx.fillStyle = `rgba(${shade},${shade + 5},${shade + 8},${Math.random() * 0.13})`
    ctx.fillRect(Math.random() * 512, Math.random() * 512, 1 + Math.random() * 2, 1)
  }
  for (let i = 0; i < 180; i += 1) {
    const y = Math.random() * 512
    ctx.strokeStyle = `rgba(225,235,238,${0.025 + Math.random() * 0.08})`
    ctx.lineWidth = Math.random() * 1.2
    ctx.beginPath()
    ctx.moveTo(Math.random() * 100, y)
    ctx.lineTo(250 + Math.random() * 262, y + Math.random() * 3)
    ctx.stroke()
  }
  map.update()
  return map
}

function metalBump(scene: Scene): DynamicTexture {
  const map = new DynamicTexture('brushed steel micro normal', 512, scene, true)
  const ctx = map.getContext() as CanvasRenderingContext2D
  ctx.fillStyle = '#8080ff'
  ctx.fillRect(0, 0, 512, 512)
  for (let i = 0; i < 1500; i += 1) {
    const y = Math.random() * 512
    ctx.strokeStyle = Math.random() > 0.5 ? 'rgba(112,128,244,.3)' : 'rgba(144,128,255,.25)'
    ctx.lineWidth = 0.5 + Math.random()
    ctx.beginPath()
    ctx.moveTo(Math.random() * 512, y)
    ctx.lineTo(Math.random() * 512, y + Math.random() * 2)
    ctx.stroke()
  }
  map.update()
  return map
}

export function createMaterialLibrary(scene: Scene): MaterialLibrary {
  const steel = pbr(scene, 'PBR · brushed marine steel', Color3.FromHexString('#89979c'), 0.88, 0.28)
  steel.albedoTexture = metalAlbedo(scene)
  steel.bumpTexture = metalBump(scene)
  steel.bumpTexture.level = 0.38

  const darkSteel = steel.clone('PBR · graphite steel')
  darkSteel.albedoColor = Color3.FromHexString('#35434a')
  darkSteel.metallic = 0.82
  darkSteel.roughness = 0.38

  const rust = pbr(scene, 'PBR · salt corrosion', Color3.FromHexString('#80523b'), 0.55, 0.76)
  rust.albedoTexture = steel.albedoTexture
  rust.bumpTexture = steel.bumpTexture
  rust.bumpTexture.level = 0.8

  const copper = pbr(scene, 'PBR · oxidised copper', Color3.FromHexString('#b47748'), 0.9, 0.3)
  copper.bumpTexture = steel.bumpTexture

  const concrete = pbr(scene, 'PBR · wet volcanic concrete', Color3.White(), 0.04, 0.88)
  concrete.albedoTexture = texture(scene, 'rock.png', 2)
  concrete.bumpTexture = texture(scene, 'rock-normal.png', 2)
  concrete.bumpTexture.level = 0.7

  const rock = pbr(scene, 'PBR · layered coastal basalt', Color3.FromHexString('#9a9c96'), 0.04, 0.82)
  rock.albedoTexture = texture(scene, 'rock.png', 2)
  rock.bumpTexture = texture(scene, 'rock-normal.png', 2)
  rock.bumpTexture.level = 1.05

  const wood = pbr(scene, 'PBR · weathered hardwood', Color3.White(), 0, 0.8)
  wood.albedoTexture = texture(scene, 'wood.jpg', 2)
  wood.bumpTexture = texture(scene, 'wood-bump.jpg', 2)
  wood.bumpTexture.level = 0.7
  wood.metallicTexture = texture(scene, 'wood-roughness.jpg', 2)
  wood.useRoughnessFromMetallicTextureGreen = true
  wood.useMetallnessFromMetallicTextureBlue = false

  const bark = pbr(scene, 'PBR · fissured tree bark', Color3.White(), 0, 0.94)
  bark.albedoTexture = texture(scene, 'bark.png', 2)
  bark.bumpTexture = texture(scene, 'wood-bump.jpg', 2)
  bark.bumpTexture.level = 0.9

  const foliage = pbr(scene, 'PBR · photographic broadleaf canopy', new Color3(0.82, 0.9, 0.78), 0, 0.94)
  foliage.albedoTexture = texture(scene, 'tree-billboard.png', 1)
  foliage.useAlphaFromAlbedoTexture = true
  foliage.transparencyMode = PBRMaterial.PBRMATERIAL_ALPHATEST
  foliage.alphaCutOff = 0.38
  foliage.backFaceCulling = false
  foliage.environmentIntensity = 0.45

  const glass = pbr(scene, 'PBR · lighthouse optical glass', Color3.FromHexString('#c2f2ed'), 0, 0.06)
  glass.alpha = 0.22
  glass.indexOfRefraction = 1.48
  glass.environmentIntensity = 1.5
  glass.backFaceCulling = false

  const lens = pbr(scene, 'PBR · dawn lens', Color3.FromHexString('#fff0b0'), 0.1, 0.12)
  lens.emissiveColor = Color3.FromHexString('#ffc56b')
  lens.emissiveIntensity = 2.2

  const crystal = pbr(scene, 'PBR · resonant quartz', Color3.FromHexString('#78e8df'), 0.15, 0.12)
  crystal.emissiveColor = Color3.FromHexString('#176b70')
  crystal.emissiveIntensity = 0.85
  crystal.alpha = 0.9
  crystal.environmentIntensity = 1.8

  const amber = pbr(scene, 'PBR · amber indicators', Color3.FromHexString('#ffbe69'), 0.2, 0.24)
  amber.emissiveColor = Color3.FromHexString('#ff7b2e')
  amber.emissiveIntensity = 1.8

  return { steel, darkSteel, rust, copper, concrete, rock, wood, bark, foliage, glass, lens, crystal, amber }
}

export type ModelName = 'soldier' | 'fox' | 'deadTree'

const MODEL_FILES: Record<ModelName, AssetName> = {
  soldier: 'soldier.glb',
  fox: 'fox.glb',
  deadTree: 'dead-tree.glb',
}

interface AnimationChannel {
  weight: number
  targetWeight: number
  speed: number
  targetSpeed: number
}

export class ModelInstance {
  currentAnimation: AnimationGroup | null = null
  currentName = ''
  private readonly channels = new Map<AnimationGroup, AnimationChannel>()
  private readonly animationObserver: Observer<Scene>
  private blendDuration = 0.24

  constructor(
    readonly root: TransformNode,
    readonly content: TransformNode,
    readonly entries: InstantiatedEntries,
  ) {
    const scene = root.getScene()
    this.animationObserver = scene.onBeforeRenderObservable.add(() => {
      if (!scene.animationsEnabled) return
      const dt = Math.min(0.05, Math.max(0.001, scene.getEngine().getDeltaTime() / 1000))
      this.updateAnimation(dt)
    })
  }

  play(fragment: string, speed = 1, loop = true, fade = 0.24): void {
    const wanted = fragment.toLowerCase()
    const next = this.entries.animationGroups.find((group) => group.name.toLowerCase().includes(wanted))
    if (!next) return
    if (this.currentName === wanted && this.currentAnimation === next) {
      const channel = this.channels.get(next)
      if (channel) channel.targetSpeed = speed
      return
    }

    this.currentAnimation = next
    this.currentName = wanted
    this.blendDuration = Math.max(0.04, fade)
    let channel = this.channels.get(next)
    if (!channel) {
      const hasVisibleAnimation = [...this.channels.values()].some((item) => item.weight > 0.001)
      channel = { weight: hasVisibleAnimation ? 0 : 1, targetWeight: 1, speed, targetSpeed: speed }
      this.channels.set(next, channel)
      next.start(loop, speed)
      next.weight = channel.weight
    } else {
      next.loopAnimation = loop
      channel.targetWeight = 1
      channel.targetSpeed = speed
    }
    for (const [group, item] of this.channels) {
      if (group !== next) item.targetWeight = 0
    }
  }

  updateAnimation(dt: number): void {
    const speedBlend = 1 - Math.exp(-dt * 9)
    const weightStep = dt / this.blendDuration
    for (const [group, channel] of [...this.channels]) {
      channel.speed += (channel.targetSpeed - channel.speed) * speedBlend
      group.speedRatio = channel.speed
      const difference = channel.targetWeight - channel.weight
      channel.weight += Math.sign(difference) * Math.min(Math.abs(difference), weightStep)
      group.weight = channel.weight
      if (channel.targetWeight === 0 && channel.weight <= 0.001) {
        group.stop()
        this.channels.delete(group)
      }
    }
  }

  dispose(): void {
    this.root.getScene().onBeforeRenderObservable.remove(this.animationObserver)
    this.channels.forEach((_channel, group) => group.stop())
    this.channels.clear()
    this.entries.animationGroups.forEach((group) => group.stop())
    this.entries.dispose()
    this.root.dispose()
  }
}

export class ModelLibrary {
  private containers = new Map<ModelName, AssetContainer>()

  constructor(
    private scene: Scene,
    private shadows: ShadowGenerator,
  ) {}

  async load(): Promise<void> {
    const names = Object.keys(MODEL_FILES) as ModelName[]
    const loaded = await Promise.all(names.map(async (name) => {
      const container = await LoadAssetContainerAsync(assetUrl(MODEL_FILES[name]), this.scene, {
        pluginExtension: '.glb',
      })
      return [name, container] as const
    }))
    loaded.forEach(([name, container]) => this.containers.set(name, container))
  }

  create(name: ModelName, targetHeight: number, castsShadow = true, cloneMaterials = false): ModelInstance {
    const container = this.containers.get(name)
    if (!container) throw new Error(`Model is not loaded: ${name}`)
    const entries = container.instantiateModelsToScene(undefined, cloneMaterials)
    const root = new TransformNode(`${name} · world pivot`, this.scene)
    const content = new TransformNode(`${name} · normalised asset`, this.scene)
    content.parent = root
    entries.rootNodes.forEach((node: Node) => { node.parent = content })
    content.computeWorldMatrix(true)
    const bounds = content.getHierarchyBoundingVectors(true)
    const size = bounds.max.subtract(bounds.min)
    const scale = targetHeight / Math.max(size.y, 0.001)
    content.scaling.setAll(scale)
    content.position.y = -bounds.min.y * scale
    root.rotationQuaternion = null
    setShadows(content, this.shadows, castsShadow)
    return new ModelInstance(root, content, entries)
  }
}

export function setShadows(root: TransformNode, shadows: ShadowGenerator, casts = true): void {
  root.getChildMeshes(false).forEach((mesh) => {
    mesh.receiveShadows = true
    if (casts && mesh instanceof Mesh && mesh.getTotalVertices() > 0) shadows.addShadowCaster(mesh, false)
  })
}

export interface BeaconVisual {
  root: TransformNode
  lens: Mesh
  rings: Mesh[]
  beam: Mesh
  light: PointLight
  socketLights: Mesh[]
}

function parent<T extends Node>(node: T, root: TransformNode): T {
  node.parent = root
  return node
}

export function createBeacon(scene: Scene, mat: MaterialLibrary, shadows: ShadowGenerator): BeaconVisual {
  const root = new TransformNode('The Last Beacon · assembly', scene)
  const base = parent(MeshBuilder.CreateCylinder('octagonal basalt foundation', {
    diameterTop: 5.6, diameterBottom: 6.4, height: 0.65, tessellation: 12,
  }, scene), root)
  base.position.y = 0.3
  base.material = mat.concrete

  const step = parent(MeshBuilder.CreateCylinder('steel service deck', {
    diameter: 4.9, height: 0.16, tessellation: 48,
  }, scene), root)
  step.position.y = 0.7
  step.material = mat.darkSteel

  const tower = parent(MeshBuilder.CreateCylinder('riveted central tower', {
    diameterTop: 1.5, diameterBottom: 2.1, height: 3.8, tessellation: 32,
  }, scene), root)
  tower.position.y = 2.65
  tower.material = mat.steel

  for (let i = 0; i < 4; i += 1) {
    const angle = i * Math.PI / 2 + Math.PI / 4
    const x = Math.cos(angle) * 1.28
    const z = Math.sin(angle) * 1.28
    const support = parent(MeshBuilder.CreateCylinder('copper coolant riser', {
      diameter: 0.17, height: 3.65, tessellation: 12,
    }, scene), root)
    support.position.set(x, 2.55, z)
    support.material = mat.copper
    for (let j = 0; j < 6; j += 1) {
      const collar = parent(MeshBuilder.CreateTorus('pipe collar', {
        diameter: 0.23, thickness: 0.045, tessellation: 12,
      }, scene), root)
      collar.position.set(x, 1.05 + j * 0.6, z)
      collar.material = mat.darkSteel
    }
  }

  for (let floor = 0; floor < 5; floor += 1) {
    const band = parent(MeshBuilder.CreateTorus('tower reinforcing band', {
      diameter: 2.12 - floor * 0.1, thickness: 0.09, tessellation: 32,
    }, scene), root)
    band.position.y = 1.05 + floor * 0.7
    band.material = mat.darkSteel
    for (let i = 0; i < 12; i += 1) {
      const angle = i * Math.PI / 6
      const rivet = parent(MeshBuilder.CreateSphere('tower rivet', {
        diameter: 0.075, segments: 6,
      }, scene), root)
      rivet.position.set(Math.cos(angle) * (1.04 - floor * 0.045), 1.05 + floor * 0.7, Math.sin(angle) * (1.04 - floor * 0.045))
      rivet.material = mat.copper
    }
  }

  const gallery = parent(MeshBuilder.CreateCylinder('lantern gallery', {
    diameter: 4.3, height: 0.22, tessellation: 48,
  }, scene), root)
  gallery.position.y = 4.6
  gallery.material = mat.darkSteel
  const rail = parent(MeshBuilder.CreateTorus('gallery handrail', {
    diameter: 4.05, thickness: 0.065, tessellation: 48,
  }, scene), root)
  rail.position.y = 5.25
  rail.material = mat.steel
  for (let i = 0; i < 16; i += 1) {
    const angle = i * Math.PI / 8
    const post = parent(MeshBuilder.CreateCylinder('gallery baluster', {
      diameter: 0.055, height: 0.65, tessellation: 8,
    }, scene), root)
    post.position.set(Math.cos(angle) * 2.01, 4.93, Math.sin(angle) * 2.01)
    post.material = mat.steel
  }

  const lanternBase = parent(MeshBuilder.CreateCylinder('lantern pedestal', {
    diameter: 2.05, height: 0.28, tessellation: 32,
  }, scene), root)
  lanternBase.position.y = 4.82
  lanternBase.material = mat.copper
  const glass = parent(MeshBuilder.CreateCylinder('optical glass housing', {
    diameter: 1.72, height: 1.35, tessellation: 32,
  }, scene), root)
  glass.position.y = 5.58
  glass.material = mat.glass
  const lens = parent(MeshBuilder.CreateSphere('Fresnel dawn lens', {
    diameter: 0.95, segments: 24,
  }, scene), root)
  lens.position.y = 5.55
  lens.material = mat.lens
  for (let i = 0; i < 7; i += 1) {
    const opticalRing = parent(MeshBuilder.CreateTorus('Fresnel prism', {
      diameter: 0.78 + Math.sin(i / 6 * Math.PI) * 0.28, thickness: 0.045, tessellation: 32,
    }, scene), root)
    opticalRing.position.y = 5.23 + i * 0.105
    opticalRing.material = mat.lens
  }
  const roof = parent(MeshBuilder.CreateCylinder('copper lantern roof', {
    diameterTop: 0.15, diameterBottom: 2.2, height: 0.65, tessellation: 32,
  }, scene), root)
  roof.position.y = 6.55
  roof.material = mat.copper
  const finial = parent(MeshBuilder.CreateSphere('roof finial', { diameter: 0.22, segments: 12 }, scene), root)
  finial.position.y = 6.95
  finial.material = mat.amber

  const socketLights: Mesh[] = []
  const rings: Mesh[] = []
  for (let i = 0; i < 3; i += 1) {
    const angle = i / 3 * Math.PI * 2 - Math.PI / 2
    const x = Math.cos(angle) * 2.25
    const z = Math.sin(angle) * 2.25
    const socket = parent(MeshBuilder.CreateCylinder(`energy socket ${i + 1}`, {
      diameter: 0.68, height: 0.75, tessellation: 12,
    }, scene), root)
    socket.position.set(x, 1.08, z)
    socket.material = mat.darkSteel
    const ring = parent(MeshBuilder.CreateTorus(`energy socket ring ${i + 1}`, {
      diameter: 0.72, thickness: 0.08, tessellation: 24,
    }, scene), root)
    ring.position.set(x, 1.47, z)
    ring.material = mat.amber
    rings.push(ring)
    const diode = parent(MeshBuilder.CreateSphere(`socket diode ${i + 1}`, { diameter: 0.16, segments: 10 }, scene), root)
    diode.position.set(x, 1.51, z)
    diode.material = mat.darkSteel
    socketLights.push(diode)
  }

  const beamMaterial = new StandardMaterial('volumetric dawn shaft', scene)
  beamMaterial.emissiveColor = Color3.FromHexString('#a9fff2')
  beamMaterial.diffuseColor = Color3.Black()
  beamMaterial.alpha = 0.065
  beamMaterial.disableLighting = true
  const beam = parent(MeshBuilder.CreateCylinder('dawn light shaft', {
    diameterTop: 4.5, diameterBottom: 0.55, height: 65, tessellation: 32,
  }, scene), root)
  beam.position.y = 38.2
  beam.material = beamMaterial
  beam.isVisible = false

  const light = new PointLight('beacon practical light', new Vector3(0, 5.55, 0), scene)
  light.parent = root
  light.diffuse = Color3.FromHexString('#ffc675')
  light.specular = light.diffuse
  light.intensity = 85
  light.range = 24
  light.radius = 5
  setShadows(root, shadows)
  glass.receiveShadows = false
  beam.receiveShadows = false
  return { root, lens, rings, beam, light, socketLights }
}

export interface BarricadeVisual { root: TransformNode }
export function createBarricade(scene: Scene, mat: MaterialLibrary, shadows: ShadowGenerator): BarricadeVisual {
  const root = new TransformNode('hardwood barricade', scene)
  for (let i = 0; i < 5; i += 1) {
    const plank = parent(MeshBuilder.CreateBox('individual weathered plank', {
      width: 3.3, height: 0.28, depth: 0.18,
    }, scene), root)
    plank.position.set((i % 2) * 0.08 - 0.04, 0.32 + i * 0.32, (i % 2) * 0.07)
    plank.rotation.z = (i % 2 ? 1 : -1) * 0.018
    plank.material = mat.wood
    for (const side of [-1, 1]) {
      const bolt = parent(MeshBuilder.CreateCylinder('forged bolt', {
        diameter: 0.1, height: 0.24, tessellation: 8,
      }, scene), root)
      bolt.rotation.x = Math.PI / 2
      bolt.position.set(side * 1.22, plank.position.y, plank.position.z)
      bolt.material = mat.rust
    }
  }
  for (const side of [-1, 1]) {
    const brace = parent(MeshBuilder.CreateBox('structural brace', {
      width: 0.25, height: 2.15, depth: 0.3,
    }, scene), root)
    brace.position.set(side * 1.2, 0.95, 0.05)
    brace.rotation.z = side * -0.18
    brace.material = mat.wood
  }
  setShadows(root, shadows)
  return { root }
}

export interface TurretVisual { root: TransformNode; head: TransformNode; muzzle: TransformNode }
export function createTurret(scene: Scene, mat: MaterialLibrary, shadows: ShadowGenerator): TurretVisual {
  const root = new TransformNode('automatic sentry', scene)
  const base = parent(MeshBuilder.CreateCylinder('sentry footing', {
    diameterTop: 0.8, diameterBottom: 1.15, height: 0.35, tessellation: 16,
  }, scene), root)
  base.position.y = 0.18
  base.material = mat.concrete
  const stem = parent(MeshBuilder.CreateCylinder('sentry traverse', {
    diameter: 0.35, height: 1.1, tessellation: 16,
  }, scene), root)
  stem.position.y = 0.82
  stem.material = mat.darkSteel
  const head = new TransformNode('sentry yaw', scene)
  head.parent = root
  head.position.y = 1.45
  const housing = parent(MeshBuilder.CreateBox('sentry armoured receiver', {
    width: 0.8, height: 0.52, depth: 0.85,
  }, scene), head)
  housing.material = mat.steel
  for (const side of [-1, 1]) {
    const barrel = parent(MeshBuilder.CreateCylinder('sentry barrel', {
      diameter: 0.1, height: 1.15, tessellation: 12,
    }, scene), head)
    barrel.rotation.x = Math.PI / 2
    barrel.position.set(side * 0.22, 0.02, 0.82)
    barrel.material = mat.darkSteel
    const jacket = parent(MeshBuilder.CreateCylinder('barrel cooling jacket', {
      diameter: 0.18, height: 0.55, tessellation: 12,
    }, scene), head)
    jacket.rotation.x = Math.PI / 2
    jacket.position.set(side * 0.22, 0.02, 0.4)
    jacket.material = mat.copper
  }
  const optic = parent(MeshBuilder.CreateSphere('sentry optic', { diameter: 0.16, segments: 12 }, scene), head)
  optic.position.set(0, 0.18, 0.45)
  optic.material = mat.lens
  const muzzle = new TransformNode('sentry muzzle', scene)
  muzzle.parent = head
  muzzle.position.set(0, 0.02, 1.45)
  setShadows(root, shadows)
  return { root, head, muzzle }
}

export interface CampfireVisual {
  root: TransformNode
  flames: Mesh[]
  light: PointLight
  fireParticles: ParticleSystem
}

export function createRadialTexture(scene: Scene, name: string, inner: string, outer: string): DynamicTexture {
  const map = new DynamicTexture(name, 64, scene, true)
  const ctx = map.getContext() as CanvasRenderingContext2D
  const gradient = ctx.createRadialGradient(32, 32, 1, 32, 32, 31)
  gradient.addColorStop(0, inner)
  gradient.addColorStop(0.35, inner)
  gradient.addColorStop(1, outer)
  ctx.fillStyle = gradient
  ctx.fillRect(0, 0, 64, 64)
  map.update()
  map.hasAlpha = true
  return map
}

export function createCampfire(scene: Scene, mat: MaterialLibrary, shadows: ShadowGenerator): CampfireVisual {
  const root = new TransformNode('survival campfire', scene)
  for (let i = 0; i < 11; i += 1) {
    const angle = i / 11 * Math.PI * 2
    const stone = parent(MeshBuilder.CreateIcoSphere('fire ring basalt', { radius: 0.22, subdivisions: 1 }, scene), root)
    stone.position.set(Math.cos(angle) * 0.72, 0.16, Math.sin(angle) * 0.72)
    stone.scaling.set(1.2, 0.75, 0.9)
    stone.rotation.y = angle
    stone.material = mat.rock
  }
  for (let i = 0; i < 4; i += 1) {
    const log = parent(MeshBuilder.CreateCylinder('charred driftwood', {
      diameter: 0.19, height: 1.15, tessellation: 10,
    }, scene), root)
    log.position.y = 0.25 + (i % 2) * 0.1
    log.rotation.z = Math.PI / 2
    log.rotation.y = i * Math.PI / 4
    log.material = mat.bark
  }
  const flameMaterial = new StandardMaterial('campfire translucent flame', scene)
  flameMaterial.emissiveColor = Color3.FromHexString('#ff8a2f')
  flameMaterial.diffuseColor = Color3.Black()
  flameMaterial.alpha = 0.58
  flameMaterial.disableLighting = true
  const flames: Mesh[] = []
  for (let i = 0; i < 3; i += 1) {
    const flame = parent(MeshBuilder.CreateSphere('animated flame tongue', {
      diameter: 0.55 - i * 0.09, segments: 10,
    }, scene), root)
    flame.position.set((i - 1) * 0.17, 0.58 + i * 0.08, (i % 2) * 0.12)
    flame.scaling.y = 1.8
    flame.material = flameMaterial
    flames.push(flame)
  }
  const light = new PointLight('campfire practical', new Vector3(0, 1, 0), scene)
  light.parent = root
  light.diffuse = Color3.FromHexString('#ff9b50')
  light.specular = Color3.FromHexString('#ffb060')
  light.intensity = 35
  light.range = 10
  light.radius = 4
  const fireParticles = new ParticleSystem('campfire sparks', 70, scene)
  fireParticles.particleTexture = createRadialTexture(scene, 'ember sprite', 'rgba(255,245,180,1)', 'rgba(255,80,0,0)')
  fireParticles.emitter = root.position
  fireParticles.minEmitBox = new Vector3(-0.25, 0.35, -0.25)
  fireParticles.maxEmitBox = new Vector3(0.25, 0.55, 0.25)
  fireParticles.color1 = new Color3(1, 0.65, 0.15).toColor4(1)
  fireParticles.color2 = new Color3(1, 0.2, 0.03).toColor4(0.8)
  fireParticles.colorDead = new Color3(0.25, 0.25, 0.25).toColor4(0)
  fireParticles.minSize = 0.025
  fireParticles.maxSize = 0.09
  fireParticles.minLifeTime = 0.4
  fireParticles.maxLifeTime = 1.1
  fireParticles.emitRate = 22
  fireParticles.direction1 = new Vector3(-0.25, 1.3, -0.25)
  fireParticles.direction2 = new Vector3(0.25, 2.2, 0.25)
  fireParticles.minEmitPower = 0.3
  fireParticles.maxEmitPower = 0.9
  fireParticles.blendMode = ParticleSystem.BLENDMODE_ONEONE
  fireParticles.start()
  setShadows(root, shadows)
  flames.forEach((flame) => { flame.receiveShadows = false })
  return { root, flames, light, fireParticles }
}

export function createScrap(scene: Scene, mat: MaterialLibrary, shadows: ShadowGenerator): TransformNode {
  const root = new TransformNode('salvaged machinery', scene)
  const panel = parent(MeshBuilder.CreateBox('level salvage base', { width: 1.25, height: 0.14, depth: 0.85 }, scene), root)
  panel.position.y = 0.08
  panel.material = mat.rust

  const pipe = parent(MeshBuilder.CreateTorus('upright pressure pipe', { diameter: 0.72, thickness: 0.11, tessellation: 24 }, scene), root)
  pipe.position.set(0.28, 0.52, -0.09)
  pipe.rotation.x = Math.PI / 2
  pipe.material = mat.steel

  const gearX = -0.35
  const gearZ = 0.18
  const gear = parent(MeshBuilder.CreateCylinder('level machine gear', { diameter: 0.58, height: 0.12, tessellation: 24 }, scene), root)
  gear.position.set(gearX, 0.22, gearZ)
  gear.material = mat.copper
  for (let i = 0; i < 12; i += 1) {
    const angle = i / 12 * Math.PI * 2
    const tooth = parent(MeshBuilder.CreateBox('aligned gear tooth', { width: 0.16, height: 0.12, depth: 0.11 }, scene), root)
    tooth.position.set(gearX + Math.cos(angle) * 0.34, 0.22, gearZ + Math.sin(angle) * 0.34)
    tooth.rotation.y = -angle
    tooth.material = mat.copper
  }
  setShadows(root, shadows)
  return root
}

export function createCrystalCluster(scene: Scene, mat: MaterialLibrary, shadows: ShadowGenerator): TransformNode {
  const root = new TransformNode('resonant quartz cluster', scene)
  const offsets = [
    [0, 0, 0, 1.2], [0.38, 0, 0.12, 0.8], [-0.35, 0, 0.15, 0.72],
    [0.1, 0, -0.35, 0.9], [-0.25, 0, -0.28, 0.55],
  ]
  offsets.forEach(([x, , z, height], index) => {
    const crystal = parent(MeshBuilder.CreateLathe('hexagonal quartz prism', {
      shape: [
        new Vector3(0, -height * 0.45, 0),
        new Vector3(0.19, -height * 0.28, 0),
        new Vector3(0.19, height * 0.28, 0),
        new Vector3(0, height * 0.52, 0),
      ],
      tessellation: 6,
    }, scene), root)
    crystal.position.set(x, height * 0.45, z)
    crystal.rotation.z = (index - 2) * 0.08
    crystal.material = mat.crystal
  })
  setShadows(root, shadows)
  return root
}

export function createBroadleafTree(
  scene: Scene,
  mat: MaterialLibrary,
  shadows: ShadowGenerator,
  height = 5.4,
  withBranches = true,
): TransformNode {
  const root = new TransformNode(withBranches ? 'mature broadleaf tree' : 'dense coastal shrub', scene)
  for (let i = 0; i < 3; i += 1) {
    const canopy = parent(MeshBuilder.CreatePlane('alpha-tested photographic foliage', {
      width: height * 1.02, height,
    }, scene), root)
    canopy.position.y = withBranches ? height * 0.5 : height * 0.32
    canopy.rotation.y = i * Math.PI / 3
    canopy.material = mat.foliage
  }
  if (withBranches) {
    const trunk = parent(MeshBuilder.CreateCylinder('tapered bark trunk', {
      diameterTop: height * 0.038, diameterBottom: height * 0.075, height: height * 0.64, tessellation: 14,
    }, scene), root)
    trunk.position.y = height * 0.32
    trunk.material = mat.bark
    for (let i = 0; i < 8; i += 1) {
      const angle = i * 2.37
      const start = new Vector3(0, height * (0.34 + i * 0.026), 0)
      const end = new Vector3(
        Math.cos(angle) * height * (0.13 + (i % 3) * 0.025),
        height * (0.57 + (i % 4) * 0.045),
        Math.sin(angle) * height * (0.13 + (i % 3) * 0.025),
      )
      const direction = end.subtract(start)
      const branch = parent(MeshBuilder.CreateCylinder('individual woody branch', {
        diameterTop: height * 0.012, diameterBottom: height * 0.026, height: direction.length(), tessellation: 10,
      }, scene), root)
      branch.position.copyFrom(start.add(end).scale(0.5))
      branch.rotationQuaternion = Quaternion.FromUnitVectorsToRef(Vector3.Up(), direction.normalize(), new Quaternion())
      branch.material = mat.bark
    }
    for (let i = 0; i < 6; i += 1) {
      const angle = i / 6 * Math.PI * 2
      const start = new Vector3(0, height * 0.055, 0)
      const end = new Vector3(Math.cos(angle) * height * 0.11, 0.04, Math.sin(angle) * height * 0.11)
      const direction = end.subtract(start)
      const rootFlare = parent(MeshBuilder.CreateCylinder('exposed root flare', {
        diameterTop: height * 0.012, diameterBottom: height * 0.035, height: direction.length(), tessellation: 9,
      }, scene), root)
      rootFlare.position.copyFrom(start.add(end).scale(0.5))
      rootFlare.rotationQuaternion = Quaternion.FromUnitVectorsToRef(Vector3.Up(), direction.normalize(), new Quaternion())
      rootFlare.material = mat.bark
    }
  }
  setShadows(root, shadows)
  return root
}

export function createRock(scene: Scene, mat: MaterialLibrary, shadows: ShadowGenerator, radius = 0.7): Mesh {
  const rock = MeshBuilder.CreateIcoSphere('eroded basalt boulder', { radius, subdivisions: 2 }, scene)
  const positions = rock.getVerticesData('position')
  if (positions) {
    for (let i = 0; i < positions.length; i += 3) {
      const factor = 0.82 + Math.random() * 0.3
      positions[i] *= factor
      positions[i + 1] *= 0.65 + Math.random() * 0.25
      positions[i + 2] *= factor
    }
    rock.updateVerticesData('position', positions)
    rock.createNormals(true)
  }
  rock.material = mat.rock
  rock.receiveShadows = true
  shadows.addShadowCaster(rock)
  return rock
}

export function spawnBurst(scene: Scene, position: Vector3, color: Color3, count = 22): void {
  const particles = new ParticleSystem('impact fragments', count, scene)
  particles.particleTexture = createRadialTexture(scene, 'impact sprite', 'rgba(255,255,255,1)', 'rgba(255,255,255,0)')
  particles.emitter = position.clone()
  particles.color1 = color.toColor4(1)
  particles.color2 = color.scale(0.65).toColor4(0.85)
  particles.colorDead = color.scale(0.2).toColor4(0)
  particles.minSize = 0.035
  particles.maxSize = 0.13
  particles.minLifeTime = 0.18
  particles.maxLifeTime = 0.55
  particles.emitRate = 0
  particles.manualEmitCount = count
  particles.direction1 = new Vector3(-2, 0.5, -2)
  particles.direction2 = new Vector3(2, 3.5, 2)
  particles.minEmitPower = 0.5
  particles.maxEmitPower = 2.5
  particles.gravity = new Vector3(0, -7, 0)
  particles.blendMode = ParticleSystem.BLENDMODE_ONEONE
  particles.targetStopDuration = 0.12
  particles.disposeOnStop = true
  particles.start()
}

export function spawnSlideDust(scene: Scene, position: Vector3, direction: Vector3): void {
  const particles = new ParticleSystem('survivor slide dust', 28, scene)
  particles.particleTexture = createRadialTexture(scene, 'soft ground dust', 'rgba(210,202,178,.48)', 'rgba(160,154,137,0)')
  particles.emitter = position.clone()
  particles.color1 = new Color3(0.55, 0.53, 0.45).toColor4(0.32)
  particles.color2 = new Color3(0.36, 0.38, 0.34).toColor4(0.2)
  particles.colorDead = new Color3(0.3, 0.32, 0.29).toColor4(0)
  particles.minSize = 0.12
  particles.maxSize = 0.36
  particles.minLifeTime = 0.22
  particles.maxLifeTime = 0.58
  particles.emitRate = 0
  particles.manualEmitCount = 20
  const backward = direction.scale(-1)
  particles.direction1 = backward.scale(1.1).add(new Vector3(-0.45, 0.12, -0.45))
  particles.direction2 = backward.scale(2.1).add(new Vector3(0.45, 0.65, 0.45))
  particles.minEmitPower = 0.35
  particles.maxEmitPower = 1.1
  particles.gravity = new Vector3(0, -0.8, 0)
  particles.blendMode = ParticleSystem.BLENDMODE_STANDARD
  particles.targetStopDuration = 0.08
  particles.disposeOnStop = true
  particles.start()
}

export function spawnTracer(scene: Scene, from: Vector3, to: Vector3, color = Color3.FromHexString('#ffe2a0')): void {
  const line = MeshBuilder.CreateLines('ballistic tracer', { points: [from, to] }, scene)
  line.color = color
  line.alpha = 0.9
  window.setTimeout(() => line.dispose(), 75)
}
