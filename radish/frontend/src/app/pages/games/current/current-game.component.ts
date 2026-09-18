import { HttpErrorResponse } from '@angular/common/http';
import { Component, OnDestroy, OnInit, inject, signal } from '@angular/core';

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
          <app-game-canvas [gameName]="current.name" />
          <div class="actions">
            <button type="button" class="secondary" [disabled]="leaving()" (click)="leaveGame()">
              Spiel verlassen
            </button>
          </div>
        } @else {
          <p class="muted">Aktuell kein laufendes Spiel.</p>
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

  readonly game = signal<GameDetail | null>(null);
  readonly loading = signal(true);
  readonly leaving = signal(false);
  readonly errorMessage = signal<string | null>(null);

  ngOnInit(): void {
    this.loadCurrentGame();
  }

  ngOnDestroy(): void {
    this.gameSocket.disconnect();
  }

  /**
   * Kein Polling mehr: der Zustand aendert sich nicht, waehrend man auf
   * dieser Ansicht bleibt, deshalb reicht eine einzelne Abfrage beim
   * Oeffnen der Ansicht (bzw. per "Aktualisieren"-Button, siehe refresh()).
   * Liefert getCurrentGame() ein Spiel, wird zusaetzlich die
   * WebSocket-Verbindung geoeffnet (siehe GameSocketService).
   */
  private loadCurrentGame(): void {
    this.api.getCurrentGame().subscribe({
      next: (response) => {
        this.game.set(response.game);
        this.loading.set(false);
        if (response.game) {
          this.gameSocket.connect();
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
    if (!window.confirm('Aktuelles Spiel wirklich verlassen?')) {
      return;
    }
    this.leaving.set(true);
    this.errorMessage.set(null);
    this.api.leaveGame().subscribe({
      next: () => {
        this.leaving.set(false);
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
}
