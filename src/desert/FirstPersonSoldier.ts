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
  fingers: { bone: Bone; child: Bone; angle: number; name: string }[]
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
    for (const finger of ['Thumb', 'Index', 'Middle', 'Ring', 'Pinky']) {
      for (let joint = 1; joint <= 3; joint += 1) {
        const bone = this.findBone(`${sideName}Hand${finger}${joint}`)
        const child = this.findBone(`${sideName}Hand${finger}${joint + 1}`)
        if (!bone || !child) continue
        const angle = finger === 'Thumb'
          ? [0.52, 0.72, 0.5][joint - 1]
          : [1.02, 1.22, 0.86][joint - 1]
        fingers.push({ bone, child, angle, name: finger })
      }
    }
    return {
      upper: this.bone(`${sideName}Arm`),
      forearm: this.bone(`${sideName}ForeArm`),
      hand: this.bone(`${sideName}Hand`),
      middle: this.bone(`${sideName}HandMiddle1`),
      index: this.bone(`${sideName}HandIndex1`),
      pinky: this.bone(`${sideName}HandPinky1`),
      fingers,
      gripHeight,
    }
  }

  reset(): void {
    this.weapon.position.set(0.26, 1.44, 0.42)
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

  private solveArm(arm: ArmRig, target: Vector3, pole: Vector3): void {
    const shoulder = arm.upper.getAbsolutePosition(this.body)
    const oldElbow = arm.forearm.getAbsolutePosition(this.body)
    const oldWrist = arm.hand.getAbsolutePosition(this.body)
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
    this.rotateWorld(arm.upper, rotationFromVectors(oldElbow.subtract(shoulder), elbow.subtract(shoulder)))
    arm.upper.computeAbsoluteMatrices()
    const currentElbow = arm.forearm.getAbsolutePosition(this.body)
    const currentWrist = arm.hand.getAbsolutePosition(this.body)
    this.rotateWorld(arm.forearm, rotationFromVectors(currentWrist.subtract(currentElbow), wrist.subtract(currentElbow)))
    arm.forearm.computeAbsoluteMatrices()
  }

  private poseHand(arm: ArmRig, inward: Vector3, shaft: Vector3, weight: number): void {
    const hand = arm.hand.getAbsolutePosition(this.body)
    const proximal = arm.middle.getAbsolutePosition(this.body).subtract(hand).normalize()
    const across = arm.index.getAbsolutePosition(this.body).subtract(arm.pinky.getAbsolutePosition(this.body)).normalize()
    const targetFinger = Vector3.Cross(shaft, inward).normalize()
    const alignment = rotationFromFrames(proximal, across, targetFinger, shaft)
    this.rotateWorld(arm.hand, Quaternion.Slerp(Quaternion.Identity(), alignment, weight))
    arm.hand.computeAbsoluteMatrices()
    if (weight < 0.001) return
    // All phalanges of one finger share its proximal hinge axis. Recomputing a
    // new axis after every curl twisted the fingers sideways into an open claw.
    for (const name of ['Thumb', 'Index', 'Middle', 'Ring', 'Pinky']) {
      const joints = arm.fingers.filter((joint) => joint.name === name)
      const first = joints[0]
      if (!first) continue
      const origin = first.bone.getAbsolutePosition(this.body)
      const direction = first.child.getAbsolutePosition(this.body).subtract(origin).normalize()
      const axis = Vector3.Cross(direction, inward).normalize()
      for (const { bone, angle } of joints) {
        this.rotateAxis(bone, axis, angle * weight)
        bone.computeAbsoluteMatrices()
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
      0.26 - narrow * 0.08 - thrust * 0.12,
      1.44 - narrow * 0.05 - dodge * 0.12 - thrust * 0.05 + windup * 0.04,
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
      this.solveArm(arm, target, pole)
      this.poseHand(arm, inward, shaft, gripWeight)
    }
  }
}
