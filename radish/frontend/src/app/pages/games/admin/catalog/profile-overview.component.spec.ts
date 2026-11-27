import { ComponentFixture, TestBed } from '@angular/core/testing';
import { Router, provideRouter } from '@angular/router';

import { adminProfile, adminUnitType } from '../../../../../testing/catalog';
import { AdminUnitType } from '../../../../core/api.service';
import { ProfileOverviewComponent } from './profile-overview.component';

describe('ProfileOverviewComponent', () => {
  let fixture: ComponentFixture<ProfileOverviewComponent>;

  async function render(unitType: AdminUnitType): Promise<HTMLElement> {
    TestBed.configureTestingModule({ imports: [ProfileOverviewComponent], providers: [provideRouter([])] });
    fixture = TestBed.createComponent(ProfileOverviewComponent);
    fixture.componentRef.setInput('unitType', unitType);
    await fixture.whenStable();
    return fixture.nativeElement as HTMLElement;
  }

  const text = (el: Element) => el.textContent?.replace(/\s+/g, ' ').trim() ?? '';

  it('fuehrt ohne Profil zum Anlegen, vorbelegt mit Spezies und Einheitentyp', async () => {
    const el = await render(adminUnitType({ id: 100, species_id: 2 }));
    expect(text(el)).toContain('Noch kein Profil');
    expect(el.querySelector('a')!.getAttribute('href')).toBe(
      '/games/admin/catalog/profiles/new?species=2&unit_type=100',
    );
  });

  it('zeigt Werte, Anzahl je Einheit und "verwendet"', async () => {
    const el = await render(
      adminUnitType({
        min_entities: 1,
        max_entities: 6,
        profiles: [
          adminProfile({ id: 1, name: 'Anfuehrer', min_count: 1, max_count: 1, in_use: true }),
          adminProfile({ id: 2, name: 'Soldat', min_count: 0, max_count: 9 }),
        ],
      }),
    );

    const rows = Array.from(el.querySelectorAll('tbody tr'));
    const cells = (row: Element) => Array.from(row.querySelectorAll('td')).map(text);
    expect(cells(rows[0])).toEqual(['Anfuehrer verwendet', '1', '2', '3', '3', '4', '1', '0', '0', '1', 'genau 1', '10']);
    expect(cells(rows[1]).at(-2)).toBe('0–9');
    // 1 + 0 verlangt, hoechstens 1 + 9 = 10 -- der Typ erlaubt aber nur 6.
    expect(text(el)).toContain('mindestens 1 und erlauben höchstens 6 Entitäten; der Einheitentyp erlaubt 1–6');
    expect(el.querySelector('.error')).toBeNull();
  });

  it('warnt, wenn die Profile die Mindestzahl des Typs nicht erreichen', async () => {
    const el = await render(
      adminUnitType({ min_entities: 5, profiles: [adminProfile({ min_count: 1, max_count: 3 })] }),
    );
    expect(text(el.querySelector('.error')!)).toContain('Mindestzahl von 5 Entitäten nicht erreichen');
  });

  it('oeffnet ein Profil per Klick auf die Zeile', async () => {
    const el = await render(adminUnitType({ profiles: [adminProfile({ id: 7 })] }));
    const navigate = vi.spyOn(TestBed.inject(Router), 'navigate').mockResolvedValue(true);

    el.querySelector<HTMLElement>('tbody tr')!.click();

    expect(navigate).toHaveBeenCalledWith(['/games/admin/catalog/profiles/edit'], { queryParams: { id: 7 } });
  });
});
