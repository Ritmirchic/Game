import {
  Color3,
  DynamicTexture,
  Mesh,
  MeshBuilder,
  PBRMaterial,
  PointLight,
  Quaternion,
  Scene,
  ShadowGenerator,
  Texture,
  TransformNode,
  Vector3,
  VertexData,
} from '@babylonjs/core'
import { PBRCustomMaterial } from '@babylonjs/materials'
import { assetUrl, type AssetName } from '../game/assets'
import { installWaterShader, setWaterTime } from './DesertShaders'
import {
  createMaterialLibrary,
  createRock,
  createScrap,
  setShadows,
  type MaterialLibrary,
} from '../game/models'

function parent<T extends TransformNode>(node: T, root: TransformNode): T {
  node.parent = root
  return node
}

function pbr(scene: Scene, name: string, color: string, roughness: number, metallic = 0): PBRMaterial {
  const material = new PBRMaterial(name, scene)
  material.albedoColor = Color3.FromHexString(color)
  material.roughness = roughness
  material.metallic = metallic
  material.environmentIntensity = 0.55
  return material
}

function surfaceTexture(scene: Scene, name: AssetName, repeat: number): Texture {
  const texture = new Texture(assetUrl(name), scene)
  texture.uScale = repeat
  texture.vScale = repeat
  texture.anisotropicFilteringLevel = 16
  return texture
}

function waterNormalTexture(scene: Scene): DynamicTexture {
  const size = 256
  const texture = new DynamicTexture('oasis crossing wave normals', size, scene, true)
  const ctx = texture.getContext() as CanvasRenderingContext2D
  const image = ctx.createImageData(size, size)
  for (let y = 0; y < size; y += 1) {
    for (let x = 0; x < size; x += 1) {
      const u = x / size * Math.PI * 2
      const v = y / size * Math.PI * 2
      const dx = Math.cos(u * 3 + Math.sin(v * 2)) * 0.24 + Math.cos((u + v) * 7) * 0.08
      const dy = Math.cos(v * 4 + Math.sin(u * 3)) * 0.2 + Math.sin((u - v) * 6) * 0.07
      const index = (y * size + x) * 4
      image.data[index] = 128 - dx * 105
      image.data[index + 1] = 128 - dy * 105
      image.data[index + 2] = 242
      image.data[index + 3] = 255
    }
  }
  ctx.putImageData(image, 0, 0)
  texture.update()
  texture.wrapU = Texture.WRAP_ADDRESSMODE
  texture.wrapV = Texture.WRAP_ADDRESSMODE
  texture.uScale = 4
  texture.vScale = 4
  texture.anisotropicFilteringLevel = 16
  return texture
}

function sandstoneAlbedoTexture(scene: Scene): DynamicTexture {
  const size = 512
  const texture = new DynamicTexture('sun bleached sandstone mineral grain', size, scene, true)
  const ctx = texture.getContext() as CanvasRenderingContext2D
  const image = ctx.createImageData(size, size)
  const hash = (x: number, y: number): number => {
    const value = Math.sin(x * 127.1 + y * 311.7) * 43758.5453
    return value - Math.floor(value)
  }
  const noise = (u: number, v: number, period: number): number => {
    const x = u * period
    const y = v * period
    const ix = Math.floor(x)
    const iy = Math.floor(y)
    let fx = x - ix
    let fy = y - iy
    fx = fx * fx * (3 - 2 * fx)
    fy = fy * fy * (3 - 2 * fy)
    const wrap = (value: number): number => (value + period) % period
    const x0 = wrap(ix)
    const x1 = wrap(ix + 1)
    const y0 = wrap(iy)
    const y1 = wrap(iy + 1)
    const a = hash(x0, y0) * (1 - fx) + hash(x1, y0) * fx
    const b = hash(x0, y1) * (1 - fx) + hash(x1, y1) * fx
    return a * (1 - fy) + b * fy
  }
  for (let y = 0; y < size; y += 1) {
    for (let x = 0; x < size; x += 1) {
      const u = x / size
      const v = y / size
      const cloudy = noise(u, v, 4) * 0.42 + noise(u, v, 12) * 0.28 + noise(u, v, 40) * 0.18
      const grain = hash(x, y) * 0.12
      const value = 0.7 + cloudy * 0.48 + grain
      const mineral = hash(x + 71, y + 19)
      const fleck = mineral > 0.988 ? 0.58 : mineral < 0.012 ? 1.13 : 1
      const index = (y * size + x) * 4
      image.data[index] = Math.min(255, 194 * value * fleck)
      image.data[index + 1] = Math.min(255, 169 * value * fleck)
      image.data[index + 2] = Math.min(255, 132 * value * fleck)
      image.data[index + 3] = 255
    }
  }
  ctx.putImageData(image, 0, 0)
  texture.update()
  texture.wrapU = Texture.WRAP_ADDRESSMODE
  texture.wrapV = Texture.WRAP_ADDRESSMODE
  texture.uScale = 1.5
  texture.vScale = 1.5
  texture.anisotropicFilteringLevel = 16
  return texture
}

function ringStone(scene: Scene, inner: number, outer: number, height: number, arc: number): Mesh {
  const a = -arc / 2
  const b = arc / 2
  const bevel = 0.028
  const point = (radius: number, angle: number, y: number): number[] => [Math.sin(angle) * radius, y, Math.cos(angle) * radius]
  const positions = [
    ...point(inner, a, 0), ...point(outer, a, 0), ...point(inner, b, 0), ...point(outer, b, 0),
    ...point(inner + bevel, a, height), ...point(outer - bevel, a, height),
    ...point(inner + bevel, b, height), ...point(outer - bevel, b, height),
  ]
  const indices = [
    0, 1, 3, 0, 3, 2, 4, 6, 7, 4, 7, 5,
    0, 4, 5, 0, 5, 1, 2, 3, 7, 2, 7, 6,
    0, 2, 6, 0, 6, 4, 1, 5, 7, 1, 7, 3,
  ]
  const uvs = [0, 0, 1, 0, 0, 0, 1, 0, 0, 1, 1, 1, 0, 1, 1, 1]
  const normals: number[] = []
  VertexData.ComputeNormals(positions, indices, normals)
  const mesh = new Mesh('individual bevelled well stone', scene)
  const data = new VertexData()
  data.positions = positions
  data.indices = indices
  data.uvs = uvs
  data.normals = normals
  data.applyToMesh(mesh)
  return mesh
}

function beam(scene: Scene, root: TransformNode, name: string, from: Vector3, to: Vector3, diameter: number, material: PBRMaterial): Mesh {
  const direction = to.subtract(from)
  const mesh = parent(MeshBuilder.CreateCylinder(name, { diameter, height: direction.length(), tessellation: 12 }, scene), root)
  mesh.position.copyFrom(from.add(to).scale(0.5))
  mesh.rotationQuaternion = Quaternion.FromUnitVectorsToRef(Vector3.Up(), direction.normalize(), new Quaternion())
  mesh.material = material
  return mesh
}

export interface RelayVisual {
  root: TransformNode
  setActive: (active: boolean) => void
}

export interface SentryVisual {
  root: TransformNode
  head: TransformNode
  muzzle: TransformNode
}

export class DesertModels {
  readonly materials: MaterialLibrary
  readonly cactusMaterial: PBRMaterial
  readonly agaveMaterial: PBRMaterial
  readonly sandstone: PBRMaterial
  readonly solar: PBRMaterial
  readonly sensor: PBRMaterial
  readonly water: PBRCustomMaterial
  readonly gloves: PBRMaterial
  private waterTime = 0

  constructor(private scene: Scene, private shadows: ShadowGenerator) {
    this.materials = createMaterialLibrary(scene)
    this.cactusMaterial = pbr(scene, 'ribbed cactus skin', '#435d3c', 0.88)
    this.agaveMaterial = pbr(scene, 'dusty agave leaves', '#667b59', 0.9)
    this.sandstone = pbr(scene, 'eroded desert sandstone', '#d5b991', 0.86)
    this.sandstone.environmentIntensity = 0.65
    this.sandstone.albedoTexture = sandstoneAlbedoTexture(scene)
    this.sandstone.bumpTexture = surfaceTexture(scene, 'rock-normal.png', 1.5)
    this.sandstone.bumpTexture.level = 0.8
    this.sandstone.backFaceCulling = false
    this.sandstone.twoSidedLighting = true
    this.cactusMaterial.bumpTexture = surfaceTexture(scene, 'wood-bump.jpg', 4)
    this.cactusMaterial.bumpTexture.level = 0.22
    this.agaveMaterial.bumpTexture = surfaceTexture(scene, 'wood-bump.jpg', 3)
    this.agaveMaterial.bumpTexture.level = 0.16
    this.solar = pbr(scene, 'weathered photovoltaic glass', '#172d37', 0.16, 0.55)
    this.sensor = pbr(scene, 'sentry amber optics', '#ffc37a', 0.18, 0.15)
    this.sensor.emissiveColor = Color3.FromHexString('#ff8b35')
    this.sensor.emissiveIntensity = 1.4
    this.gloves = pbr(scene, 'black leather spear gloves', '#080a0c', 0.72, 0.05)
    this.gloves.environmentIntensity = 0.3
    this.gloves.bumpTexture = surfaceTexture(scene, 'rock-normal.png', 7)
    this.gloves.bumpTexture.level = 0.12
    this.water = new PBRCustomMaterial('shallow oasis water · animated GLSL', scene)
    installWaterShader(this.water)
    this.water.albedoColor = Color3.FromHexString('#195e63')
    this.water.roughness = 0.075
    this.water.metallic = 0.16
    this.water.environmentIntensity = 0.95
    this.water.alpha = 0.82
    this.water.indexOfRefraction = 1.33
    this.water.backFaceCulling = false
    this.water.bumpTexture = waterNormalTexture(scene)
    this.water.bumpTexture.level = 0.34
  }

  update(dt: number): void {
    this.waterTime = (this.waterTime + dt) % 1000
    setWaterTime(this.water, this.waterTime)
  }

  rock(radius: number): Mesh {
    return createRock(this.scene, this.materials, this.shadows, radius)
  }

  salvage(): TransformNode {
    return createScrap(this.scene, this.materials, this.shadows)
  }

  cactus(height = 2.5): TransformNode {
    const root = new TransformNode('saguaro cactus', this.scene)
    const trunk = parent(MeshBuilder.CreateCylinder('cactus trunk', {
      diameterTop: 0.3, diameterBottom: 0.42, height, tessellation: 24,
    }, this.scene), root)
    trunk.position.y = height / 2
    trunk.material = this.cactusMaterial
    const cap = parent(MeshBuilder.CreateSphere('cactus crown', { diameter: 0.31, segments: 16 }, this.scene), root)
    cap.position.y = height
    cap.scaling.y = 0.65
    cap.material = this.cactusMaterial
    for (const side of [-1, 1]) {
      const armBase = new Vector3(side * 0.36, height * 0.53, 0)
      beam(this.scene, root, 'cactus horizontal arm', new Vector3(0, armBase.y, 0), armBase, 0.24, this.cactusMaterial)
      beam(this.scene, root, 'cactus raised arm', armBase, armBase.add(new Vector3(0, height * 0.26, 0)), 0.22, this.cactusMaterial)
    }
    for (let i = 0; i < 12; i += 1) {
      const angle = i / 12 * Math.PI * 2
      const rib = parent(MeshBuilder.CreateCylinder('cactus rib', { diameter: 0.018, height: height * 0.9, tessellation: 5 }, this.scene), root)
      rib.position.set(Math.cos(angle) * 0.19, height * 0.48, Math.sin(angle) * 0.19)
      rib.material = this.agaveMaterial
    }
    setShadows(root, this.shadows)
    root.getChildMeshes(false).forEach((mesh) => {
      if (mesh.name === 'cactus rib') this.shadows.removeShadowCaster(mesh)
    })
    return root
  }

  agave(scale = 1): TransformNode {
    const root = new TransformNode('desert agave', this.scene)
    for (let i = 0; i < 11; i += 1) {
      const angle = i / 11 * Math.PI * 2
      const leaf = parent(MeshBuilder.CreateBox('agave spear leaf', { width: 0.1, height: 0.045, depth: 1.15 }, this.scene), root)
      leaf.position.set(Math.sin(angle) * 0.35, 0.24, Math.cos(angle) * 0.35)
      leaf.rotation.set(-0.34 - (i % 3) * 0.08, angle, 0)
      leaf.scaling.set(1, 1, 0.8 + (i % 4) * 0.09)
      leaf.material = this.agaveMaterial
    }
    root.scaling.setAll(scale)
    // Thin leaves receive the large-scale shadows but do not each need a
    // separate shadow-map draw; the plant silhouette remains fully lit.
    setShadows(root, this.shadows, false)
    return root
  }

  well(): TransformNode {
    const root = new TransformNode('desert water well', this.scene)
    const stone = parent(ringStone(this.scene, 0.58, 0.88, 0.22, Math.PI * 2 / 14 * 0.94), root)
    stone.material = this.sandstone
    let firstStone = true
    for (let layer = 0; layer < 5; layer += 1) {
      for (let i = 0; i < 14; i += 1) {
        const node = firstStone ? stone : stone.createInstance(`well masonry ${layer}:${i}`)
        firstStone = false
        node.parent = root
        const angle = (i + (layer % 2) * 0.5) / 14 * Math.PI * 2
        node.rotation.y = angle
        node.position.y = layer === 0 ? 0.02 : 0.18 + (layer - 1) * 0.22
        const horizontal = layer === 0 ? 1.12 : layer === 4 ? 1.06 : 1
        const vertical = layer === 0 ? 0.72 : layer === 4 ? 0.68 : 1
        node.scaling.set(horizontal, vertical, horizontal)
      }
    }
    const shaftMaterial = this.sandstone.clone('damp well interior')
    shaftMaterial.albedoTexture = this.sandstone.albedoTexture
    shaftMaterial.bumpTexture = this.sandstone.bumpTexture
    shaftMaterial.albedoColor = Color3.FromHexString('#4c4c43')
    shaftMaterial.roughness = 0.72
    const shaft = parent(MeshBuilder.CreateCylinder('deep stone lined well shaft', {
      diameter: 1.17, height: 1.35, tessellation: 48, sideOrientation: Mesh.BACKSIDE,
    }, this.scene), root)
    shaft.position.y = 0.14
    shaft.material = shaftMaterial
    const darkness = parent(MeshBuilder.CreateDisc('well depth shadow', { radius: 0.59, tessellation: 48 }, this.scene), root)
    darkness.rotation.x = Math.PI / 2
    darkness.position.y = 0.1
    darkness.material = this.materials.darkSteel
    const water = parent(MeshBuilder.CreateDisc('well water below rim', { radius: 0.56, tessellation: 64 }, this.scene), root)
    water.rotation.x = Math.PI / 2
    water.position.y = 0.2
    water.material = this.water
    water.isPickable = false

    const rope = pbr(this.scene, 'frayed well rope', '#80613e', 1)
    rope.albedoTexture = surfaceTexture(this.scene, 'wood-bump.jpg', 5)
    rope.bumpTexture = surfaceTexture(this.scene, 'wood-bump.jpg', 5)
    rope.bumpTexture.level = 0.65
    for (const side of [-1, 1]) {
      const x = side * 1.08
      beam(this.scene, root, 'well hardwood upright', new Vector3(x, 0.05, 0), new Vector3(x, 2.3, 0), 0.16, this.materials.wood)
      beam(this.scene, root, 'well diagonal brace', new Vector3(x, 1.65, 0), new Vector3(side * 0.58, 2.27, 0), 0.09, this.materials.wood)
      const footing = parent(MeshBuilder.CreateBox('well support footing', { width: 0.36, height: 0.24, depth: 0.42 }, this.scene), root)
      footing.position.set(x, 0.12, 0)
      footing.rotation.y = side * 0.12
      footing.material = this.sandstone
      for (const y of [0.43, 1.72]) {
        const band = parent(MeshBuilder.CreateTorus('well post iron band', { diameter: 0.18, thickness: 0.022, tessellation: 20 }, this.scene), root)
        band.position.set(x, y, 0)
        band.material = this.materials.darkSteel
      }
    }
    beam(this.scene, root, 'well structural crossbeam', new Vector3(-1.28, 2.28, 0), new Vector3(1.28, 2.28, 0), 0.19, this.materials.wood)
    beam(this.scene, root, 'well pulley axle', new Vector3(0, 2.01, -0.24), new Vector3(0, 2.01, 0.24), 0.065, this.materials.steel)
    const pulley = parent(MeshBuilder.CreateTorus('grooved well pulley', { diameter: 0.44, thickness: 0.055, tessellation: 40 }, this.scene), root)
    pulley.position.set(0, 2.01, 0)
    pulley.rotation.x = Math.PI / 2
    pulley.material = this.materials.copper
    beam(this.scene, root, 'hanging well rope', new Vector3(0, 2.02, 0.03), new Vector3(0, 0.27, 0.03), 0.026, rope)
    for (let i = 0; i < 3; i += 1) {
      const coil = parent(MeshBuilder.CreateTorus('coiled spare rope', { diameter: 0.36 + i * 0.045, thickness: 0.022, tessellation: 32 }, this.scene), root)
      coil.position.set(-0.42, 1.01 + i * 0.012, 0.59)
      coil.material = rope
    }

    const bucket = new TransformNode('weathered well bucket', this.scene)
    bucket.parent = root
    bucket.position.set(1.16, 0.04, 0.4)
    bucket.rotation.z = -0.13
    const bucketBody = parent(MeshBuilder.CreateCylinder('bucket staves', {
      diameterTop: 0.43, diameterBottom: 0.33, height: 0.46, tessellation: 24,
    }, this.scene), bucket)
    bucketBody.position.y = 0.25
    bucketBody.material = this.materials.wood
    for (const y of [0.09, 0.4]) {
      const band = parent(MeshBuilder.CreateTorus('bucket iron hoop', { diameter: y > 0.2 ? 0.43 : 0.35, thickness: 0.025, tessellation: 28 }, this.scene), bucket)
      band.position.y = y
      band.material = this.materials.darkSteel
    }
    const handle = parent(MeshBuilder.CreateTorus('bucket carrying handle', { diameter: 0.47, thickness: 0.018, tessellation: 32 }, this.scene), bucket)
    handle.position.y = 0.51
    handle.rotation.x = Math.PI / 2
    handle.material = this.materials.steel
    setShadows(root, this.shadows)
    water.receiveShadows = false
    return root
  }

  ruins(): TransformNode {
    const root = new TransformNode('buried caravan ruin', this.scene)
    for (let i = 0; i < 3; i += 1) {
      const height = 1.4 + (i % 2) * 1.15
      const column = parent(MeshBuilder.CreateCylinder('broken sandstone column', {
        diameterTop: 0.48, diameterBottom: 0.62, height, tessellation: 8,
      }, this.scene), root)
      column.position.set((i - 1) * 1.55, height / 2, (i % 2) * 0.35)
      column.rotation.z = (i - 1) * 0.045
      column.material = this.sandstone
    }
    const lintel = parent(MeshBuilder.CreateBox('fallen carved lintel', { width: 3.5, height: 0.42, depth: 0.68 }, this.scene), root)
    lintel.position.set(0.3, 0.28, -1.15)
    lintel.rotation.set(0.08, 0.28, -0.06)
    lintel.material = this.sandstone
    for (let i = 0; i < 5; i += 1) {
      const block = parent(MeshBuilder.CreateBox('ruin rubble', { size: 0.45 + (i % 2) * 0.2 }, this.scene), root)
      block.position.set(-1.8 + i * 0.72, 0.18, 1 + (i % 2) * 0.45)
      block.rotation.set(i * 0.12, i * 0.7, i * 0.08)
      block.material = this.sandstone
    }
    setShadows(root, this.shadows)
    return root
  }

  landmarkArch(): TransformNode {
    const root = new TransformNode('half buried caravan arch', this.scene)
    for (const side of [-1, 1]) {
      const column = parent(MeshBuilder.CreateCylinder('arch weathered pier', {
        diameterTop: 0.68, diameterBottom: 0.9, height: 2.55, tessellation: 9,
      }, this.scene), root)
      column.position.set(side * 1.35, 1.275, 0)
      column.rotation.z = side * 0.035
      column.material = this.sandstone
    }
    for (let i = 0; i < 9; i += 1) {
      const angle = Math.PI - (i / 8) * Math.PI
      const block = parent(MeshBuilder.CreateBox('arch voussoir', {
        width: 0.58, height: 0.52, depth: 0.78,
      }, this.scene), root)
      block.position.set(Math.cos(angle) * 1.35, 2.55 + Math.sin(angle) * 1.35, 0)
      block.rotation.z = angle - Math.PI / 2
      block.rotation.y = (i % 2 ? 1 : -1) * 0.035
      block.material = this.sandstone
    }
    for (let i = 0; i < 6; i += 1) {
      const rubble = parent(MeshBuilder.CreateBox('arch fallen masonry', {
        width: 0.45 + (i % 2) * 0.2, height: 0.3 + (i % 3) * 0.08, depth: 0.5,
      }, this.scene), root)
      rubble.position.set(-2.1 + i * 0.78, 0.18, 0.7 + (i % 2) * 0.45)
      rubble.rotation.set(0.08 * i, i * 0.61, -0.05 * i)
      rubble.material = this.sandstone
    }
    setShadows(root, this.shadows)
    return root
  }

  oasis(radius = 6): TransformNode {
    const root = new TransformNode('shallow desert oasis', this.scene)
    const water = parent(MeshBuilder.CreateDisc('oasis water surface', { radius, tessellation: 128 }, this.scene), root)
    water.rotation.x = Math.PI / 2
    water.position.y = 0.07
    water.material = this.water
    water.isPickable = false
    for (let i = 0; i < 18; i += 1) {
      const angle = i / 18 * Math.PI * 2
      const stone = parent(MeshBuilder.CreateIcoSphere('oasis edge stone', { radius: 0.32 + (i % 3) * 0.09, subdivisions: 2 }, this.scene), root)
      stone.position.set(Math.cos(angle) * (radius + 0.15), 0.12, Math.sin(angle) * (radius + 0.15))
      stone.scaling.set(1.3, 0.55, 0.9)
      stone.rotation.y = angle
      stone.material = this.sandstone
    }
    setShadows(root, this.shadows)
    water.receiveShadows = false
    return root
  }

  spear(): TransformNode {
    const root = new TransformNode('survivor spear pivot', this.scene)
    const shaft = parent(MeshBuilder.CreateCylinder('spear hardwood shaft', { diameter: 0.05, height: 1.55, tessellation: 16 }, this.scene), root)
    shaft.material = this.materials.wood
    const bladeMaterial = this.materials.steel.clone('honed spear steel')
    bladeMaterial.albedoTexture = this.materials.steel.albedoTexture
    bladeMaterial.bumpTexture = this.materials.steel.bumpTexture
    bladeMaterial.albedoColor = Color3.FromHexString('#b8c4c5')
    bladeMaterial.roughness = 0.2
    bladeMaterial.environmentIntensity = 1.15
    const blade = parent(MeshBuilder.CreateCylinder('forged leaf spear point', { diameterTop: 0, diameterBottom: 0.21, height: 0.58, tessellation: 4 }, this.scene), root)
    blade.position.y = 1.05
    blade.rotation.y = Math.PI / 4
    blade.scaling.z = 0.42
    blade.material = bladeMaterial
    const socket = parent(MeshBuilder.CreateCylinder('spear forged socket', { diameter: 0.078, height: 0.17, tessellation: 16 }, this.scene), root)
    socket.position.y = 0.75
    socket.material = bladeMaterial
    for (let i = 0; i < 6; i += 1) {
      const wrap = parent(MeshBuilder.CreateTorus('spear binding', { diameter: 0.074, thickness: 0.012, tessellation: 12 }, this.scene), root)
      wrap.position.y = 0.55 + i * 0.045
      wrap.material = this.materials.copper
    }
    root.position.set(0.3, 1, -0.18)
    setShadows(root, this.shadows)
    return root
  }

  relay(): RelayVisual {
    const root = new TransformNode('desert radio relay', this.scene)
    const base = parent(MeshBuilder.CreateCylinder('relay foundation', { diameterTop: 2.7, diameterBottom: 3.1, height: 0.35, tessellation: 12 }, this.scene), root)
    base.position.y = 0.18
    base.material = this.materials.concrete
    const levels = [0.35, 2.1, 3.85, 5.6]
    for (let leg = 0; leg < 3; leg += 1) {
      const angle = leg / 3 * Math.PI * 2
      const bottom = new Vector3(Math.cos(angle) * 1.18, 0.35, Math.sin(angle) * 1.18)
      const top = new Vector3(Math.cos(angle) * 0.48, 5.75, Math.sin(angle) * 0.48)
      beam(this.scene, root, 'relay tapered leg', bottom, top, 0.14, this.materials.rust)
      for (let level = 0; level < levels.length - 1; level += 1) {
        const y0 = levels[level]
        const y1 = levels[level + 1]
        const r0 = 1.18 - (y0 / 5.75) * 0.7
        const r1 = 1.18 - (y1 / 5.75) * 0.7
        const next = (leg + 1) % 3
        const angleNext = next / 3 * Math.PI * 2
        beam(this.scene, root, 'relay diagonal brace', new Vector3(Math.cos(angle) * r0, y0, Math.sin(angle) * r0), new Vector3(Math.cos(angleNext) * r1, y1, Math.sin(angleNext) * r1), 0.065, this.materials.steel)
      }
    }
    const platform = parent(MeshBuilder.CreateCylinder('relay service platform', { diameter: 1.65, height: 0.12, tessellation: 18 }, this.scene), root)
    platform.position.y = 5.45
    platform.material = this.materials.darkSteel
    beam(this.scene, root, 'relay antenna mast', new Vector3(0, 5.45, 0), new Vector3(0, 8.2, 0), 0.1, this.materials.steel)
    for (const side of [-1, 1]) {
      const panel = parent(MeshBuilder.CreateBox('relay solar panel', { width: 1.5, height: 0.07, depth: 0.85 }, this.scene), root)
      panel.position.set(side * 1.2, 3.65, 0)
      panel.rotation.z = side * -0.22
      panel.material = this.solar
      for (let line = -2; line <= 2; line += 1) {
        const strip = parent(MeshBuilder.CreateBox('solar cell busbar', { width: 0.018, height: 0.075, depth: 0.82 }, this.scene), root)
        strip.position.set(side * 1.2 + line * 0.25, 3.65, 0)
        strip.rotation.z = side * -0.22
        strip.material = this.materials.copper
      }
    }
    const dish = parent(MeshBuilder.CreateSphere('relay microwave reflector', { diameter: 1.35, segments: 20 }, this.scene), root)
    dish.position.set(0.45, 6.55, 0)
    dish.scaling.set(1, 0.22, 1)
    dish.rotation.set(0.45, 0, -0.35)
    dish.material = this.materials.steel
    const statusMaterial = this.sensor.clone('relay signal diode')
    const status = parent(MeshBuilder.CreateSphere('relay status diode', { diameter: 0.2, segments: 12 }, this.scene), root)
    status.position.set(0, 8.25, 0)
    status.material = statusMaterial
    const light = new PointLight('relay status practical', new Vector3(0, 7.7, 0), this.scene)
    light.parent = root
    light.diffuse = Color3.FromHexString('#ff9b4a')
    light.intensity = 8
    light.range = 9
    setShadows(root, this.shadows)
    return {
      root,
      setActive: (active: boolean) => {
        statusMaterial.emissiveColor = Color3.FromHexString(active ? '#79ffd0' : '#ff8b35')
        statusMaterial.albedoColor = Color3.FromHexString(active ? '#a5ffe0' : '#ffc37a')
        light.diffuse = Color3.FromHexString(active ? '#79ffd0' : '#ff9b4a')
        light.intensity = active ? 32 : 8
      },
    }
  }

  sentry(): SentryVisual {
    const root = new TransformNode('tripod desert sentry', this.scene)
    for (let i = 0; i < 3; i += 1) {
      const angle = i / 3 * Math.PI * 2
      beam(this.scene, root, 'sentry tripod leg', new Vector3(0, 0.9, 0), new Vector3(Math.cos(angle) * 0.72, 0.05, Math.sin(angle) * 0.72), 0.09, this.materials.darkSteel)
      const foot = parent(MeshBuilder.CreateBox('sentry anchored foot', { width: 0.28, height: 0.08, depth: 0.38 }, this.scene), root)
      foot.position.set(Math.cos(angle) * 0.72, 0.05, Math.sin(angle) * 0.72)
      foot.rotation.y = -angle
      foot.material = this.materials.rust
    }
    const traverse = parent(MeshBuilder.CreateCylinder('sentry traverse bearing', { diameter: 0.42, height: 0.28, tessellation: 16 }, this.scene), root)
    traverse.position.y = 0.94
    traverse.material = this.materials.copper
    const head = new TransformNode('sentry tracking head', this.scene)
    head.parent = root
    head.position.y = 1.22
    const receiver = parent(MeshBuilder.CreateBox('sentry angular receiver', { width: 0.72, height: 0.42, depth: 0.78 }, this.scene), head)
    receiver.material = this.materials.darkSteel
    const shield = parent(MeshBuilder.CreateBox('sentry front armour', { width: 0.82, height: 0.5, depth: 0.08 }, this.scene), head)
    shield.position.set(0, 0, 0.43)
    shield.rotation.x = -0.12
    shield.material = this.materials.rust
    for (const side of [-1, 1]) {
      const barrel = parent(MeshBuilder.CreateCylinder('sentry twin barrel', { diameter: 0.075, height: 1.05, tessellation: 10 }, this.scene), head)
      barrel.rotation.x = Math.PI / 2
      barrel.position.set(side * 0.2, -0.04, 0.92)
      barrel.material = this.materials.steel
    }
    const optic = parent(MeshBuilder.CreateSphere('sentry tracking optic', { diameter: 0.18, segments: 12 }, this.scene), head)
    optic.position.set(0, 0.18, 0.48)
    optic.material = this.sensor
    const ammo = parent(MeshBuilder.CreateBox('sentry side ammunition', { width: 0.24, height: 0.38, depth: 0.48 }, this.scene), head)
    ammo.position.set(-0.47, -0.02, -0.05)
    ammo.material = this.materials.rust
    const muzzle = new TransformNode('sentry muzzle', this.scene)
    muzzle.parent = head
    muzzle.position.set(0, -0.04, 1.47)
    setShadows(root, this.shadows)
    return { root, head, muzzle }
  }
}
