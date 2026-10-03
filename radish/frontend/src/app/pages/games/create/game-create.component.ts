import { HttpErrorResponse } from '@angular/common/http';
import { Component, inject, signal } from '@angular/core';
import { FormsModule } from '@angular/forms';
import { Router } from '@angular/router';

import { apiErrorMessage } from '../../../core/api-error';
import { ApiService } from '../../../core/api.service';
import { ArmyPickerComponent } from '../army-picker.component';

@Component({
  selector: 'app-game-create',
  standalone: true,
  imports: [FormsModule, ArmyPickerComponent],
  template: `
    <section class="card">
      <h2>Spiel erstellen</h2>

      @if (errorMessage(); as message) {
        <p class="error">{{ message }}</p>
      }

      <form (ngSubmit)="createGame()">
        <label>
          Name
          <input type="text" name="name" [(ngModel)]="name" required />
        </label>
        <label>
          Passwort
          <input type="password" name="password" [(ngModel)]="password" required />
        </label>
        <label>
          Punkte
          <input
            type="number"
            name="pointsLimit"
            min="1"
            placeholder="z.B. 500"
            [(ngModel)]="pointsLimit"
            required
          />
          <span class="muted hint">Keine der beiden Armeen darf mehr Punkte kosten.</span>
        </label>
        <app-army-picker [pointsLimit]="pointsLimit" [(selectedId)]="armyId" />
        <button type="submit" [disabled]="creating() || !canCreate()">Erstellen</button>
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
export class GameCreateComponent {
  private readonly api = inject(ApiService);
  private readonly router = inject(Router);

  readonly creating = signal(false);
  readonly errorMessage = signal<string | null>(null);

  name = '';
  password = '';
  pointsLimit: number | null = null;
  armyId: number | null = null;

  canCreate(): boolean {
    return (
      !!this.name && !!this.password && !!this.pointsLimit && this.pointsLimit > 0 && this.armyId !== null
    );
  }

  createGame(): void {
    if (!this.canCreate() || this.pointsLimit === null || this.armyId === null) {
      return;
    }
    this.creating.set(true);
    this.errorMessage.set(null);
    this.api.createGame(this.name, this.password, this.pointsLimit, this.armyId).subscribe({
      next: (game) => {
        this.creating.set(false);
        // Erst in die Lobby: das Spiel laeuft erst, wenn der Host es gestartet
        // und die Spielinstanz sich bereit gemeldet hat -- dann fuehrt die
        // Lobby selbst weiter auf "Aktuelles Spiel" (siehe LobbyComponent).
        void this.router.navigate(['/games/lobby']);
      },
      error: (err: HttpErrorResponse) => {
        this.creating.set(false);
        this.errorMessage.set(apiErrorMessage(err, 'Spiel konnte nicht erstellt werden.'));
      },
    });
  }
}
