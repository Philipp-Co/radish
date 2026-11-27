import { HttpErrorResponse } from '@angular/common/http';
import { TestBed } from '@angular/core/testing';
import { provideRouter } from '@angular/router';
import { RouterTestingHarness } from '@angular/router/testing';
import { of, throwError } from 'rxjs';

import { adminUnitType } from '../../../../../testing/catalog';
import { ApiService, CatalogResource } from '../../../../core/api.service';
import { UnitSpriteService } from '../../../../core/unit-sprite.service';
import { CatalogCreateComponent } from './catalog-create.component';

const BASE = 'games/admin/catalog';
const RESOURCES: CatalogResource[] = ['species', 'unit-types', 'profiles', 'weapons', 'equipment'];

describe('CatalogCreateComponent', () => {
  let api: Record<'listCatalogEntries' | 'createCatalogEntry', ReturnType<typeof vi.fn>>;
  let harness: RouterTestingHarness;

  async function open(url: string): Promise<HTMLElement> {
    TestBed.configureTestingModule({
      providers: [
        provideRouter(
          RESOURCES.map((resource) => ({
            path: `${BASE}/${resource}/new`,
            component: CatalogCreateComponent,
            data: { resource },
          })),
        ),
        { provide: ApiService, useValue: api },
        { provide: UnitSpriteService, useValue: { keys: vi.fn().mockResolvedValue([]) } },
      ],
    });
    harness = await RouterTestingHarness.create();
    await harness.navigateByUrl(url);
    return harness.routeNativeElement!;
  }

  const stable = () => harness.fixture.whenStable();
  const button = (el: HTMLElement, label: string) =>
    Array.from(el.querySelectorAll('button')).find((b) => b.textContent?.trim() === label)!;
  const selects = (el: HTMLElement) => Array.from(el.querySelectorAll<HTMLSelectElement>('.parents select'));
  const selectedText = (select: HTMLSelectElement) => select.selectedOptions[0]?.textContent?.trim();

  async function typeName(el: HTMLElement, value: string): Promise<void> {
    const input = el.querySelector<HTMLInputElement>('app-catalog-entry-form input[type=text]')!;
    input.value = value;
    input.dispatchEvent(new Event('input'));
    await stable();
  }

  beforeEach(() => {
    api = {
      listCatalogEntries: vi.fn((resource: CatalogResource, filter?: { species?: number }) => {
        if (resource === 'species') {
          return of([
            { id: 1, name: 'Menschen' },
            { id: 2, name: 'Orks' },
          ]);
        }
        return of(
          filter?.species === 2
            ? [adminUnitType({ id: 200, name: 'Mob', species_id: 2 }), adminUnitType({ id: 201, name: 'Boss', species_id: 2 })]
            : [adminUnitType({ id: 100, name: 'Trupp' })],
        );
      }),
      createCatalogEntry: vi.fn((_resource: string, entry: { name: string }) => of({ id: 42, name: entry.name })),
    };
  });

  it('legt eine Spezies ohne Zuordnung an', async () => {
    const el = await open(`/${BASE}/species/new`);
    expect(el.querySelector('h2')?.textContent).toBe('Spezies');
    expect(selects(el)).toEqual([]);
    expect(api.listCatalogEntries).not.toHaveBeenCalled();

    await typeName(el, 'Elfen');
    button(el, 'Anlegen').click();
    await stable();

    expect(api.createCatalogEntry).toHaveBeenCalledWith('species', { name: 'Elfen' });
    expect(el.querySelector('.success')?.textContent?.replace(/\s+/g, ' ').trim()).toBe(
      'Spezies „Elfen“ angelegt. Bearbeiten',
    );
  });

  it('haengt eine Waffe an die gewaehlte Spezies und leert danach das Formular', async () => {
    const el = await open(`/${BASE}/weapons/new?species=2`);
    expect(selectedText(selects(el)[0])).toBe('Orks');

    await typeName(el, 'Spalta');
    button(el, 'Anlegen').click();
    await stable();

    expect(api.createCatalogEntry).toHaveBeenCalledWith(
      'weapons',
      expect.objectContaining({ name: 'Spalta', species_id: 2, weapon_class: 'standard', min_range: 0 }),
    );
    const link = el.querySelector<HTMLAnchorElement>('.success a')!;
    expect(link.getAttribute('href')).toBe('/games/admin/catalog/weapons/edit?id=42');
    expect(el.querySelector<HTMLInputElement>('app-catalog-entry-form input[type=text]')!.value).toBe('');
  });

  it('waehlt fuer ein Profil Spezies und Einheitentyp aus der URL vor', async () => {
    const el = await open(`/${BASE}/profiles/new?species=2&unit_type=201`);

    expect(api.listCatalogEntries).toHaveBeenCalledWith('unit-types', { species: 2 });
    expect(selects(el).map(selectedText)).toEqual(['Orks', 'Boss']);

    await typeName(el, 'Waaaghboss');
    button(el, 'Anlegen').click();
    await stable();

    expect(api.createCatalogEntry).toHaveBeenCalledWith(
      'profiles',
      expect.objectContaining({ name: 'Waaaghboss', unit_type_id: 201 }),
    );
    expect(api.createCatalogEntry.mock.calls[0][1]).not.toHaveProperty('species_id');
  });

  it('laesst kein Profil ohne Einheitentyp anlegen', async () => {
    api.listCatalogEntries.mockImplementation((resource: CatalogResource) =>
      of(resource === 'species' ? [{ id: 1, name: 'Menschen' }] : []),
    );
    const el = await open(`/${BASE}/profiles/new`);
    expect(el.textContent).toContain('Für diese Spezies gibt es noch keine Einheitentypen.');

    button(el, 'Anlegen').click();
    await stable();

    expect(api.createCatalogEntry).not.toHaveBeenCalled();
    expect(el.querySelector('.error')?.textContent).toBe('Bitte zuerst einen Einheitentyp wählen.');
  });

  it('weist ohne Spezies auf das Anlegen einer Spezies hin', async () => {
    api.listCatalogEntries.mockReturnValue(of([]));
    const el = await open(`/${BASE}/equipment/new`);
    expect(el.textContent).toContain('Es gibt noch keine Spezies.');
  });

  it('nennt bei Fehlern des Backends das Feld und behaelt die Eingaben', async () => {
    api.createCatalogEntry.mockReturnValue(
      throwError(() => new HttpErrorResponse({ status: 400, error: { cost: ['Dieses Feld darf nicht null sein.'] } })),
    );
    const el = await open(`/${BASE}/weapons/new`);
    await typeName(el, 'Gewehr');

    button(el, 'Anlegen').click();
    await stable();

    expect(el.querySelector('.error')?.textContent).toBe('Kosten: Dieses Feld darf nicht null sein.');
    expect(el.querySelector<HTMLInputElement>('app-catalog-entry-form input[type=text]')!.value).toBe('Gewehr');
    expect(el.querySelector('.success')).toBeNull();
  });
});
