import { HttpErrorResponse } from '@angular/common/http';
import { TestBed } from '@angular/core/testing';
import { Router, provideRouter } from '@angular/router';
import { RouterTestingHarness } from '@angular/router/testing';
import { of, throwError } from 'rxjs';

import { adminProfile, adminUnitType, adminWeapon } from '../../../../../testing/catalog';
import { AdminWeapon, ApiService, CatalogResource } from '../../../../core/api.service';
import { UnitSpriteService } from '../../../../core/unit-sprite.service';
import { CatalogEditComponent } from './catalog-edit.component';

const BASE = 'games/admin/catalog';

const rifle = adminWeapon({ id: 1, name: 'Gewehr', species_name: 'Menschen' });
const cannon = adminWeapon({ id: 2, name: 'Kanone', weapon_class: 'heavy', in_use: true });
const slugga = adminWeapon({ id: 3, name: 'Wumme', species_id: 2, species_name: 'Orks' });

describe('CatalogEditComponent', () => {
  let api: Record<
    'listCatalogEntries' | 'getCatalogEntry' | 'updateCatalogEntry' | 'patchCatalogEntry' | 'deleteCatalogEntry',
    ReturnType<typeof vi.fn>
  >;
  let harness: RouterTestingHarness;

  async function open(url: string): Promise<HTMLElement> {
    TestBed.configureTestingModule({
      providers: [
        provideRouter(
          (['weapons', 'unit-types'] as CatalogResource[]).map((resource) => ({
            path: `${BASE}/${resource}/edit`,
            component: CatalogEditComponent,
            data: { resource },
          })),
        ),
        { provide: ApiService, useValue: api },
        {
          provide: UnitSpriteService,
          useValue: { keys: vi.fn().mockResolvedValue(['heavy_tank']), images: vi.fn().mockResolvedValue([]) },
        },
      ],
    });
    harness = await RouterTestingHarness.create();
    await harness.navigateByUrl(url);
    return harness.routeNativeElement!;
  }

  const stable = () => harness.fixture.whenStable();
  const url = () => TestBed.inject(Router).url;
  const button = (el: HTMLElement, label: string) =>
    Array.from(el.querySelectorAll('button')).find((b) => b.textContent?.trim() === label)!;
  const results = (el: HTMLElement) =>
    Array.from(el.querySelectorAll('.results li span:first-child')).map((s) => s.textContent);

  beforeEach(() => {
    const weapons: Record<number, AdminWeapon> = { 1: rifle, 2: cannon, 3: slugga };
    api = {
      listCatalogEntries: vi.fn((resource: CatalogResource, filter: { species?: number; weapon_class?: string }) => {
        if (resource === 'species') {
          return of([
            { id: 1, name: 'Menschen' },
            { id: 2, name: 'Orks' },
          ]);
        }
        if (resource === 'unit-types') {
          return of([adminUnitType({ id: 100, name: 'Trupp', sprite: 'heavy_tank' })]);
        }
        return of(
          Object.values(weapons).filter(
            (w) =>
              (filter.species === undefined || w.species_id === filter.species) &&
              (filter.weapon_class === undefined || w.weapon_class === filter.weapon_class),
          ),
        );
      }),
      getCatalogEntry: vi.fn((resource: CatalogResource, id: number) =>
        of(
          resource === 'unit-types'
            ? adminUnitType({ id, name: 'Trupp', sprite: 'heavy_tank', profiles: [adminProfile()] })
            : weapons[id],
        ),
      ),
      updateCatalogEntry: vi.fn((_r: string, id: number, entry: object) => of({ ...weapons[id], ...entry })),
      patchCatalogEntry: vi.fn((_r: string, id: number, fields: object) => of({ ...weapons[id], ...fields })),
      deleteCatalogEntry: vi.fn(() => of(undefined)),
    };
  });

  afterEach(() => vi.restoreAllMocks());

  describe('Suche', () => {
    it('listet alle Eintraege und filtert nach Namen ohne neue Anfrage', async () => {
      const el = await open(`/${BASE}/weapons/edit`);
      expect(results(el)).toEqual(['Gewehr', 'Kanone', 'Wumme']);
      expect(el.querySelector('.count')?.textContent?.trim()).toBe('3 Einträge');

      const search = el.querySelector<HTMLInputElement>('.search input')!;
      search.value = 'KAN';
      search.dispatchEvent(new Event('input'));
      await stable();

      expect(results(el)).toEqual(['Kanone']);
      expect(el.querySelector('.count')?.textContent?.trim()).toBe('1 von 3 Einträgen');
      expect(api.listCatalogEntries).toHaveBeenCalledTimes(2); // Spezies + Waffen
    });

    it('filtert ueber das Backend nach Spezies und Waffenklasse', async () => {
      const el = await open(`/${BASE}/weapons/edit`);
      const [speciesSelect, classSelect] = Array.from(el.querySelectorAll<HTMLSelectElement>('.search select'));

      speciesSelect.selectedIndex = 1; // Menschen
      speciesSelect.dispatchEvent(new Event('change'));
      await stable();
      expect(results(el)).toEqual(['Gewehr', 'Kanone']);

      classSelect.selectedIndex = Array.from(classSelect.options).findIndex((o) => o.textContent?.includes('Schwer'));
      classSelect.dispatchEvent(new Event('change'));
      await stable();

      expect(api.listCatalogEntries).toHaveBeenLastCalledWith('weapons', { species: 1, weapon_class: 'heavy' });
      expect(results(el)).toEqual(['Kanone']);
    });

    it('meldet einen Fehler und sucht danach weiter', async () => {
      const list = api.listCatalogEntries.getMockImplementation()!;
      api.listCatalogEntries.mockImplementation((resource: CatalogResource, filter: { species?: number }) =>
        resource === 'weapons' && filter.species === undefined
          ? throwError(() => new HttpErrorResponse({ status: 500 }))
          : list(resource, filter),
      );
      const el = await open(`/${BASE}/weapons/edit`);
      expect(el.querySelector('.error')?.textContent).toBe('Liste konnte nicht geladen werden.');

      const speciesSelect = el.querySelector<HTMLSelectElement>('.search select')!;
      speciesSelect.selectedIndex = 2; // Orks
      speciesSelect.dispatchEvent(new Event('change'));
      await stable();

      expect(el.querySelector('.error')).toBeNull();
      expect(results(el)).toEqual(['Wumme']);
    });
  });

  describe('Bearbeiten', () => {
    it('oeffnet einen Treffer ueber ?id= und zeigt das Formular', async () => {
      const el = await open(`/${BASE}/weapons/edit`);

      Array.from(el.querySelectorAll<HTMLButtonElement>('.result'))[0].click();
      await stable();

      expect(url()).toBe(`/${BASE}/weapons/edit?id=1`);
      expect(api.getCatalogEntry).toHaveBeenCalledWith('weapons', 1);
      expect(el.querySelector('h3')?.textContent).toContain('Gewehr');
      expect(el.querySelector('.result.selected')?.textContent).toContain('Gewehr');
      expect(el.querySelector('app-carrier-restrictions')).not.toBeNull();
    });

    it('speichert per PUT mit Spezies und aktualisiert die Trefferliste', async () => {
      const el = await open(`/${BASE}/weapons/edit?id=1`);
      const name = el.querySelector<HTMLInputElement>('app-catalog-entry-form input[type=text]')!;
      name.value = 'Sturmgewehr';
      name.dispatchEvent(new Event('input'));
      await stable();

      button(el, 'Speichern').click();
      await stable();

      const [resource, id, payload] = api.updateCatalogEntry.mock.calls[0];
      expect([resource, id]).toEqual(['weapons', 1]);
      expect(payload).toMatchObject({ name: 'Sturmgewehr', species_id: 1, weapon_class: 'standard' });
      expect(payload).not.toHaveProperty('in_use');
      expect(el.querySelector('.success')?.textContent).toBe('„Sturmgewehr“ gespeichert.');
      expect(results(el)).toContain('Sturmgewehr');
    });

    it('speichert, wer die Waffe tragen darf, per PATCH', async () => {
      const el = await open(`/${BASE}/weapons/edit?id=1`);
      await vi.waitFor(() => expect(el.querySelector('app-carrier-restrictions .tree')).not.toBeNull());

      el.querySelector<HTMLInputElement>('app-carrier-restrictions .tree input')!.click();
      await stable();
      Array.from(el.querySelectorAll<HTMLButtonElement>('app-carrier-restrictions button'))
        .find((b) => b.textContent?.trim() === 'Speichern')!
        .click();
      await stable();

      expect(api.patchCatalogEntry).toHaveBeenCalledWith('weapons', 1, { unit_type_ids: [100], profile_ids: [] });
      expect(el.querySelector('.success')?.textContent).toBe('Wer „Gewehr“ tragen darf, ist gespeichert.');
    });

    it('sperrt das Loeschen verwendeter Eintraege', async () => {
      const el = await open(`/${BASE}/weapons/edit?id=2`);
      expect(button(el, 'Löschen').disabled).toBe(true);
    });

    it('loescht nach Rueckfrage, nimmt den Eintrag aus der Liste und schliesst ihn', async () => {
      vi.spyOn(window, 'confirm').mockReturnValue(true);
      const el = await open(`/${BASE}/weapons/edit?id=1`);

      button(el, 'Löschen').click();
      await stable();

      expect(window.confirm).toHaveBeenCalledWith('Waffe „Gewehr“ wirklich löschen?');
      expect(api.deleteCatalogEntry).toHaveBeenCalledWith('weapons', 1);
      expect(results(el)).toEqual(['Kanone', 'Wumme']);
      expect(url()).toBe(`/${BASE}/weapons/edit`);
      expect(el.querySelector('app-catalog-entry-form')).toBeNull();
      expect(el.querySelector('.success')?.textContent).toBe('„Gewehr“ gelöscht.');
    });

    it('zeigt den Fehler, wenn das Loeschen scheitert', async () => {
      vi.spyOn(window, 'confirm').mockReturnValue(true);
      api.deleteCatalogEntry.mockReturnValue(
        throwError(() => new HttpErrorResponse({ status: 409, error: { detail: 'Wird noch verwendet.' } })),
      );
      const el = await open(`/${BASE}/weapons/edit?id=1`);

      button(el, 'Löschen').click();
      await stable();

      expect(el.querySelector('app-catalog-entry-form .error')?.textContent).toBe('Wird noch verwendet.');
      expect(url()).toBe(`/${BASE}/weapons/edit?id=1`);
    });

    it('zeigt einen Ladefehler fuer eine unbekannte id', async () => {
      api.getCatalogEntry.mockReturnValue(throwError(() => new HttpErrorResponse({ status: 404 })));
      const el = await open(`/${BASE}/weapons/edit?id=99`);
      expect(Array.from(el.querySelectorAll('.error')).map((e) => e.textContent)).toContain(
        'Waffe konnte nicht geladen werden.',
      );
    });
  });

  describe('Einheitentypen', () => {
    it('nimmt das Bild aus dem Formular und schickt beim Speichern das gespeicherte mit', async () => {
      api.updateCatalogEntry.mockImplementation((_r: string, id: number, entry: object) =>
        of(adminUnitType({ id, ...entry })),
      );
      const el = await open(`/${BASE}/unit-types/edit?id=100`);

      const formLabels = Array.from(el.querySelectorAll('app-catalog-entry-form label')).map((l) =>
        l.textContent?.trim(),
      );
      expect(formLabels).not.toContain('Bild');
      expect(el.querySelector('app-profile-overview')).not.toBeNull();
      expect(el.querySelector('app-sprite-images')).not.toBeNull();

      button(el, 'Speichern').click();
      await stable();

      expect(api.updateCatalogEntry.mock.calls[0][2]).toMatchObject({ sprite: 'heavy_tank', species_id: 1 });
    });
  });
});
