import {
  Color3,
  Mesh,
  MeshBuilder,
  PBRMaterial,
  PointLight,
  Quaternion,
  Scene,
  ShadowGenerator,
  TransformNode,
  Vector3,
} from '@babylonjs/core'
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

function beam(scene: Scene, root: TransformNode, name: string, from: Vector3, to: Vector3, diameter: number, material: PBRMaterial): Mesh {
  const direction = to.subtract(from)
  const mesh = parent(MeshBuilder.CreateCylinder(name, { diameter, height: direction.length(), tessellation: 8 }, scene), root)
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
  readonly water: PBRMaterial

  constructor(private scene: Scene, private shadows: ShadowGenerator) {
    this.materials = createMaterialLibrary(scene)
    this.cactusMaterial = pbr(scene, 'ribbed cactus skin', '#435d3c', 0.88)
    this.agaveMaterial = pbr(scene, 'dusty agave leaves', '#667b59', 0.9)
    this.sandstone = pbr(scene, 'eroded desert sandstone', '#9a7952', 0.94)
    this.solar = pbr(scene, 'weathered photovoltaic glass', '#172d37', 0.2, 0.48)
    this.sensor = pbr(scene, 'sentry amber optics', '#ffc37a', 0.18, 0.15)
    this.sensor.emissiveColor = Color3.FromHexString('#ff8b35')
    this.sensor.emissiveIntensity = 1.4
    this.water = pbr(scene, 'shallow oasis water', '#174f53', 0.1, 0.08)
    this.water.environmentIntensity = 0.45
    this.water.alpha = 0.86
    this.water.indexOfRefraction = 1.33
    this.water.backFaceCulling = false
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
      diameterTop: 0.3, diameterBottom: 0.42, height, tessellation: 12,
    }, this.scene), root)
    trunk.position.y = height / 2
    trunk.material = this.cactusMaterial
    const cap = parent(MeshBuilder.CreateSphere('cactus crown', { diameter: 0.31, segments: 10 }, this.scene), root)
    cap.position.y = height
    cap.scaling.y = 0.65
    cap.material = this.cactusMaterial
    for (const side of [-1, 1]) {
      const armBase = new Vector3(side * 0.36, height * 0.53, 0)
      beam(this.scene, root, 'cactus horizontal arm', new Vector3(0, armBase.y, 0), armBase, 0.24, this.cactusMaterial)
      beam(this.scene, root, 'cactus raised arm', armBase, armBase.add(new Vector3(0, height * 0.26, 0)), 0.22, this.cactusMaterial)
    }
    for (let i = 0; i < 8; i += 1) {
      const angle = i / 8 * Math.PI * 2
      const rib = parent(MeshBuilder.CreateCylinder('cactus rib', { diameter: 0.018, height: height * 0.9, tessellation: 5 }, this.scene), root)
      rib.position.set(Math.cos(angle) * 0.19, height * 0.48, Math.sin(angle) * 0.19)
      rib.material = this.agaveMaterial
    }
    setShadows(root, this.shadows)
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
    setShadows(root, this.shadows)
    return root
  }

  well(): TransformNode {
    const root = new TransformNode('desert water well', this.scene)
    const ring = parent(MeshBuilder.CreateTorus('sandstone well rim', { diameter: 1.65, thickness: 0.32, tessellation: 20 }, this.scene), root)
    ring.rotation.x = Math.PI / 2
    ring.position.y = 0.62
    ring.material = this.sandstone
    const shaft = parent(MeshBuilder.CreateCylinder('well shaft', { diameter: 1.52, height: 0.62, tessellation: 20 }, this.scene), root)
    shaft.position.y = 0.31
    shaft.material = this.sandstone
    const water = parent(MeshBuilder.CreateDisc('well water', { radius: 0.62, tessellation: 24 }, this.scene), root)
    water.rotation.x = Math.PI / 2
    water.position.y = 0.56
    water.material = this.water
    for (const side of [-1, 1]) {
      beam(this.scene, root, 'well support', new Vector3(side * 0.82, 0, 0), new Vector3(side * 0.82, 2.15, 0), 0.1, this.materials.wood)
    }
    beam(this.scene, root, 'well crossbeam', new Vector3(-0.95, 2.12, 0), new Vector3(0.95, 2.12, 0), 0.12, this.materials.wood)
    setShadows(root, this.shadows)
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

  oasis(radius = 6): TransformNode {
    const root = new TransformNode('shallow desert oasis', this.scene)
    const water = parent(MeshBuilder.CreateDisc('oasis water surface', { radius, tessellation: 64 }, this.scene), root)
    water.rotation.x = Math.PI / 2
    water.position.y = 0.07
    water.material = this.water
    water.isPickable = false
    for (let i = 0; i < 18; i += 1) {
      const angle = i / 18 * Math.PI * 2
      const stone = parent(MeshBuilder.CreateIcoSphere('oasis edge stone', { radius: 0.32 + (i % 3) * 0.09, subdivisions: 1 }, this.scene), root)
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
    const shaft = parent(MeshBuilder.CreateCylinder('spear hardwood shaft', { diameter: 0.05, height: 1.55, tessellation: 10 }, this.scene), root)
    shaft.material = this.materials.wood
    const blade = parent(MeshBuilder.CreateCylinder('forged spear point', { diameterTop: 0, diameterBottom: 0.22, height: 0.5, tessellation: 4 }, this.scene), root)
    blade.position.y = 1.02
    blade.material = this.materials.steel
    for (let i = 0; i < 4; i += 1) {
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
