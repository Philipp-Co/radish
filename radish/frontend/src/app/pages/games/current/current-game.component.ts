import { HttpErrorResponse } from '@angular/common/http';
import { Component, OnDestroy, OnInit, inject, signal } from '@angular/core';
import { Router } from '@angular/router';
import { Subscription, catchError, interval, of, switchMap } from 'rxjs';

import { ApiService, GameDetail } from '../../../core/api.service';
import { GameSocketService } from '../../../core/game-socket.service';
import { GameCanvasComponent } from '../../../shared/game-canvas/game-canvas.component';

/**
 * Zeigt das laufende Spiel des angemeldeten Nutzers, falls es eins gibt
 * (siehe api/client/game/current/, radish/backend/api/client_views.py,
 * CurrentGameView) -- Einstieg, um nach einem Reload oder erneuten Login
 * direkt ins eigene Spiel zurueckzufinden, ohne es in der Liste suchen zu
 * muessen. Bettet dafuer die Canvas-Komponente (siehe shared/game-canvas/)
 * direkt hier ein, bewusst ohne Umleitung auf "/game/:name" -- anders als
 * z.B. GameCreateComponent/GameJoinComponent, die nach dem Erstellen/
 * Beitreten dorthin navigieren.
 *
 * Nur fuer laufende Spiele: steht das eigene Spiel noch in der Lobby oder
 * wird es gerade vorbereitet (siehe GameDetail.status), leitet die Seite auf
 * die Lobby um (LobbyComponent).
 */
@Component({
  selector: 'app-current-game',
  standalone: true,
  imports: [GameCanvasComponent],
  template: `
    <section class="card">
      <div class="list-header">
        <h2>Aktuelles Spiel</h2>
        <button class="secondary" (click)="refresh()">Aktualisieren</button>
      </div>

      @if (errorMessage(); as message) {
        <p class="error">{{ message }}</p>
      } @else if (loading()) {
        <p class="muted">Lade...</p>
      } @else {
        <!-- "as" ist nur am ersten @if einer Kette erlaubt (NG5002), deshalb
             hier ein eigener, verschachtelter @if/@else statt @else if. -->
        @if (game(); as current) {
          <p>
            Du spielst gerade <strong>{{ current.name }}</strong> --
            <span class="muted">{{ current.second_player_identifier ? '2' : '1' }}/2 Spieler</span>
          </p>
          <p class="muted armies">
            {{ current.points_limit }} Punkte ·
            {{ current.host_army_name ?? '–' }} gegen
            {{ current.second_player_army_name ?? 'noch offen' }}
          </p>
          <app-game-canvas [gameName]="current.name" />
          <div class="actions">
            <button type="button" class="secondary" [disabled]="leaving()" (click)="leaveGame()">
              Spiel verlassen
            </button>
          </div>
        } @else {
          <p class="muted">
            {{ ended() ? 'Das Spiel wurde beendet.' : 'Aktuell kein laufendes Spiel.' }}
          </p>
        }
      }
    </section>
  `,
  styles: [
    `
      .list-header {
        display: flex;
        align-items: center;
        justify-content: space-between;
        margin-bottom: 0.75rem;
      }
      .actions {
        display: flex;
        gap: 0.75rem;
        margin-top: 0.75rem;
      }
    `,
  ],
})
export class CurrentGameComponent implements OnInit, OnDestroy {
  private readonly api = inject(ApiService);
  private readonly gameSocket = inject(GameSocketService);
  private readonly router = inject(Router);

  readonly game = signal<GameDetail | null>(null);
  readonly loading = signal(true);
  readonly leaving = signal(false);
  readonly errorMessage = signal<string | null>(null);
  /** Das Spiel ist verschwunden, waehrend diese Ansicht offen war -- vom Gegner beendet. */
  readonly ended = signal(false);

  private endPollSubscription: Subscription | null = null;

  ngOnInit(): void {
    this.loadCurrentGame();
  }

  ngOnDestroy(): void {
    this.stopEndPolling();
    this.gameSocket.disconnect();
  }

  /**
   * Eine Abfrage beim Oeffnen der Ansicht (bzw. per "Aktualisieren"-Button,
   * siehe refresh()). Liefert getCurrentGame() ein laufendes Spiel, wird
   * zusaetzlich die WebSocket-Verbindung geoeffnet (siehe GameSocketService)
   * und per Polling beobachtet, ob der Gegner das Spiel beendet (siehe
   * startEndPolling()).
   */
  private loadCurrentGame(): void {
    this.api.getCurrentGame().subscribe({
      next: (response) => {
        if (response.game && response.game.status !== 'running') {
          void this.router.navigate(['/games/lobby']);
          return;
        }
        this.game.set(response.game);
        this.loading.set(false);
        if (response.game) {
          this.gameSocket.connect();
          this.startEndPolling();
        }
      },
      error: () => {
        this.errorMessage.set('Aktuelles Spiel konnte nicht geladen werden.');
        this.loading.set(false);
      },
    });
  }

  refresh(): void {
    this.loading.set(true);
    this.loadCurrentGame();
  }

  leaveGame(): void {
    if (!window.confirm('Das Spiel wird damit für beide Spieler beendet. Wirklich verlassen?')) {
      return;
    }
    this.leaving.set(true);
    this.errorMessage.set(null);
    this.api.leaveGame().subscribe({
      next: () => {
        this.leaving.set(false);
        this.stopEndPolling();
        this.game.set(null);
        this.gameSocket.disconnect();
      },
      error: (err: HttpErrorResponse) => {
        this.leaving.set(false);
        this.errorMessage.set(
          (err.error as { detail?: string } | null)?.detail ?? 'Spiel konnte nicht verlassen werden.',
        );
      },
    });
  }

  /**
   * Verlaesst der Gegner ein laufendes Spiel, beendet das Backend es fuer
   * beide (siehe radish/backend/api/client_views.py, LeaveGameView) -- davon
   * erfaehrt diese Ansicht nur durch Nachfragen. Fehlgeschlagene Abrufe
   * werden beim naechsten Intervall einfach wiederholt.
   */
  private startEndPolling(): void {
    this.stopEndPolling();
    this.endPollSubscription = interval(5000)
      .pipe(switchMap(() => this.api.getCurrentGame().pipe(catchError(() => of(null)))))
      .subscribe((response) => {
        if (response && response.game === null) {
          this.stopEndPolling();
          this.game.set(null);
          this.ended.set(true);
          this.gameSocket.disconnect();
        }
      });
  }

  private stopEndPolling(): void {
    this.endPollSubscription?.unsubscribe();
    this.endPollSubscription = null;
  }
}
