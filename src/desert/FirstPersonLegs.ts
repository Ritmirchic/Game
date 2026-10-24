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
import { FirstPersonGeometry, rotationFromVectors, SoldierRegion } from './FirstPersonGeometry'

/** Original Soldier trousers and detailed boots with a stable first-person stance. */
export class FirstPersonLegs {
  private readonly root: TransformNode
  private readonly legs: TransformNode[] = []
  private readonly thighs: Mesh[] = []
  private readonly shins: Mesh[] = []
  private readonly basePositions: Vector3[] = []
  private phase = 0
  private weight = 0

  constructor(scene: Scene, parent: TransformNode, shadows: ShadowGenerator, body: Mesh) {
    this.root = new TransformNode('first-person original lower body', scene)
    this.root.parent = parent
    const geometry = new FirstPersonGeometry(body, parent)
    const original = body.material
    if (!(original instanceof PBRMaterial)) throw new Error('The original Soldier material is required')
    const material = original.clone('FP original Soldier trousers and boots · two-sided')
    material.albedoTexture = original.albedoTexture
    material.bumpTexture = original.bumpTexture
    material.metallicTexture = original.metallicTexture
    material.ambientTexture = original.ambientTexture
    material.backFaceCulling = false
    for (const sideName of ['Left', 'Right'] as const) {
      const side = sideName === 'Left' ? -1 : 1
      const leg = new TransformNode(`FP ${sideName.toLowerCase()} leg`, scene)
      leg.parent = this.root
      const hip = geometry.joint(`${sideName}UpLeg`)
      const ankle = geometry.joint(`${sideName}Foot`)
      const toe = geometry.joint(`${sideName}ToeBase`)
      const toeDirection = toe.subtract(ankle)
      toeDirection.y = 0
      const alignment = toeDirection.lengthSquared() > 0.000001
        ? rotationFromVectors(toeDirection, Vector3.Forward())
        : Quaternion.Identity()
      const regions = sideName === 'Left'
        ? { thigh: SoldierRegion.LeftUpperLeg, shin: SoldierRegion.LeftLowerLeg, foot: SoldierRegion.LeftFoot }
        : { thigh: SoldierRegion.RightUpperLeg, shin: SoldierRegion.RightLowerLeg, foot: SoldierRegion.RightFoot }
      const thigh = geometry.bake(regions.thigh, `FP ${sideName.toLowerCase()} · original thigh`, material, hip, alignment, Vector3.Zero())
      const shin = geometry.bake(regions.shin, `FP ${sideName.toLowerCase()} · original trouser shin`, material, hip, alignment, Vector3.Zero())
      // Keep the established diagnostic name, but this is now the genuine
      // sculpted Soldier boot with its original seams, sole, UVs and normal map.
      const boot = geometry.bake(regions.foot, 'FP rounded boot', material, hip, alignment, Vector3.Zero())
      thigh.parent = leg
      shin.parent = leg
      boot.parent = leg
      this.thighs.push(thigh)
      this.shins.push(shin)
      const ankleOffset = ankle.subtract(hip).applyRotationQuaternion(alignment)
      const base = new Vector3(side * 0.2, 0.12, 0.52).subtract(ankleOffset)
      leg.position.copyFrom(base)
      this.basePositions.push(base)
      this.legs.push(leg)
    }
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
    this.thighs.forEach((thigh) => { thigh.visibility = 1; thigh.isVisible = true })
    this.legs.forEach((leg, index) => {
      leg.position.copyFrom(this.basePositions[index])
      leg.rotation.set(0, 0, 0)
    })
  }

  update(dt: number, speed: number, dodgeWeight: number, pitch: number): void {
    // Hide only the close thighs at a steep downward angle. Original shins
    // remain inside the sculpted boot collars, so there are no hollow openings.
    const thighFade = Math.min(1, Math.max(0, (pitch - 0.82) / 0.34))
    const thighVisibility = 1 - thighFade * thighFade * (3 - 2 * thighFade)
    this.thighs.forEach((thigh) => {
      thigh.visibility = thighVisibility
      thigh.isVisible = thighVisibility > 0.01
    })
    const targetWeight = Math.min(1, Math.max(0, speed / 4.45))
    this.weight += (targetWeight - this.weight) * (1 - Math.exp(-dt * 8))
    if (speed > 0.18) this.phase += dt * (4.4 + speed * 0.85)
    for (let index = 0; index < this.legs.length; index += 1) {
      const wave = Math.sin(this.phase + index * Math.PI)
      const lift = Math.max(0, wave) * 0.075 * this.weight
      const stride = wave * 0.14 * this.weight
      const leg = this.legs[index]
      leg.position.copyFrom(this.basePositions[index])
      leg.position.y += lift
      leg.position.z += stride - dodgeWeight * 0.08
      leg.rotation.x = wave * 0.055 * this.weight
      leg.rotation.z = (index === 0 ? -1 : 1) * dodgeWeight * 0.08
    }
  }
}
