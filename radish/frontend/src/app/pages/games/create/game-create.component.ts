import { HttpErrorResponse } from '@angular/common/http';
import { Component, inject, signal } from '@angular/core';
import { FormsModule } from '@angular/forms';
import { Router } from '@angular/router';

import { ApiService } from '../../../core/api.service';

@Component({
  selector: 'app-game-create',
  standalone: true,
  imports: [FormsModule],
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
        <button type="submit" [disabled]="creating()">Erstellen</button>
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

  createGame(): void {
    if (!this.name || !this.password) {
      return;
    }
    this.creating.set(true);
    this.errorMessage.set(null);
    this.api.createGame(this.name, this.password).subscribe({
      next: (game) => {
        this.creating.set(false);
        // Nicht mehr direkt auf die eigenstaendige Spielseite ("/game/:name"):
        // die Umleitung nach Erstellen/Beitreten soll zu "Aktuelles Spiel"
        // gehen (siehe CurrentGameComponent, die das Canvas selbst einbettet),
        // nicht direkt auf die reine Canvas-Seite.
        void this.router.navigate(['/games/current']);
      },
      error: (err: HttpErrorResponse) => {
        this.creating.set(false);
        this.errorMessage.set(
          (err.error as { detail?: string } | null)?.detail ?? 'Spiel konnte nicht erstellt werden.',
        );
      },
    });
  }
}
