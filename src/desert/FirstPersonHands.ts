import {
  Color3,
  Mesh,
  MeshBuilder,
  PBRMaterial,
  Quaternion,
  Scene,
  TransformNode,
  Vector3,
} from '@babylonjs/core'

function segment(scene: Scene, parent: TransformNode, name: string, from: Vector3, to: Vector3, radius: number, material: PBRMaterial, meshes: Mesh[]): Mesh {
  const direction = to.subtract(from)
  const mesh = MeshBuilder.CreateCapsule(name, {
    height: Math.max(0.025, direction.length()), radius, tessellation: 10, subdivisions: 3, capSubdivisions: 4,
  }, scene)
  mesh.parent = parent
  mesh.position.copyFrom(from.add(to).scale(0.5))
  mesh.rotationQuaternion = Quaternion.FromUnitVectorsToRef(Vector3.Up(), direction.normalize(), new Quaternion())
  mesh.material = material
  mesh.isPickable = false
  mesh.receiveShadows = false
  meshes.push(mesh)
  return mesh
}

/**
 * A rigid first-person grip. Both palms are children of the spear itself, so
 * the left hand cannot drift when the imported body animation changes length.
 */
export class FirstPersonHands {
  private readonly right: TransformNode
  private readonly left: TransformNode
  private readonly meshes: Mesh[] = []

  constructor(private readonly scene: Scene, weapon: TransformNode, private readonly glove: PBRMaterial) {
    const sleeve = new PBRMaterial('FP dark canvas forearm sleeves', scene)
    sleeve.albedoColor = Color3.FromHexString('#202927')
    sleeve.roughness = 0.88
    sleeve.metallic = 0.04
    sleeve.environmentIntensity = 0.34
    this.right = this.buildHand(weapon, 'FP right spear grip', 1, sleeve)
    this.left = this.buildHand(weapon, 'FP left spear grip', -1, sleeve)
    this.left.position.y = 0.38
    this.right.position.y = 0
    weapon.computeWorldMatrix(true)
    const warmed = new Set<PBRMaterial>()
    this.meshes.forEach((mesh) => {
      const material = mesh.material
      if (material instanceof PBRMaterial && !warmed.has(material)) {
        warmed.add(material)
        material.forceCompilation(mesh)
      }
    })
  }

  private buildHand(weapon: TransformNode, name: string, side: number, sleeve: PBRMaterial): TransformNode {
    const hand = new TransformNode(name, this.scene)
    hand.parent = weapon
    const palm = MeshBuilder.CreateSphere(`${name} · black glove palm`, { diameter: 1, segments: 16 }, this.scene)
    palm.parent = hand
    palm.position.set(side * 0.078, 0, 0.025)
    palm.scaling.set(0.125, 0.18, 0.095)
    palm.material = this.glove
    palm.isPickable = false
    palm.receiveShadows = false
    this.meshes.push(palm)

    // Four fingers curl around the front of the shaft in two rounded segments.
    const fingerOffsets = [-0.105, -0.035, 0.035, 0.105]
    fingerOffsets.forEach((y, index) => {
      const base = new Vector3(side * 0.086, y, 0.035)
      const middle = new Vector3(side * 0.052, y + 0.006, -0.047)
      const tip = new Vector3(side * 0.008, y + 0.004, -0.066)
      segment(this.scene, hand, `${name} · finger ${index + 1} proximal`, base, middle, 0.0165, this.glove, this.meshes)
      segment(this.scene, hand, `${name} · finger ${index + 1} tip`, middle, tip, 0.016, this.glove, this.meshes)
    })
    const thumbBase = new Vector3(side * 0.08, -0.12, 0.04)
    const thumbMiddle = new Vector3(side * 0.018, -0.07, -0.063)
    const thumbTip = new Vector3(side * -0.022, -0.018, -0.052)
    segment(this.scene, hand, `${name} · thumb proximal`, thumbBase, thumbMiddle, 0.02, this.glove, this.meshes)
    segment(this.scene, hand, `${name} · thumb tip`, thumbMiddle, thumbTip, 0.018, this.glove, this.meshes)

    const cuff = MeshBuilder.CreateCylinder(`${name} · glove cuff`, { diameter: 0.115, height: 0.08, tessellation: 16 }, this.scene)
    cuff.parent = hand
    cuff.position.set(side * 0.08, -0.15, 0.035)
    cuff.material = this.glove
    cuff.isPickable = false
    cuff.receiveShadows = false
    this.meshes.push(cuff)
    segment(this.scene, hand, `${name} · canvas forearm`,
      new Vector3(side * 0.08, -0.17, 0.035),
      new Vector3(side * 0.13, -0.5, 0.13), 0.048, sleeve, this.meshes)
    return hand
  }

  setVisibility(visibility: number): void {
    this.meshes.forEach((mesh) => {
      mesh.visibility = visibility
      mesh.isVisible = visibility > 0.01
    })
  }

  update(weapon: TransformNode, thrust: number, windup: number, pitch: number): void {
    const reach = thrust * 0.34
    const pullback = windup * 0.24
    const pitchDrop = Math.sin(pitch) * 0.06
    const aspect = this.scene.getEngine().getRenderWidth() / Math.max(1, this.scene.getEngine().getRenderHeight())
    const narrow = Math.max(0, Math.min(1, (1.05 - aspect) / 0.55))
    const lateral = 0.58 - narrow * 0.48
    const depth = 1.14 + narrow * 0.3
    // This is the rear grip in player-root space. The front grip is exactly
    // 38 cm up the same local-Y shaft, regardless of camera or body yaw.
    weapon.position.set(lateral, 1.2 - pitchDrop + windup * 0.05, depth + reach - pullback)
    this.right.position.set(0, 0, 0)
    this.left.position.set(0, 0.38 + thrust * 0.025, 0)
  }
}
