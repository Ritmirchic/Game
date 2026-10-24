import {
  Color3,
  Mesh,
  PBRMaterial,
  Scene,
  TransformNode,
  Vector3,
} from '@babylonjs/core'
import {
  FirstPersonGeometry,
  rotationFromFrames,
  SoldierRegion,
} from './FirstPersonGeometry'

/** Original Soldier sleeves and hands, baked into a stable two-handed view-model. */
export class FirstPersonHands {
  private readonly right: TransformNode
  private readonly left: TransformNode
  private readonly meshes: Mesh[] = []

  constructor(
    private readonly scene: Scene,
    weapon: TransformNode,
    glove: PBRMaterial,
    body: Mesh,
    playerRoot: TransformNode,
  ) {
    const geometry = new FirstPersonGeometry(body, playerRoot)
    const original = body.material
    if (!(original instanceof PBRMaterial)) throw new Error('The original Soldier material is required')
    const detailedGlove = original.clone('FP detailed black leather spear gloves')
    // Share the imported maps explicitly. The original skin microdetail remains
    // visible, while the dark albedo turns the entire hand into a black glove.
    detailedGlove.albedoTexture = original.albedoTexture
    detailedGlove.bumpTexture = original.bumpTexture
    detailedGlove.metallicTexture = original.metallicTexture
    detailedGlove.ambientTexture = original.ambientTexture
    detailedGlove.albedoColor = Color3.FromHexString('#30383d')
    detailedGlove.roughness = 0.66
    detailedGlove.metallic = 0.04
    detailedGlove.environmentIntensity = 0.65
    detailedGlove.backFaceCulling = false
    const viewSleeve = original.clone('FP original Soldier fabric · two-sided')
    viewSleeve.albedoTexture = original.albedoTexture
    viewSleeve.bumpTexture = original.bumpTexture
    viewSleeve.metallicTexture = original.metallicTexture
    viewSleeve.ambientTexture = original.ambientTexture
    viewSleeve.backFaceCulling = false
    if (!detailedGlove.bumpTexture) {
      detailedGlove.bumpTexture = glove.bumpTexture
      if (detailedGlove.bumpTexture) detailedGlove.bumpTexture.level = 0.12
    }
    this.right = this.buildArm(geometry, weapon, 'Right', 1, viewSleeve, detailedGlove)
    this.left = this.buildArm(geometry, weapon, 'Left', -1, viewSleeve, detailedGlove)
    this.left.position.y = 0.38
    weapon.computeWorldMatrix(true)
    const warmed = new Set<PBRMaterial>()
    this.meshes.forEach((mesh) => {
      mesh.receiveShadows = false
      const material = mesh.material
      if (material instanceof PBRMaterial && !warmed.has(material)) {
        warmed.add(material)
        material.forceCompilation(mesh)
      }
    })
  }

  private buildArm(
    geometry: FirstPersonGeometry,
    weapon: TransformNode,
    sideName: 'Left' | 'Right',
    side: number,
    sleeve: PBRMaterial,
    glove: PBRMaterial,
  ): TransformNode {
    const root = new TransformNode(`FP ${sideName.toLowerCase()} original spear arm`, this.scene)
    root.parent = weapon
    const wrist = geometry.joint(`${sideName}Hand`)
    const elbow = geometry.joint(`${sideName}ForeArm`)
    const restTip = geometry.joint(`${sideName}HandMiddle3`)
    const curledTip = geometry.gripJoint(`${sideName}HandMiddle3`)
    const sourceLength = wrist.subtract(elbow).normalize()
    const sourcePalm = curledTip.subtract(restTip).normalize()
    // Align the actual curling direction with the shaft, not an arbitrary
    // roll around the wrist. Both palms therefore close toward the wood.
    const targetLength = new Vector3(-side * 0.25, 1, -0.35).normalize()
    const targetPalm = new Vector3(-side, 0, 0)
    const gripRotation = rotationFromFrames(sourceLength, sourcePalm, targetLength, targetPalm)
    const targetWrist = new Vector3(side * 0.09, -0.075, 0.025)
    const regions = sideName === 'Left'
      ? { hand: SoldierRegion.LeftHand, forearm: SoldierRegion.LeftForearm, upper: SoldierRegion.LeftUpperArm }
      : { hand: SoldierRegion.RightHand, forearm: SoldierRegion.RightForearm, upper: SoldierRegion.RightUpperArm }
    const hand = geometry.bake(regions.hand, `FP ${sideName.toLowerCase()} · black glove palm`, glove, wrist, gripRotation, targetWrist, true)
    const forearm = geometry.bake(regions.forearm, `FP ${sideName.toLowerCase()} · original forearm sleeve`, sleeve, wrist, gripRotation, targetWrist)
    hand.parent = root
    forearm.parent = root
    this.meshes.push(hand, forearm)

    // Keep the entire imported arm in one continuous bind-pose frame. A small
    // longitudinal extension moves the open shoulder beyond the screen edge.
    const targetElbow = elbow.subtract(wrist).applyRotationQuaternion(gripRotation).add(targetWrist)
    const upper = geometry.bake(regions.upper, `FP ${sideName.toLowerCase()} · original upper arm sleeve`, sleeve, elbow, gripRotation, targetElbow, false, new Vector3(1, 1.55, 1))
    upper.parent = root
    this.meshes.push(upper)
    return root
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
    weapon.position.set(lateral, 1.2 - pitchDrop + windup * 0.05, depth + reach - pullback)
    this.right.position.set(0, 0, 0)
    this.left.position.set(0, 0.38 + thrust * 0.025, 0)
  }
}
