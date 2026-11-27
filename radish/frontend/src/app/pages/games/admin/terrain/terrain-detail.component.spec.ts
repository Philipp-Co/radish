import { HttpErrorResponse } from '@angular/common/http';
import { TestBed } from '@angular/core/testing';
import { provideRouter } from '@angular/router';
import { RouterTestingHarness } from '@angular/router/testing';
import { of, throwError } from 'rxjs';

import { pngImage, stubObjectUrls, terrainType } from '../../../../../testing/admin-media';
import { AdminTerrainType, ApiService } from '../../../../core/api.service';
import { TerrainDetailComponent } from './terrain-detail.component';

describe('TerrainDetailComponent', () => {
  let api: Record<'getTerrainType' | 'updateTerrainType' | 'terrainImageBlob' | 'setTerrainLook', ReturnType<typeof vi.fn>>;
  let harness: RouterTestingHarness;

  async function open(terrain: AdminTerrainType = terrainType()): Promise<HTMLElement> {
    api.getTerrainType.mockReturnValue(of(terrain));
    TestBed.configureTestingModule({
      providers: [
        provideRouter([{ path: 'games/admin/terrain/:code', component: TerrainDetailComponent }]),
        { provide: ApiService, useValue: api },
      ],
    });
    harness = await RouterTestingHarness.create();
    await harness.navigateByUrl(`/games/admin/terrain/${terrain.code}`, TerrainDetailComponent);
    return harness.routeNativeElement!;
  }

  const stable = () => harness.fixture.whenStable();
  const saveButton = (el: HTMLElement) => el.querySelector<HTMLButtonElement>('form.values button[type=submit]')!;
  const discardButton = (el: HTMLElement) =>
    Array.from(el.querySelectorAll<HTMLButtonElement>('form.values button')).find((b) => b.textContent?.trim() === 'Verwerfen');

  async function setNumber(el: HTMLElement, name: string, value: string): Promise<void> {
    const input = el.querySelector<HTMLInputElement>(`input[name=${name}]`)!;
    input.value = value;
    input.dispatchEvent(new Event('input'));
    await stable();
  }

  async function toggle(el: HTMLElement, name: string): Promise<void> {
    el.querySelector<HTMLInputElement>(`input[name=${name}]`)!.click();
    await stable();
  }

  beforeEach(() => {
    api = {
      getTerrainType: vi.fn(),
      updateTerrainType: vi.fn(),
      terrainImageBlob: vi.fn(() => of(new Blob(['png']))),
      setTerrainLook: vi.fn(),
    };
    stubObjectUrls();
  });

  it('zeigt den Typ mit seinen Werten, Speichern erst nach einer Aenderung', async () => {
    const el = await open();

    expect(el.querySelector('h2')?.textContent?.replace(/\s+/g, ' ').trim()).toBe('Wald forest · Würfel in Typfarbe');
    expect(api.getTerrainType).toHaveBeenCalledWith('forest');
    await stable();
    expect(el.querySelector<HTMLInputElement>('input[name=movement_modifier]')!.value).toBe('-1');
    expect(el.querySelector<HTMLInputElement>('input[name=blocks_sight]')!.checked).toBe(true);
    expect(saveButton(el).disabled).toBe(true);
    expect(discardButton(el)).toBeUndefined();
  });

  it('speichert geaenderte Werte und meldet es', async () => {
    api.updateTerrainType.mockReturnValue(of(terrainType({ cover: 3, impassable: true })));
    const el = await open();

    await setNumber(el, 'cover', '3');
    await toggle(el, 'impassable');
    expect(saveButton(el).disabled).toBe(false);

    saveButton(el).click();
    await stable();

    expect(api.updateTerrainType).toHaveBeenCalledWith('forest', {
      movement_modifier: -1,
      cover: 3,
      impassable: true,
      blocks_sight: true,
    });
    expect(el.querySelector('.success')?.textContent).toBe('Werte gespeichert.');
    expect(saveButton(el).disabled).toBe(true);
  });

  it('verwirft Aenderungen', async () => {
    const el = await open();
    await toggle(el, 'blocks_sight');

    discardButton(el)!.click();
    await stable();

    expect(el.querySelector<HTMLInputElement>('input[name=blocks_sight]')!.checked).toBe(true);
    expect(saveButton(el).disabled).toBe(true);
  });

  it('nennt bei einem Fehler das Feld mit seinem Kuerzel', async () => {
    api.updateTerrainType.mockReturnValue(
      throwError(() => new HttpErrorResponse({ status: 400, error: { cover: ['Höchstens 10.'] } })),
    );
    const el = await open();
    await setNumber(el, 'cover', '11');

    saveButton(el).click();
    await stable();

    expect(el.querySelector('form.values ~ .error')?.textContent).toBe('CV: Höchstens 10.');
    // Die Eingabe bleibt stehen.
    expect(el.querySelector<HTMLInputElement>('input[name=cover]')!.value).toBe('11');
  });

  it('uebernimmt ein neues Aussehen, ohne die Eingaben der Werte zu verwerfen', async () => {
    const png = pngImage({ id: 4, name: 'forest.png' });
    api.setTerrainLook.mockReturnValue(of(terrainType({ image_id: 4, images: [png] })));
    const el = await open(terrainType({ images: [png] }));
    await setNumber(el, 'cover', '5');

    const select = el.querySelector<HTMLSelectElement>('app-terrain-look select')!;
    select.value = select.options[1].value;
    select.dispatchEvent(new Event('change'));
    await stable();
    Array.from(el.querySelectorAll<HTMLButtonElement>('app-terrain-look button'))
      .find((b) => b.textContent?.trim() === 'Aussehen speichern')!
      .click();
    await stable();

    expect(el.querySelector('h2')?.textContent).toContain('Grundbild');
    expect(el.querySelector<HTMLInputElement>('input[name=cover]')!.value).toBe('5');
  });

  it('zeigt den Ladefehler mit dem Typnamen', async () => {
    api.getTerrainType.mockReturnValue(throwError(() => new HttpErrorResponse({ status: 404 })));
    TestBed.configureTestingModule({
      providers: [
        provideRouter([{ path: 'games/admin/terrain/:code', component: TerrainDetailComponent }]),
        { provide: ApiService, useValue: api },
      ],
    });
    harness = await RouterTestingHarness.create();
    await harness.navigateByUrl('/games/admin/terrain/lava');
    expect(harness.routeNativeElement!.querySelector('.error')?.textContent).toBe(
      'Geländetyp „lava“ konnte nicht geladen werden.',
    );
  });
});
