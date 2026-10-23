import * as THREE from 'three';
import type { ResourceType } from './types';

export interface CharacterRig {
  root: THREE.Group;
  leftArm: THREE.Group;
  rightArm: THREE.Group;
  leftLeg: THREE.Group;
  rightLeg: THREE.Group;
  head: THREE.Group;
  tool?: THREE.Group;
}

export interface BeaconRig {
  root: THREE.Group;
  rings: THREE.Mesh[];
  coreSockets: THREE.Mesh<THREE.BoxGeometry, THREE.MeshStandardMaterial>[];
  glow: THREE.Mesh<THREE.SphereGeometry, THREE.MeshStandardMaterial>;
  beam: THREE.Mesh<THREE.CylinderGeometry, THREE.MeshBasicMaterial>;
  light: THREE.PointLight;
}

export interface TurretRig {
  root: THREE.Group;
  head: THREE.Group;
  muzzle: THREE.Object3D;
}

const standard = (color: THREE.ColorRepresentation, roughness = 0.82, metalness = 0.02): THREE.MeshStandardMaterial =>
  new THREE.MeshStandardMaterial({ color, roughness, metalness, flatShading: true });

function box(
  parent: THREE.Object3D,
  size: [number, number, number],
  position: [number, number, number],
  material: THREE.Material,
): THREE.Mesh {
  const mesh = new THREE.Mesh(new THREE.BoxGeometry(...size), material);
  mesh.position.set(...position);
  mesh.castShadow = true;
  mesh.receiveShadow = true;
  parent.add(mesh);
  return mesh;
}

function cylinder(
  parent: THREE.Object3D,
  radiusTop: number,
  radiusBottom: number,
  height: number,
  position: [number, number, number],
  material: THREE.Material,
  segments = 8,
): THREE.Mesh {
  const mesh = new THREE.Mesh(new THREE.CylinderGeometry(radiusTop, radiusBottom, height, segments), material);
  mesh.position.set(...position);
  mesh.castShadow = true;
  mesh.receiveShadow = true;
  parent.add(mesh);
  return mesh;
}

function limb(root: THREE.Group, x: number, y: number, length: number, width: number, material: THREE.Material): THREE.Group {
  const pivot = new THREE.Group();
  pivot.position.set(x, y, 0);
  box(pivot, [width, length, width], [0, -length / 2, 0], material);
  root.add(pivot);
  return pivot;
}

export function createPlayer(): CharacterRig {
  const root = new THREE.Group();
  root.name = 'Выживший';
  const jacket = standard(0x2f827c);
  const jacketDark = standard(0x235b5b);
  const trousers = standard(0x263c49);
  const boots = standard(0x172831);
  const skin = standard(0xe8b887);
  const hair = standard(0x3b2c29);
  const amber = standard(0xe99b55);

  box(root, [0.94, 0.95, 0.52], [0, 1.38, 0], jacket);
  box(root, [0.98, 0.13, 0.56], [0, 1.04, 0], jacketDark);
  box(root, [0.16, 0.76, 0.55], [0, 1.42, 0.02], amber);
  box(root, [0.68, 0.65, 0.25], [0, 1.43, -0.38], jacketDark);
  box(root, [0.5, 0.15, 0.3], [0, 1.73, -0.51], amber);

  const leftLeg = limb(root, -0.24, 0.94, 0.84, 0.34, trousers);
  const rightLeg = limb(root, 0.24, 0.94, 0.84, 0.34, trousers);
  box(leftLeg, [0.37, 0.22, 0.54], [0, -0.77, 0.09], boots);
  box(rightLeg, [0.37, 0.22, 0.54], [0, -0.77, 0.09], boots);

  const leftArm = limb(root, -0.62, 1.79, 0.89, 0.29, jacket);
  const rightArm = limb(root, 0.62, 1.79, 0.89, 0.29, jacket);
  box(leftArm, [0.3, 0.24, 0.3], [0, -0.84, 0], skin);
  box(rightArm, [0.3, 0.24, 0.3], [0, -0.84, 0], skin);

  const head = new THREE.Group();
  head.position.y = 2.18;
  box(head, [0.72, 0.7, 0.68], [0, 0, 0], skin);
  box(head, [0.76, 0.2, 0.72], [0, 0.32, -0.01], hair);
  box(head, [0.76, 0.3, 0.16], [0, 0.2, -0.34], hair);
  const face = standard(0x29343a);
  box(head, [0.075, 0.09, 0.025], [-0.17, 0.04, 0.35], face);
  box(head, [0.075, 0.09, 0.025], [0.17, 0.04, 0.35], face);
  box(head, [0.2, 0.045, 0.025], [0, -0.17, 0.35], face);
  root.add(head);

  const tool = new THREE.Group();
  tool.position.set(0, -0.9, 0.08);
  tool.rotation.z = -0.18;
  box(tool, [0.09, 1.2, 0.09], [0, -0.35, 0], standard(0x76513a));
  const axeHead = box(tool, [0.62, 0.3, 0.13], [-0.18, -0.86, 0], standard(0xa7b4b2, 0.4, 0.65));
  axeHead.rotation.z = -0.14;
  rightArm.add(tool);

  root.traverse((child) => {
    if (child instanceof THREE.Mesh) child.castShadow = true;
  });
  return { root, leftArm, rightArm, leftLeg, rightLeg, head, tool };
}

export interface EnemyRig extends CharacterRig {
  healthBar: THREE.Group;
  healthFill: THREE.Mesh;
}

export function createEnemy(kind: 'walker' | 'runner' | 'brute'): EnemyRig {
  const root = new THREE.Group();
  const scale = kind === 'brute' ? 1.35 : kind === 'runner' ? 0.85 : 1;
  const body = standard(kind === 'brute' ? 0x30333f : 0x252c38);
  const limbs = standard(0x1b2430);
  const ash = standard(0x52545c);
  const eye = new THREE.MeshStandardMaterial({ color: 0xffa06c, emissive: 0xff5d35, emissiveIntensity: 3 });

  box(root, [0.94, 0.95, 0.55], [0, 1.38, 0], body);
  box(root, [0.65, 0.13, 0.59], [0, 1.54, 0], ash).rotation.z = 0.25;
  const leftLeg = limb(root, -0.24, 0.94, 0.84, 0.34, limbs);
  const rightLeg = limb(root, 0.24, 0.94, 0.84, 0.34, limbs);
  const leftArm = limb(root, -0.63, 1.79, 0.96, 0.28, limbs);
  const rightArm = limb(root, 0.63, 1.79, 0.96, 0.28, limbs);
  leftArm.rotation.x = -0.65;
  rightArm.rotation.x = -0.65;

  const head = new THREE.Group();
  head.position.y = 2.18;
  box(head, [0.72, 0.7, 0.68], [0, 0, 0], body);
  box(head, [0.75, 0.14, 0.72], [0, 0.31, 0], ash).rotation.z = -0.09;
  box(head, [0.18, 0.075, 0.035], [-0.17, 0.04, 0.355], eye);
  box(head, [0.18, 0.075, 0.035], [0.17, 0.04, 0.355], eye);
  root.add(head);

  const healthBar = new THREE.Group();
  healthBar.position.y = 2.98;
  const back = box(healthBar, [1.05, 0.1, 0.025], [0, 0, 0], new THREE.MeshBasicMaterial({ color: 0x111820 }));
  back.castShadow = false;
  const healthFill = box(healthBar, [1, 0.065, 0.035], [0, 0, 0.02], new THREE.MeshBasicMaterial({ color: 0xf29a69 }));
  healthFill.castShadow = false;
  healthBar.visible = false;
  root.add(healthBar);
  root.scale.setScalar(scale);
  return { root, leftArm, rightArm, leftLeg, rightLeg, head, healthBar, healthFill };
}

export function createBeacon(): BeaconRig {
  const root = new THREE.Group();
  root.name = 'Последний маяк';
  const stone = standard(0x7d8982);
  const stoneDark = standard(0x4b5e5d);
  const metal = standard(0x314c52, 0.45, 0.55);
  const copper = standard(0xb9794f, 0.45, 0.45);
  const glowMaterial = new THREE.MeshStandardMaterial({
    color: 0xffd29a,
    emissive: 0xff9b4a,
    emissiveIntensity: 2.4,
    roughness: 0.25,
  });

  cylinder(root, 2.25, 2.5, 0.45, [0, 0.22, 0], stoneDark, 12);
  cylinder(root, 1.75, 2.05, 0.45, [0, 0.63, 0], stone, 12);
  cylinder(root, 0.62, 0.9, 4.7, [0, 3.05, 0], metal, 8);
  for (let i = 0; i < 3; i += 1) {
    const angle = (i / 3) * Math.PI * 2;
    const leg = box(root, [0.22, 4.3, 0.22], [Math.sin(angle) * 0.78, 2.8, Math.cos(angle) * 0.78], copper);
    leg.rotation.z = Math.sin(angle) * -0.1;
    leg.rotation.x = Math.cos(angle) * 0.1;
  }
  cylinder(root, 1.2, 1.2, 0.18, [0, 5.35, 0], copper, 12);
  cylinder(root, 0.85, 1.05, 0.32, [0, 6.25, 0], metal, 8);

  const rings: THREE.Mesh[] = [];
  for (const y of [5.55, 5.95]) {
    const ring = new THREE.Mesh(new THREE.TorusGeometry(0.92, 0.075, 6, 20), copper);
    ring.position.y = y;
    ring.rotation.x = Math.PI / 2;
    ring.castShadow = true;
    root.add(ring);
    rings.push(ring);
  }

  const glow = new THREE.Mesh(new THREE.SphereGeometry(0.5, 12, 8), glowMaterial);
  glow.position.y = 5.76;
  root.add(glow);

  const beamMaterial = new THREE.MeshBasicMaterial({
    color: 0xffc681,
    transparent: true,
    opacity: 0.04,
    depthWrite: false,
    blending: THREE.AdditiveBlending,
    side: THREE.DoubleSide,
  });
  const beam = new THREE.Mesh(new THREE.CylinderGeometry(0.32, 0.8, 70, 12, 1, true), beamMaterial);
  beam.position.y = 40;
  root.add(beam);

  const light = new THREE.PointLight(0xffa45f, 4, 22, 1.6);
  light.position.y = 5.8;
  light.castShadow = false;
  root.add(light);

  const coreSockets: BeaconRig['coreSockets'] = [];
  for (let i = 0; i < 3; i += 1) {
    const angle = (i / 3) * Math.PI * 2 + Math.PI / 3;
    const material = new THREE.MeshStandardMaterial({ color: 0x334c50, emissive: 0x152a2b, emissiveIntensity: 0.3, metalness: 0.5 });
    const socket = new THREE.Mesh(new THREE.BoxGeometry(0.48, 0.65, 0.32), material);
    socket.position.set(Math.sin(angle) * 1.45, 0.95, Math.cos(angle) * 1.45);
    socket.rotation.y = angle;
    socket.castShadow = true;
    root.add(socket);
    coreSockets.push(socket);
  }
  return { root, rings, coreSockets, glow, beam, light };
}

export function createResource(type: ResourceType, variation: number): THREE.Group {
  const root = new THREE.Group();
  if (type === 'wood') {
    const trunk = standard(variation > 0.5 ? 0x806147 : 0x6d513c);
    const leaves = standard(variation > 0.5 ? 0x47775c : 0x3b6955);
    const leavesLight = standard(variation > 0.5 ? 0x5c8963 : 0x4d7c5d);
    cylinder(root, 0.28, 0.42, 2.8, [0, 1.4, 0], trunk, 7);
    const crown1 = new THREE.Mesh(new THREE.IcosahedronGeometry(1.4, 0), leaves);
    crown1.position.set(0, 3.25, 0);
    crown1.scale.set(1, 1.25, 1);
    const crown2 = new THREE.Mesh(new THREE.IcosahedronGeometry(1.05, 0), leavesLight);
    crown2.position.set(0.7, 3.1, 0.15);
    const crown3 = new THREE.Mesh(new THREE.IcosahedronGeometry(0.95, 0), leaves);
    crown3.position.set(-0.65, 3.4, -0.2);
    for (const crown of [crown1, crown2, crown3]) {
      crown.castShadow = true;
      crown.receiveShadow = true;
      root.add(crown);
    }
  } else if (type === 'scrap') {
    const rust = standard(0x9a654b, 0.6, 0.45);
    const metal = standard(0x627575, 0.5, 0.65);
    const dark = standard(0x3d5256, 0.55, 0.5);
    const piece1 = box(root, [1.25, 0.3, 0.65], [0, 0.25, 0], rust);
    piece1.rotation.y = 0.35;
    const piece2 = box(root, [0.75, 0.28, 1.15], [0.18, 0.52, 0], metal);
    piece2.rotation.y = -0.4;
    piece2.rotation.z = 0.18;
    box(root, [0.28, 0.75, 0.28], [-0.4, 0.55, 0.2], dark).rotation.z = -0.5;
    const gear = new THREE.Mesh(new THREE.TorusGeometry(0.38, 0.12, 6, 10), rust);
    gear.position.set(0.45, 0.5, 0.35);
    gear.rotation.x = Math.PI / 2;
    gear.castShadow = true;
    root.add(gear);
  } else {
    const crystalMaterial = new THREE.MeshStandardMaterial({
      color: 0x79e6d1,
      emissive: 0x2ba994,
      emissiveIntensity: 1.3,
      roughness: 0.25,
      metalness: 0.15,
      flatShading: true,
    });
    const stone = standard(0x536b68);
    cylinder(root, 0.8, 1, 0.3, [0, 0.15, 0], stone, 7);
    const shards: Array<[number, number, number, number, number]> = [
      [0, 0.95, 0, 0.42, 1.4],
      [0.48, 0.62, 0.1, 0.28, 0.9],
      [-0.4, 0.55, 0.2, 0.25, 0.75],
      [0.1, 0.45, -0.42, 0.22, 0.65],
    ];
    for (const [x, y, z, radius, height] of shards) {
      const shard = new THREE.Mesh(new THREE.OctahedronGeometry(1, 0), crystalMaterial);
      shard.scale.set(radius, height, radius);
      shard.position.set(x, y, z);
      shard.rotation.z = x * 0.35;
      shard.castShadow = true;
      root.add(shard);
    }
  }
  return root;
}

export function createBarricade(): THREE.Group {
  const root = new THREE.Group();
  const wood = standard(0x8a6849);
  const woodLight = standard(0xb0885d);
  const metal = standard(0x546669, 0.5, 0.6);
  box(root, [0.24, 1.8, 0.25], [-1.25, 0.9, 0], wood);
  box(root, [0.24, 1.8, 0.25], [1.25, 0.9, 0], wood);
  const plank1 = box(root, [3.15, 0.4, 0.22], [0, 0.85, 0], woodLight);
  plank1.rotation.z = 0.28;
  const plank2 = box(root, [3.15, 0.4, 0.22], [0, 0.85, 0.03], wood);
  plank2.rotation.z = -0.28;
  box(root, [3.1, 0.28, 0.25], [0, 1.4, 0], woodLight);
  for (const x of [-1.25, 1.25]) cylinder(root, 0.1, 0.1, 0.3, [x, 1.92, 0], metal, 6);
  return root;
}

export function createTurret(): TurretRig {
  const root = new THREE.Group();
  const metal = standard(0x3f5b60, 0.45, 0.65);
  const copper = standard(0xc18656, 0.45, 0.55);
  const glow = new THREE.MeshStandardMaterial({ color: 0x8ce6d3, emissive: 0x3bc4ab, emissiveIntensity: 1.8 });
  cylinder(root, 0.72, 0.9, 0.3, [0, 0.15, 0], metal, 8);
  cylinder(root, 0.18, 0.24, 1.1, [0, 0.8, 0], copper, 8);
  const head = new THREE.Group();
  head.position.y = 1.42;
  box(head, [1.05, 0.55, 0.85], [0, 0, 0], metal);
  box(head, [0.2, 0.2, 1.25], [0, 0.05, 0.85], copper);
  box(head, [0.45, 0.16, 0.06], [0, 0.12, 0.46], glow);
  const muzzle = new THREE.Object3D();
  muzzle.position.set(0, 0.05, 1.52);
  head.add(muzzle);
  root.add(head);
  return { root, head, muzzle };
}
