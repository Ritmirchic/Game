export type AssetName =
  | 'soldier.glb'
  | 'fox.glb'
  | 'dead-tree.glb'
  | 'tree-billboard.png'
  | 'outdoor.env'
  | 'grass-photo.jpg'
  | 'grass-normal.png'
  | 'sand.jpg'
  | 'water-bump.png'
  | 'rock.png'
  | 'rock-normal.png'
  | 'bark.png'
  | 'wood.jpg'
  | 'wood-bump.jpg'
  | 'wood-roughness.jpg'

declare global {
  interface Window {
    __LAST_BEACON_ASSETS__?: Partial<Record<AssetName, string>>
    __MIRAGE_ASSETS__?: Partial<Record<AssetName, string>>
  }
}

export function assetUrl(name: AssetName): string {
  return window.__MIRAGE_ASSETS__?.[name] ?? window.__LAST_BEACON_ASSETS__?.[name] ?? `/assets/${name}`
}
