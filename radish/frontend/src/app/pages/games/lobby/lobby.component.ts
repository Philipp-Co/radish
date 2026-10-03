import { HttpErrorResponse } from '@angular/common/http';
import { Component, OnDestroy, OnInit, inject, signal } from '@angular/core';
import { Router, RouterLink } from '@angular/router';
import { Subscription, catchError, interval, of, startWith, switchMap } from 'rxjs';

import { apiErrorMessage } from '../../../core/api-error';
import { ApiService, GameDetail } from '../../../core/api.service';

/**
 * Wie oft die Lobby den Zustand des eigenen Spiels abfragt. Bewusst Polling
 * statt WebSocket: die Lobby braucht nur wenige, seltene Zustandswechsel
 * (Gegner da, Gegner weg, gestartet) -- so erfaehrt vor allem der zweite
 * Spieler, dass der Host gestartet hat.
 */
const POLL_INTERVAL_MS = 2000;

/**
 * Wartebereich nach dem Erstellen bzw. Beitreten (siehe radish/backend/api/
 * models.py, GameStatus): zeigt beide Spieler, laesst den Host das Spiel
 * starten, sobald ein Gegner da ist, und fuehrt auf "Aktuelles Spiel"
 * (CurrentGameComponent), sobald die Spielinstanz bereit ist.
 */
@Component({
  selector: 'app-lobby',
  standalone: true,
  imports: [RouterLink],
  template: `
    <section class="card">
      <h2>Lobby</h2>

      @if (errorMessage(); as message) {
        <p class="error">{{ message }}</p>
      }
      @if (pollFailed()) {
        <p class="error">Spielzustand konnte nicht geladen werden -- versuche es weiter...</p>
      }

      @if (loading()) {
        <p class="muted">Lade...</p>
      } @else {
        @if (game(); as current) {
          <p>
            <strong>{{ current.name }}</strong> ·
            <span class="muted">{{ current.points_limit }} Punkte</span>
          </p>
          <ul class="players">
            <li>
              <span>{{ current.host_identifier }}</span>
              <span class="muted">Host · {{ current.host_army_name ?? '–' }}</span>
            </li>
            <li>
              @if (current.second_player_identifier) {
                <span>{{ current.second_player_identifier }}</span>
                <span class="muted">{{ current.second_player_army_name ?? '–' }}</span>
              } @else {
                <span class="muted">Freier Platz</span>
              }
            </li>
          </ul>

          <p class="status">
            @if (current.status === 'running') {
              Spiel ist bereit.
            } @else if (starting()) {
              Spielinstanz wird vorbereitet...
            } @else if (!current.second_player_identifier) {
              Warte auf Gegner...
            } @else if (current.is_host) {
              Beide Spieler sind da -- du kannst das Spiel starten.
            } @else {
              Warte, bis der Host das Spiel startet...
            }
          </p>

          <div class="actions">
            @if (current.is_host && current.status === 'lobby') {
              <button
                type="button"
                [disabled]="!current.second_player_identifier || busy()"
                (click)="startGame()"
              >
                Spiel starten
              </button>
            }
            <button type="button" class="secondary" [disabled]="busy()" (click)="leaveGame()">
              Verlassen
            </button>
          </div>
        } @else {
          <p class="muted">
            {{ hadGame ? 'Das Spiel wurde beendet.' : 'Du bist gerade in keinem Spiel.' }}
          </p>
          <p><a routerLink="/games/list">Zu den offenen Spielen</a></p>
        }
      }
    </section>
  `,
  styles: [
    `
      .players {
        list-style: none;
        margin: 0 0 0.75rem;
        padding: 0;
        display: flex;
        flex-direction: column;
        gap: 0.5rem;
      }
      .players li {
        display: flex;
        justify-content: space-between;
        gap: 0.75rem;
      }
      .actions {
        display: flex;
        gap: 0.75rem;
        margin-top: 0.75rem;
      }
    `,
  ],
})
export class LobbyComponent implements OnInit, OnDestroy {
  private readonly api = inject(ApiService);
  private readonly router = inject(Router);

  readonly game = signal<GameDetail | null>(null);
  readonly loading = signal(true);
  readonly busy = signal(false);
  /** Der Start-Aufruf laeuft -- die Spielinstanz setzt das Spiel gerade auf. */
  readonly starting = signal(false);
  readonly errorMessage = signal<string | null>(null);
  /** Der letzte Abruf des Pollings ist fehlgeschlagen; der naechste versucht es erneut. */
  readonly pollFailed = signal(false);

  /** Ob diese Ansicht schon ein Spiel gesehen hat -- fuer "Das Spiel wurde beendet". */
  hadGame = false;

  private pollSubscription: Subscription | null = null;

  ngOnInit(): void {
    this.pollSubscription = interval(POLL_INTERVAL_MS)
      .pipe(
        startWith(0),
        // Ein fehlgeschlagener Abruf beendet das Polling nicht, er wird beim
        // naechsten Intervall einfach wiederholt.
        switchMap(() => this.api.getCurrentGame().pipe(catchError(() => of(null)))),
      )
      .subscribe((response) => {
        this.loading.set(false);
        this.pollFailed.set(response === null);
        if (response !== null) {
          this.showGame(response.game);
        }
      });
  }

  ngOnDestroy(): void {
    this.stopPolling();
  }

  startGame(): void {
    this.busy.set(true);
    this.starting.set(true);
    this.errorMessage.set(null);
    this.api.startGame().subscribe({
      next: (game) => {
        this.busy.set(false);
        this.starting.set(false);
        this.showGame(game);
      },
      error: (err: HttpErrorResponse) => {
        this.busy.set(false);
        this.starting.set(false);
        this.errorMessage.set(apiErrorMessage(err, 'Spiel konnte nicht gestartet werden.'));
      },
    });
  }

  leaveGame(): void {
    const started = this.game()?.status === 'running';
    const question = started
      ? 'Das Spiel ist schon gestartet und wird damit für beide Spieler beendet. Wirklich verlassen?'
      : 'Lobby wirklich verlassen?';
    if (!window.confirm(question)) {
      return;
    }
    this.busy.set(true);
    this.errorMessage.set(null);
    this.api.leaveGame().subscribe({
      next: () => {
        this.busy.set(false);
        this.stopPolling();
        void this.router.navigate(['/games/list']);
      },
      error: (err: HttpErrorResponse) => {
        this.busy.set(false);
        this.errorMessage.set(apiErrorMessage(err, 'Spiel konnte nicht verlassen werden.'));
      },
    });
  }

  private showGame(game: GameDetail | null): void {
    this.game.set(game);
    if (game === null) {
      // Kein Spiel mehr (z.B. vom Gegner nach dem Start beendet) -- danach
      // aendert sich nichts mehr, das Polling kann aufhoeren.
      this.stopPolling();
      return;
    }
    this.hadGame = true;
    if (game.status === 'running') {
      this.stopPolling();
      void this.router.navigate(['/games/current']);
    }
  }

  private stopPolling(): void {
    this.pollSubscription?.unsubscribe();
    this.pollSubscription = null;
  }
}
