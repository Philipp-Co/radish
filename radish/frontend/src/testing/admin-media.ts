import {
  AdminEffect,
  AdminSprite,
  AdminSpriteImage,
  AdminTerrainType,
} from '../app/core/api.service';

/*
 * Testdaten fuer die Admin-Medienseiten (Einheitenbilder, Effekte, Gelaende):
 * jede Funktion liefert einen gueltigen Eintrag mit festen Werten, ein Test
 * ueberschreibt nur, worauf es ihm ankommt.
 */

export function pngImage(overrides: Partial<AdminSpriteImage> = {}): AdminSpriteImage {
  return { id: 1, name: 'turn.png', width: 512, height: 128, sha256: 'aaa', ...overrides };
}

export function adminSprite(overrides: Partial<AdminSprite> = {}): AdminSprite {
  return {
    id: 5,
    key: 'heavy_tank',
    anchor_x: 0.5,
    anchor_y: 0.8,
    draw_width: 128,
    own_shadow: false,
    step_ms: null,
    turn_ms: null,
    aim_ms: null,
    aim_hold_ms: null,
    images: [],
    atlases: [],
    complete: false,
    problems: [],
    used_by: [],
    updated_at: '2026-11-27T12:00:00Z',
    ...overrides,
  };
}

export function adminEffect(overrides: Partial<AdminEffect> = {}): AdminEffect {
  return {
    id: 3,
    key: 'explosion_small',
    kind: 'explosion',
    draw_width: 256,
    duration_ms: 1000,
    images: [],
    atlases: [],
    complete: false,
    problems: [],
    updated_at: '2026-11-27T12:00:00Z',
    ...overrides,
  };
}

export function terrainType(overrides: Partial<AdminTerrainType> = {}): AdminTerrainType {
  return {
    code: 'forest',
    label: 'Wald',
    movement_modifier: -1,
    cover: 2,
    impassable: false,
    blocks_sight: true,
    image_id: null,
    images: [],
    decor: [],
    ...overrides,
  };
}

/**
 * jsdom kennt keine Object-URLs: ersetzt URL.createObjectURL (fortlaufend
 * "blob:preview-1", "-2", ...) und URL.revokeObjectURL durch Mocks.
 */
export function stubObjectUrls(): void {
  let next = 0;
  URL.createObjectURL = vi.fn(() => `blob:preview-${++next}`);
  URL.revokeObjectURL = vi.fn();
}
