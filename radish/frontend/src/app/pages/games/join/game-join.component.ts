import { HttpErrorResponse } from '@angular/common/http';
import { Component, inject, signal } from '@angular/core';
import { FormsModule } from '@angular/forms';
import { ActivatedRoute, Router } from '@angular/router';

import { ApiService } from '../../../core/api.service';

@Component({
  selector: 'app-game-join',
  standalone: true,
  imports: [FormsModule],
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
        </label>
        <label>
          Passwort
          <input type="password" name="password" [(ngModel)]="password" required />
        </label>
        <button type="submit" [disabled]="joining()">Beitreten</button>
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
export class GameJoinComponent {
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

  joinGame(): void {
    if (!this.name || !this.password) {
      return;
    }
    this.joining.set(true);
    this.errorMessage.set(null);
    this.api.joinGame(this.name, this.password).subscribe({
      next: (game) => {
        this.joining.set(false);
        // Nicht mehr direkt auf die eigenstaendige Spielseite ("/game/:name"):
        // die Umleitung nach Erstellen/Beitreten soll zu "Aktuelles Spiel"
        // gehen (siehe CurrentGameComponent, die das Canvas selbst einbettet),
        // nicht direkt auf die reine Canvas-Seite.
        void this.router.navigate(['/games/current']);
      },
      error: (err: HttpErrorResponse) => {
        this.joining.set(false);
        this.errorMessage.set(
          (err.error as { detail?: string } | null)?.detail ?? 'Spiel konnte nicht beigetreten werden.',
        );
      },
    });
  }
}
