import { HttpErrorResponse } from '@angular/common/http';
import { Component } from '@angular/core';
import { TestBed } from '@angular/core/testing';
import { Router, provideRouter } from '@angular/router';
import { RouterTestingHarness } from '@angular/router/testing';
import { Subject, of, throwError } from 'rxjs';

import { adminEffect, pngImage, stubObjectUrls } from '../../../../../testing/admin-media';
import { AdminEffect, AdminEffectAtlas, ApiService } from '../../../../core/api.service';
import { EffectEditComponent } from './effect-edit.component';

@Component({ template: '' })
class StubPage {}

type Api = Record<
  | 'getEffect'
  | 'updateEffect'
  | 'deleteEffect'
  | 'uploadEffectImage'
  | 'replaceEffectImage'
  | 'deleteEffectImage'
  | 'putEffectAtlases'
  | 'effectImageBlob',
  ReturnType<typeof vi.fn>
>;

/** 4 Spalten × 2 Zeilen zu je 128 × 128 Pixeln. */
const sheet = pngImage({ id: 1, name: 'boom.png', width: 512, height: 256, sha256: 'a' });

function atlas(overrides: Partial<AdminEffectAtlas> = {}): AdminEffectAtlas {
  return {
    facing: 'sw',
    image_id: 1,
    frame_width: 128,
    frame_height: 128,
    first_frame: 0,
    frame_count: 8,
    anchor_x: 0.5,
    anchor_y: 0.5,
    ...overrides,
  };
}

describe('EffectEditComponent', () => {
  let api: Api;
  let harness: RouterTestingHarness;
  let component: EffectEditComponent;

  async function open(effect: AdminEffect = adminEffect()): Promise<HTMLElement> {
    api.getEffect.mockReturnValue(of(effect));
    TestBed.configureTestingModule({
      providers: [
        provideRouter([
          { path: 'games/admin/effects/:id', component: EffectEditComponent },
          { path: 'games/admin/effects', component: StubPage },
        ]),
        { provide: ApiService, useValue: api },
      ],
    });
    harness = await RouterTestingHarness.create();
    component = await harness.navigateByUrl(`/games/admin/effects/${effect.id}`, EffectEditComponent);
    return harness.routeNativeElement!;
  }

  const stable = () => harness.fixture.whenStable();
  const button = (el: HTMLElement, label: string) =>
    Array.from(el.querySelectorAll('button')).find((b) => b.textContent?.trim() === label)!;

  beforeEach(() => {
    // Die Vorschau laeuft ueber requestAnimationFrame -- hier nur, wenn ein Test es will.
    vi.useFakeTimers({ toFake: ['requestAnimationFrame', 'cancelAnimationFrame'] });
    api = {
      getEffect: vi.fn(),
      updateEffect: vi.fn(),
      deleteEffect: vi.fn(),
      uploadEffectImage: vi.fn(),
      replaceEffectImage: vi.fn(),
      deleteEffectImage: vi.fn(),
      putEffectAtlases: vi.fn(),
      effectImageBlob: vi.fn(() => of(new Blob(['png']))),
    };
    stubObjectUrls();
  });

  afterEach(() => {
    vi.useRealTimers();
    vi.restoreAllMocks();
  });

  it('zeigt den Ladefehler', async () => {
    api.getEffect.mockReturnValue(throwError(() => new HttpErrorResponse({ status: 404 })));
    TestBed.configureTestingModule({
      providers: [
        provideRouter([{ path: 'games/admin/effects/:id', component: EffectEditComponent }]),
        { provide: ApiService, useValue: api },
      ],
    });
    harness = await RouterTestingHarness.create();
    await harness.navigateByUrl('/games/admin/effects/4');
    expect(harness.routeNativeElement!.querySelector('.error')?.textContent).toBe('Effekt konnte nicht geladen werden.');
  });

  it('belegt die vier Richtungen aus den Atlanten, fehlende mit Vorgaben', async () => {
    await open(adminEffect({ images: [sheet], atlases: [atlas({ facing: 'ne', first_frame: 8 })] }));
    expect(component.rows.map((row) => [row.facing, row.image_id, row.first_frame])).toEqual([
      ['sw', null, 0],
      ['se', null, 0],
      ['ne', 1, 8],
      ['nw', null, 0],
    ]);
  });

  describe('Vorschau (animationFrame)', () => {
    async function openWithSheet(overrides: Partial<AdminEffectAtlas> = {}) {
      const effect = adminEffect({ images: [sheet], atlases: [atlas(overrides)], draw_width: 256, duration_ms: 1000 });
      await open(effect);
      return { effect, row: component.rows[0] };
    }

    it('verkleinert, bis das Bild auf die Buehne passt, und schneidet Bild 0 aus', async () => {
      const { effect, row } = await openWithSheet();
      const shown = component.animationFrame(effect, row)!;

      // 288 / 2 / 256 -- das Bild muss bei jedem Anker auf die Buehne passen.
      expect(shown.scale).toBe(0.5625);
      expect(shown.frame).toBe(0);
      // Eine Zelle wird 256 * 0.5625 = 144 px breit gezeichnet, die PNG im selben Mass.
      expect(shown.cell).toEqual({
        url: 'blob:preview-1',
        width: 144,
        height: 144,
        size: '576px 288px',
        position: '0px 0px',
      });
    });

    it('rechnet das Bild aus der abgespielten Zeit, zeilenweise durch die PNG', async () => {
      const { effect, row } = await openWithSheet();

      component.elapsed.set(500);
      const half = component.animationFrame(effect, row)!;
      expect(half.frame).toBe(4);
      // Bild 4 ist die erste Zelle der zweiten Zeile.
      expect(half.cell.position).toBe('0px -144px');

      component.elapsed.set(999);
      expect(component.animationFrame(effect, row)!.frame).toBe(7);
    });

    it('zeigt in der Pause nach dem Durchlauf kein Bild und beginnt danach von vorn', async () => {
      const { effect, row } = await openWithSheet();

      component.elapsed.set(1100);
      expect(component.animationFrame(effect, row)!.frame).toBeNull();

      component.elapsed.set(1600 + 250);
      expect(component.animationFrame(effect, row)!.frame).toBe(2);
    });

    it('beginnt bei "Ab Zelle"', async () => {
      const { effect, row } = await openWithSheet({ first_frame: 2, frame_count: 4 });
      component.elapsed.set(500);
      // Bild 2 von 4 ab Zelle 2 ist Zelle 4: zweite Zeile, erste Spalte.
      expect(component.animationFrame(effect, row)!.cell.position).toBe('0px -144px');
    });

    it('ist null, wenn die Bilder nicht in die PNG passen', async () => {
      const { effect, row } = await openWithSheet({ frame_count: 9 });
      expect(component.animationFrame(effect, row)).toBeNull();
    });

    it.each([
      ['ohne PNG', { image_id: 99 }],
      ['mit Zelle breiter als die PNG', { frame_width: 600 }],
      ['ohne Bilder', { frame_count: 0 }],
    ])('ist null %s', async (_name, overrides) => {
      const { effect, row } = await openWithSheet(overrides);
      expect(component.animationFrame(effect, row)).toBeNull();
    });

    it('ist null ohne gueltige Dauer oder Breite', async () => {
      const { effect, row } = await openWithSheet();
      component.values.duration_ms = 0;
      expect(component.animationFrame(effect, row)).toBeNull();
      component.values.duration_ms = 1000;
      component.values.draw_width = '' as unknown as number;
      expect(component.animationFrame(effect, row)).toBeNull();
    });

    it('zaehlt die Zeit je Schritt hoechstens 100 ms weiter und steht beim Anhalten', async () => {
      const el = await open();
      vi.advanceTimersToNextFrame();
      vi.advanceTimersToNextFrame();
      const running = component.elapsed();
      expect(running).toBeGreaterThan(0);
      expect(running).toBeLessThanOrEqual(100);

      button(el, 'Anhalten').click();
      await stable();
      vi.advanceTimersToNextFrame();
      vi.advanceTimersToNextFrame();
      expect(component.elapsed()).toBe(running);
      expect(button(el, 'Abspielen')).toBeDefined();
    });

    it('zeichnet das getroffene Feld als Raute um die Buehnenmitte', async () => {
      await open();
      expect(component.tilePoints(1)).toBe('144,80 272,144 144,208 16,144');
      expect(component.tilePoints(0.5)).toBe('144,112 208,144 144,176 80,144');
    });
  });

  describe('Anker ziehen', () => {
    const pointer = (clientX: number, clientY: number) =>
      ({
        clientX,
        clientY,
        pointerId: 1,
        currentTarget: { setPointerCapture: vi.fn(), releasePointerCapture: vi.fn() },
      }) as unknown as PointerEvent;

    it('verschiebt den Anker gegenlaeufig zur Maus, gerundet auf Tausendstel', async () => {
      await open(adminEffect({ images: [sheet], atlases: [atlas()] }));
      const row = component.rows[0];
      const cell = { url: '', width: 72, height: 72, size: '', position: '' };

      component.startAnchor(row, cell, pointer(100, 100));
      component.moveAnchor(row, pointer(118, 91));

      expect(row.anchor_x).toBe(0.25);
      expect(row.anchor_y).toBe(0.625);
    });

    it('bleibt zwischen 0 und 1 und bewegt nur die gezogene Richtung', async () => {
      await open(adminEffect({ images: [sheet], atlases: [atlas()] }));
      const [row, other] = component.rows;
      const cell = { url: '', width: 72, height: 72, size: '', position: '' };

      component.startAnchor(row, cell, pointer(100, 100));
      component.moveAnchor(row, pointer(-500, 900));
      component.moveAnchor(other, pointer(0, 0));

      expect([row.anchor_x, row.anchor_y]).toEqual([1, 0]);
      expect([other.anchor_x, other.anchor_y]).toEqual([0.5, 0.5]);

      component.endAnchor(pointer(0, 0));
      component.moveAnchor(row, pointer(100, 100));
      expect([row.anchor_x, row.anchor_y]).toEqual([1, 0]);
    });
  });

  describe('fuer alle Richtungen uebernehmen', () => {
    it('kopiert die erste Richtung in die uebrigen', async () => {
      const el = await open(adminEffect({ images: [sheet], atlases: [atlas({ first_frame: 2, frame_count: 2, anchor_x: 0.4 })] }));

      el.querySelector<HTMLButtonElement>('.copy button')!.click();
      await stable();

      expect(component.rows.map((row) => [row.image_id, row.first_frame, row.frame_count, row.anchor_x])).toEqual([
        [1, 2, 2, 0.4],
        [1, 2, 2, 0.4],
        [1, 2, 2, 0.4],
        [1, 2, 2, 0.4],
      ]);
    });

    it('legt die Richtungen mit "hintereinander" in derselben PNG nacheinander', async () => {
      const el = await open(adminEffect({ images: [sheet], atlases: [atlas({ first_frame: 0, frame_count: 2 })] }));
      // Aus dem Eingabefeld kann ein Text kommen.
      component.rows[0].frame_count = '2' as unknown as number;

      el.querySelectorAll<HTMLButtonElement>('.copy button')[1].click();
      await stable();

      expect(component.rows.map((row) => row.first_frame)).toEqual([0, 2, 4, 6]);
    });
  });

  it('speichert nur belegte Richtungen, Zahlen aus den Feldern als Zahlen', async () => {
    api.putEffectAtlases.mockReturnValue(of(adminEffect({ images: [sheet] })));
    const el = await open(adminEffect({ images: [sheet] }));
    const [sw] = component.rows;
    sw.image_id = 1;
    sw.frame_count = '6' as unknown as number;
    sw.anchor_y = '0.75' as unknown as number;

    button(el, 'Atlanten speichern').click();
    await stable();

    expect(api.putEffectAtlases).toHaveBeenCalledWith(3, [
      { facing: 'sw', image_id: 1, frame_width: 128, frame_height: 128, first_frame: 0, frame_count: 6, anchor_x: 0.5, anchor_y: 0.75 },
    ]);
    expect(el.querySelector('.success')?.textContent).toBe('Atlanten gespeichert.');
  });

  it('speichert die Werte', async () => {
    api.updateEffect.mockReturnValue(of(adminEffect()));
    const el = await open();
    component.values.draw_width = '320' as unknown as number;

    button(el, 'Werte speichern').click();
    await stable();

    expect(api.updateEffect).toHaveBeenCalledWith(3, { kind: 'explosion', draw_width: 320, duration_ms: 1000 });
  });

  it('zeigt Fehler beim Speichern der Atlanten aus dem Backend', async () => {
    api.putEffectAtlases.mockReturnValue(
      throwError(() => new HttpErrorResponse({ status: 400, error: [{ first_frame: ['Passt nicht in die PNG.'] }] })),
    );
    const el = await open(adminEffect({ images: [sheet] }));
    component.rows[0].image_id = 1;

    button(el, 'Atlanten speichern').click();
    await stable();

    expect(el.querySelector('p.error')?.textContent).toBe('Passt nicht in die PNG.');
  });

  it('loescht den Effekt nach Rueckfrage', async () => {
    vi.spyOn(window, 'confirm').mockReturnValue(true);
    api.deleteEffect.mockReturnValue(of(undefined));
    const el = await open();

    el.querySelector<HTMLButtonElement>('.header button.danger')!.click();
    await stable();

    expect(window.confirm).toHaveBeenCalledWith('Effekt „explosion_small“ mit allen PNGs wirklich löschen?');
    expect(TestBed.inject(Router).url).toBe('/games/admin/effects');
  });

  it('beendet die Vorschau-Schleife beim Verlassen', async () => {
    await open();
    vi.advanceTimersToNextFrame();
    vi.advanceTimersToNextFrame();
    harness.fixture.destroy();
    const stopped = component.elapsed();

    vi.advanceTimersToNextFrame();
    vi.advanceTimersToNextFrame();
    expect(component.elapsed()).toBe(stopped);
  });

  it('fordert eine noch ladende Vorschau beim naechsten Upload nicht erneut an', async () => {
    const second = pngImage({ id: 2, name: 'boom_2.png', sha256: 'b' });
    // Die Vorschau der ersten PNG antwortet erst, wenn der zweite Upload durch ist.
    const firstPreview = new Subject<Blob>();
    api.effectImageBlob.mockImplementation((_id: number, imageId: number) =>
      imageId === 1 ? firstPreview : of(new Blob(['png'])),
    );
    const files = [new File(['a'], 'boom.png'), new File(['b'], 'boom_2.png')];
    api.uploadEffectImage
      .mockReturnValueOnce(of(adminEffect({ images: [sheet] })))
      .mockReturnValueOnce(of(adminEffect({ images: [sheet, second] })));
    const el = await open();

    const input = el.querySelector<HTMLInputElement>('input[type=file][multiple]')!;
    Object.defineProperty(input, 'files', { value: files, configurable: true });
    input.dispatchEvent(new Event('change'));
    await stable();
    firstPreview.next(new Blob(['png']));
    firstPreview.complete();
    await stable();

    expect(api.effectImageBlob.mock.calls).toEqual([
      [3, 1],
      [3, 2],
    ]);
    expect(URL.createObjectURL).toHaveBeenCalledTimes(2);
  });
});
