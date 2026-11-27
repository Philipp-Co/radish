import { HttpErrorResponse } from '@angular/common/http';
import { ComponentFixture, TestBed } from '@angular/core/testing';
import { of, throwError } from 'rxjs';

import { adminProfile, adminUnitType } from '../../../../../testing/catalog';
import { ApiService, ProfileRestricted } from '../../../../core/api.service';
import { CarrierRestrictionsComponent, RestrictedEntry } from './carrier-restrictions.component';

const squad = adminUnitType({
  id: 100,
  name: 'Trupp',
  profiles: [adminProfile({ id: 10, name: 'Soldat' }), adminProfile({ id: 11, name: 'Anfuehrer' })],
});
const tank = adminUnitType({ id: 101, name: 'Panzer', profiles: [adminProfile({ id: 20, name: 'Besatzung' })] });

function entry(overrides: Partial<RestrictedEntry> = {}): RestrictedEntry {
  return { species_id: 1, unit_type_ids: [], profile_ids: [], ...overrides };
}

describe('CarrierRestrictionsComponent', () => {
  let fixture: ComponentFixture<CarrierRestrictionsComponent>;
  let listCatalogEntries: ReturnType<typeof vi.fn>;
  let saved: ProfileRestricted[];

  async function render(value: RestrictedEntry): Promise<HTMLElement> {
    TestBed.configureTestingModule({
      imports: [CarrierRestrictionsComponent],
      providers: [{ provide: ApiService, useValue: { listCatalogEntries } }],
    });
    fixture = TestBed.createComponent(CarrierRestrictionsComponent);
    fixture.componentRef.setInput('entry', value);
    fixture.componentRef.setInput('noun', 'die Waffe');
    saved = [];
    fixture.componentInstance.save.subscribe((r) => saved.push(r));
    await fixture.whenStable();
    return fixture.nativeElement as HTMLElement;
  }

  const checkbox = (el: HTMLElement, label: string) =>
    Array.from(el.querySelectorAll('label'))
      .find((l) => l.textContent?.trim().startsWith(label))!
      .querySelector('input')!;
  const button = (el: HTMLElement, label: string) =>
    Array.from(el.querySelectorAll('button')).find((b) => b.textContent?.trim() === label)!;
  const summary = (el: HTMLElement) => el.querySelector('.hint')?.textContent;

  async function click(input: HTMLElement): Promise<void> {
    input.click();
    await fixture.whenStable();
  }

  beforeEach(() => {
    listCatalogEntries = vi.fn(() => of([squad, tank]));
  });

  it('laedt die Einheitentypen der Spezies und zeigt "keine Einschraenkung"', async () => {
    const el = await render(entry());
    expect(listCatalogEntries).toHaveBeenCalledWith('unit-types', { species: 1 });
    expect(summary(el)).toBe('Keine Einschränkung: jede Entität der Spezies darf die Waffe tragen.');
    expect(button(el, 'Speichern').disabled).toBe(true);
  });

  it('ein Einheitentyp schliesst seine Profile ein und ersetzt deren Haken', async () => {
    const el = await render(entry({ profile_ids: [10, 20] }));
    expect(summary(el)).toBe('Nur für 2 einzelne Profile.');

    await click(checkbox(el, 'Trupp'));

    expect(checkbox(el, 'Soldat').checked).toBe(true);
    expect(checkbox(el, 'Soldat').disabled).toBe(true);
    expect(checkbox(el, 'Anfuehrer').checked).toBe(true);
    expect(summary(el)).toBe('Nur für 1 Einheitentyp (mit allen Profilen) und 1 einzelnes Profil.');

    button(el, 'Speichern').click();
    expect(saved).toEqual([{ unit_type_ids: [100], profile_ids: [20] }]);
  });

  it('erkennt das Zuruecknehmen einer Aenderung und verwirft auf Wunsch', async () => {
    const el = await render(entry({ profile_ids: [10] }));

    await click(checkbox(el, 'Anfuehrer'));
    expect(button(el, 'Speichern').disabled).toBe(false);
    await click(checkbox(el, 'Anfuehrer'));
    expect(button(el, 'Speichern').disabled).toBe(true);

    await click(checkbox(el, 'Besatzung'));
    button(el, 'Verwerfen').click();
    await fixture.whenStable();
    expect(checkbox(el, 'Besatzung').checked).toBe(false);
    expect(summary(el)).toBe('Nur für 1 einzelnes Profil.');
  });

  it('laedt nur bei einer anderen Spezies neu', async () => {
    await render(entry());
    fixture.componentRef.setInput('entry', entry({ profile_ids: [10] }));
    await fixture.whenStable();
    expect(listCatalogEntries).toHaveBeenCalledTimes(1);

    fixture.componentRef.setInput('entry', entry({ species_id: 2 }));
    await fixture.whenStable();
    expect(listCatalogEntries).toHaveBeenLastCalledWith('unit-types', { species: 2 });
  });

  it('zeigt einen Ladefehler', async () => {
    listCatalogEntries.mockReturnValue(
      throwError(() => new HttpErrorResponse({ status: 500, error: { detail: 'Kaputt.' } })),
    );
    const el = await render(entry());
    expect(el.querySelector('.error')?.textContent).toBe('Kaputt.');
  });
});
