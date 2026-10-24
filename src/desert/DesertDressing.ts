import {
  Color3,
  Matrix,
  Mesh,
  MeshBuilder,
  PBRMaterial,
  Quaternion,
  Scene,
  TransformNode,
  Vector3,
  VertexBuffer,
  VertexData,
} from '@babylonjs/core'
import type { DesertModels } from './DesertModels'

export type DesertHeightAt = (x: number, z: number) => number

function hash(x: number, z: number, seed = 0): number {
  const value = Math.sin(x * 127.1 + z * 311.7 + seed * 73.31) * 43758.5453123
  return value - Math.floor(value)
}

function parent<T extends TransformNode>(node: T, root: TransformNode): T {
  node.parent = root
  return node
}

function tuftGeometry(scene: Scene, name: string, material: PBRMaterial): Mesh {
  const positions: number[] = []
  const indices: number[] = []
  // Each tuft is a handful of tapered, bent blades. It is intentionally one
  // mesh so hundreds of instances cost no extra materials or draw calls.
  for (let blade = 0; blade < 7; blade += 1) {
    const angle = blade / 7 * Math.PI * 2
    const lean = 0.12 + (blade % 3) * 0.045
    const height = 0.42 + (blade % 4) * 0.095
    const width = 0.045 + (blade % 2) * 0.018
    const dx = Math.cos(angle)
    const dz = Math.sin(angle)
    const px = -dz * width
    const pz = dx * width
    const base = positions.length / 3
    positions.push(
      -px, 0, -pz,
      px, 0, pz,
      dx * lean * 0.45 - px * 0.62, height * 0.56, dz * lean * 0.45 - pz * 0.62,
      dx * lean * 0.45 + px * 0.62, height * 0.56, dz * lean * 0.45 + pz * 0.62,
      dx * lean, height, dz * lean,
    )
    indices.push(
      base, base + 1, base + 2,
      base + 1, base + 3, base + 2,
      base + 2, base + 3, base + 4,
    )
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

function grassMaterial(scene: Scene, name: string, color: string): PBRMaterial {
  const material = new PBRMaterial(name, scene)
  material.albedoColor = Color3.FromHexString(color)
  material.roughness = 0.94
  material.metallic = 0
  material.environmentIntensity = 0.38
  material.backFaceCulling = false
  material.twoSidedLighting = true
  return material
}

/** Batched, deterministic set dressing for the streamed desert and its start basin. */
export class DesertDressing {
  private readonly grass: Mesh
  private readonly dryGrass: Mesh
  private readonly pebble: Mesh
  private readonly pendingBatches = new Map<TransformNode, Map<Mesh, Matrix[]>>()

  constructor(private readonly scene: Scene, private readonly models: DesertModels, private readonly highQuality: boolean) {
    this.grass = tuftGeometry(scene, 'instanced green desert grass', grassMaterial(scene, 'desert grass blades', '#526543'))
    this.dryGrass = tuftGeometry(scene, 'instanced dry desert grass', grassMaterial(scene, 'sun dried grass blades', '#9a7d4d'))
    this.pebble = MeshBuilder.CreateIcoSphere('instanced desert pebble', { radius: 1, subdivisions: 1 }, scene)
    this.pebble.material = models.materials.rock
    ;[this.grass, this.dryGrass, this.pebble].forEach((mesh) => {
      // The source meshes are geometry/material holders only. Their transformed
      // triangles are baked into a few ordinary meshes per streamed chunk.
      mesh.isVisible = false
      mesh.isPickable = false
      mesh.receiveShadows = false
    })
  }

  private beginBatch(root: TransformNode): void {
    this.pendingBatches.set(root, new Map())
  }

  private instance(base: Mesh, root: TransformNode, x: number, y: number, z: number, scale: number, rotation: number): void {
    const batch = this.pendingBatches.get(root)
    if (!batch) return
    let matrices = batch.get(base)
    if (!matrices) {
      matrices = []
      batch.set(base, matrices)
    }
    matrices.push(Matrix.Compose(
      new Vector3(scale, scale, scale),
      Quaternion.RotationAxis(Vector3.Up(), rotation),
      new Vector3(x, y, z),
    ))
  }

  /** Bake each static batch into one ordinary mesh: no per-instance scene nodes
   * and no driver-dependent thin-instance shader variants in the standalone build. */
  private flushBatch(root: TransformNode): void {
    const batches = this.pendingBatches.get(root)
    if (!batches) return
    batches.forEach((matrices, base) => {
      if (!matrices.length) return
      const sourcePositions = base.getVerticesData(VertexBuffer.PositionKind)
      const sourceNormals = base.getVerticesData(VertexBuffer.NormalKind)
      const sourceUvs = base.getVerticesData(VertexBuffer.UVKind)
      const sourceIndices = base.getIndices()
      if (!sourcePositions || !sourceIndices) return
      const positions: number[] = []
      const normals: number[] = []
      const uvs: number[] = []
      const indices: number[] = []
      const point = new Vector3()
      const normal = new Vector3()
      const vertexCount = sourcePositions.length / 3
      matrices.forEach((matrix) => {
        const vertexOffset = positions.length / 3
        for (let vertex = 0; vertex < vertexCount; vertex += 1) {
          const offset = vertex * 3
          Vector3.TransformCoordinatesFromFloatsToRef(sourcePositions[offset], sourcePositions[offset + 1], sourcePositions[offset + 2], matrix, point)
          positions.push(point.x, point.y, point.z)
          if (sourceNormals) {
            Vector3.TransformNormalFromFloatsToRef(sourceNormals[offset], sourceNormals[offset + 1], sourceNormals[offset + 2], matrix, normal)
            normal.normalize()
            normals.push(normal.x, normal.y, normal.z)
          }
          if (sourceUvs) uvs.push(sourceUvs[vertex * 2], sourceUvs[vertex * 2 + 1])
        }
        for (const index of sourceIndices) indices.push(index + vertexOffset)
      })
      if (!sourceNormals) VertexData.ComputeNormals(positions, indices, normals)
      const mesh = new Mesh(`${base.name} · baked batch`, this.scene)
      mesh.parent = root
      const data = new VertexData()
      data.positions = positions
      data.indices = indices
      data.normals = normals
      if (sourceUvs) data.uvs = uvs
      data.applyToMesh(mesh)
      mesh.material = base.material
      mesh.isPickable = false
      mesh.receiveShadows = false
    })
    this.pendingBatches.delete(root)
  }

  private patch(root: TransformNode, x: number, z: number, radius: number, count: number, heightAt: DesertHeightAt, seed: number): void {
    for (let i = 0; i < count; i += 1) {
      const angle = hash(x, z, seed + i * 2) * Math.PI * 2
      const distance = Math.sqrt(hash(x, z, seed + i * 2 + 1)) * radius
      const px = x + Math.cos(angle) * distance
      const pz = z + Math.sin(angle) * distance
      if (Math.hypot(px - 22, pz + 17) < 5.65 || Math.hypot(px, pz) < 2.8) continue
      const scale = 0.62 + hash(px, pz, seed + 9) * 0.72
      const base = hash(px, pz, seed + 12) > 0.44 ? this.grass : this.dryGrass
      this.instance(base, root, px, heightAt(px, pz) + 0.015, pz, scale, angle)
      if (i % 4 === 0) {
        this.instance(this.pebble, root, px + 0.22, heightAt(px, pz) + 0.06, pz - 0.18, 0.12 + scale * 0.1, angle)
      }
    }
  }

  /** Small repeated detail is cheap enough to keep in every streamed chunk. */
  populateChunk(root: TransformNode, cx: number, cz: number, heightAt: DesertHeightAt): void {
    this.beginBatch(root)
    const count = this.highQuality ? 13 : 6
    for (let i = 0; i < count; i += 1) {
      const x = (cx + hash(cx, cz, i * 7 + 101) - 0.5) * 64
      const z = (cz + hash(cx, cz, i * 7 + 102) - 0.5) * 64
      if (Math.hypot(x, z) < 27 || Math.hypot(x - 22, z + 17) < 7) continue
      this.patch(root, x, z, 1.8 + hash(x, z, 103) * 1.8, this.highQuality ? 5 : 3, heightAt, i * 19 + cx * 3 + cz)
    }
    this.flushBatch(root)
  }

  private mesa(root: TransformNode, x: number, z: number, scale: number, heightAt: DesertHeightAt): void {
    const mesa = parent(new TransformNode('distant eroded sandstone mesa', this.scene), root)
    mesa.position.set(x, heightAt(x, z), z)
    mesa.rotation.y = hash(x, z, 200) * Math.PI * 2
    mesa.scaling.setAll(scale)
    const layers = [
      { y: 1.3, height: 2.6, bottom: 10.5, top: 8.3 },
      { y: 3.2, height: 1.5, bottom: 8.4, top: 6.4 },
      { y: 4.35, height: 0.85, bottom: 6.5, top: 5.3 },
    ]
    layers.forEach((layer, index) => {
      const rock = parent(MeshBuilder.CreateCylinder(`mesa erosion terrace ${index}`, {
        diameterTop: layer.top, diameterBottom: layer.bottom, height: layer.height, tessellation: 7,
      }, this.scene), mesa)
      const positions = rock.getVerticesData(VertexBuffer.PositionKind)
      const indices = rock.getIndices()
      if (positions && indices) {
        for (let i = 0; i < positions.length; i += 3) {
          const angle = Math.atan2(positions[i + 2], positions[i])
          const distortion = 1 + Math.sin(angle * 3 + index * 1.7) * 0.1
            + Math.cos(angle * 5 - index) * 0.045
          positions[i] *= distortion
          positions[i + 2] *= distortion
        }
        const normals = new Float32Array(positions.length)
        VertexData.ComputeNormals(positions, indices, normals)
        rock.updateVerticesData(VertexBuffer.PositionKind, positions)
        rock.updateVerticesData(VertexBuffer.NormalKind, normals)
      }
      rock.position.y = layer.y
      rock.position.x = Math.sin(index * 2.1 + x) * 0.32
      rock.position.z = Math.cos(index * 1.7 + z) * 0.28
      rock.rotation.y = index * 0.17 + hash(x, z, index + 220) * 0.24
      rock.material = this.models.sandstone
      rock.isPickable = false
      rock.receiveShadows = true
    })
    for (let i = 0; i < 4; i += 1) {
      const shard = parent(MeshBuilder.CreateBox('mesa fallen slab', {
        width: 1.2 + i * 0.22, height: 0.35 + (i % 2) * 0.18, depth: 0.8 + (i % 3) * 0.2,
      }, this.scene), mesa)
      const angle = i / 4 * Math.PI * 2 + 0.3
      shard.position.set(Math.cos(angle) * 5.2, 0.25, Math.sin(angle) * 5.2)
      shard.rotation.set(0.08 * i, angle, -0.06 * i)
      shard.material = this.models.sandstone
      shard.isPickable = false
    }
  }

  /** A composed start basin gives the first frame landmarks in every direction. */
  createStarter(heightAt: DesertHeightAt): TransformNode {
    const root = new TransformNode('curated mirage start basin', this.scene)
    this.beginBatch(root)
    const patches = [
      [2.5, 12.5, 2.8, 12], [11.5, 11.5, 2.4, 11], [-7, 3, 2.5, 10],
      [13.5, -1.5, 2.3, 10], [17.5, -10.5, 2.1, 9], [28, -12, 2.5, 10],
      [27, -23, 2.4, 9], [-11, 15, 2.6, 10],
      [-5, 9, 2.2, 14], [-14, 8, 2.6, 14], [-18, -2, 2.4, 12], [1, -8, 2.2, 12],
    ]
    patches.forEach(([x, z, radius, count], index) => this.patch(root, x, z, radius, count, heightAt, 400 + index * 31))

    const agaves = [
      [12.5, 13.5, 1.15], [-8.5, 4.5, 0.9], [15.5, -8.5, 1.2],
      [29, -10, 1.05], [30, -23, 0.9], [-14, 16, 1.1],
      [-4, 11, 0.85], [-13, 7, 1.05], [-18, -3, 1.15], [1, -9, 0.75],
    ]
    agaves.forEach(([x, z, scale], index) => {
      const plant = parent(this.models.agave(scale), root)
      plant.position.set(x, heightAt(x, z), z)
      plant.rotation.y = index * 1.7
    })

    const boulders = [
      [-9, 13, 0.85], [13, 15, 0.72], [16, -4, 0.62],
      [29, -25, 0.95], [-20, -10, 1.1], [33, -4, 0.7],
      [-5, 14, 0.55], [-15, 2, 0.7], [-23, -5, 0.8],
    ]
    boulders.forEach(([x, z, radius], index) => {
      const rock = parent(this.models.rock(radius), root)
      rock.position.set(x, heightAt(x, z) + radius * 0.35, z)
      rock.rotation.y = index * 1.13
    })

    const ruin = parent(this.models.ruins(), root)
    ruin.position.set(-23, heightAt(-23, -18), -18)
    ruin.rotation.y = -0.42
    ruin.scaling.setAll(1.15)

    const arch = parent(this.models.landmarkArch(), root)
    arch.position.set(-22, heightAt(-22, 4), 4)
    arch.rotation.y = 0.42
    arch.scaling.setAll(1.15)

    this.mesa(root, -44, -38, 1.25, heightAt)
    this.mesa(root, 53, -42, 1.05, heightAt)
    this.mesa(root, -57, 29, 0.9, heightAt)
    this.mesa(root, 45, 53, 1.35, heightAt)
    this.mesa(root, -4, -72, 1.1, heightAt)
    this.flushBatch(root)
    return root
  }
}
