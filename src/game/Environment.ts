import * as THREE from 'three';
import type { Phase } from './types';

const DAY_SKY = new THREE.Color(0x9bc8c0);
const NIGHT_SKY = new THREE.Color(0x081722);
const DAY_FOG = new THREE.Color(0x9bc8c0);
const NIGHT_FOG = new THREE.Color(0x0b1c28);

function seededRandom(seed: number): () => number {
  return () => {
    seed |= 0;
    seed = (seed + 0x6d2b79f5) | 0;
    let value = Math.imul(seed ^ (seed >>> 15), 1 | seed);
    value = (value + Math.imul(value ^ (value >>> 7), 61 | value)) ^ value;
    return ((value ^ (value >>> 14)) >>> 0) / 4294967296;
  };
}

export class Environment {
  readonly sun: THREE.DirectionalLight;
  private readonly hemisphere: THREE.HemisphereLight;
  private readonly water: THREE.Mesh<THREE.PlaneGeometry, THREE.MeshStandardMaterial>;
  private readonly ash: THREE.Points<THREE.BufferGeometry, THREE.PointsMaterial>;
  private readonly stars: THREE.Points<THREE.BufferGeometry, THREE.PointsMaterial>;
  private readonly clouds: THREE.Group;
  private nightBlend = 0;
  private normalFrame = 0;

  constructor(private readonly scene: THREE.Scene) {
    scene.background = DAY_SKY.clone();
    scene.fog = new THREE.Fog(DAY_FOG.clone(), 34, 82);

    this.hemisphere = new THREE.HemisphereLight(0xd8f1e7, 0x506354, 2.25);
    scene.add(this.hemisphere);
    this.sun = new THREE.DirectionalLight(0xffe4bd, 3.4);
    this.sun.position.set(-18, 30, 16);
    this.sun.castShadow = true;
    this.sun.shadow.mapSize.set(2048, 2048);
    this.sun.shadow.camera.left = -29;
    this.sun.shadow.camera.right = 29;
    this.sun.shadow.camera.top = 29;
    this.sun.shadow.camera.bottom = -29;
    this.sun.shadow.camera.near = 1;
    this.sun.shadow.camera.far = 80;
    this.sun.shadow.bias = -0.0005;
    this.sun.shadow.normalBias = 0.025;
    scene.add(this.sun);

    this.createIsland();
    this.water = this.createWater();
    this.ash = this.createAsh();
    this.stars = this.createStars();
    this.clouds = this.createClouds();
  }

  update(dt: number, time: number, phase: Phase, animateWorld = true): void {
    const target = phase === 'night' ? 1 : 0;
    this.nightBlend = THREE.MathUtils.damp(this.nightBlend, target, 1.3, dt);
    const blend = this.nightBlend;
    (this.scene.background as THREE.Color).copy(DAY_SKY).lerp(NIGHT_SKY, blend);
    const fog = this.scene.fog as THREE.Fog;
    fog.color.copy(DAY_FOG).lerp(NIGHT_FOG, blend);
    fog.near = THREE.MathUtils.lerp(36, 19, blend);
    fog.far = THREE.MathUtils.lerp(86, 52, blend);
    this.hemisphere.intensity = THREE.MathUtils.lerp(2.25, 0.55, blend);
    this.hemisphere.color.setHex(blend > 0.5 ? 0x5e7ea1 : 0xd8f1e7);
    this.sun.intensity = THREE.MathUtils.lerp(3.4, 0.65, blend);
    this.sun.color.setHex(blend > 0.5 ? 0x8aa5cc : 0xffe4bd);
    this.sun.position.x = -18 + Math.sin(time * 0.025) * 8;
    this.sun.position.y = THREE.MathUtils.lerp(30, 14, blend);
    this.water.material.color.copy(new THREE.Color(0x3a8e91)).lerp(new THREE.Color(0x102e40), blend);
    this.water.material.emissive.copy(new THREE.Color(0x082529)).lerp(new THREE.Color(0x06131f), blend);
    this.ash.material.opacity = blend * 0.62;
    this.ash.visible = blend > 0.04;
    this.stars.material.opacity = blend * 0.85;
    this.stars.visible = blend > 0.06;
    this.clouds.visible = blend < 0.92;

    if (!animateWorld) return;
    const positions = this.water.geometry.attributes.position as THREE.BufferAttribute;
    for (let i = 0; i < positions.count; i += 1) {
      const x = positions.getX(i);
      const y = positions.getY(i);
      positions.setZ(i, Math.sin(x * 0.28 + time * 0.7) * 0.09 + Math.cos(y * 0.23 + time * 0.55) * 0.07);
    }
    positions.needsUpdate = true;
    this.normalFrame += 1;
    if (this.normalFrame % 8 === 0) this.water.geometry.computeVertexNormals();

    const ashPositions = this.ash.geometry.attributes.position as THREE.BufferAttribute;
    if (this.ash.visible) {
      for (let i = 0; i < ashPositions.count; i += 1) {
        let y = ashPositions.getY(i) - dt * (1.4 + (i % 7) * 0.15);
        let x = ashPositions.getX(i) + dt * 0.55;
        if (y < 0) y = 18 + (i % 5);
        if (x > 30) x = -30;
        ashPositions.setXY(i, x, y);
      }
      ashPositions.needsUpdate = true;
    }
    this.clouds.children.forEach((cloud, index) => {
      cloud.position.x += dt * (0.16 + index * 0.015);
      if (cloud.position.x > 55) cloud.position.x = -55;
    });
  }

  private createIsland(): void {
    const grass = new THREE.MeshStandardMaterial({ color: 0x6e9270, roughness: 0.95, flatShading: true });
    const cliff = new THREE.MeshStandardMaterial({ color: 0x697a70, roughness: 1, flatShading: true });
    const sand = new THREE.MeshStandardMaterial({ color: 0xc8b88e, roughness: 1, flatShading: true });
    const island = new THREE.Mesh(new THREE.CylinderGeometry(23.5, 20.5, 2.2, 48, 2), [cliff, grass, cliff]);
    island.position.y = -1.1;
    island.receiveShadow = true;
    island.castShadow = true;
    this.scene.add(island);
    const beach = new THREE.Mesh(new THREE.CylinderGeometry(24.3, 23.5, 0.35, 48), sand);
    beach.position.y = -0.55;
    beach.receiveShadow = true;
    this.scene.add(beach);

    const center = new THREE.Mesh(new THREE.CircleGeometry(5.1, 32), new THREE.MeshStandardMaterial({ color: 0x8c9a7c, roughness: 1 }));
    center.rotation.x = -Math.PI / 2;
    center.position.y = 0.018;
    center.receiveShadow = true;
    this.scene.add(center);

    const random = seededRandom(91);
    const stoneMaterial = new THREE.MeshStandardMaterial({ color: 0x879384, roughness: 1, flatShading: true });
    for (let i = 0; i < 18; i += 1) {
      const angle = random() * Math.PI * 2;
      const radius = 3.1 + random() * 1.4;
      const stone = new THREE.Mesh(new THREE.CylinderGeometry(0.32 + random() * 0.25, 0.4, 0.08, 7), stoneMaterial);
      stone.position.set(Math.sin(angle) * radius, 0.07, Math.cos(angle) * radius);
      stone.rotation.y = random() * Math.PI;
      stone.receiveShadow = true;
      this.scene.add(stone);
    }

    const grassGeometry = new THREE.ConeGeometry(0.16, 0.65, 3);
    grassGeometry.translate(0, 0.32, 0);
    const grassTufts = new THREE.InstancedMesh(grassGeometry, new THREE.MeshStandardMaterial({ color: 0x496f57, roughness: 1 }), 150);
    const dummy = new THREE.Object3D();
    for (let i = 0; i < 150; i += 1) {
      const angle = random() * Math.PI * 2;
      const radius = 5 + Math.sqrt(random()) * 17.5;
      dummy.position.set(Math.sin(angle) * radius, 0, Math.cos(angle) * radius);
      dummy.rotation.y = random() * Math.PI;
      const scale = 0.55 + random() * 0.9;
      dummy.scale.set(scale, scale, scale);
      dummy.updateMatrix();
      grassTufts.setMatrixAt(i, dummy.matrix);
    }
    grassTufts.receiveShadow = true;
    this.scene.add(grassTufts);

    const rockGeometry = new THREE.DodecahedronGeometry(0.55, 0);
    const rocks = new THREE.InstancedMesh(rockGeometry, new THREE.MeshStandardMaterial({ color: 0x71827a, roughness: 1, flatShading: true }), 34);
    for (let i = 0; i < 34; i += 1) {
      const angle = random() * Math.PI * 2;
      const radius = 8 + Math.sqrt(random()) * 15;
      dummy.position.set(Math.sin(angle) * radius, 0.2, Math.cos(angle) * radius);
      dummy.rotation.set(random(), random() * Math.PI, random() * 0.3);
      const scale = 0.4 + random() * 1.2;
      dummy.scale.set(scale, scale * 0.65, scale);
      dummy.updateMatrix();
      rocks.setMatrixAt(i, dummy.matrix);
    }
    rocks.castShadow = true;
    rocks.receiveShadow = true;
    this.scene.add(rocks);

    const ruinMaterial = new THREE.MeshStandardMaterial({ color: 0x6f8179, roughness: 1 });
    for (const [x, z, rotation] of [[-18, -9, 0.3], [17, 10, -0.5], [-6, 19, 0.8]] as const) {
      const ruin = new THREE.Group();
      const left = new THREE.Mesh(new THREE.BoxGeometry(0.65, 2.5, 0.65), ruinMaterial);
      left.position.set(-1, 1.25, 0);
      const right = left.clone();
      right.position.x = 1;
      right.scale.y = 0.7;
      right.position.y = 0.88;
      const top = new THREE.Mesh(new THREE.BoxGeometry(2.8, 0.55, 0.65), ruinMaterial);
      top.position.y = 2.55;
      top.rotation.z = 0.08;
      ruin.add(left, right, top);
      ruin.position.set(x, 0, z);
      ruin.rotation.y = rotation;
      ruin.traverse((child) => { if (child instanceof THREE.Mesh) { child.castShadow = true; child.receiveShadow = true; } });
      this.scene.add(ruin);
    }
  }

  private createWater(): Environment['water'] {
    const geometry = new THREE.PlaneGeometry(150, 150, 42, 42);
    const material = new THREE.MeshStandardMaterial({
      color: 0x3a8e91,
      emissive: 0x082529,
      roughness: 0.3,
      metalness: 0.12,
      transparent: true,
      opacity: 0.93,
      flatShading: true,
    });
    const water = new THREE.Mesh(geometry, material);
    water.rotation.x = -Math.PI / 2;
    water.position.y = -1.05;
    water.receiveShadow = true;
    this.scene.add(water);
    return water;
  }

  private createAsh(): Environment['ash'] {
    const random = seededRandom(717);
    const positions = new Float32Array(500 * 3);
    for (let i = 0; i < 500; i += 1) {
      positions[i * 3] = (random() - 0.5) * 60;
      positions[i * 3 + 1] = random() * 22;
      positions[i * 3 + 2] = (random() - 0.5) * 60;
    }
    const geometry = new THREE.BufferGeometry();
    geometry.setAttribute('position', new THREE.BufferAttribute(positions, 3));
    const material = new THREE.PointsMaterial({ color: 0xd3c4b1, size: 0.085, transparent: true, opacity: 0, depthWrite: false });
    const ash = new THREE.Points(geometry, material);
    ash.visible = false;
    this.scene.add(ash);
    return ash;
  }

  private createStars(): Environment['stars'] {
    const random = seededRandom(19);
    const positions = new Float32Array(320 * 3);
    for (let i = 0; i < 320; i += 1) {
      const angle = random() * Math.PI * 2;
      const elevation = random() * Math.PI * 0.42 + 0.12;
      const radius = 68;
      positions[i * 3] = Math.cos(angle) * Math.cos(elevation) * radius;
      positions[i * 3 + 1] = Math.sin(elevation) * radius;
      positions[i * 3 + 2] = Math.sin(angle) * Math.cos(elevation) * radius;
    }
    const geometry = new THREE.BufferGeometry();
    geometry.setAttribute('position', new THREE.BufferAttribute(positions, 3));
    const material = new THREE.PointsMaterial({ color: 0xe6edda, size: 0.24, transparent: true, opacity: 0, depthWrite: false });
    const stars = new THREE.Points(geometry, material);
    stars.visible = false;
    this.scene.add(stars);
    return stars;
  }

  private createClouds(): THREE.Group {
    const clouds = new THREE.Group();
    const material = new THREE.MeshStandardMaterial({ color: 0xe7e5cf, roughness: 1, flatShading: true });
    const random = seededRandom(44);
    for (let i = 0; i < 7; i += 1) {
      const cloud = new THREE.Group();
      for (let j = 0; j < 4; j += 1) {
        const puff = new THREE.Mesh(new THREE.DodecahedronGeometry(1.5 + random(), 0), material);
        puff.position.set(j * 1.8, random() * 0.8, random() * 1.4);
        puff.scale.y = 0.55;
        cloud.add(puff);
      }
      cloud.position.set(-45 + i * 15, 17 + random() * 8, -25 + random() * 50);
      cloud.scale.setScalar(0.8 + random() * 0.7);
      clouds.add(cloud);
    }
    this.scene.add(clouds);
    return clouds;
  }
}
