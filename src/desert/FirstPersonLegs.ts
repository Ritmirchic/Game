import {
  Material,
  Mesh,
  PBRMaterial,
  Quaternion,
  Scene,
  ShadowGenerator,
  TransformNode,
  Vector3,
} from '@babylonjs/core'
import { FirstPersonGeometry, rotationFromVectors } from './FirstPersonGeometry'

function smoothstep(edge0: number, edge1: number, value: number): number {
  const t = Math.min(1, Math.max(0, (value - edge0) / (edge1 - edge0)))
  return t * t * (3 - 2 * t)
}

/** Both original Soldier legs in one continuous, rigid first-person frame. */
export class FirstPersonLegs {
  private readonly root: TransformNode
  readonly bootCenters: Vector3[] = []
  private phase = 0
  private weight = 0

  constructor(scene: Scene, private readonly playerRoot: TransformNode, shadows: ShadowGenerator, body: Mesh) {
    this.root = new TransformNode('first-person continuous Soldier lower body', scene)
    const geometry = new FirstPersonGeometry(body, playerRoot)
    const original = body.material
    if (!(original instanceof PBRMaterial)) throw new Error('The original Soldier material is required')
    const material = original.clone('FP original Soldier trousers and boots · two-sided')
    material.albedoTexture = original.albedoTexture
    material.bumpTexture = original.bumpTexture
    material.metallicTexture = original.metallicTexture
    material.ambientTexture = original.ambientTexture
    material.backFaceCulling = false

    const hips = geometry.joint('Hips')
    const leftAnkle = geometry.joint('LeftFoot')
    const rightAnkle = geometry.joint('RightFoot')
    const leftToe = geometry.joint('LeftToeBase')
    const rightToe = geometry.joint('RightToeBase')
    const toeDirection = leftToe.subtract(leftAnkle).add(rightToe.subtract(rightAnkle))
    toeDirection.y = 0
    const alignment = toeDirection.lengthSquared() > 0.000001
      ? rotationFromVectors(toeDirection, Vector3.Forward())
      : Quaternion.Identity()
    const scaling = new Vector3(1.1, 0.82, 0.82)
    for (const [ankle, toe] of [[leftAnkle, leftToe], [rightAnkle, rightToe]]) {
      const center = ankle.add(toe).scale(0.5).subtract(hips).applyRotationQuaternion(alignment)
      center.x *= scaling.x; center.y *= scaling.y; center.z *= scaling.z
      this.bootCenters.push(center)
    }
    // Clip the complete original surface at one planar waistline. Unlike bone
    // weight partitions this keeps every seam, strap and boot exactly connected.
    const part = geometry.bakeLowerBody(
      'FP continuous Soldier legs', material,
      hips, alignment, Vector3.Zero(), scaling, 0.95,
    )
    part.parent = this.root
    this.root.position.copyFrom(this.playerRoot.position)
    this.root.position.y += 0.875
    this.root.rotation.y = this.playerRoot.rotation.y
    this.root.getChildMeshes(false).forEach((mesh) => {
      mesh.receiveShadows = true
      mesh.isPickable = false
      shadows.addShadowCaster(mesh, false)
    })
    this.root.computeWorldMatrix(true)
    const compiled = new Set<Material>()
    this.root.getChildMeshes(false).forEach((mesh) => {
      const meshMaterial = mesh.material
      if (meshMaterial instanceof PBRMaterial && !compiled.has(meshMaterial)) {
        compiled.add(meshMaterial)
        meshMaterial.forceCompilation(mesh)
      }
    })
  }

  reset(): void {
    this.phase = 0
    this.weight = 0
    this.root.position.copyFrom(this.playerRoot.position)
    this.root.position.y += 0.875
    this.root.rotation.set(0, this.playerRoot.rotation.y, 0)
  }

  update(dt: number, speed: number, dodgeWeight: number, pitch: number, cameraYaw: number): void {
    const targetWeight = Math.min(1, Math.max(0, speed / 4.45))
    this.weight += (targetWeight - this.weight) * (1 - Math.exp(-dt * 8))
    if (speed > 0.18) this.phase += dt * (4.4 + speed * 0.85)
    const step = Math.sin(this.phase * 2) * 0.018 * this.weight
    const middle = smoothstep(0.65, 0.82, pitch)
    const down = smoothstep(0.9, 1.4, pitch)
    // The two complete legs move as one view-model. Their closed upper edges
    // stay near the top of the frame while both boots remain readable.
    const distance = 2.5 - middle * 0.1 - down * 1.25 - dodgeWeight * 0.06
    const forwardX = -Math.cos(cameraYaw)
    const forwardZ = -Math.sin(cameraYaw)
    this.root.position.set(
      this.playerRoot.position.x + forwardX * distance,
      this.playerRoot.position.y + 0.875 + step - dodgeWeight * 0.025,
      this.playerRoot.position.z + forwardZ * distance,
    )
    this.root.rotation.y = -cameraYaw - Math.PI / 2
    this.root.rotation.z = dodgeWeight * 0.025
    const visibility = smoothstep(0.58, 0.82, pitch)
    this.root.getChildMeshes(false).forEach((mesh) => {
      mesh.visibility = visibility
      mesh.isVisible = visibility > 0.01
    })
  }
}
