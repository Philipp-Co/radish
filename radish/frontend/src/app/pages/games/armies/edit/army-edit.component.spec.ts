import { BrowserPlatformLocation, PlatformLocation } from '@angular/common';
import { HttpErrorResponse } from '@angular/common/http';
import { TestBed } from '@angular/core/testing';
import { Router, provideRouter } from '@angular/router';
import { RouterTestingHarness } from '@angular/router/testing';
import { of, throwError } from 'rxjs';

import { profile, species, unitType, weapon } from '../../../../../testing/catalog';
import { ApiService, ArmyDetail, ArmyInput } from '../../../../core/api.service';
import { ArmyEditComponent } from './army-edit.component';

const soldier = profile({ id: 10, name: 'Soldat', cost: 10, weapon_slots: 1, min_count: 1, max_count: 5 });
// Der Typ ohne Profil steht vorn: vorgewaehlt werden soll trotzdem der Trupp.
const tank = unitType({ id: 101, name: 'Panzer', profiles: [] });
const squad = unitType({ id: 100, name: 'Trupp', min_entities: 1, max_entities: 5, profiles: [soldier] });
const humans = species({
  id: 1,
  name: 'Menschen',
  unit_types: [tank, squad],
  weapons: [weapon({ id: 1000, name: 'Gewehr', cost: 5 })],
});
const orcs = species({ id: 2, name: 'Orks' });

function savedArmy(overrides: Partial<ArmyDetail> = {}): ArmyDetail {
  return {
    id: 7,
    name: 'Vorhut',
    species_id: 1,
    species_name: 'Menschen',
    total_cost: 15,
    units: [
      {
        unit_type_id: 100,
        total_cost: 15,
        entities: [
          {
            profile_id: 10,
            weapon_ids: [1000],
            heavy_weapon_ids: [],
            super_heavy_weapon_ids: [],
            equipment_ids: [],
            total_cost: 15,
          },
        ],
      },
    ],
    ...overrides,
  };
}

describe('ArmyEditComponent', () => {
  let api: Record<'getCatalog' | 'getArmy' | 'createArmy' | 'updateArmy', ReturnType<typeof vi.fn>>;
  let harness: RouterTestingHarness;

  /**
   * realHistory: TestBed ersetzt die Browser-History durch einen Mock; die
   * Komponente liest den Navigations-State aber aus window.history.
   */
  async function open(url: string, { realHistory = false } = {}): Promise<HTMLElement> {
    TestBed.configureTestingModule({
      providers: [
        ...(realHistory ? [{ provide: PlatformLocation, useClass: BrowserPlatformLocation }] : []),
        provideRouter([
          { path: 'games/armies/new', component: ArmyEditComponent },
          { path: 'games/armies/:id', component: ArmyEditComponent },
        ]),
        { provide: ApiService, useValue: api },
      ],
    });
    harness = await RouterTestingHarness.create();
    await harness.navigateByUrl(url, ArmyEditComponent);
    return harness.routeNativeElement!;
  }

  const stable = () => harness.fixture.whenStable();
  const text = (el: HTMLElement) => el.textContent?.replace(/\s+/g, ' ') ?? '';
  const button = (el: HTMLElement, label: string) =>
    Array.from(el.querySelectorAll('button')).find((b) => b.textContent?.trim() === label)!;
  const nameInput = (el: HTMLElement) => el.querySelector<HTMLInputElement>('input[type=text]')!;

  async function typeName(el: HTMLElement, value: string): Promise<void> {
    const input = nameInput(el);
    input.value = value;
    input.dispatchEvent(new Event('input'));
    await stable();
  }

  beforeEach(() => {
    api = {
      getCatalog: vi.fn(() => of({ species: [humans, orcs] })),
      getArmy: vi.fn(() => of(savedArmy())),
      createArmy: vi.fn(),
      updateArmy: vi.fn(),
    };
  });

  afterEach(() => vi.restoreAllMocks());

  describe('neue Armee', () => {
    it('waehlt die erste Spezies vor und laesst ohne Namen nicht speichern', async () => {
      const el = await open('/games/armies/new');

      expect(el.querySelector('h2')?.textContent).toBe('Neue Armee');
      expect(text(el)).toContain('0 Punkte · 0 Einheiten');
      expect(text(el)).toContain('Noch keine Einheit.');
      expect(button(el, 'Speichern').disabled).toBe(true);
      expect(api.getArmy).not.toHaveBeenCalled();
    });

    it('legt eine Einheit mit gueltiger Grundbesetzung an und rechnet die Punkte', async () => {
      const el = await open('/games/armies/new');

      button(el, 'Einheit hinzufügen').click();
      await stable();

      const row = el.querySelector('tbody tr')!;
      expect(text(row as HTMLElement)).toContain('Trupp');
      expect(text(row as HTMLElement)).toContain('1× Soldat');
      expect(row.querySelector('.error')).toBeNull();
      expect(text(el)).toContain('10 Punkte · 1 Einheit');
      // Mit Einheiten ist die Spezies fest.
      expect(el.querySelector('select')!.disabled).toBe(true);
    });

    it('speichert und wechselt auf die URL der neuen Armee', async () => {
      api.createArmy.mockReturnValue(of(savedArmy({ id: 7, name: 'Neue Vorhut', units: [] })));
      api.getArmy.mockReturnValue(of(savedArmy({ id: 7, name: 'Neue Vorhut', units: [] })));
      const el = await open('/games/armies/new', { realHistory: true });

      await typeName(el, 'Neue Vorhut');
      button(el, 'Einheit hinzufügen').click();
      await stable();
      button(el, 'Speichern').click();
      await stable();

      expect(api.createArmy).toHaveBeenCalledWith({
        name: 'Neue Vorhut',
        species_id: 1,
        units: [
          {
            unit_type_id: 100,
            entities: [
              { profile_id: 10, weapon_ids: [], heavy_weapon_ids: [], super_heavy_weapon_ids: [], equipment_ids: [] },
            ],
          },
        ],
      } satisfies ArmyInput);
      expect(TestBed.inject(Router).url).toBe('/games/armies/7');
      expect(api.getArmy).toHaveBeenCalledWith(7);
      expect(text(harness.routeNativeElement!)).toContain('Gespeichert.');
    });
  });

  describe('bestehende Armee', () => {
    it('laedt die Armee und zeigt keine ungespeicherten Aenderungen', async () => {
      const el = await open('/games/armies/7');

      expect(api.getArmy).toHaveBeenCalledWith(7);
      expect(el.querySelector('h2')?.textContent).toBe('Armee bearbeiten');
      expect(nameInput(el).value).toBe('Vorhut');
      expect(text(el)).toContain('15 Punkte · 1 Einheit');
      expect(text(el)).not.toContain('Nicht gespeicherte Änderungen');
    });

    it('merkt Aenderungen und speichert sie per PUT', async () => {
      api.updateArmy.mockReturnValue(of(savedArmy({ name: 'Hauptmacht' })));
      const el = await open('/games/armies/7');

      await typeName(el, 'Hauptmacht');
      expect(text(el)).toContain('Nicht gespeicherte Änderungen');

      button(el, 'Speichern').click();
      await stable();

      expect(api.updateArmy).toHaveBeenCalledWith(7, expect.objectContaining({ name: 'Hauptmacht' }));
      expect(text(el)).not.toContain('Nicht gespeicherte Änderungen');
      expect(el.querySelector('.success')?.textContent).toBe('Gespeichert.');
    });

    it('zeigt den Fehler des Backends, wenn das Speichern scheitert', async () => {
      api.updateArmy.mockReturnValue(
        throwError(
          () => new HttpErrorResponse({ status: 400, error: { units: [{ entities: ['Zu viele Waffen.'] }] } }),
        ),
      );
      const el = await open('/games/armies/7');

      button(el, 'Speichern').click();
      await stable();

      expect(el.querySelector('p.error')?.textContent).toBe('Zu viele Waffen.');
      expect(button(el, 'Speichern').disabled).toBe(false);
    });

    it('behaelt Waffen aus gestrichenen Slots und meldet sie', async () => {
      const army = savedArmy();
      army.units[0].entities[0].weapon_ids = [1000, 1000];
      api.getArmy.mockReturnValue(of(army));

      const el = await open('/games/armies/7');

      expect(text(el)).toContain('⚠ Entität 1: 2× Waffe, nur 1 Slots');
      expect(text(el)).toContain('20 Punkte');
    });

    it('oeffnet die Einheit aus ?unit= in der Detailansicht und kommt zurueck', async () => {
      const el = await open('/games/armies/7?unit=1');

      expect(el.querySelector('app-unit-detail')).not.toBeNull();
      expect(text(el)).toContain('1. Trupp');

      button(el, '← Übersicht').click();
      await stable();

      expect(TestBed.inject(Router).url).toBe('/games/armies/7');
      expect(el.querySelector('app-unit-detail')).toBeNull();
      expect(el.querySelector('tbody tr')).not.toBeNull();
    });

    it('entfernt eine Einheit nach Rueckfrage', async () => {
      vi.spyOn(window, 'confirm').mockReturnValue(true);
      const el = await open('/games/armies/7');

      button(el, '✕').click();
      await stable();

      expect(window.confirm).toHaveBeenCalledWith('Einheit 1 (Trupp) entfernen?');
      expect(text(el)).toContain('Noch keine Einheit.');
      expect(text(el)).toContain('Nicht gespeicherte Änderungen');
    });
  });

  it('zeigt einen Ladefehler', async () => {
    api.getArmy.mockReturnValue(throwError(() => new HttpErrorResponse({ status: 404, error: { detail: 'Nicht gefunden.' } })));
    const el = await open('/games/armies/99');
    expect(el.querySelector('p.error')?.textContent).toBe('Nicht gefunden.');
  });
});
