import { TestBed } from '@angular/core/testing';

import { UnitSpriteService, unitSpriteOptions } from './unit-sprite.service';

const MANIFEST = {
  '/assets/units/scout/sprite.json': 'units/scout/sprite.1a2b.json',
  '/assets/units/heavy_tank/sprite.json': 'units/heavy_tank/sprite.3c4d.json',
  '/assets/units/heavy_tank/turn.png': 'units/heavy_tank/turn.5e6f.png',
  '/assets/units/heavy_tank/aim_ne.png': 'units/heavy_tank/aim_ne.7a8b.png',
  '/assets/units/heavy_tank/notes.txt': 'units/heavy_tank/notes.txt',
  // Ohne sprite.json kein vollstaendiges Einheitenbild.
  '/assets/units/half_done/turn.png': 'units/half_done/turn.9c0d.png',
  '/assets/units/Bad-Name/sprite.json': 'units/Bad-Name/sprite.json',
  '/assets/tiles/grass.png': 'tiles/grass.1111.png',
};

describe('UnitSpriteService', () => {
  let fetchMock: ReturnType<typeof vi.fn>;

  const respond = (body: unknown, ok = true, status = 200) =>
    Promise.resolve({ ok, status, json: () => Promise.resolve(body) } as Response);

  beforeEach(() => {
    fetchMock = vi.fn(() => respond(MANIFEST));
    vi.stubGlobal('fetch', fetchMock);
  });

  afterEach(() => vi.unstubAllGlobals());

  const service = () => TestBed.inject(UnitSpriteService);

  it('liest die Schluessel aller Bilder mit sprite.json, alphabetisch', async () => {
    await expect(service().keys()).resolves.toEqual(['heavy_tank', 'scout']);
    expect(fetchMock).toHaveBeenCalledWith('/wasm/assets/manifest.json', { cache: 'no-cache' });
  });

  it('liefert die PNG-Dateien eines Bildes mit gehashter URL, nach Namen sortiert', async () => {
    await expect(service().images('heavy_tank')).resolves.toEqual([
      { name: 'aim_ne.png', url: '/wasm/assets/units/heavy_tank/aim_ne.7a8b.png' },
      { name: 'turn.png', url: '/wasm/assets/units/heavy_tank/turn.5e6f.png' },
    ]);
    await expect(service().images('unbekannt')).resolves.toEqual([]);
  });

  it('laedt das Manifest nur einmal, nach refresh() neu', async () => {
    const sprites = service();
    await sprites.keys();
    await sprites.images('scout');
    expect(fetchMock).toHaveBeenCalledTimes(1);

    sprites.refresh();
    await sprites.keys();
    expect(fetchMock).toHaveBeenCalledTimes(2);
  });

  it('versucht es nach einem Fehler beim naechsten Aufruf erneut', async () => {
    fetchMock.mockReturnValueOnce(respond(null, false, 503));
    const sprites = service();

    await expect(sprites.keys()).rejects.toThrow('/wasm/assets/manifest.json: HTTP 503');
    await expect(sprites.keys()).resolves.toEqual(['heavy_tank', 'scout']);
  });
});

describe('unitSpriteOptions', () => {
  it('bietet "kein Bild" und alle Schluessel an', () => {
    expect(unitSpriteOptions(['scout'], 'scout')).toEqual([
      { value: '', label: 'Kein Bild (Rechteck)' },
      { value: 'scout', label: 'scout' },
    ]);
  });

  it('behaelt einen gespeicherten Schluessel, den es in den Assets nicht gibt', () => {
    expect(unitSpriteOptions([], 'alt').at(-1)).toEqual({ value: 'alt', label: 'alt (nicht in den Assets)' });
    expect(unitSpriteOptions([], '')).toHaveLength(1);
  });
});
