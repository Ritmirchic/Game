import {
  ArcRotateCamera,
  Color3,
  Color4,
  CubeTextureCreateFromPrefilteredData,
  DefaultRenderingPipeline,
  DirectionalLight,
  HemisphericLight,
  ImageProcessingConfiguration,
  Mesh,
  MeshBuilder,
  ParticleSystem,
  PBRMaterial,
  Scene,
  SSAO2RenderingPipeline,
  ShadowGenerator,
  Texture,
  TransformNode,
  Vector2,
  Vector3,
  VertexData,
} from '@babylonjs/core'
import { SkyMaterial, WaterMaterial } from '@babylonjs/materials'
import { assetUrl } from './assets'
import { createRadialTexture } from './models'

function clamp01(value: number): number {
  return Math.max(0, Math.min(1, value))
}

function smoothstep(edge0: number, edge1: number, value: number): number {
  const t = clamp01((value - edge0) / (edge1 - edge0))
  return t * t * (3 - 2 * t)
}

function hash(x: number, z: number): number {
  const value = Math.sin(x * 127.1 + z * 311.7) * 43758.5453123
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
  return (a + (b - a) * ux) + ((c + (d - c) * ux) - (a + (b - a) * ux)) * uz
}

export function islandHeight(x: number, z: number): number {
  const radius = Math.hypot(x, z)
  const broad = (noise(x * 0.075 + 20, z * 0.075 + 20) - 0.5) * 1.05
  const detail = (noise(x * 0.31, z * 0.31) - 0.5) * 0.24
  const hill = Math.exp(-((x + 8) ** 2 + (z - 5) ** 2) / 90) * 0.8
  const coast = smoothstep(22.5, 34, radius)
  return 0.24 + broad + detail + hill - coast * 3.8
}

function tiledTexture(scene: Scene, name: Parameters<typeof assetUrl>[0], repeat: number): Texture {
  const map = new Texture(assetUrl(name), scene)
  map.uScale = repeat
  map.vScale = repeat
  map.wrapU = Texture.WRAP_ADDRESSMODE
  map.wrapV = Texture.WRAP_ADDRESSMODE
  map.anisotropicFilteringLevel = 8
  return map
}


export class Environment {
  readonly camera: ArcRotateCamera
  readonly ground: Mesh
  readonly water: Mesh
  readonly waterMaterial: WaterMaterial
  readonly sun: DirectionalLight
  readonly hemi: HemisphericLight
  readonly shadows: ShadowGenerator
  readonly highQuality: boolean
  private skyMaterial: SkyMaterial
  private sky: Mesh
  private ash: ParticleSystem
  private stars: ParticleSystem
  private starsVisible = false
  private cameraTarget = new Vector3(0, 1, 0)
  private elapsed = 0

  constructor(private scene: Scene) {
    this.highQuality = !window.matchMedia('(pointer: coarse)').matches
      && new URLSearchParams(window.location.search).get('quality') !== 'low'
    scene.clearColor = new Color4(0.12, 0.2, 0.25, 1)
    scene.ambientColor = new Color3(0.18, 0.2, 0.22)
    scene.fogMode = Scene.FOGMODE_EXP2
    scene.fogDensity = 0.006
    scene.fogColor = Color3.FromHexString('#91afb5')
    scene.environmentTexture = CubeTextureCreateFromPrefilteredData(assetUrl('outdoor.env'), scene, '.env')
    scene.environmentIntensity = 0.78
    scene.imageProcessingConfiguration.toneMappingEnabled = true
    scene.imageProcessingConfiguration.toneMappingType = ImageProcessingConfiguration.TONEMAPPING_ACES
    scene.imageProcessingConfiguration.exposure = 1.08
    scene.imageProcessingConfiguration.contrast = 1.18

    this.camera = new ArcRotateCamera('survivor camera', 0.5, 1.02, 14, new Vector3(0, 2, 0), scene)
    this.camera.minZ = 0.1
    this.camera.maxZ = 280
    this.camera.lowerBetaLimit = 0.28
    this.camera.upperBetaLimit = 1.48
    this.camera.fov = 0.82
    this.camera.panningSensibility = 0

    this.hemi = new HemisphericLight('sky hemisphere', new Vector3(0.2, 1, 0.1), scene)
    this.hemi.diffuse = Color3.FromHexString('#c4e1e5')
    this.hemi.groundColor = Color3.FromHexString('#33413d')
    this.hemi.intensity = 0.58
    this.sun = new DirectionalLight('coastal sunlight', new Vector3(-0.55, -1, -0.35), scene)
    this.sun.position = new Vector3(28, 42, 20)
    this.sun.diffuse = Color3.FromHexString('#ffe0ae')
    this.sun.specular = Color3.FromHexString('#ffe8c5')
    this.sun.intensity = 1.25
    this.sun.shadowMinZ = 1
    this.sun.shadowMaxZ = 85
    this.shadows = new ShadowGenerator(this.highQuality ? 2048 : 1024, this.sun)
    this.shadows.usePercentageCloserFiltering = true
    this.shadows.filteringQuality = this.highQuality ? ShadowGenerator.QUALITY_HIGH : ShadowGenerator.QUALITY_MEDIUM
    this.shadows.bias = 0.00035
    this.shadows.normalBias = 0.025
    this.shadows.darkness = 0.28
    this.shadows.setTransparencyShadow(false)

    const terrain = new PBRMaterial('PBR · moss and coastal grass', scene)
    terrain.albedoColor = new Color3(0.78, 0.84, 0.72)
    terrain.albedoTexture = tiledTexture(scene, 'grass-photo.jpg', 16)
    terrain.bumpTexture = tiledTexture(scene, 'grass-normal.png', 16)
    terrain.bumpTexture.level = 0.75
    terrain.metallic = 0
    terrain.roughness = 0.96
    terrain.environmentIntensity = 0.35

    this.ground = MeshBuilder.CreateGround('sculpted island terrain', {
      width: 70, height: 70, subdivisions: this.highQuality ? 120 : 80, updatable: true,
    }, scene)
    const positions = this.ground.getVerticesData('position')
    const indices = this.ground.getIndices()
    if (positions && indices) {
      for (let i = 0; i < positions.length; i += 3) positions[i + 1] = islandHeight(positions[i], positions[i + 2])
      const normals: number[] = []
      VertexData.ComputeNormals(positions, indices, normals)
      this.ground.updateVerticesData('position', positions)
      this.ground.updateVerticesData('normal', normals)
    }
    this.ground.material = terrain
    this.ground.receiveShadows = true
    this.ground.isPickable = false

    const beachPositions: number[] = []
    const beachIndices: number[] = []
    const beachUvs: number[] = []
    const beachSegments = 128
    const beachRings = 18
    for (let ring = 0; ring <= beachRings; ring += 1) {
      const radius = 21.2 + ring / beachRings * 14.2
      for (let segment = 0; segment <= beachSegments; segment += 1) {
        const angle = segment / beachSegments * Math.PI * 2
        const x = Math.cos(angle) * radius
        const z = Math.sin(angle) * radius
        beachPositions.push(x, islandHeight(x, z) + 0.035, z)
        beachUvs.push(x / 5, z / 5)
        if (ring < beachRings && segment < beachSegments) {
          const current = ring * (beachSegments + 1) + segment
          const next = current + beachSegments + 1
          beachIndices.push(current, current + 1, next, current + 1, next + 1, next)
        }
      }
    }
    const beachNormals: number[] = []
    VertexData.ComputeNormals(beachPositions, beachIndices, beachNormals)
    const beach = new Mesh('curved sandy shoreline', scene)
    const beachData = new VertexData()
    beachData.positions = beachPositions
    beachData.indices = beachIndices
    beachData.uvs = beachUvs
    beachData.normals = beachNormals
    beachData.applyToMesh(beach)
    const sand = new PBRMaterial('PBR · wet granular sand', scene)
    sand.albedoColor = new Color3(0.68, 0.66, 0.58)
    sand.albedoTexture = tiledTexture(scene, 'sand.jpg', 1)
    sand.bumpTexture = tiledTexture(scene, 'rock-normal.png', 1)
    sand.bumpTexture.level = 0.12
    sand.metallic = 0
    sand.roughness = 0.9
    sand.environmentIntensity = 0.3
    beach.material = sand
    beach.receiveShadows = true
    beach.isPickable = false

    this.waterMaterial = new WaterMaterial('reflective North Sea', scene,
      new Vector2(this.highQuality ? 512 : 256, this.highQuality ? 512 : 256))
    this.waterMaterial.bumpTexture = tiledTexture(scene, 'water-bump.png', 1)
    this.waterMaterial.windForce = 7
    this.waterMaterial.windDirection = new Vector2(0.8, 0.35)
    this.waterMaterial.waveHeight = 0.32
    this.waterMaterial.bumpHeight = 0.22
    this.waterMaterial.waveLength = 0.32
    this.waterMaterial.waveSpeed = 0.65
    this.waterMaterial.waveCount = 18
    this.waterMaterial.waterColor = Color3.FromHexString('#164f5d')
    this.waterMaterial.waterColor2 = Color3.FromHexString('#315f6c')
    this.waterMaterial.colorBlendFactor = 0.32
    this.waterMaterial.colorBlendFactor2 = 0.18
    this.waterMaterial.specularColor = Color3.FromHexString('#d7f2ed')
    this.waterMaterial.specularPower = 128
    this.waterMaterial.bumpSuperimpose = true
    this.waterMaterial.bumpAffectsReflection = true
    this.waterMaterial.fresnelSeparate = true
    this.water = MeshBuilder.CreateGround('animated reflective ocean', {
      width: 240, height: 240, subdivisions: this.highQuality ? 64 : 24,
    }, scene)
    this.water.position.y = -1.3
    this.water.material = this.waterMaterial
    this.water.isPickable = false
    this.waterMaterial.addToRenderList(this.ground)
    this.waterMaterial.addToRenderList(beach)

    this.skyMaterial = new SkyMaterial('physical storm sky', scene)
    this.skyMaterial.useSunPosition = true
    this.skyMaterial.sunPosition = new Vector3(-45, 55, -30)
    this.skyMaterial.turbidity = 7.5
    this.skyMaterial.rayleigh = 1.7
    this.skyMaterial.mieCoefficient = 0.006
    this.skyMaterial.mieDirectionalG = 0.82
    this.skyMaterial.luminance = 0.82
    this.skyMaterial.cloudiness = 0.38
    this.skyMaterial.dithering = true
    this.skyMaterial.backFaceCulling = false
    this.sky = MeshBuilder.CreateBox('atmospheric sky dome', { size: 600 }, scene)
    this.sky.material = this.skyMaterial
    this.sky.infiniteDistance = true
    this.sky.applyFog = false
    this.sky.isPickable = false
    this.waterMaterial.addToRenderList(this.sky)

    const pipeline = new DefaultRenderingPipeline('cinematic coastal pipeline', true, scene, [this.camera])
    pipeline.samples = this.highQuality ? 2 : 1
    pipeline.fxaaEnabled = true
    pipeline.bloomEnabled = true
    pipeline.bloomThreshold = 0.72
    pipeline.bloomWeight = this.highQuality ? 0.32 : 0.24
    pipeline.bloomKernel = 48
    pipeline.bloomScale = 0.5
    pipeline.imageProcessingEnabled = true
    if (this.highQuality) {
      const ssao = new SSAO2RenderingPipeline('contact ambient occlusion', scene, {
        ssaoRatio: 0.55, blurRatio: 0.5,
      }, [this.camera])
      ssao.samples = 16
      ssao.radius = 1.7
      ssao.totalStrength = 1.05
      ssao.epsilon = 0.025
      ssao.maxZ = 90
      ssao.expensiveBlur = true
      ssao.bilateralSamples = 12
    }

    this.ash = new ParticleSystem('windborne salt and ash', 700, scene)
    this.ash.particleTexture = createRadialTexture(scene, 'ash mote', 'rgba(225,235,229,.8)', 'rgba(180,200,195,0)')
    this.ash.emitter = new Vector3(0, 8, 0)
    this.ash.minEmitBox = new Vector3(-30, -5, -30)
    this.ash.maxEmitBox = new Vector3(30, 16, 30)
    this.ash.color1 = new Color3(0.7, 0.75, 0.72).toColor4(0.45)
    this.ash.color2 = new Color3(0.36, 0.42, 0.42).toColor4(0.3)
    this.ash.colorDead = new Color3(0.2, 0.22, 0.22).toColor4(0)
    this.ash.minSize = 0.025
    this.ash.maxSize = 0.11
    this.ash.minLifeTime = 5
    this.ash.maxLifeTime = 12
    this.ash.emitRate = 35
    this.ash.direction1 = new Vector3(0.8, -0.18, 0.25)
    this.ash.direction2 = new Vector3(1.7, 0.12, 0.65)
    this.ash.minEmitPower = 0.3
    this.ash.maxEmitPower = 1.2
    this.ash.updateSpeed = 0.012
    this.ash.start()

    this.stars = new ParticleSystem('northern stars', 450, scene)
    this.stars.particleTexture = createRadialTexture(scene, 'star sprite', 'rgba(255,255,240,1)', 'rgba(170,215,255,0)')
    this.stars.emitter = new Vector3(0, 0, 0)
    this.stars.minEmitBox = new Vector3(-90, 24, -90)
    this.stars.maxEmitBox = new Vector3(90, 85, 90)
    this.stars.color1 = new Color3(0.8, 0.9, 1).toColor4(0.8)
    this.stars.color2 = new Color3(1, 0.92, 0.72).toColor4(0.65)
    this.stars.colorDead = new Color3(0.7, 0.8, 1).toColor4(0.6)
    this.stars.minSize = 0.08
    this.stars.maxSize = 0.22
    this.stars.minLifeTime = 10000
    this.stars.maxLifeTime = 10000
    this.stars.emitRate = 0
    this.stars.manualEmitCount = 450
    this.stars.blendMode = ParticleSystem.BLENDMODE_ONEONE
  }

  addReflections(root: TransformNode): void {
    root.getChildMeshes(false).forEach((mesh) => this.waterMaterial.addToRenderList(mesh))
  }

  updateCamera(dt: number, target: Vector3, yaw: number, pitch: number, distance: number, shake = 0): void {
    const blend = 1 - Math.exp(-dt * 12)
    this.cameraTarget = Vector3.Lerp(this.cameraTarget, target, blend)
    this.camera.target.copyFrom(this.cameraTarget)
    if (shake > 0) {
      this.camera.target.x += (Math.random() - 0.5) * shake
      this.camera.target.y += (Math.random() - 0.5) * shake
    }
    this.camera.alpha = yaw
    this.camera.beta = Math.max(0.3, Math.min(1.47, Math.PI / 2 - pitch))
    this.camera.radius += (distance - this.camera.radius) * blend
  }

  menuCamera(dt: number): void {
    this.elapsed += dt
    this.camera.alpha = 0.45 + this.elapsed * 0.035
    this.camera.beta = 1.03
    this.camera.radius = 28
    this.camera.target = new Vector3(-1, 2.2, 0)
    this.cameraTarget.copyFrom(this.camera.target)
  }

  update(dt: number, night: number): void {
    this.elapsed += dt
    const amount = clamp01(night)
    this.sun.intensity = 1.25 - amount * 1.14
    this.hemi.intensity = 0.58 - amount * 0.34
    this.sun.diffuse = Color3.Lerp(Color3.FromHexString('#ffe0ae'), Color3.FromHexString('#738aa8'), amount)
    this.scene.fogColor = Color3.Lerp(Color3.FromHexString('#91afb5'), Color3.FromHexString('#101b29'), amount)
    this.scene.clearColor = new Color4(0.12 - amount * 0.09, 0.2 - amount * 0.15, 0.25 - amount * 0.16, 1)
    this.scene.fogDensity = 0.006 + amount * 0.008
    this.scene.environmentIntensity = 0.78 - amount * 0.53
    this.skyMaterial.sunPosition.set(-45, 55 - amount * 95, -30)
    this.skyMaterial.luminance = 0.82 - amount * 0.75
    this.skyMaterial.turbidity = 7.5 + amount * 4
    this.skyMaterial.cloudiness = 0.38 + amount * 0.32
    this.waterMaterial.waterColor = Color3.Lerp(Color3.FromHexString('#164f5d'), Color3.FromHexString('#071b2a'), amount)
    this.waterMaterial.waterColor2 = Color3.Lerp(Color3.FromHexString('#315f6c'), Color3.FromHexString('#102b3a'), amount)
    this.ash.emitRate = 30 + amount * 75
    const showStars = amount > 0.56
    if (showStars && !this.starsVisible) {
      this.stars.start()
      this.starsVisible = true
    } else if (!showStars && this.starsVisible) {
      this.stars.stop()
      this.stars.reset()
      this.starsVisible = false
    }
  }
}
