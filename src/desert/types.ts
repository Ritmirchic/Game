import type { CampaignHUD, StoryDialog } from './Campaign'

export type DesertState = 'menu' | 'playing' | 'paused' | 'story' | 'gameover' | 'victory'
export type ResourceKind = 'scrap' | 'fiber' | 'water'
export type PerspectiveMode = 'first' | 'third'

export interface DesertSnapshot {
  state: DesertState
  perspective: PerspectiveMode
  health: number
  maxHealth: number
  stamina: number
  hydration: number
  scrap: number
  fiber: number
  water: number
  kills: number
  enemies: number
  elapsed: number
  day: number
  dayProgress: number
  temperature: number
  heading: number
  x: number
  z: number
  prompt: string
  promptDetail: string
  nearRelay: boolean
  relayActive: boolean
  signalTime: number
  canSentry: boolean
  canBandage: boolean
  canRelay: boolean
  dodgeCooldown: number
  campaign: CampaignHUD
}

export interface DesertEndStats {
  victory: boolean
  reason: string
  elapsed: number
  kills: number
  distance: number
}

export interface DesertCallbacks {
  onSnapshot: (snapshot: DesertSnapshot) => void
  onState: (state: DesertState, stats?: DesertEndStats) => void
  onNotify: (message: string, tone?: 'info' | 'good' | 'danger') => void
  onAnnounce: (eyebrow: string, title: string, detail: string) => void
  onLoot: (message: string, kind: ResourceKind | 'health') => void
  onDamage: () => void
  onStory: (dialog: StoryDialog | null) => void
}
