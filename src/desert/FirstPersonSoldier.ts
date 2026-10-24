import {
  Bone,
  Mesh,
  PBRMaterial,
  Quaternion,
  Space,
  TransformNode,
  Vector3,
  VertexBuffer,
} from '@babylonjs/core'
import type { ModelInstance } from '../game/models'

function clamp01(value: number): number {
  return Math.max(0, Math.min(1, value))
}

function smoothstep(edge0: number, edge1: number, value: number): number {
  const t = clamp01((value - edge0) / (edge1 - edge0))
  return t * t * (3 - 2 * t)
}

function rotationFromVectors(from: Vector3, to: Vector3): Quaternion {
  const source = from.normalize()
  const target = to.normalize()
  if (source.lengthSquared() < 0.000001 || target.lengthSquared() < 0.000001) return Quaternion.Identity()
  return Quaternion.FromUnitVectorsToRef(source, target, new Quaternion())
}

/** Align a segment and its roll without changing the imported bone lengths. */
function rotationFromFrames(sourceLength: Vector3, sourceAcross: Vector3, targetLength: Vector3, targetAcross: Vector3): Quaternion {
  const first = rotationFromVectors(sourceLength, targetLength)
  const across = sourceAcross.applyRotationQuaternion(first)
  const axis = targetLength.normalize()
  const current = across.subtract(axis.scale(Vector3.Dot(across, axis))).normalize()
  const wanted = targetAcross.subtract(axis.scale(Vector3.Dot(targetAcross, axis))).normalize()
  if (current.lengthSquared() < 0.000001 || wanted.lengthSquared() < 0.000001) return first
  return rotationFromVectors(current, wanted).multiply(first)
}

interface ArmRig {
  upper: Bone
  forearm: Bone
  hand: Bone
  middle: Bone
  index: Bone
  pinky: Bone
  thumb: { base: Bone; middle: Bone | null; tip: Bone | null }
  fingers: { bone: Bone; child: Bone; name: string }[]
  gripHeight: number
}

/**
 * The actual, complete animated Soldier used as the first-person body.
 * There is no baked body, detached view-model, resized lower body or clipped
 * waist. Only the visor and head fragments are hidden around the eye point.
 */
export class FirstPersonSoldier {
  private readonly arms: ArmRig[]
  private readonly weaponMeshes: Mesh[]

  constructor(
    private readonly player: ModelInstance,
    private readonly weapon: TransformNode,
    private readonly body: Mesh,
  ) {
    if (!body.skeleton) throw new Error('The original Soldier skeleton is required')
    this.player.content.getChildMeshes(false).forEach((mesh) => {
      const visor = mesh !== body && mesh.name.toLowerCase().includes('visor')
      mesh.isVisible = !visor
      mesh.visibility = 1
      mesh.isPickable = false
    })
    this.installBodyAttributes()
    this.arms = [
      this.createArm('Right', 0),
      this.createArm('Left', 0.32),
    ]
    this.weapon.parent = this.player.root
    this.weaponMeshes = this.weapon.getChildMeshes(false)
      .filter((mesh): mesh is Mesh => mesh instanceof Mesh)
    this.reset()
  }

  /** Vertex colour keeps the original UV/normal detail and makes real black gloves. */
  private installBodyAttributes(): void {
    const skeleton = this.body.skeleton
    const indices = this.body.getVerticesData(VertexBuffer.MatricesIndicesKind)
    const weights = this.body.getVerticesData(VertexBuffer.MatricesWeightsKind)
    const original = this.body.material
    if (!skeleton || !indices || !weights || !(original instanceof PBRMaterial)) {
      throw new Error('The original Soldier skin and PBR material are required')
    }
    const material = original.clone('Soldier · continuous first-person skin')
    material.albedoTexture = original.albedoTexture
    material.bumpTexture = original.bumpTexture
    material.metallicTexture = original.metallicTexture
    material.ambientTexture = original.ambientTexture
    material.transparencyMode = PBRMaterial.PBRMATERIAL_ALPHATEST
    material.alphaCutOff = 0.5
    this.body.material = material
    const colors = new Float32Array(this.body.getTotalVertices() * 4)
    for (let vertex = 0; vertex < this.body.getTotalVertices(); vertex += 1) {
      let head = 0
      let hand = 0
      for (let influence = 0; influence < 4; influence += 1) {
        const bone = skeleton.bones[Math.floor(indices[vertex * 4 + influence])]
        const weight = weights[vertex * 4 + influence]
        if (/Head/.test(bone?.name ?? '')) head += weight
        if (/Hand|Thumb|Index|Middle|Ring|Pinky/.test(bone?.name ?? '')) hand += weight
      }
      const glove = smoothstep(0.18, 0.82, hand)
      colors[vertex * 4] = 1 - glove * 0.82
      colors[vertex * 4 + 1] = 1 - glove * 0.805
      colors[vertex * 4 + 2] = 1 - glove * 0.79
      colors[vertex * 4 + 3] = head > 0.35 ? 0 : 1
    }
    this.body.setVerticesData(VertexBuffer.ColorKind, colors, true, 4)
    this.body.hasVertexAlpha = true
    this.body.useVertexColors = true
    material.forceCompilation(this.body)
  }

  private findBone(fragment: string): Bone | null {
    return this.body.skeleton?.bones.find((candidate) => candidate.name.endsWith(fragment)) ?? null
  }

  private bone(fragment: string): Bone {
    const bone = this.findBone(fragment)
    if (!bone) throw new Error(`Soldier joint is missing: ${fragment}`)
    return bone
  }

  private createArm(sideName: 'Left' | 'Right', gripHeight: number): ArmRig {
    const fingers: ArmRig['fingers'] = []
    for (const finger of ['Index', 'Middle', 'Ring', 'Pinky']) {
      for (let joint = 1; joint <= 3; joint += 1) {
        const bone = this.findBone(`${sideName}Hand${finger}${joint}`)
        const child = this.findBone(`${sideName}Hand${finger}${joint + 1}`)
        if (!bone || !child) continue
        fingers.push({ bone, child, name: finger })
      }
    }
    return {
      upper: this.bone(`${sideName}Arm`),
      forearm: this.bone(`${sideName}ForeArm`),
      hand: this.bone(`${sideName}Hand`),
      middle: this.bone(`${sideName}HandMiddle1`),
      index: this.bone(`${sideName}HandIndex1`),
      pinky: this.bone(`${sideName}HandPinky1`),
      thumb: {
        base: this.bone(`${sideName}HandThumb1`),
        middle: this.findBone(`${sideName}HandThumb2`),
        tip: this.findBone(`${sideName}HandThumb3`),
      },
      fingers,
      gripHeight,
    }
  }

  reset(): void {
    this.weapon.position.set(0.2, 1.36, 0.42)
    this.weapon.rotation.set(0.3, 0, 1.25)
    this.weaponMeshes?.forEach((mesh) => {
      mesh.isVisible = true
      mesh.visibility = 1
    })
  }

  private syncLinkedNode(bone: Bone): void {
    const node = bone.getTransformNode()
    if (!node) return
    node.position.copyFrom(bone.position)
    node.scaling.copyFrom(bone.scaling)
    node.rotationQuaternion = node.rotationQuaternion ?? Quaternion.Identity()
    node.rotationQuaternion.copyFrom(bone.rotationQuaternion)
  }

  private syncFromAnimation(): void {
    const skeleton = this.body.skeleton
    if (!skeleton) return
    // Animation groups write to the linked glTF nodes. At onAfterAnimations the
    // Bone objects still contain the previous frame until Skeleton.prepare().
    // Pull that current pose in explicitly before measuring any IK segment.
    for (const bone of skeleton.bones) {
      const node = bone.getTransformNode()
      if (!node) continue
      bone.position = node.position
      if (node.rotationQuaternion) bone.rotationQuaternion = node.rotationQuaternion
      else bone.rotation = node.rotation
      bone.scaling = node.scaling
    }
    skeleton.computeAbsoluteMatrices(true)
  }

  private rotateAxis(bone: Bone, axis: Vector3, angle: number): void {
    if (Math.abs(angle) < 0.00001 || axis.lengthSquared() < 0.000001) return
    bone.rotate(axis, angle, Space.WORLD, this.body)
    // glTF animations target linked TransformNodes. Skeleton.prepare() copies
    // those nodes back into the bones, so commit the post-animation IK pose to
    // the same nodes instead of letting the next render restore idle arms.
    this.syncLinkedNode(bone)
  }

  private rotateWorld(bone: Bone, rotation: Quaternion): void {
    const axis = new Vector3()
    const angle = rotation.toAxisAngleToRef(axis)
    this.rotateAxis(bone, axis, angle)
  }

  private solveChain(upper: Bone, forearm: Bone, end: Bone, target: Vector3, pole: Vector3): void {
    const shoulder = upper.getAbsolutePosition(this.body)
    const oldElbow = forearm.getAbsolutePosition(this.body)
    const oldWrist = end.getAbsolutePosition(this.body)
    const upperLength = Vector3.Distance(shoulder, oldElbow)
    const lowerLength = Vector3.Distance(oldElbow, oldWrist)
    const offset = target.subtract(shoulder)
    const distance = Math.max(0.0001, offset.length())
    const direction = offset.scale(1 / distance)
    const solvedDistance = Math.min(upperLength + lowerLength - 0.002, Math.max(Math.abs(upperLength - lowerLength) + 0.002, distance))
    const along = (upperLength * upperLength + solvedDistance * solvedDistance - lowerLength * lowerLength) / (2 * solvedDistance)
    const height = Math.sqrt(Math.max(0, upperLength * upperLength - along * along))
    let bend = pole.subtract(shoulder)
    bend = bend.subtract(direction.scale(Vector3.Dot(bend, direction))).normalize()
    if (bend.lengthSquared() < 0.000001) bend = Vector3.Cross(direction, Vector3.Up()).normalize()
    const elbow = shoulder.add(direction.scale(along)).add(bend.scale(height))
    const wrist = shoulder.add(direction.scale(solvedDistance))
    this.rotateWorld(upper, rotationFromVectors(oldElbow.subtract(shoulder), elbow.subtract(shoulder)))
    upper.computeAbsoluteMatrices()
    const currentElbow = forearm.getAbsolutePosition(this.body)
    const currentWrist = end.getAbsolutePosition(this.body)
    this.rotateWorld(forearm, rotationFromVectors(currentWrist.subtract(currentElbow), wrist.subtract(currentElbow)))
    forearm.computeAbsoluteMatrices()
  }

  private poseHand(arm: ArmRig, inward: Vector3, shaft: Vector3, grip: Vector3, weight: number): void {
    const hand = arm.hand.getAbsolutePosition(this.body)
    const proximal = arm.middle.getAbsolutePosition(this.body).subtract(hand).normalize()
    const across = arm.index.getAbsolutePosition(this.body).subtract(arm.pinky.getAbsolutePosition(this.body)).normalize()
    const targetFinger = Vector3.Cross(shaft, inward).normalize()
    const alignment = rotationFromFrames(proximal, across, targetFinger, shaft)
    this.rotateWorld(arm.hand, Quaternion.Slerp(Quaternion.Identity(), alignment, weight))
    arm.hand.computeAbsoluteMatrices()
    if (weight < 0.001) return
    // Place each fingertip on the shaft instead of hoping that a fixed curl
    // angle reaches it. The middle joint bows away from the wood, so the finger
    // wraps around the cylinder rather than pointing across its top.
    for (const name of ['Index', 'Middle', 'Ring', 'Pinky']) {
      const joints = arm.fingers.filter((joint) => joint.name === name)
      const first = joints[0]
      if (!first) continue
      const base = first.bone.getAbsolutePosition(this.body)
      const along = Vector3.Dot(base.subtract(grip), shaft)
      const target = grip.add(shaft.scale(along)).add(inward.scale(0.012))
      const pole = base.subtract(inward.scale(0.065))
      if (joints[1]) {
        this.solveChain(first.bone, joints[1].bone, joints[1].child, target, pole)
      } else {
        const direction = first.child.getAbsolutePosition(this.body).subtract(base)
        this.rotateWorld(first.bone, rotationFromVectors(direction, target.subtract(base)))
        first.bone.computeAbsoluteMatrices()
      }
    }
    // The thumb opposes the four fingers instead of sharing their curl axis.
    // Plant its tip on the near side of the shaft with its own two-bone chain.
    const thumbBase = arm.thumb.base.getAbsolutePosition(this.body)
    const thumbTarget = grip.add(inward.scale(0.028)).add(shaft.scale(0.012))
    const thumbPole = thumbBase.add(shaft.scale(0.055)).subtract(inward.scale(0.025))
    if (arm.thumb.middle && arm.thumb.tip) {
      this.solveChain(arm.thumb.base, arm.thumb.middle, arm.thumb.tip, thumbTarget, thumbPole)
    } else {
      const end = arm.thumb.tip ?? arm.thumb.middle
      if (end) {
        const direction = end.getAbsolutePosition(this.body).subtract(thumbBase)
        const curlAxis = Vector3.Cross(direction, inward).normalize()
        this.rotateWorld(arm.thumb.base, rotationFromVectors(direction, thumbTarget.subtract(thumbBase)))
        arm.thumb.base.computeAbsoluteMatrices()
        if (arm.thumb.middle) {
          this.rotateAxis(arm.thumb.middle, curlAxis, 1.15)
          arm.thumb.middle.computeAbsoluteMatrices()
        }
      }
    }
  }

  update(pitch: number, thrust: number, windup: number, dodge: number, attacking: boolean): void {
    const aspect = this.body.getScene().getEngine().getRenderWidth() / Math.max(1, this.body.getScene().getEngine().getRenderHeight())
    const narrow = clamp01((1.05 - aspect) / 0.55)
    const weaponVisibility = attacking || pitch < 0.34 ? 1 : 0
    // Never blend the arm IK halfway toward the camera. That transitional
    // pose put a forearm across the view; the original idle rig takes over
    // directly when the spear leaves the frame.
    const gripWeight = attacking || pitch < 0.34 ? 1 : 0
    const readyRoll = 1.25 - narrow * 0.25
    this.weapon.position.set(
      0.26 - (1 - narrow) * 0.06 - narrow * 0.08 - thrust * 0.12,
      1.44 - (1 - narrow) * 0.08 - narrow * 0.05 - dodge * 0.12 - thrust * 0.05 + windup * 0.04,
      0.42 + narrow * 0.04 + thrust * 0.12 - windup * 0.12,
    )
    const thrustTilt = Math.PI / 2 + pitch
    this.weapon.rotation.set(
      0.3 * (1 - thrust) + thrustTilt * thrust - windup * 0.3,
      0,
      readyRoll * (1 - thrust) + 0.08 * thrust - dodge * 0.25,
    )
    this.weapon.computeWorldMatrix(true)
    this.weaponMeshes.forEach((mesh) => {
      mesh.visibility = weaponVisibility
      mesh.isVisible = weaponVisibility > 0.01
    })
    if (gripWeight < 0.001) return

    this.syncFromAnimation()
    this.player.root.computeWorldMatrix(true)
    const root = this.player.root.getWorldMatrix()
    const rootRight = Vector3.TransformNormal(Vector3.Right(), root).normalize()
    const rootForward = Vector3.TransformNormal(Vector3.Forward(), root).normalize()
    const rootUp = Vector3.TransformNormal(Vector3.Up(), root).normalize()
    const shaft = Vector3.TransformNormal(Vector3.Up(), this.weapon.getWorldMatrix()).normalize()
    for (const arm of this.arms) {
      const gripHeight = arm.gripHeight * (1 - thrust * 0.65)
      const grip = Vector3.TransformCoordinates(new Vector3(0, gripHeight, 0), this.weapon.getWorldMatrix())
      const shoulder = arm.upper.getAbsolutePosition(this.body)
      // Use the actual animated shoulder side rather than assuming that the
      // imported asset's X axis matches the player's local X axis.
      const centre = this.bone('Hips').getAbsolutePosition(this.body)
      const side = Vector3.Dot(shoulder.subtract(centre), rootRight) >= 0 ? 1 : -1
      const outward = rootRight.scale(side * 0.03).add(rootForward.scale(-0.012))
      const inward = outward.scale(-1).normalize()
      const fingerDirection = Vector3.Cross(shaft, inward).normalize()
      // The shaft sits across the knuckles, not inside the wrist joint. The
      // original palm length is preserved and the fingers close around it.
      const desiredWrist = grip.add(outward).subtract(fingerDirection.scale(0.095))
      const animatedWrist = arm.hand.getAbsolutePosition(this.body)
      const target = animatedWrist.add(desiredWrist.subtract(animatedWrist).scale(gripWeight))
      const pole = shoulder
        .add(rootRight.scale(side * (0.62 - thrust * 0.45)))
        .add(rootForward.scale(-0.3 + thrust * 0.55))
        .add(rootUp.scale(-0.12 - thrust * 0.22))
      this.solveChain(arm.upper, arm.forearm, arm.hand, target, pole)
      this.poseHand(arm, inward, shaft, grip, gripWeight)
    }
  }
}
