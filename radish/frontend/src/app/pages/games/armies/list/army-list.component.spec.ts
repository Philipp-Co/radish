import { HttpErrorResponse } from '@angular/common/http';
import { ComponentFixture, TestBed } from '@angular/core/testing';
import { provideRouter } from '@angular/router';
import { Subject, of, throwError } from 'rxjs';

import { armyEntry as army } from '../../../../../testing/catalog';
import { ApiService } from '../../../../core/api.service';
import { ArmyListComponent } from './army-list.component';

describe('ArmyListComponent', () => {
  let api: { listArmies: ReturnType<typeof vi.fn>; deleteArmy: ReturnType<typeof vi.fn> };
  let fixture: ComponentFixture<ArmyListComponent>;

  async function render(): Promise<HTMLElement> {
    TestBed.configureTestingModule({
      imports: [ArmyListComponent],
      providers: [provideRouter([]), { provide: ApiService, useValue: api }],
    });
    fixture = TestBed.createComponent(ArmyListComponent);
    await fixture.whenStable();
    return fixture.nativeElement as HTMLElement;
  }

  const buttonsLabelled = (el: HTMLElement, label: string) =>
    Array.from(el.querySelectorAll('button')).filter((b) => b.textContent?.trim() === label);

  beforeEach(() => {
    api = { listArmies: vi.fn(), deleteArmy: vi.fn() };
  });

  afterEach(() => vi.restoreAllMocks());

  it('zeigt "Lade...", solange die Liste unterwegs ist', async () => {
    api.listArmies.mockReturnValue(new Subject());
    const el = await render();
    expect(el.textContent).toContain('Lade...');
  });

  it('zeigt die Armeen mit Spezies, Einheitenzahl und Punkten', async () => {
    api.listArmies.mockReturnValue(
      of({ armies: [army(), army({ id: 2, name: 'Vorhut', unit_count: 1, total_cost: 120 })] }),
    );
    const el = await render();

    const items = Array.from(el.querySelectorAll('li')).map((li) => li.textContent?.replace(/\s+/g, ' ').trim());
    expect(items).toEqual([
      'Erste Kompanie Menschen · 3 Einheiten · 450 Punkte Löschen',
      'Vorhut Menschen · 1 Einheit · 120 Punkte Löschen',
    ]);
  });

  it('zeigt einen Hinweis ohne Armeen', async () => {
    api.listArmies.mockReturnValue(of({ armies: [] }));
    expect((await render()).textContent).toContain('Du hast noch keine Armee.');
  });

  it('zeigt die Fehlermeldung der API, wenn das Laden scheitert', async () => {
    api.listArmies.mockReturnValue(
      throwError(() => new HttpErrorResponse({ status: 500, error: { detail: 'Datenbank weg.' } })),
    );
    const el = await render();
    expect(el.querySelector('.error')?.textContent).toBe('Datenbank weg.');
  });

  it('loescht nach Rueckfrage und nimmt die Armee aus der Liste', async () => {
    api.listArmies.mockReturnValue(of({ armies: [army(), army({ id: 2, name: 'Vorhut' })] }));
    api.deleteArmy.mockReturnValue(of(undefined));
    vi.spyOn(window, 'confirm').mockReturnValue(true);
    const el = await render();

    buttonsLabelled(el, 'Löschen')[0].click();
    await fixture.whenStable();

    expect(window.confirm).toHaveBeenCalledWith('Armee "Erste Kompanie" wirklich löschen?');
    expect(api.deleteArmy).toHaveBeenCalledWith(1);
    expect(el.querySelectorAll('li').length).toBe(1);
    expect(el.textContent).toContain('Vorhut');
  });

  it('loescht nichts, wenn die Rueckfrage abgebrochen wird', async () => {
    api.listArmies.mockReturnValue(of({ armies: [army()] }));
    vi.spyOn(window, 'confirm').mockReturnValue(false);
    const el = await render();

    buttonsLabelled(el, 'Löschen')[0].click();
    await fixture.whenStable();

    expect(api.deleteArmy).not.toHaveBeenCalled();
    expect(el.querySelectorAll('li').length).toBe(1);
  });

  it('sperrt den Knopf waehrend des Loeschens und zeigt Fehler', async () => {
    const response = new Subject<void>();
    api.listArmies.mockReturnValue(of({ armies: [army()] }));
    api.deleteArmy.mockReturnValue(response);
    vi.spyOn(window, 'confirm').mockReturnValue(true);
    const el = await render();

    const button = buttonsLabelled(el, 'Löschen')[0];
    button.click();
    await fixture.whenStable();
    expect(button.disabled).toBe(true);

    response.error(new HttpErrorResponse({ status: 409, error: { detail: 'Armee ist in einem Spiel.' } }));
    await fixture.whenStable();

    expect(button.disabled).toBe(false);
    expect(el.querySelector('.error')?.textContent).toBe('Armee ist in einem Spiel.');
    expect(el.querySelectorAll('li').length).toBe(1);
  });
});
