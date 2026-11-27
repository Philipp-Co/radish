import { ComponentFixture, TestBed } from '@angular/core/testing';
import { Router, provideRouter } from '@angular/router';
import { Observable, of, throwError } from 'rxjs';

import { ApiService, GameListEntry, GameListPage } from '../../../core/api.service';
import { GamesListComponent } from './games-list.component';

function gamesPage(page: number, count: number, names: string[]): GameListPage {
  const games: GameListEntry[] = names.map((name) => ({ name, player_count: 1, points_limit: 1000 }));
  return { games, count, page, page_size: 2 };
}

describe('GamesListComponent', () => {
  let respond: (page: number) => Observable<GameListPage>;
  let listGames: ReturnType<typeof vi.fn>;
  let fixture: ComponentFixture<GamesListComponent>;

  async function render(): Promise<HTMLElement> {
    TestBed.configureTestingModule({
      imports: [GamesListComponent],
      providers: [provideRouter([]), { provide: ApiService, useValue: { listGames } }],
    });
    fixture = TestBed.createComponent(GamesListComponent);
    await fixture.whenStable();
    return fixture.nativeElement as HTMLElement;
  }

  const names = (el: HTMLElement) =>
    Array.from(el.querySelectorAll('li > span:first-child')).map((span) => span.textContent);
  const button = (el: HTMLElement, label: string) =>
    Array.from(el.querySelectorAll('button')).find((b) => b.textContent?.trim() === label)!;
  const text = (el: HTMLElement) => el.textContent?.replace(/\s+/g, ' ') ?? '';

  beforeEach(() => {
    vi.useFakeTimers({ toFake: ['setInterval', 'clearInterval'] });
    respond = (page) => of(page === 1 ? gamesPage(1, 3, ['Alpha', 'Beta']) : gamesPage(2, 3, ['Gamma']));
    listGames = vi.fn((page: number) => respond(page));
  });

  afterEach(() => {
    fixture.destroy();
    vi.useRealTimers();
  });

  it('zeigt die erste Seite mit Punktelimit und Spielerzahl', async () => {
    const el = await render();
    expect(listGames).toHaveBeenCalledWith(1);
    expect(names(el)).toEqual(['Alpha', 'Beta']);
    const spans = Array.from(el.querySelectorAll('li')[0].querySelectorAll('span'));
    expect(spans.map((span) => span.textContent)).toEqual(['Alpha', '1000 P', '1/2']);
    expect(text(el)).toContain('Seite 1 von 2');
    expect(button(el, '← Zurueck').disabled).toBe(true);
  });

  it('blaettert sofort weiter, ohne auf das Polling zu warten', async () => {
    const el = await render();

    button(el, 'Weiter →').click();
    await fixture.whenStable();

    expect(listGames).toHaveBeenLastCalledWith(2);
    expect(names(el)).toEqual(['Gamma']);
    expect(text(el)).toContain('Seite 2 von 2');
    expect(button(el, 'Weiter →').disabled).toBe(true);
  });

  it('pollt alle 5 s die aktuelle Seite', async () => {
    await render();
    vi.advanceTimersByTime(5000);
    expect(listGames).toHaveBeenCalledTimes(2);
    expect(listGames).toHaveBeenLastCalledWith(1);
  });

  it('springt auf die Seite, die das Backend zurueckgibt', async () => {
    const el = await render();
    button(el, 'Weiter →').click();
    await fixture.whenStable();

    // Das letzte Spiel auf Seite 2 ist weg -- das Backend liefert Seite 1.
    respond = () => of(gamesPage(1, 2, ['Alpha', 'Beta']));
    vi.advanceTimersByTime(5000);
    await fixture.whenStable();

    expect(text(el)).toContain('Seite 1 von 1');
    expect(names(el)).toEqual(['Alpha', 'Beta']);
  });

  it('zeigt einen Fehler und laeuft danach weiter', async () => {
    respond = () => throwError(() => new Error('offline'));
    const el = await render();
    expect(el.querySelector('.error')?.textContent).toBe('Spieleliste konnte nicht geladen werden.');

    respond = () => of(gamesPage(1, 1, ['Alpha']));
    vi.advanceTimersByTime(5000);
    await fixture.whenStable();

    expect(el.querySelector('.error')).toBeNull();
    expect(names(el)).toEqual(['Alpha']);
  });

  it('zeigt einen Hinweis ohne offene Spiele', async () => {
    respond = () => of(gamesPage(1, 0, []));
    expect(text(await render())).toContain('Aktuell kein offenes Spiel.');
  });

  it('fuehrt mit dem Spielnamen zur Beitreten-Seite', async () => {
    const el = await render();
    const navigate = vi.spyOn(TestBed.inject(Router), 'navigate').mockResolvedValue(true);

    button(el, 'Beitreten').click();

    expect(navigate).toHaveBeenCalledWith(['/games/join'], { queryParams: { name: 'Alpha' } });
  });
});
