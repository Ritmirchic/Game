import {
  Bone,
  Material,
  Matrix,
  Mesh,
  Quaternion,
  TransformNode,
  Vector3,
  VertexBuffer,
  VertexData,
} from '@babylonjs/core'

export const enum SoldierRegion {
  Other,
  LeftHand,
  LeftForearm,
  LeftUpperArm,
  RightHand,
  RightForearm,
  RightUpperArm,
  LeftUpperLeg,
  LeftLowerLeg,
  LeftFoot,
  RightUpperLeg,
  RightLowerLeg,
  RightFoot,
}

export function rotationFromVectors(from: Vector3, to: Vector3): Quaternion {
  const source = from.normalize()
  const target = to.normalize()
  if (source.lengthSquared() < 0.000001 || target.lengthSquared() < 0.000001) return Quaternion.Identity()
  return Quaternion.FromUnitVectorsToRef(source, target, new Quaternion())
}

/** Align both the length and the roll of an imported anatomical segment. */
export function rotationFromFrames(sourceLength: Vector3, sourceAcross: Vector3, targetLength: Vector3, targetAcross: Vector3): Quaternion {
  const first = rotationFromVectors(sourceLength, targetLength)
  const across = sourceAcross.applyRotationQuaternion(first)
  const axis = targetLength.normalize()
  const current = across.subtract(axis.scale(Vector3.Dot(across, axis))).normalize()
  const wanted = targetAcross.subtract(axis.scale(Vector3.Dot(targetAcross, axis))).normalize()
  if (current.lengthSquared() < 0.000001 || wanted.lengthSquared() < 0.000001) return first
  const twist = rotationFromVectors(current, wanted)
  return twist.multiply(first)
}

/**
 * Bakes the original Soldier vertices into independent first-person parts.
 * The imported bind-pose geometry and UVs are retained; only the fingers are
 * skinned once into a grip, avoiding the old full-body IK deformation.
 */
export class FirstPersonGeometry {
  private readonly positions: Float32Array | number[]
  private readonly normals: Float32Array | number[] | null
  private readonly uvs: Float32Array | number[] | null
  private readonly boneIndices: Float32Array | number[]
  private readonly weights: Float32Array | number[]
  private readonly indices: IndicesArray
  private readonly bones: Bone[]
  private readonly regions: Uint8Array
  private readonly triangleRegions: Uint8Array
  private readonly inverseBind: Matrix[]
  private readonly restGlobal: Matrix[]
  private readonly bodyWorld: Matrix
  private readonly rootInverse: Matrix
  private readonly flipWinding: boolean
  private gripSkin: Matrix[] | null = null

  constructor(private readonly body: Mesh, playerRoot: TransformNode) {
    const skeleton = body.skeleton
    const positions = body.getVerticesData(VertexBuffer.PositionKind)
    const boneIndices = body.getVerticesData(VertexBuffer.MatricesIndicesKind)
    const weights = body.getVerticesData(VertexBuffer.MatricesWeightsKind)
    const indices = body.getIndices()
    if (!skeleton || !positions || !boneIndices || !weights || !indices) throw new Error('Soldier bind-pose geometry is unavailable')
    this.positions = positions
    this.normals = body.getVerticesData(VertexBuffer.NormalKind)
    this.uvs = body.getVerticesData(VertexBuffer.UVKind)
    this.boneIndices = boneIndices
    this.weights = weights
    this.indices = indices
    this.bones = skeleton.bones
    body.computeWorldMatrix(true)
    playerRoot.computeWorldMatrix(true)
    this.bodyWorld = body.getWorldMatrix().clone()
    this.rootInverse = playerRoot.getWorldMatrix().clone()
    this.rootInverse.invert()
    this.flipWinding = this.bodyWorld.determinant() * this.rootInverse.determinant() < 0
    this.inverseBind = this.bones.map((bone) => bone.getAbsoluteInverseBindMatrix().clone())
    this.restGlobal = this.inverseBind.map((matrix) => {
      const rest = matrix.clone()
      rest.invert()
      return rest
    })
    this.regions = new Uint8Array(body.getTotalVertices())
    for (let vertex = 0; vertex < this.regions.length; vertex += 1) {
      let strongest = 0
      for (let influence = 1; influence < 4; influence += 1) {
        if (weights[vertex * 4 + influence] > weights[vertex * 4 + strongest]) strongest = influence
      }
      const bone = this.bones[Math.floor(boneIndices[vertex * 4 + strongest])]
      this.regions[vertex] = this.regionForBone(bone?.name ?? '')
    }
    this.triangleRegions = new Uint8Array(indices.length / 3)
    for (let triangle = 0; triangle < this.triangleRegions.length; triangle += 1) {
      const a = this.regions[indices[triangle * 3]]
      const b = this.regions[indices[triangle * 3 + 1]]
      const c = this.regions[indices[triangle * 3 + 2]]
      this.triangleRegions[triangle] = a === b || a === c ? a : b === c ? b : a
    }
  }

  get material(): Material | null {
    return this.body.material
  }

  joint(fragment: string): Vector3 {
    const index = this.bones.findIndex((bone) => bone.name.endsWith(fragment))
    if (index < 0) throw new Error(`Soldier joint is missing: ${fragment}`)
    const raw = new Vector3(this.restGlobal[index].m[12], this.restGlobal[index].m[13], this.restGlobal[index].m[14])
    return this.toRoot(raw, false)
  }

  gripJoint(fragment: string): Vector3 {
    const index = this.bones.findIndex((bone) => bone.name.endsWith(fragment))
    if (index < 0) throw new Error(`Soldier joint is missing: ${fragment}`)
    const raw = new Vector3(this.restGlobal[index].m[12], this.restGlobal[index].m[13], this.restGlobal[index].m[14])
    const posed = Vector3.TransformCoordinates(raw, this.createGripSkin()[index])
    return this.toRoot(posed, false)
  }

  bake(
    region: SoldierRegion,
    name: string,
    material: Material,
    pivot: Vector3,
    rotation: Quaternion,
    translation: Vector3,
    curlFingers = false,
    scaling: Vector3 = Vector3.One(),
  ): Mesh {
    const remap = new Map<number, number>()
    const sourceVertices: number[] = []
    const outputIndices: number[] = []
    for (let triangle = 0; triangle < this.triangleRegions.length; triangle += 1) {
      if (this.triangleRegions[triangle] !== region) continue
      const triangleVertices = [this.indices[triangle * 3], this.indices[triangle * 3 + 1], this.indices[triangle * 3 + 2]]
      if (this.flipWinding) [triangleVertices[1], triangleVertices[2]] = [triangleVertices[2], triangleVertices[1]]
      for (const source of triangleVertices) {
        let mapped = remap.get(source)
        if (mapped === undefined) {
          mapped = sourceVertices.length
          remap.set(source, mapped)
          sourceVertices.push(source)
        }
        outputIndices.push(mapped)
      }
    }
    const positions: number[] = []
    const normals: number[] = []
    const uvs: number[] = []
    const skin = curlFingers ? this.createGripSkin() : null
    for (const vertex of sourceVertices) {
      const raw = new Vector3(this.positions[vertex * 3], this.positions[vertex * 3 + 1], this.positions[vertex * 3 + 2])
      const normal = this.normals
        ? new Vector3(this.normals[vertex * 3], this.normals[vertex * 3 + 1], this.normals[vertex * 3 + 2])
        : Vector3.Up()
      const posed = skin ? this.skinVertex(raw, normal, vertex, skin) : { position: raw, normal }
      const rootPosition = this.toRoot(posed.position, false)
      const rootNormal = this.toRoot(posed.normal, true)
      const rotated = rootPosition.subtract(pivot).applyRotationQuaternion(rotation)
      rotated.x *= scaling.x
      rotated.y *= scaling.y
      rotated.z *= scaling.z
      const positioned = rotated.add(translation)
      const orientedNormal = rootNormal.applyRotationQuaternion(rotation)
      orientedNormal.x /= scaling.x
      orientedNormal.y /= scaling.y
      orientedNormal.z /= scaling.z
      orientedNormal.normalize()
      positions.push(positioned.x, positioned.y, positioned.z)
      normals.push(orientedNormal.x, orientedNormal.y, orientedNormal.z)
      uvs.push(this.uvs?.[vertex * 2] ?? 0, this.uvs?.[vertex * 2 + 1] ?? 0)
    }
    const data = new VertexData()
    data.positions = positions
    data.indices = outputIndices
    data.normals = normals
    data.uvs = uvs
    const mesh = new Mesh(name, this.body.getScene())
    data.applyToMesh(mesh)
    mesh.material = material
    mesh.isPickable = false
    return mesh
  }

  private regionForBone(name: string): SoldierRegion {
    const left = name.includes('Left')
    if (/(?:Hand|Finger|Thumb|Index|Middle|Ring|Pinky)/.test(name)) return left ? SoldierRegion.LeftHand : SoldierRegion.RightHand
    if (name.includes('ForeArm')) return left ? SoldierRegion.LeftForearm : SoldierRegion.RightForearm
    if (name.includes('Arm') && !name.includes('Shoulder')) return left ? SoldierRegion.LeftUpperArm : SoldierRegion.RightUpperArm
    if (/(?:Foot|Toe)/.test(name)) return left ? SoldierRegion.LeftFoot : SoldierRegion.RightFoot
    if (name.includes('UpLeg')) return left ? SoldierRegion.LeftUpperLeg : SoldierRegion.RightUpperLeg
    if (name.includes('Leg')) return left ? SoldierRegion.LeftLowerLeg : SoldierRegion.RightLowerLeg
    return SoldierRegion.Other
  }

  private toRoot(value: Vector3, normal: boolean): Vector3 {
    const world = normal
      ? Vector3.TransformNormal(value, this.bodyWorld)
      : Vector3.TransformCoordinates(value, this.bodyWorld)
    const result = normal
      ? Vector3.TransformNormal(world, this.rootInverse)
      : Vector3.TransformCoordinates(world, this.rootInverse)
    return normal ? result.normalize() : result
  }

  private skinVertex(position: Vector3, normal: Vector3, vertex: number, skin: Matrix[]): { position: Vector3; normal: Vector3 } {
    const posedPosition = new Vector3()
    const posedNormal = new Vector3()
    for (let influence = 0; influence < 4; influence += 1) {
      const weight = this.weights[vertex * 4 + influence]
      if (weight <= 0) continue
      const bone = Math.floor(this.boneIndices[vertex * 4 + influence])
      const matrix = skin[bone]
      if (!matrix) continue
      posedPosition.addInPlace(Vector3.TransformCoordinates(position, matrix).scale(weight))
      posedNormal.addInPlace(Vector3.TransformNormal(normal, matrix).scale(weight))
    }
    return { position: posedPosition, normal: posedNormal.normalize() }
  }

  private createGripSkin(): Matrix[] {
    if (this.gripSkin) return this.gripSkin
    const restLocal = this.bones.map((bone, index) => {
      if (!bone.parent) return this.restGlobal[index]
      const parentIndex = this.bones.indexOf(bone.parent)
      const inverseParent = this.restGlobal[parentIndex].clone()
      inverseParent.invert()
      return this.restGlobal[index].multiply(inverseParent)
    })
    const posedGlobal: Matrix[] = []
    const pose = (index: number): Matrix => {
      const cached = posedGlobal[index]
      if (cached) return cached
      const bone = this.bones[index]
      const curl = this.fingerCurl(bone.name)
      let local = restLocal[index]
      if (curl > 0) {
        const extra = Matrix.Compose(Vector3.One(), Quaternion.RotationAxis(Vector3.Forward(), curl), Vector3.Zero())
        local = extra.multiply(local)
      }
      const parentIndex = bone.parent ? this.bones.indexOf(bone.parent) : -1
      const global = parentIndex >= 0 ? local.multiply(pose(parentIndex)) : local
      posedGlobal[index] = global
      return global
    }
    this.gripSkin = this.bones.map((_bone, index) => this.inverseBind[index].multiply(pose(index)))
    return this.gripSkin
  }

  private fingerCurl(name: string): number {
    if (!/Hand(?:Thumb|Index|Middle|Ring|Pinky)[123]$/.test(name)) return 0
    const thumb = name.includes('Thumb')
    const joint = Number(name.at(-1))
    if (joint === 1) return thumb ? 0.42 : 0.95
    if (joint === 2) return thumb ? 0.62 : 1.15
    return thumb ? 0.55 : 0.75
  }
}

type IndicesArray = number[] | Int32Array | Uint32Array | Uint16Array
