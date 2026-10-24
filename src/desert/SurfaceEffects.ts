import {
  Color3,
  DynamicTexture,
  Mesh,
  MeshBuilder,
  Scene,
  StandardMaterial,
  Vector3,
} from '@babylonjs/core'
import { desertHeight } from './DesertWorld'

interface Footprint {
  mesh: Mesh
  age: number
  active: boolean
}

interface Ripple {
  mesh: Mesh
  age: number
  active: boolean
}

const OASIS_X = 22
const OASIS_Z = -17
const OASIS_RADIUS = 5.25
const FOOTPRINT_LIFE = 36
const RIPPLE_LIFE = 1.35

function smoothstep(edge0: number, edge1: number, value: number): number {
  const t = Math.max(0, Math.min(1, (value - edge0) / (edge1 - edge0)))
  return t * t * (3 - 2 * t)
}

function footprintTexture(scene: Scene): DynamicTexture {
  const texture = new DynamicTexture('boot sole impression alpha', { width: 128, height: 256 }, scene, true)
  const ctx = texture.getContext() as CanvasRenderingContext2D
  ctx.clearRect(0, 0, 128, 256)
  ctx.fillStyle = 'rgba(75, 51, 30, .88)'
  ctx.beginPath()
  ctx.moveTo(35, 22)
  ctx.bezierCurveTo(53, 7, 91, 10, 99, 34)
  ctx.lineTo(92, 139)
  ctx.bezierCurveTo(89, 157, 85, 166, 88, 184)
  ctx.lineTo(94, 222)
  ctx.bezierCurveTo(83, 244, 43, 245, 31, 224)
  ctx.lineTo(39, 181)
  ctx.bezierCurveTo(43, 159, 35, 151, 31, 130)
  ctx.closePath()
  ctx.fill()
  ctx.globalCompositeOperation = 'destination-out'
  for (let y = 48; y < 139; y += 22) {
    ctx.save()
    ctx.translate(64, y)
    ctx.rotate((y % 44 === 0 ? 1 : -1) * 0.18)
    ctx.fillRect(-29, -4, 58, 8)
    ctx.restore()
  }
  ctx.fillRect(38, 192, 51, 9)
  ctx.fillRect(36, 218, 55, 8)
  ctx.globalCompositeOperation = 'source-over'
  texture.update()
  texture.hasAlpha = true
  return texture
}

/** Persistent, pooled foot impressions and oasis ripples. No per-step allocations of materials or geometry. */
export class DesertSurfaceEffects {
  private readonly footprints: Footprint[] = []
  private readonly ripples: Ripple[] = []
  private readonly lastPosition = new Vector3(Number.NaN, 0, Number.NaN)
  private footprintCursor = 0
  private rippleCursor = 0
  private stepDistance = 0
  private stepSide = -1

  constructor(scene: Scene, highQuality: boolean) {
    const printTexture = footprintTexture(scene)
    const printMaterial = new StandardMaterial('compressed sand footprint', scene)
    printMaterial.diffuseTexture = printTexture
    printMaterial.useAlphaFromDiffuseTexture = true
    printMaterial.diffuseColor = new Color3(0.52, 0.4, 0.27)
    printMaterial.specularColor = Color3.Black()
    printMaterial.disableLighting = true
    printMaterial.zOffset = -3
    const printBase = MeshBuilder.CreateGround('footprint geometry', { width: 0.25, height: 0.5 }, scene)
    printBase.material = printMaterial
    printBase.isPickable = false
    printBase.receiveShadows = false
    const printCount = highQuality ? 72 : 40
    for (let i = 0; i < printCount; i += 1) {
      const mesh = i === 0 ? printBase : printBase.clone(`sand footprint ${i}`)
      mesh.setEnabled(false)
      this.footprints.push({ mesh, age: 0, active: false })
    }

    const rippleMaterial = new StandardMaterial('oasis capillary crest', scene)
    rippleMaterial.diffuseColor = Color3.Black()
    rippleMaterial.specularColor = Color3.Black()
    rippleMaterial.emissiveColor = Color3.FromHexString('#c6f4e8')
    rippleMaterial.disableLighting = true
    rippleMaterial.alpha = 0.88
    rippleMaterial.backFaceCulling = false
    rippleMaterial.zOffset = -2
    const rippleBase = MeshBuilder.CreateTorus('water ripple geometry', {
      diameter: 2, thickness: 0.034, tessellation: highQuality ? 72 : 44,
    }, scene)
    rippleBase.material = rippleMaterial
    rippleBase.isPickable = false
    rippleBase.receiveShadows = false
    const rippleCount = highQuality ? 20 : 12
    for (let i = 0; i < rippleCount; i += 1) {
      const mesh = i === 0 ? rippleBase : rippleBase.clone(`oasis step ripple ${i}`)
      mesh.setEnabled(false)
      this.ripples.push({ mesh, age: 0, active: false })
    }
  }

  isInWater(position: Vector3): boolean {
    return Math.hypot(position.x - OASIS_X, position.z - OASIS_Z) < OASIS_RADIUS
  }

  reset(): void {
    this.lastPosition.set(Number.NaN, 0, Number.NaN)
    this.stepDistance = 0
    this.footprints.forEach((print) => { print.active = false; print.mesh.setEnabled(false) })
    this.ripples.forEach((ripple) => { ripple.active = false; ripple.mesh.setEnabled(false) })
  }

  private spawnFootprint(position: Vector3, direction: Vector3): void {
    const print = this.footprints[this.footprintCursor]
    this.footprintCursor = (this.footprintCursor + 1) % this.footprints.length
    const right = Vector3.Cross(Vector3.Up(), direction).normalize()
    const point = position.subtract(direction.scale(0.12)).add(right.scale(this.stepSide * 0.15))
    print.mesh.position.set(point.x, desertHeight(point.x, point.z) + 0.024, point.z)
    print.mesh.rotation.set(0, Math.atan2(direction.x, direction.z), 0)
    print.mesh.visibility = 0.52
    print.mesh.setEnabled(true)
    print.age = 0
    print.active = true
  }

  private spawnRipple(position: Vector3, direction: Vector3): void {
    const ripple = this.ripples[this.rippleCursor]
    this.rippleCursor = (this.rippleCursor + 1) % this.ripples.length
    const right = Vector3.Cross(Vector3.Up(), direction).normalize()
    const point = position.add(right.scale(this.stepSide * 0.17))
    const waterY = desertHeight(OASIS_X, OASIS_Z) + 0.105
    ripple.mesh.position.set(point.x, waterY, point.z)
    ripple.mesh.scaling.set(0.14, 1, 0.14)
    ripple.mesh.visibility = 0.72
    ripple.mesh.setEnabled(true)
    ripple.age = 0
    ripple.active = true
  }

  update(dt: number, position: Vector3, velocity: Vector3, sliding: boolean): void {
    for (const print of this.footprints) {
      if (!print.active) continue
      print.age += dt
      print.mesh.visibility = 0.52 * (1 - smoothstep(FOOTPRINT_LIFE - 10, FOOTPRINT_LIFE, print.age))
      if (print.age >= FOOTPRINT_LIFE) {
        print.active = false
        print.mesh.setEnabled(false)
      }
    }
    for (const ripple of this.ripples) {
      if (!ripple.active) continue
      ripple.age += dt
      const progress = Math.min(1, ripple.age / RIPPLE_LIFE)
      const radius = 0.14 + progress * 1.15
      ripple.mesh.scaling.set(radius, 1, radius)
      ripple.mesh.visibility = 0.88 * (1 - progress)
      if (progress >= 1) {
        ripple.active = false
        ripple.mesh.setEnabled(false)
      }
    }

    if (!Number.isFinite(this.lastPosition.x) || distance2D(position, this.lastPosition) > 3) {
      this.lastPosition.copyFrom(position)
      return
    }
    const travelled = distance2D(position, this.lastPosition)
    this.lastPosition.copyFrom(position)
    const speed = Math.hypot(velocity.x, velocity.z)
    if (sliding || speed < 0.3 || travelled <= 0.0001) return
    this.stepDistance += travelled
    const spacing = speed > 5 ? 1.12 : 0.86
    if (this.stepDistance < spacing) return
    this.stepDistance %= spacing
    this.stepSide *= -1
    const direction = new Vector3(velocity.x, 0, velocity.z).normalize()
    if (this.isInWater(position)) this.spawnRipple(position, direction)
    else this.spawnFootprint(position, direction)
  }
}

function distance2D(a: Vector3, b: Vector3): number {
  return Math.hypot(a.x - b.x, a.z - b.z)
}
