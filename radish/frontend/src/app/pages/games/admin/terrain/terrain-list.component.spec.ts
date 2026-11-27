import { HttpErrorResponse } from '@angular/common/http';
import { ComponentFixture, TestBed } from '@angular/core/testing';
import { Router, provideRouter } from '@angular/router';
import { of, throwError } from 'rxjs';

import { terrainType } from '../../../../../testing/admin-media';
import { ApiService } from '../../../../core/api.service';
import { TerrainListComponent, terrainLookSummary } from './terrain-list.component';

describe('TerrainListComponent', () => {
  let listTerrainTypes: ReturnType<typeof vi.fn>;
  let fixture: ComponentFixture<TerrainListComponent>;

  const decor = { image_id: 1, offset_x: 0, offset_y: 0, in_front: false };
  const types = [
    terrainType({ code: 'grass', label: 'Wiese', movement_modifier: 0, cover: 0, blocks_sight: false }),
    terrainType({ code: 'forest', label: 'Wald', image_id: 1, decor: [decor] }),
    terrainType({ code: 'dragons_teeth', label: 'Panzersperre', impassable: true, blocks_sight: false, decor: [decor, decor] }),
  ];

  async function render(): Promise<HTMLElement> {
    TestBed.configureTestingModule({
      imports: [TerrainListComponent],
      providers: [provideRouter([]), { provide: ApiService, useValue: { listTerrainTypes } }],
    });
    fixture = TestBed.createComponent(TerrainListComponent);
    await fixture.whenStable();
    return fixture.nativeElement as HTMLElement;
  }

  const rows = (el: HTMLElement) =>
    Array.from(el.querySelectorAll('tbody tr')).map((row) =>
      Array.from(row.querySelectorAll('td')).map((cell) => cell.textContent?.replace(/\s+/g, ' ').trim()),
    );

  async function search(el: HTMLElement, term: string): Promise<void> {
    const input = el.querySelector<HTMLInputElement>('input')!;
    input.value = term;
    input.dispatchEvent(new Event('input'));
    await fixture.whenStable();
  }

  beforeEach(() => {
    listTerrainTypes = vi.fn(() => of(types));
  });

  it('zeigt Werte, Eigenschaften und Aussehen je Typ', async () => {
    const el = await render();
    expect(rows(el)).toEqual([
      ['Wiesegrass', '0', '0', '–', 'Würfel in Typfarbe'],
      ['Waldforest', '-1', '2', 'sichtblockierend', 'Grundbild, 1 Dekoration'],
      ['Panzersperredragons_teeth', '-1', '2', 'unbegehbar', '2 Dekorationen'],
    ]);
    expect(el.querySelector('.count')?.textContent?.trim()).toBe('3 Typen');
  });

  it('sucht in Bezeichnung (ohne Gross-/Kleinschreibung) und Typname', async () => {
    const el = await render();

    await search(el, 'WALD');
    expect(rows(el).map((row) => row[0])).toEqual(['Waldforest']);
    expect(el.querySelector('.count')?.textContent?.replace(/\s+/g, ' ').trim()).toBe('1 von 3 Typen');

    await search(el, 'teeth');
    expect(rows(el).map((row) => row[0])).toEqual(['Panzersperredragons_teeth']);

    await search(el, 'xyz');
    expect(el.textContent).toContain('Keine Treffer.');
  });

  it('oeffnet einen Typ per Klick auf die Zeile', async () => {
    const el = await render();
    const navigate = vi.spyOn(TestBed.inject(Router), 'navigate').mockResolvedValue(true);
    el.querySelectorAll<HTMLElement>('tbody tr')[1].click();
    expect(navigate).toHaveBeenCalledWith(['/games/admin/terrain', 'forest']);
  });

  it('zeigt den Ladefehler', async () => {
    listTerrainTypes.mockReturnValue(throwError(() => new HttpErrorResponse({ status: 500 })));
    const el = await render();
    expect(el.querySelector('.error')?.textContent).toBe('Gelände konnte nicht geladen werden.');
  });

  it.each([
    [null, 0, 'Würfel in Typfarbe'],
    [null, 1, '1 Dekoration'],
    [1, 0, 'Grundbild'],
    [1, 2, 'Grundbild, 2 Dekorationen'],
  ])('terrainLookSummary: Bild %s, %i Dekorationen -> %s', (imageId, decorCount, expected) => {
    const entry = terrainType({ image_id: imageId, decor: Array.from({ length: decorCount }, () => decor) });
    expect(terrainLookSummary(entry)).toBe(expected);
  });
});
