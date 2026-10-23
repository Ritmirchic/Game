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
import { setShadows } from '../game/models'
import { DesertModels } from './DesertModels'

function parent<T extends TransformNode>(node: T, root: TransformNode): T {
  node.parent = root
  return node
}

export class CampaignModels {
  private readonly emissive: PBRMaterial
  private readonly warning: PBRMaterial

  constructor(private scene: Scene, private shadows: ShadowGenerator, private base: DesertModels) {
    this.emissive = base.sensor.clone('campaign signal amber')
    this.warning = new PBRMaterial('telegraphed danger zone', scene)
    this.warning.albedoColor = Color3.FromHexString('#7d1e12')
    this.warning.emissiveColor = Color3.FromHexString('#ff3f1f')
    this.warning.emissiveIntensity = 1.4
    this.warning.alpha = 0.42
    this.warning.backFaceCulling = false
  }

  private beam(root: TransformNode, name: string, from: Vector3, to: Vector3, diameter: number, material: PBRMaterial): Mesh {
    const direction = to.subtract(from)
    const mesh = parent(MeshBuilder.CreateCylinder(name, { diameter, height: direction.length(), tessellation: 8 }, this.scene), root)
    mesh.position.copyFrom(from.add(to).scale(0.5))
    mesh.rotationQuaternion = Quaternion.FromUnitVectorsToRef(Vector3.Up(), direction.normalize(), new Quaternion())
    mesh.material = material
    return mesh
  }

  cache(): TransformNode {
    const root = new TransformNode('campaign supply cache', this.scene)
    const crate = parent(MeshBuilder.CreateBox('sealed caravan container', { width: 1.35, height: 0.72, depth: 0.85 }, this.scene), root)
    crate.position.y = 0.38
    crate.material = this.base.materials.darkSteel
    for (const x of [-0.48, 0.48]) {
      const strap = parent(MeshBuilder.CreateBox('container copper strap', { width: 0.09, height: 0.76, depth: 0.89 }, this.scene), root)
      strap.position.set(x, 0.4, 0)
      strap.material = this.base.materials.copper
    }
    const beacon = parent(MeshBuilder.CreateCylinder('cache emergency beacon', { diameter: 0.17, height: 0.34, tessellation: 12 }, this.scene), root)
    beacon.position.set(0, 0.93, 0)
    beacon.material = this.emissive
    const ring = parent(MeshBuilder.CreateTorus('cache signal ring', { diameter: 0.5, thickness: 0.035, tessellation: 20 }, this.scene), root)
    ring.position.y = 1.12
    ring.material = this.emissive
    setShadows(root, this.shadows)
    return root
  }

  console(): TransformNode {
    const root = new TransformNode('campaign field console', this.scene)
    const foot = parent(MeshBuilder.CreateCylinder('console footing', { diameter: 1.25, height: 0.18, tessellation: 12 }, this.scene), root)
    foot.position.y = 0.09
    foot.material = this.base.materials.concrete
    const pedestal = parent(MeshBuilder.CreateBox('console pedestal', { width: 0.58, height: 1.05, depth: 0.52 }, this.scene), root)
    pedestal.position.y = 0.65
    pedestal.material = this.base.materials.rust
    const panel = parent(MeshBuilder.CreateBox('console angled display', { width: 0.92, height: 0.12, depth: 0.62 }, this.scene), root)
    panel.position.set(0, 1.25, 0.1)
    panel.rotation.x = -0.28
    panel.material = this.base.solar
    for (const x of [-0.25, 0, 0.25]) {
      const diode = parent(MeshBuilder.CreateSphere('console status diode', { diameter: 0.09, segments: 8 }, this.scene), root)
      diode.position.set(x, 1.34, -0.03)
      diode.material = this.emissive
    }
    setShadows(root, this.shadows)
    return root
  }

  caravan(): TransformNode {
    const root = new TransformNode('landmark dead caravan', this.scene)
    for (let wagon = 0; wagon < 3; wagon += 1) {
      const x = (wagon - 1) * 5.2
      const z = wagon % 2 * 2.2
      const bed = parent(MeshBuilder.CreateBox('caravan wagon bed', { width: 3.5, height: 0.45, depth: 1.9 }, this.scene), root)
      bed.position.set(x, 0.85, z)
      bed.rotation.y = wagon * 0.18 - 0.2
      bed.material = this.base.materials.wood
      for (const side of [-1, 1]) {
        for (const axle of [-1, 1]) {
          const wheel = parent(MeshBuilder.CreateTorus('caravan iron wheel', { diameter: 1.15, thickness: 0.12, tessellation: 16 }, this.scene), root)
          wheel.position.set(x + axle * 1.15, 0.6, z + side * 1.02)
          wheel.rotation.x = Math.PI / 2
          wheel.material = this.base.materials.rust
        }
      }
      const canopy = parent(MeshBuilder.CreateCylinder('collapsed wagon canopy', { diameter: 2.1, height: 3.1, tessellation: 12 }, this.scene), root)
      canopy.position.set(x, 1.65, z)
      canopy.rotation.z = Math.PI / 2
      canopy.scaling.y = 0.52
      canopy.material = wagon === 1 ? this.base.sandstone : this.base.materials.rust
      this.beam(root, 'broken wagon shaft', new Vector3(x + 1.4, 0.8, z), new Vector3(x + 3.5, 0.25, z + 0.3), 0.13, this.base.materials.wood)
    }
    for (let i = 0; i < 7; i += 1) {
      const crate = parent(MeshBuilder.CreateBox('abandoned caravan cargo', { size: 0.65 + (i % 2) * 0.25 }, this.scene), root)
      crate.position.set(-6 + i * 1.9, 0.38, -2.2 + (i % 3) * 0.7)
      crate.rotation.set(0.05 * i, i * 0.43, 0.04)
      crate.material = this.base.materials.wood
    }
    setShadows(root, this.shadows)
    return root
  }

  wellStation(): TransformNode {
    const root = new TransformNode('landmark salt wells', this.scene)
    const well = this.base.well()
    well.parent = root
    for (let i = 0; i < 3; i += 1) {
      const angle = i / 3 * Math.PI * 2
      const position = new Vector3(Math.cos(angle) * 5.2, 0, Math.sin(angle) * 5.2)
      this.beam(root, 'salt well underground pipe', position, position.add(new Vector3(0, 1.4, 0)), 0.28, this.base.materials.copper)
      const cap = parent(MeshBuilder.CreateCylinder('pressure vessel', { diameter: 0.8, height: 1.35, tessellation: 14 }, this.scene), root)
      cap.position.copyFrom(position.add(new Vector3(0, 1.35, 0)))
      cap.material = this.base.materials.rust
    }
    setShadows(root, this.shadows)
    return root
  }

  city(): TransformNode {
    const root = new TransformNode('landmark buried city', this.scene)
    for (let i = 0; i < 8; i += 1) {
      const angle = i / 8 * Math.PI * 2
      const radius = 6 + (i % 3) * 2.2
      const height = 3.4 + (i % 4) * 1.4
      const tower = parent(MeshBuilder.CreateCylinder('buried city tower', { diameterTop: 0.85, diameterBottom: 1.35, height, tessellation: 8 }, this.scene), root)
      tower.position.set(Math.cos(angle) * radius, height / 2 - 0.35, Math.sin(angle) * radius)
      tower.rotation.z = (i % 2 ? 1 : -1) * 0.05
      tower.material = this.base.sandstone
      const crown = parent(MeshBuilder.CreateBox('fractured tower crown', { width: 1.55, height: 0.25, depth: 1.55 }, this.scene), root)
      crown.position.set(tower.position.x, height - 0.25, tower.position.z)
      crown.rotation.y = angle
      crown.material = this.base.materials.concrete
    }
    for (const side of [-1, 1]) {
      this.beam(root, 'city gate pillar', new Vector3(side * 2.2, 0, -1), new Vector3(side * 2.2, 4.4, -1), 0.8, this.base.sandstone)
    }
    this.beam(root, 'city gate lintel', new Vector3(-2.4, 4.25, -1), new Vector3(2.4, 4.25, -1), 0.72, this.base.sandstone)
    setShadows(root, this.shadows)
    return root
  }

  stormSpire(): TransformNode {
    const root = new TransformNode('landmark storm spire', this.scene)
    const base = parent(MeshBuilder.CreateCylinder('storm stabilizer base', { diameter: 4.2, height: 0.55, tessellation: 12 }, this.scene), root)
    base.position.y = 0.28
    base.material = this.base.materials.concrete
    const monolith = parent(MeshBuilder.CreateCylinder('black storm monolith', { diameterTop: 0.5, diameterBottom: 1.6, height: 7.5, tessellation: 5 }, this.scene), root)
    monolith.position.y = 4
    monolith.material = this.base.materials.darkSteel
    for (let i = 0; i < 4; i += 1) {
      const angle = i / 4 * Math.PI * 2
      this.beam(root, 'storm lightning conductor', new Vector3(Math.cos(angle) * 1.8, 0.4, Math.sin(angle) * 1.8), new Vector3(Math.cos(angle) * 0.65, 6.7, Math.sin(angle) * 0.65), 0.11, this.base.materials.copper)
      const crystal = parent(MeshBuilder.CreateCylinder('storm crystal', { diameterTop: 0, diameterBottom: 0.48, height: 1.4, tessellation: 5 }, this.scene), root)
      crystal.position.set(Math.cos(angle) * 1.25, 2.1 + i * 0.7, Math.sin(angle) * 1.25)
      crystal.rotation.z = 0.4
      crystal.material = this.base.materials.crystal
    }
    const light = new PointLight('storm spire glow', new Vector3(0, 5, 0), this.scene)
    light.parent = root
    light.diffuse = Color3.FromHexString('#75b8ff')
    light.intensity = 18
    light.range = 18
    setShadows(root, this.shadows)
    return root
  }

  citadel(): TransformNode {
    const root = new TransformNode('landmark meridian citadel', this.scene)
    const dais = parent(MeshBuilder.CreateCylinder('meridian central dais', { diameter: 8.5, height: 0.42, tessellation: 24 }, this.scene), root)
    dais.position.y = 0.21
    dais.material = this.base.materials.concrete
    for (let i = 0; i < 6; i += 1) {
      const angle = i / 6 * Math.PI * 2
      const x = Math.cos(angle) * 7.4
      const z = Math.sin(angle) * 7.4
      const pillar = parent(MeshBuilder.CreateBox('meridian monolith', { width: 1.1, height: 6.8, depth: 1.1 }, this.scene), root)
      pillar.position.set(x, 3.25, z)
      pillar.rotation.y = -angle
      pillar.material = this.base.materials.darkSteel
      const band = parent(MeshBuilder.CreateTorus('meridian suspended ring', { diameter: 1.65, thickness: 0.08, tessellation: 24 }, this.scene), root)
      band.position.set(x, 4.8, z)
      band.rotation.x = Math.PI / 2
      band.material = this.emissive
    }
    const core = parent(MeshBuilder.CreateSphere('meridian central core', { diameter: 1.35, segments: 20 }, this.scene), root)
    core.position.y = 3.3
    core.material = this.base.materials.crystal
    this.beam(root, 'meridian core spindle', new Vector3(0, 0.4, 0), new Vector3(0, 5.8, 0), 0.15, this.base.materials.copper)
    setShadows(root, this.shadows)
    return root
  }

  waypoint(): TransformNode {
    const root = new TransformNode('campaign objective beacon', this.scene)
    const material = this.emissive.clone('objective translucent amber')
    material.alpha = 0.18
    const beam = parent(MeshBuilder.CreateCylinder('objective vertical light', { diameter: 0.32, height: 56, tessellation: 8 }, this.scene), root)
    beam.position.y = 28
    beam.material = material
    beam.applyFog = false
    beam.isPickable = false
    for (const y of [1.2, 3.2]) {
      const ring = parent(MeshBuilder.CreateTorus('objective ground ring', { diameter: 3 + y * 0.35, thickness: 0.06, tessellation: 32 }, this.scene), root)
      ring.rotation.x = Math.PI / 2
      ring.position.y = y
      ring.material = this.emissive
      ring.applyFog = false
      ring.isPickable = false
    }
    return root
  }

  warningZone(radius: number): TransformNode {
    const root = new TransformNode('incoming area strike', this.scene)
    const disc = parent(MeshBuilder.CreateDisc('danger area fill', { radius, tessellation: 40 }, this.scene), root)
    disc.rotation.x = Math.PI / 2
    disc.position.y = 0.045
    disc.material = this.warning
    disc.isPickable = false
    const ring = parent(MeshBuilder.CreateTorus('danger area outline', { diameter: radius * 2, thickness: 0.09, tessellation: 40 }, this.scene), root)
    ring.rotation.x = Math.PI / 2
    ring.position.y = 0.075
    ring.material = this.warning
    ring.isPickable = false
    return root
  }

  enemyAccessory(height: number, armored: boolean, boss: boolean): TransformNode {
    const root = new TransformNode('enemy campaign armour', this.scene)
    if (armored) {
      for (const side of [-1, 1]) {
        const plate = parent(MeshBuilder.CreateBox('beast improvised armour', { width: height * 0.38, height: height * 0.14, depth: height * 0.45 }, this.scene), root)
        plate.position.set(side * height * 0.22, height * 0.58, -height * 0.05)
        plate.rotation.z = side * 0.28
        plate.material = this.base.materials.rust
      }
    }
    if (boss) {
      for (let i = 0; i < 5; i += 1) {
        const spike = parent(MeshBuilder.CreateCylinder('boss resonant crown', { diameterTop: 0, diameterBottom: height * 0.09, height: height * 0.35, tessellation: 5 }, this.scene), root)
        spike.position.set((i - 2) * height * 0.1, height * (0.77 + (i % 2) * 0.05), -height * 0.12)
        spike.rotation.z = (i - 2) * -0.13
        spike.material = this.base.materials.crystal
      }
    }
    setShadows(root, this.shadows)
    return root
  }
}
