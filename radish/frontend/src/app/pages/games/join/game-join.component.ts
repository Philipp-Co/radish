import { HttpErrorResponse } from '@angular/common/http';
import { Component, OnInit, inject, signal } from '@angular/core';
import { FormsModule } from '@angular/forms';
import { ActivatedRoute, Router } from '@angular/router';

import { apiErrorMessage } from '../../../core/api-error';
import { ApiService, GameListEntry } from '../../../core/api.service';
import { ArmyPickerComponent } from '../army-picker.component';

@Component({
  selector: 'app-game-join',
  standalone: true,
  imports: [FormsModule, ArmyPickerComponent],
  template: `
    <section class="card">
      <h2>Spiel beitreten</h2>

      @if (errorMessage(); as message) {
        <p class="error">{{ message }}</p>
      }

      <form (ngSubmit)="joinGame()">
        <label>
          Name
          <input type="text" name="name" [(ngModel)]="name" required />
          @if (game(); as found) {
            <span class="muted hint">
              Gespielt wird mit {{ found.points_limit }} Punkten ({{ found.player_count }}/2 Spieler).
            </span>
          }
        </label>
        <label>
          Passwort
          <input type="password" name="password" [(ngModel)]="password" required />
        </label>
        <app-army-picker [pointsLimit]="game()?.points_limit ?? null" [(selectedId)]="armyId" />
        <button type="submit" [disabled]="joining() || !name || !password || armyId === null">
          Beitreten
        </button>
      </form>
    </section>
  `,
  styles: [
    `
      form {
        display: flex;
        flex-direction: column;
        gap: 0.75rem;
      }
      label {
        display: flex;
        flex-direction: column;
        gap: 0.25rem;
        font-size: 0.9rem;
      }
      .hint {
        font-size: 0.8rem;
      }
    `,
  ],
})
export class GameJoinComponent implements OnInit {
  private readonly api = inject(ApiService);
  private readonly router = inject(Router);
  private readonly route = inject(ActivatedRoute);

  readonly joining = signal(false);
  readonly errorMessage = signal<string | null>(null);

  // Aus der Spieleliste vorbelegt, wenn man dort "Beitreten" gewaehlt hat
  // (siehe GamesListComponent.joinGame) -- bleibt trotzdem frei aenderbar,
  // falls man den Namen bereits kennt statt ihn aus der Liste zu waehlen.
  name = this.route.snapshot.queryParamMap.get('name') ?? '';
  password = '';
  armyId: number | null = null;

  /** Offene Spiele, um das Punktelimit zum eingegebenen Namen anzuzeigen. */
  private readonly games = signal<GameListEntry[]>([]);

  ngOnInit(): void {
    this.api.listGames().subscribe({ next: (response) => this.games.set(response.games) });
  }

  /** Das Spiel zum eingegebenen Namen, falls es in der Liste steht. */
  game(): GameListEntry | undefined {
    return this.games().find((entry) => entry.name === this.name.trim());
  }

  joinGame(): void {
    if (!this.name || !this.password || this.armyId === null) {
      return;
    }
    this.joining.set(true);
    this.errorMessage.set(null);
    this.api.joinGame(this.name, this.password, this.armyId).subscribe({
      next: (game) => {
        this.joining.set(false);
        // Erst in die Lobby: das Spiel laeuft erst, wenn der Host es gestartet
        // und die Spielinstanz sich bereit gemeldet hat -- dann fuehrt die
        // Lobby selbst weiter auf "Aktuelles Spiel" (siehe LobbyComponent).
        void this.router.navigate(['/games/lobby']);
      },
      error: (err: HttpErrorResponse) => {
        this.joining.set(false);
        this.errorMessage.set(apiErrorMessage(err, 'Spiel konnte nicht beigetreten werden.'));
      },
    });
  }
}
