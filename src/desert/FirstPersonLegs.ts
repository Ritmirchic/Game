import {
  Color3,
  Mesh,
  MeshBuilder,
  PBRMaterial,
  Scene,
  ShadowGenerator,
  TransformNode,
  VertexData,
} from '@babylonjs/core'

/** A beveled shoe last with an explicit +Z toe and a matching sole footprint. */
function createBootMesh(scene: Scene, name: string, material: PBRMaterial, sole = false): Mesh {
  const last = [
    { z: -0.28, width: 0.05, bottom: 0.06, top: 0.14 },
    { z: -0.25, width: 0.085, bottom: 0.055, top: 0.17 },
    { z: -0.16, width: 0.11, bottom: 0.04, top: 0.25 },
    { z: -0.02, width: 0.115, bottom: 0.035, top: 0.17 },
    { z: 0.15, width: 0.13, bottom: 0.03, top: 0.13 },
    { z: 0.25, width: 0.105, bottom: 0.04, top: 0.105 },
    { z: 0.28, width: 0.07, bottom: 0.045, top: 0.09 },
    { z: 0.295, width: 0.025, bottom: 0.05, top: 0.075 },
  ]
  const rings = last.map((ring) => sole
    ? { ...ring, width: ring.width + 0.012, bottom: 0.015, top: 0.055 }
    : ring)
  const positions: number[] = []
  const indices: number[] = []
  const ringSize = 8
  rings.forEach((ring) => {
    const { width: w, bottom: b, top: t, z } = ring
    const h = t - b
    const section = [
      [-w * 0.72, b], [w * 0.72, b], [w, b + h * 0.2], [w, t - h * 0.2],
      [w * 0.64, t], [-w * 0.64, t], [-w, t - h * 0.2], [-w, b + h * 0.2],
    ]
    section.forEach(([x, y]) => positions.push(x, y, z))
  })
  for (let ring = 0; ring < rings.length - 1; ring += 1) {
    for (let corner = 0; corner < ringSize; corner += 1) {
      const next = (corner + 1) % ringSize
      const a = ring * ringSize + corner
      const b = ring * ringSize + next
      const c = (ring + 1) * ringSize + next
      const d = (ring + 1) * ringSize + corner
      indices.push(a, b, c, a, c, d)
    }
  }
  // Closed heel and toe caps keep the silhouette solid at steep camera angles.
  const heelCenter = positions.length / 3
  const heel = rings[0]
  positions.push(0, (heel.bottom + heel.top) * 0.5, heel.z)
  const toeCenter = positions.length / 3
  const toe = rings[rings.length - 1]
  positions.push(0, (toe.bottom + toe.top) * 0.5, toe.z)
  for (let corner = 0; corner < ringSize; corner += 1) {
    const next = (corner + 1) % ringSize
    indices.push(heelCenter, next, corner)
    const toeStart = (rings.length - 1) * ringSize
    indices.push(toeCenter, toeStart + corner, toeStart + next)
  }
  // VertexData.ComputeNormals uses Babylon's left-handed face convention;
  // reverse the ring triangles so the bevel normals point out of the shoe.
  for (let i = 0; i < indices.length; i += 3) {
    const second = indices[i + 1]
    indices[i + 1] = indices[i + 2]
    indices[i + 2] = second
  }
  const normals: number[] = []
  VertexData.ComputeNormals(positions, indices, normals)
  const data = new VertexData()
  data.positions = positions
  data.indices = indices
  data.normals = normals
  const mesh = new Mesh(name, scene)
  data.applyToMesh(mesh)
  mesh.material = material
  return mesh
}

/**
 * A small rigid first-person lower body. The full Soldier rig is retained for
 * arms and combat, while these meshes avoid close-camera skinning artefacts.
 */
export class FirstPersonLegs {
  private readonly root: TransformNode
  private readonly legs: TransformNode[] = []
  private readonly shins: Mesh[] = []
  private readonly straps: Mesh[] = []
  private phase = 0
  private weight = 0

  constructor(scene: Scene, parent: TransformNode, shadows: ShadowGenerator) {
    this.root = new TransformNode('first-person lower body', scene)
    this.root.parent = parent
    // Do not clone the Soldier/prop materials here: their procedural texture
    // clones can remain in a loading state and hold a mesh out of the render
    // list indefinitely. These small view-model materials are self-contained.
    const armor = new PBRMaterial('FP worn shin armour', scene)
    armor.albedoColor = Color3.FromHexString('#5b4635')
    armor.metallic = 0.28
    armor.roughness = 0.76
    armor.environmentIntensity = 0.65
    const leather = new PBRMaterial('FP flexible boot leather', scene)
    leather.albedoColor = Color3.FromHexString('#2e241e')
    leather.metallic = 0.04
    leather.roughness = 0.9
    leather.environmentIntensity = 0.45
    const sole = new PBRMaterial('FP boot rubber and fastening', scene)
    sole.albedoColor = Color3.FromHexString('#151b1a')
    sole.metallic = 0.12
    sole.roughness = 0.82
    sole.environmentIntensity = 0.4

    for (const side of [-1, 1]) {
      const leg = new TransformNode(side < 0 ? 'FP left leg' : 'FP right leg', scene)
      leg.parent = this.root
      leg.position.set(side * 0.2, 0, 0.52)
      this.legs.push(leg)

      const shin = MeshBuilder.CreateCapsule('FP shin guard', {
        height: 0.46, radius: 0.068, tessellation: 12, subdivisions: 3, capSubdivisions: 4,
      }, scene)
      shin.parent = leg
      shin.position.set(0, 0.38, -0.11)
      shin.material = armor
      this.shins.push(shin)

      const boot = createBootMesh(scene, 'FP rounded boot', leather)
      boot.parent = leg
      boot.position.z = 0.03

      const soleMesh = createBootMesh(scene, 'FP boot sole', sole, true)
      soleMesh.parent = leg
      soleMesh.position.z = 0.03

      const strap = MeshBuilder.CreateTorus('FP boot fastening', { diameter: 0.15, thickness: 0.014, tessellation: 16 }, scene)
      strap.parent = leg
      strap.position.set(0, 0.34, -0.11)
      strap.material = sole
      this.straps.push(strap)
    }
    this.root.getChildMeshes(false).forEach((mesh) => {
      mesh.receiveShadows = true
      mesh.isPickable = false
      shadows.addShadowCaster(mesh, false)
    })
    // Warm the three shader variants while the menu is still visible. This
    // avoids a first-person shader compilation hitch on the first down look.
    this.root.computeWorldMatrix(true)
    const compiled = new Set<PBRMaterial>()
    this.root.getChildMeshes(false).forEach((mesh) => {
      const material = mesh.material
      if (material instanceof PBRMaterial && !compiled.has(material)) {
        compiled.add(material)
        material.forceCompilation(mesh)
      }
    })
  }

  reset(): void {
    this.phase = 0
    this.weight = 0
    this.shins.forEach((shin) => { shin.visibility = 1; shin.isVisible = true })
    this.straps.forEach((strap) => { strap.visibility = 1; strap.isVisible = true })
    this.legs.forEach((leg, index) => {
      leg.position.set(index === 0 ? -0.2 : 0.2, 0, 0.52)
      leg.rotation.set(0, 0, 0)
    })
  }

  update(dt: number, speed: number, dodgeWeight: number, pitch: number): void {
    const shinFade = Math.min(1, Math.max(0, (pitch - 0.82) / 0.34))
    const shinVisibility = 1 - shinFade * shinFade * (3 - 2 * shinFade)
    this.shins.forEach((shin) => {
      shin.visibility = shinVisibility
      shin.isVisible = shinVisibility > 0.01
    })
    this.straps.forEach((strap) => {
      strap.visibility = shinVisibility
      strap.isVisible = shinVisibility > 0.01
    })
    const targetWeight = Math.min(1, Math.max(0, speed / 4.45))
    this.weight += (targetWeight - this.weight) * (1 - Math.exp(-dt * 8))
    if (speed > 0.18) this.phase += dt * (4.4 + speed * 0.85)
    for (let index = 0; index < this.legs.length; index += 1) {
      const wave = Math.sin(this.phase + index * Math.PI)
      const lift = Math.max(0, wave) * 0.075 * this.weight
      const stride = wave * 0.14 * this.weight
      const leg = this.legs[index]
      leg.position.set(index === 0 ? -0.2 : 0.2, lift, 0.52 + stride - dodgeWeight * 0.08)
      leg.rotation.x = wave * 0.055 * this.weight
      leg.rotation.z = (index === 0 ? -1 : 1) * dodgeWeight * 0.08
    }
  }
}
