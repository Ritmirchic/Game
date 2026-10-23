export type GameState = 'menu' | 'playing' | 'paused' | 'gameover' | 'victory'
export type Phase = 'day' | 'night'
export type ResourceType = 'wood' | 'scrap' | 'crystal'
export type RecipeId = 'barricade' | 'turret' | 'campfire' | 'repair' | 'medkit'

export interface Inventory {
  wood: number
  scrap: number
  crystal: number
}

export interface GameSnapshot {
  state: GameState
  health: number
  stamina: number
  warmth: number
  beaconHealth: number
  inventory: Inventory
  cores: number
  day: number
  phase: Phase
  phaseTime: number
  phaseDuration: number
  finalCharge: number | null
  finalChargeDuration: number
  prompt: string
  promptDetail: string
  enemies: number
  kills: number
  gathered: number
  elapsed: number
  canAfford: Record<RecipeId, boolean>
  nearBeacon: boolean
  dodgeCooldown: number
  campfires: number
}

export interface EndStats {
  victory: boolean
  reason: string
  kills: number
  gathered: number
  elapsed: number
  day: number
  cores: number
}

export interface GameCallbacks {
  onSnapshot: (snapshot: GameSnapshot) => void
  onState: (state: GameState, stats?: EndStats) => void
  onNotify: (message: string, tone?: 'info' | 'good' | 'danger') => void
  onAnnounce: (eyebrow: string, title: string, detail: string) => void
  onLoot: (message: string, resource: ResourceType | 'health' | 'warmth') => void
  onDamage: () => void
}
