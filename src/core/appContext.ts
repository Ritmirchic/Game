import { SoundEngine } from '../audio/soundEngine';
import { SaveManager } from './saveManager';

export const saveManager = new SaveManager();
export const soundEngine = new SoundEngine(saveManager.getData().settings);
