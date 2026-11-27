import { HttpErrorResponse } from '@angular/common/http';
import { Component, signal, viewChild } from '@angular/core';
import { ComponentFixture, TestBed } from '@angular/core/testing';
import { Subject, of, throwError } from 'rxjs';

import { pngImage, stubObjectUrls, terrainType } from '../../../../../testing/admin-media';
import { AdminTerrainType, ApiService } from '../../../../core/api.service';
import { TerrainLookComponent } from './terrain-look.component';

type Api = Record<
  'setTerrainLook' | 'uploadTerrainImage' | 'replaceTerrainImage' | 'deleteTerrainImage' | 'terrainImageBlob',
  ReturnType<typeof vi.fn>
>;

const tile = pngImage({ id: 1, name: 'tile.png', sha256: 'a' });
const bush = pngImage({ id: 2, name: 'bush.png', sha256: 'b' });

describe('TerrainLookComponent', () => {
  let api: Api;
  let fixture: ComponentFixture<TerrainLookComponent>;
  let component: TerrainLookComponent;
  let updated: AdminTerrainType[];

  async function render(terrain: AdminTerrainType): Promise<HTMLElement> {
    TestBed.configureTestingModule({
      imports: [TerrainLookComponent],
      providers: [{ provide: ApiService, useValue: api }],
    });
    fixture = TestBed.createComponent(TerrainLookComponent);
    component = fixture.componentInstance;
    updated = [];
    // Wie die Detailseite: den Typ nach jeder Aenderung zurueckreichen.
    component.updated.subscribe((next) => {
      updated.push(next);
      fixture.componentRef.setInput('terrain', next);
    });
    fixture.componentRef.setInput('terrain', terrain);
    await fixture.whenStable();
    return fixture.nativeElement as HTMLElement;
  }

  const stable = () => fixture.whenStable();
  /** Nach direkten Aenderungen an Feldern der Komponente neu zeichnen (ohne zone.js). */
  async function redraw(): Promise<void> {
    fixture.componentRef.changeDetectorRef.markForCheck();
    await fixture.whenStable();
  }
  const button = (el: HTMLElement, label: string) =>
    Array.from(el.querySelectorAll('button')).find((b) => b.textContent?.trim() === label);

  beforeEach(() => {
    api = {
      setTerrainLook: vi.fn(),
      uploadTerrainImage: vi.fn(),
      replaceTerrainImage: vi.fn(),
      deleteTerrainImage: vi.fn(),
      terrainImageBlob: vi.fn(() => of(new Blob(['png']))),
    };
    stubObjectUrls();
  });

  afterEach(() => vi.restoreAllMocks());

  it('uebernimmt Bild und Dekorationen aus dem gespeicherten Stand', async () => {
    await render(
      terrainType({ image_id: 1, images: [tile, bush], decor: [{ image_id: 2, offset_x: 10, offset_y: -5, in_front: true }] }),
    );
    expect(component.image_id).toBe(1);
    expect(component.decor).toEqual([{ image_id: 2, offset_x: 10, offset_y: -5, in_front: true }]);
    expect(component.frontDecor().length).toBe(1);
    expect(component.backDecor().length).toBe(0);
    expect(component.changed()).toBe(false);
  });

  it('laedt je PNG eine Vorschau, auch wenn derselbe Typ mehrfach ankommt', async () => {
    const blob = new Subject<Blob>();
    api.terrainImageBlob.mockReturnValue(blob);
    await render(terrainType({ images: [tile] }));

    // Vor der Antwort kommt derselbe Typ noch einmal (z.B. nach dem Speichern).
    fixture.componentRef.setInput('terrain', terrainType({ images: [tile] }));
    await stable();
    expect(api.terrainImageBlob).toHaveBeenCalledTimes(1);

    blob.next(new Blob(['png']));
    blob.complete();
    expect(component.imageUrl(1)).toBe('blob:preview-1');
  });

  it('verwirft Vorschauen eines anderen Typs, die erst nach dem Wechsel ankommen', async () => {
    const late = new Subject<Blob>();
    api.terrainImageBlob.mockReturnValueOnce(late);
    await render(terrainType({ code: 'forest', images: [tile] }));

    fixture.componentRef.setInput('terrain', terrainType({ code: 'grass', images: [] }));
    await stable();
    late.next(new Blob(['png']));

    expect(component.previews()).toEqual({});
  });

  describe('Dekorationen', () => {
    it('erlaubt hoechstens zwei und braucht vor dem Speichern je eine PNG', async () => {
      const el = await render(terrainType({ images: [tile, bush] }));

      button(el, 'Dekoration hinzufügen')!.click();
      await stable();
      expect(component.changed()).toBe(true);
      expect(component.complete()).toBe(false);
      expect(button(el, 'Aussehen speichern')!.disabled).toBe(true);

      button(el, 'Dekoration hinzufügen')!.click();
      await stable();
      expect(button(el, 'Dekoration hinzufügen')).toBeUndefined();

      component.decor[0].image_id = 2;
      component.decor[1].image_id = 1;
      expect(component.complete()).toBe(true);
    });

    it.each([
      [128, true],
      [-128, true],
      [129, false],
      [1.5, false],
    ])('Versatz %s ist %s', async (offset, ok) => {
      await render(terrainType({ images: [bush] }));
      component.addDecor();
      component.decor[0].image_id = 2;
      component.decor[0].offset_x = offset;
      expect(component.complete()).toBe(ok);
    });

    it('ist ohne PNGs nicht hinzufuegbar', async () => {
      const el = await render(terrainType({ images: [] }));
      expect(button(el, 'Dekoration hinzufügen')!.disabled).toBe(true);
    });
  });

  it('speichert das Aussehen und meldet den neuen Typ nach oben', async () => {
    const saved = terrainType({ image_id: 1, images: [tile, bush], decor: [{ image_id: 2, offset_x: 4, offset_y: 0, in_front: false }] });
    api.setTerrainLook.mockReturnValue(of(saved));
    const el = await render(terrainType({ images: [tile, bush] }));
    component.image_id = 1;
    component.addDecor();
    component.decor[0].image_id = 2;
    component.decor[0].offset_x = 4;
    await redraw();

    button(el, 'Aussehen speichern')!.click();
    await stable();

    expect(api.setTerrainLook).toHaveBeenCalledWith('forest', {
      image_id: 1,
      decor: [{ image_id: 2, offset_x: 4, offset_y: 0, in_front: false }],
    });
    expect(updated).toEqual([saved]);
    expect(el.querySelector('.notice')?.textContent).toBe('Aussehen gespeichert.');
    expect(component.changed()).toBe(false);
  });

  it('zeigt Fehler beim Speichern und behaelt die Eingaben', async () => {
    api.setTerrainLook.mockReturnValue(
      throwError(() => new HttpErrorResponse({ status: 400, error: { image_id: ['Gehört nicht zu diesem Typ.'] } })),
    );
    const el = await render(terrainType({ images: [tile] }));
    component.image_id = 1;
    await redraw();

    button(el, 'Aussehen speichern')!.click();
    await stable();

    expect(el.querySelector('.error')?.textContent).toBe('Gehört nicht zu diesem Typ.');
    expect(component.image_id).toBe(1);
    expect(updated).toEqual([]);
  });

  it('verwirft Aenderungen am Aussehen', async () => {
    const el = await render(terrainType({ image_id: 1, images: [tile] }));
    component.image_id = null;
    await redraw();

    button(el, 'Verwerfen')!.click();
    await stable();

    expect(component.image_id).toBe(1);
    expect(button(el, 'Verwerfen')).toBeUndefined();
  });

  it('loescht eine PNG nach Rueckfrage und gibt ihre Vorschau frei', async () => {
    vi.spyOn(window, 'confirm').mockReturnValue(true);
    api.deleteTerrainImage.mockReturnValue(of(undefined));
    const el = await render(terrainType({ images: [tile, bush] }));
    const bushUrl = component.imageUrl(2);

    const bushItem = Array.from(el.querySelectorAll('li')).find((li) => li.textContent?.includes('bush.png'))!;
    Array.from(bushItem.querySelectorAll('button')).find((b) => b.textContent?.trim() === 'Löschen')!.click();
    await stable();

    expect(api.deleteTerrainImage).toHaveBeenCalledWith('forest', 2);
    expect(updated.at(-1)?.images).toEqual([tile]);
    expect(URL.revokeObjectURL).toHaveBeenCalledWith(bushUrl);
    expect(el.querySelector('.notice')?.textContent).toBe('bush.png gelöscht.');
  });

  it('laedt mehrere PNGs nacheinander hoch', async () => {
    api.uploadTerrainImage
      .mockReturnValueOnce(of(terrainType({ images: [tile] })))
      .mockReturnValueOnce(of(terrainType({ images: [tile, bush] })));
    const el = await render(terrainType());
    const input = el.querySelector<HTMLInputElement>('input[type=file][multiple]')!;
    const files = [new File(['a'], 'tile.png'), new File(['b'], 'bush.png')];
    Object.defineProperty(input, 'files', { value: files, configurable: true });

    input.dispatchEvent(new Event('change'));
    await stable();

    expect(api.uploadTerrainImage.mock.calls).toEqual([
      ['forest', files[0]],
      ['forest', files[1]],
    ]);
    expect(el.querySelector('.notice')?.textContent).toBe('2 von 2 PNGs hochgeladen.');
  });

  describe('in der Detailseite eingebunden', () => {
    /**
     * Reicht den Typ wie TerrainDetailComponent ueber ein Signal und die
     * Template-Bindung zurueck -- also erst beim naechsten Neuzeichnen, nicht
     * sofort wie setInput() in den Tests oben.
     */
    @Component({
      imports: [TerrainLookComponent],
      template: `<app-terrain-look [terrain]="terrain()" (updated)="terrain.set($event)" />`,
    })
    class HostPage {
      readonly terrain = signal(terrainType({ images: [tile, bush] }));
      readonly look = viewChild.required(TerrainLookComponent);
    }

    it('zeigt nach dem Speichern die gespeicherten Werte, nicht die alten', async () => {
      const before = terrainType({
        images: [tile, bush],
        decor: [{ image_id: 2, offset_x: 6, offset_y: -86, in_front: false }],
      });
      const saved = terrainType({
        images: [tile, bush],
        decor: [{ image_id: 2, offset_x: 10, offset_y: -80, in_front: true }],
      });
      api.setTerrainLook.mockReturnValue(of(saved));
      TestBed.configureTestingModule({ imports: [HostPage], providers: [{ provide: ApiService, useValue: api }] });
      const host = TestBed.createComponent(HostPage);
      host.componentInstance.terrain.set(before);
      await host.whenStable();
      const look = host.componentInstance.look();

      look.decor[0] = { image_id: 2, offset_x: 10, offset_y: -80, in_front: true };
      look.saveLook();
      await host.whenStable();

      expect(look.decor).toEqual([{ image_id: 2, offset_x: 10, offset_y: -80, in_front: true }]);
      expect(look.changed()).toBe(false);
    });
  });
});
