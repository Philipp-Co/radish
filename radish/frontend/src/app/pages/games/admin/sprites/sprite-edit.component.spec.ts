import { HttpErrorResponse } from '@angular/common/http';
import { TestBed } from '@angular/core/testing';
import { Component } from '@angular/core';
import { Router, provideRouter } from '@angular/router';
import { RouterTestingHarness } from '@angular/router/testing';
import { Subject, of, throwError } from 'rxjs';

import { adminSprite, pngImage, stubObjectUrls } from '../../../../../testing/admin-media';
import { AdminSprite, AdminSpriteAtlas, ApiService } from '../../../../core/api.service';
import { UnitSpriteService } from '../../../../core/unit-sprite.service';
import { SpriteEditComponent } from './sprite-edit.component';

@Component({ template: '' })
class StubPage {}

type Api = Record<
  | 'getSprite'
  | 'updateSprite'
  | 'deleteSprite'
  | 'uploadSpriteImage'
  | 'replaceSpriteImage'
  | 'deleteSpriteImage'
  | 'putSpriteAtlases'
  | 'spriteImageBlob',
  ReturnType<typeof vi.fn>
>;

const turnPng = pngImage({ id: 1, name: 'turn.png', sha256: 'a' });
const movePng = pngImage({ id: 2, name: 'move.png', sha256: 'b' });

describe('SpriteEditComponent', () => {
  let api: Api;
  let unitSprites: { refresh: ReturnType<typeof vi.fn> };
  let harness: RouterTestingHarness;
  let component: SpriteEditComponent;

  async function open(sprite: AdminSprite = adminSprite()): Promise<HTMLElement> {
    api.getSprite.mockReturnValue(of(sprite));
    TestBed.configureTestingModule({
      providers: [
        provideRouter([
          { path: 'games/admin/sprites/:id', component: SpriteEditComponent },
          { path: 'games/admin/sprites', component: StubPage },
        ]),
        { provide: ApiService, useValue: api },
        { provide: UnitSpriteService, useValue: unitSprites },
      ],
    });
    harness = await RouterTestingHarness.create();
    component = await harness.navigateByUrl(`/games/admin/sprites/${sprite.id}`, SpriteEditComponent);
    return harness.routeNativeElement!;
  }

  const stable = () => harness.fixture.whenStable();
  const text = (el: Element) => el.textContent?.replace(/\s+/g, ' ') ?? '';
  const button = (el: HTMLElement, label: string) =>
    Array.from(el.querySelectorAll('button')).find((b) => b.textContent?.trim() === label)!;

  /** Ein change-Event an einem Datei-Input mit diesen Dateien. */
  function chooseFiles(input: HTMLInputElement, files: File[]): void {
    Object.defineProperty(input, 'files', { value: files, configurable: true });
    input.dispatchEvent(new Event('change'));
  }

  beforeEach(() => {
    api = {
      getSprite: vi.fn(),
      updateSprite: vi.fn(),
      deleteSprite: vi.fn(),
      uploadSpriteImage: vi.fn(),
      replaceSpriteImage: vi.fn(),
      deleteSpriteImage: vi.fn(),
      putSpriteAtlases: vi.fn(),
      spriteImageBlob: vi.fn(() => of(new Blob(['png']))),
    };
    unitSprites = { refresh: vi.fn() };
    stubObjectUrls();
  });

  afterEach(() => vi.restoreAllMocks());

  it('zeigt Schluessel, Entwurfsstatus, offene Punkte und Verwendung', async () => {
    const el = await open(
      adminSprite({ problems: ['Drehen fehlt.'], used_by: ['Panzer (Menschen)'] }),
    );
    expect(el.querySelector('h2')?.textContent).toBe('heavy_tank');
    expect(text(el)).toContain('Entwurf');
    expect(text(el)).toContain('Offen: 1');
    expect(text(el)).toContain('Verwendet von Panzer (Menschen).');
    // Ein verwendetes Bild laesst sich nicht loeschen.
    expect(button(el, 'Löschen').disabled).toBe(true);
  });

  it('zeigt den Ladefehler', async () => {
    api.getSprite.mockReturnValue(throwError(() => new HttpErrorResponse({ status: 404, error: { detail: 'Gibt es nicht.' } })));
    TestBed.configureTestingModule({
      providers: [
        provideRouter([{ path: 'games/admin/sprites/:id', component: SpriteEditComponent }]),
        { provide: ApiService, useValue: api },
        { provide: UnitSpriteService, useValue: unitSprites },
      ],
    });
    harness = await RouterTestingHarness.create();
    await harness.navigateByUrl('/games/admin/sprites/99');
    expect(harness.routeNativeElement!.querySelector('.error')?.textContent).toBe('Gibt es nicht.');
    expect(api.getSprite).toHaveBeenCalledWith(99);
  });

  describe('Atlas-Tabelle', () => {
    it('hat 17 Zeilen: Drehen einmal fuer alle, die uebrigen Animationen je vier Richtungen', async () => {
      const el = await open();
      const rows = Array.from(el.querySelectorAll('tbody tr'));
      expect(rows.length).toBe(17);
      expect(rows.filter((row) => row.classList.contains('group')).map((row) => text(row.querySelector('td')!).trim()))
        .toEqual(['Drehen', 'Zielen für alle Richtungen', 'Ziehen für alle Richtungen', 'Schießen für alle Richtungen', 'Zerstört für alle Richtungen']);
    });

    it('belegt Zeilen aus gespeicherten Atlanten, die uebrigen mit Vorgaben', async () => {
      const saved: AdminSpriteAtlas = {
        animation: 'move',
        facing: 'ne',
        image_id: 2,
        frame_width: 96,
        frame_height: 64,
        frame_count: 8,
        key_frames: null,
        fps: 12,
      };
      await open(adminSprite({ images: [turnPng, movePng], atlases: [saved] }));

      const move = component.rows.find((row) => row.animation === 'move' && row.facing === 'ne')!;
      expect(move).toMatchObject({ image_id: 2, frame_width: 96, frame_height: 64, frame_count: 8, fps: 12 });
      const aim = component.rows.find((row) => row.animation === 'aim' && row.facing === 'sw')!;
      expect(aim).toMatchObject({ image_id: null, frame_width: 128, frame_count: 4, key_frames: [0, 1, 2, 3] });
      const fire = component.rows.find((row) => row.animation === 'fire' && row.facing === 'sw')!;
      expect(fire).toMatchObject({ frame_count: 1, fps: 10 });
    });

    it('speichert nur belegte Zeilen, Drehungen mit Schluesselbildern, Folgen mit fps', async () => {
      api.putSpriteAtlases.mockReturnValue(of(adminSprite({ images: [turnPng, movePng] })));
      const el = await open(adminSprite({ images: [turnPng, movePng] }));

      const turn = component.rows.find((row) => row.animation === 'turn')!;
      turn.image_id = 1;
      // Aus Eingabefeldern kommen auch Texte.
      turn.key_frames = ['0', '2', '4', '6'] as unknown as number[];
      const move = component.rows.find((row) => row.animation === 'move' && row.facing === 'sw')!;
      move.image_id = 2;
      move.frame_count = '8' as unknown as number;
      move.fps = 12;

      button(el, 'Atlanten speichern').click();
      await stable();

      expect(api.putSpriteAtlases).toHaveBeenCalledWith(5, [
        { animation: 'turn', facing: '', image_id: 1, frame_width: 128, frame_height: 128, frame_count: 4, key_frames: [0, 2, 4, 6], fps: null },
        { animation: 'move', facing: 'sw', image_id: 2, frame_width: 128, frame_height: 128, frame_count: 8, key_frames: null, fps: 12 },
      ]);
      expect(el.querySelector('.success')?.textContent).toBe('Atlanten gespeichert.');
      expect(unitSprites.refresh).toHaveBeenCalled();
    });

    it('"fuer alle Richtungen" uebernimmt die Zeile nur innerhalb ihrer Animation', async () => {
      const el = await open(adminSprite({ images: [turnPng, movePng] }));
      const move = component.rows.find((row) => row.animation === 'move' && row.facing === 'sw')!;
      move.image_id = 2;
      move.frame_width = 64;
      move.frame_count = 6;
      move.fps = 8;

      const moveHeader = Array.from(el.querySelectorAll('tbody tr')).find((row) => text(row).includes('Ziehen'))!;
      moveHeader.querySelector<HTMLButtonElement>('button.link')!.click();
      await stable();

      const others = component.rows.filter((row) => row.animation === 'move');
      expect(others.map((row) => [row.facing, row.image_id, row.frame_width, row.frame_count, row.fps])).toEqual([
        ['sw', 2, 64, 6, 8],
        ['se', 2, 64, 6, 8],
        ['ne', 2, 64, 6, 8],
        ['nw', 2, 64, 6, 8],
      ]);
      expect(component.rows.filter((row) => row.animation === 'fire').every((row) => row.image_id === null)).toBe(true);
    });

    it('kopiert die Schluesselbilder, statt das Array zu teilen', async () => {
      await open();
      const [sw, se] = component.rows.filter((row) => row.animation === 'aim');
      sw.key_frames = [3, 2, 1, 0];
      component.copyToAll(sw);
      sw.key_frames[0] = 9;
      expect(se.key_frames).toEqual([3, 2, 1, 0]);
    });
  });

  it('speichert die Werte und macht aus leeren Zeiten null', async () => {
    api.updateSprite.mockReturnValue(of(adminSprite()));
    const el = await open(adminSprite({ step_ms: 200, turn_ms: 400 }));
    component.values.anchor_x = '0.4' as unknown as number;
    component.values.step_ms = '' as unknown as number;
    component.values.own_shadow = true;

    button(el, 'Werte speichern').click();
    await stable();

    expect(api.updateSprite).toHaveBeenCalledWith(5, {
      anchor_x: 0.4,
      anchor_y: 0.8,
      draw_width: 128,
      own_shadow: true,
      step_ms: null,
      turn_ms: 400,
      aim_ms: null,
      aim_hold_ms: null,
    });
    expect(el.querySelector('.success')?.textContent).toBe('Werte gespeichert.');
  });

  it('behaelt Eingaben in der Atlas-Tabelle, wenn die Werte gespeichert werden', async () => {
    api.updateSprite.mockReturnValue(of(adminSprite({ draw_width: 200 })));
    const el = await open();
    component.rows[0].frame_width = 77;

    button(el, 'Werte speichern').click();
    await stable();

    expect(component.rows[0].frame_width).toBe(77);
  });

  describe('PNGs', () => {
    it('laedt fuer jede PNG eine Vorschau', async () => {
      const el = await open(adminSprite({ images: [turnPng, movePng] }));
      expect(api.spriteImageBlob).toHaveBeenCalledWith(5, 1);
      expect(api.spriteImageBlob).toHaveBeenCalledWith(5, 2);
      expect(el.querySelectorAll('.thumb img').length).toBe(2);
    });

    it('laedt mehrere PNGs nacheinander hoch', async () => {
      const files = [new File(['a'], 'a.png'), new File(['b'], 'b.png')];
      api.uploadSpriteImage
        .mockReturnValueOnce(of(adminSprite({ images: [turnPng] })))
        .mockReturnValueOnce(of(adminSprite({ images: [turnPng, movePng] })));
      const el = await open();

      chooseFiles(el.querySelector<HTMLInputElement>('input[type=file][multiple]')!, files);
      await stable();

      expect(api.uploadSpriteImage.mock.calls).toEqual([
        [5, files[0]],
        [5, files[1]],
      ]);
      expect(el.querySelector('.success')?.textContent).toBe('2 von 2 PNGs hochgeladen.');
      expect(text(el)).toContain('move.png');
    });

    it('hoert beim ersten Fehler auf und nennt die Datei', async () => {
      const files = [new File(['a'], 'a.png'), new File(['b'], 'b.png')];
      api.uploadSpriteImage.mockReturnValueOnce(throwError(() => new HttpErrorResponse({ status: 400 })));
      const el = await open();

      chooseFiles(el.querySelector<HTMLInputElement>('input[type=file][multiple]')!, files);
      await stable();

      expect(api.uploadSpriteImage).toHaveBeenCalledTimes(1);
      expect(el.querySelector('p.error')?.textContent).toBe('a.png nicht hochgeladen.');
    });

    it('gibt die Vorschau einer ersetzten PNG frei', async () => {
      api.replaceSpriteImage.mockReturnValue(of(adminSprite({ images: [{ ...turnPng, sha256: 'neu' }] })));
      const el = await open(adminSprite({ images: [turnPng] }));
      const oldUrl = component.previews()['1:a'];

      const replaceInput = el.querySelector<HTMLInputElement>('.image-actions input[type=file]')!;
      chooseFiles(replaceInput, [new File(['x'], 'neu.png')]);
      await stable();

      expect(api.replaceSpriteImage).toHaveBeenCalledWith(5, 1, expect.any(File));
      expect(URL.revokeObjectURL).toHaveBeenCalledWith(oldUrl);
      expect(Object.keys(component.previews())).toEqual(['1:neu']);
      expect(el.querySelector('.success')?.textContent).toBe('turn.png ersetzt.');
    });

    it('loescht eine PNG nach Rueckfrage und laedt das Bild neu', async () => {
      vi.spyOn(window, 'confirm').mockReturnValue(true);
      api.deleteSpriteImage.mockReturnValue(of(undefined));
      const el = await open(adminSprite({ images: [turnPng] }));
      api.getSprite.mockReturnValue(of(adminSprite({ images: [] })));

      el.querySelector<HTMLButtonElement>('.image-actions button')!.click();
      await stable();

      expect(window.confirm).toHaveBeenCalledWith('turn.png wirklich löschen?');
      expect(api.deleteSpriteImage).toHaveBeenCalledWith(5, 1);
      expect(text(el)).toContain('Noch keine PNG.');
      expect(el.querySelector('.success')?.textContent).toBe('turn.png gelöscht.');
    });
  });

  it('loescht das ganze Bild und kehrt zur Liste zurueck', async () => {
    vi.spyOn(window, 'confirm').mockReturnValue(true);
    api.deleteSprite.mockReturnValue(of(undefined));
    const el = await open();

    button(el, 'Löschen').click();
    await stable();

    expect(window.confirm).toHaveBeenCalledWith('Einheitenbild „heavy_tank“ mit allen PNGs wirklich löschen?');
    expect(unitSprites.refresh).toHaveBeenCalled();
    expect(TestBed.inject(Router).url).toBe('/games/admin/sprites');
  });

  it('gibt beim Verlassen alle Vorschauen frei', async () => {
    await open(adminSprite({ images: [turnPng, movePng] }));
    const urls = Object.values(component.previews());
    harness.fixture.destroy();
    for (const url of urls) {
      expect(URL.revokeObjectURL).toHaveBeenCalledWith(url);
    }
  });

  it('fordert eine noch ladende Vorschau beim naechsten Upload nicht erneut an', async () => {
    // Die Vorschau von turn.png antwortet erst, wenn der zweite Upload durch ist.
    const turnPreview = new Subject<Blob>();
    api.spriteImageBlob.mockImplementation((_id: number, imageId: number) =>
      imageId === 1 ? turnPreview : of(new Blob(['png'])),
    );
    const files = [new File(['a'], 'turn.png'), new File(['b'], 'move.png')];
    api.uploadSpriteImage
      .mockReturnValueOnce(of(adminSprite({ images: [turnPng] })))
      .mockReturnValueOnce(of(adminSprite({ images: [turnPng, movePng] })));
    const el = await open();

    chooseFiles(el.querySelector<HTMLInputElement>('input[type=file][multiple]')!, files);
    await stable();
    turnPreview.next(new Blob(['png']));
    turnPreview.complete();
    await stable();

    expect(api.spriteImageBlob.mock.calls).toEqual([
      [5, 1],
      [5, 2],
    ]);
    expect(URL.createObjectURL).toHaveBeenCalledTimes(2);
    expect(el.querySelectorAll('.thumb img').length).toBe(2);
  });
});
