import { HttpErrorResponse } from '@angular/common/http';
import { Component, OnInit, inject, signal } from '@angular/core';
import { FormsModule } from '@angular/forms';
import { ActivatedRoute, Router } from '@angular/router';

import { apiErrorMessage } from '../../../core/api-error';
import { ApiService, GameListEntry } from '../../../core/api.service';
import { ArmyPickerComponent } from '../army-picker.component';

/**
 * Beitreten zu einem offenen Spiel. Nur ueber "Beitreten" in der Spieleliste
 * erreichbar (siehe GamesListComponent.joinGame), die den Namen als
 * "?name=" mitgibt -- der Name steht damit fest und ist hier nicht aenderbar.
 * Ohne Namen geht es direkt zurueck zur Liste.
 */
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
        <div class="game-name">
          <span class="muted">Spiel</span>
          <strong>{{ name }}</strong>
          @if (game(); as found) {
            <span class="muted hint">
              Gespielt wird mit {{ found.points_limit }} Punkten ({{ found.player_count }}/2 Spieler).
            </span>
          }
        </div>
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
      label,
      .game-name {
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

  /** Fest aus der Spieleliste uebernommen, siehe Docstring oben. */
  readonly name = this.route.snapshot.queryParamMap.get('name')?.trim() ?? '';
  password = '';
  armyId: number | null = null;

  /**
   * Das offene Spiel zu diesem Namen, um dessen Punktelimit anzuzeigen. Die
   * Spieleliste ist seitenweise (siehe GamesListComponent), deshalb wird per
   * Namensfilter gezielt nachgeschlagen. Schlaegt das fehl, fehlt nur der
   * Hinweis -- beitreten geht trotzdem.
   */
  readonly game = signal<GameListEntry | null>(null);

  ngOnInit(): void {
    if (!this.name) {
      void this.router.navigate(['/games/list']);
      return;
    }
    this.api.findGame(this.name).subscribe({
      next: (game) => this.game.set(game),
      error: () => this.game.set(null),
    });
  }

  joinGame(): void {
    if (!this.name || !this.password || this.armyId === null) {
      return;
    }
    this.joining.set(true);
    this.errorMessage.set(null);
    this.api.joinGame(this.name, this.password, this.armyId).subscribe({
      next: () => {
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
