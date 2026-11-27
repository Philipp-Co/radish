import { HttpErrorResponse } from '@angular/common/http';
import { ComponentFixture, TestBed } from '@angular/core/testing';
import { Router, provideRouter } from '@angular/router';
import { Observable, of, throwError } from 'rxjs';
import type { MockInstance } from 'vitest';

import { ApiService, GameDetail } from '../../../core/api.service';
import { LobbyComponent } from './lobby.component';

function game(overrides: Partial<GameDetail> = {}): GameDetail {
  return {
    id: 1,
    name: 'Alpha',
    host_identifier: 'anna',
    second_player_identifier: null,
    points_limit: 1000,
    host_army_name: 'Vorhut',
    second_player_army_name: null,
    status: 'lobby',
    is_host: true,
    ...overrides,
  };
}

describe('LobbyComponent', () => {
  let current: () => Observable<{ game: GameDetail | null }>;
  let api: {
    getCurrentGame: ReturnType<typeof vi.fn>;
    startGame: ReturnType<typeof vi.fn>;
    leaveGame: ReturnType<typeof vi.fn>;
  };
  let fixture: ComponentFixture<LobbyComponent>;
  let navigate: MockInstance<Router['navigate']>;

  async function render(): Promise<HTMLElement> {
    TestBed.configureTestingModule({
      imports: [LobbyComponent],
      providers: [provideRouter([]), { provide: ApiService, useValue: api }],
    });
    navigate = vi.spyOn(TestBed.inject(Router), 'navigate').mockResolvedValue(true);
    fixture = TestBed.createComponent(LobbyComponent);
    fixture.detectChanges();
    await fixture.whenStable();
    return fixture.nativeElement as HTMLElement;
  }

  /** Naechster Abruf des Pollings (alle 2 s). */
  async function poll(): Promise<void> {
    vi.advanceTimersByTime(2000);
    await fixture.whenStable();
  }

  const text = (el: HTMLElement) => el.textContent?.replace(/\s+/g, ' ') ?? '';
  const button = (el: HTMLElement, label: string) =>
    Array.from(el.querySelectorAll('button')).find((b) => b.textContent?.trim() === label);

  beforeEach(() => {
    vi.useFakeTimers({ toFake: ['setInterval', 'clearInterval'] });
    current = () => of({ game: game() });
    api = {
      getCurrentGame: vi.fn(() => current()),
      startGame: vi.fn(),
      leaveGame: vi.fn(),
    };
  });

  afterEach(() => {
    fixture.destroy();
    vi.useRealTimers();
    vi.restoreAllMocks();
  });

  it('wartet als Host auf einen Gegner, Start ist gesperrt', async () => {
    const el = await render();
    expect(text(el)).toContain('Alpha · 1000 Punkte');
    expect(text(el)).toContain('Freier Platz');
    expect(text(el)).toContain('Warte auf Gegner...');
    expect(button(el, 'Spiel starten')?.disabled).toBe(true);
  });

  it('bemerkt per Polling, dass ein Gegner beigetreten ist', async () => {
    const el = await render();
    current = () => of({ game: game({ second_player_identifier: 'bert', second_player_army_name: 'Horde' }) });

    await poll();

    const secondSeat = el.querySelectorAll('.players li')[1];
    expect(Array.from(secondSeat.querySelectorAll('span')).map((s) => s.textContent)).toEqual([
      'bert',
      'Horde',
    ]);
    expect(text(el)).toContain('du kannst das Spiel starten');
    expect(button(el, 'Spiel starten')?.disabled).toBe(false);
  });

  it('zeigt dem zweiten Spieler keinen Start-Knopf', async () => {
    current = () => of({ game: game({ is_host: false, second_player_identifier: 'bert' }) });
    const el = await render();
    expect(button(el, 'Spiel starten')).toBeUndefined();
    expect(text(el)).toContain('Warte, bis der Host das Spiel startet...');
  });

  it('geht zum aktuellen Spiel, sobald es laeuft, und hoert auf zu pollen', async () => {
    await render();
    current = () => of({ game: game({ status: 'running', second_player_identifier: 'bert' }) });

    await poll();
    expect(navigate).toHaveBeenCalledWith(['/games/current']);

    const calls = api.getCurrentGame.mock.calls.length;
    await poll();
    expect(api.getCurrentGame.mock.calls.length).toBe(calls);
  });

  it('pollt nach einem Fehler weiter und zeigt ihn an', async () => {
    current = () => throwError(() => new HttpErrorResponse({ status: 502 }));
    const el = await render();
    expect(text(el)).toContain('Spielzustand konnte nicht geladen werden');

    current = () => of({ game: game() });
    await poll();
    expect(text(el)).not.toContain('Spielzustand konnte nicht geladen werden');
    expect(text(el)).toContain('Warte auf Gegner...');
  });

  it('meldet, dass das Spiel beendet wurde, wenn es verschwindet', async () => {
    const el = await render();
    current = () => of({ game: null });
    await poll();
    expect(text(el)).toContain('Das Spiel wurde beendet.');
  });

  it('zeigt die Fehlermeldung, wenn der Start scheitert', async () => {
    current = () => of({ game: game({ second_player_identifier: 'bert' }) });
    api.startGame.mockReturnValue(
      throwError(() => new HttpErrorResponse({ status: 503, error: { detail: 'Kein Spielserver frei.' } })),
    );
    const el = await render();

    button(el, 'Spiel starten')!.click();
    await fixture.whenStable();

    expect(el.querySelector('.error')?.textContent).toBe('Kein Spielserver frei.');
    expect(button(el, 'Spiel starten')?.disabled).toBe(false);
  });

  it('fragt beim Verlassen nach und geht zur Spieleliste', async () => {
    api.leaveGame.mockReturnValue(of({ status: 'left' }));
    vi.spyOn(window, 'confirm').mockReturnValue(true);
    const el = await render();

    button(el, 'Verlassen')!.click();
    await fixture.whenStable();

    expect(window.confirm).toHaveBeenCalledWith('Lobby wirklich verlassen?');
    expect(navigate).toHaveBeenCalledWith(['/games/list']);
  });
});
