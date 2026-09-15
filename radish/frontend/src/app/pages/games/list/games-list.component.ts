import { Component, OnDestroy, OnInit, inject, signal } from '@angular/core';
import { Router } from '@angular/router';
import { Subscription, interval, startWith, switchMap } from 'rxjs';

import { ApiService, GameListEntry } from '../../../core/api.service';

@Component({
  selector: 'app-games-list',
  standalone: true,
  template: `
    <section class="card">
      <div class="list-header">
        <h2>Laufende Spiele</h2>
        <button class="secondary" (click)="refresh()">Aktualisieren</button>
      </div>

      @if (errorMessage(); as message) {
        <p class="error">{{ message }}</p>
      }

      @if (loading()) {
        <p class="muted">Lade...</p>
      } @else if (games().length === 0) {
        <p class="muted">Aktuell kein offenes Spiel.</p>
      } @else {
        <ul class="game-list">
          @for (game of games(); track game.name) {
            <li>
              <span>{{ game.name }}</span>
              <span class="muted">{{ game.player_count }}/2</span>
              <button class="secondary" type="button" (click)="joinGame(game.name)">
                Beitreten
              </button>
            </li>
          }
        </ul>
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
      .game-list {
        list-style: none;
        margin: 0;
        padding: 0;
        display: flex;
        flex-direction: column;
        gap: 0.5rem;
      }
      .game-list li {
        display: flex;
        align-items: center;
        gap: 0.75rem;
      }
      .game-list li span:first-child {
        flex: 1;
      }
    `,
  ],
})
export class GamesListComponent implements OnInit, OnDestroy {
  private readonly api = inject(ApiService);
  private readonly router = inject(Router);

  readonly games = signal<GameListEntry[]>([]);
  readonly loading = signal(true);
  readonly errorMessage = signal<string | null>(null);

  private pollSubscription: Subscription | null = null;

  ngOnInit(): void {
    // Kein Server-Push (StreamView in radish/backend/api/client_views.py ist
    // noch ein Platzhalter, siehe dessen Docstring) -- deshalb einfaches
    // Polling alle 5s statt eines Live-Updates.
    this.pollSubscription = interval(5000)
      .pipe(
        startWith(0),
        switchMap(() => this.api.listGames()),
      )
      .subscribe({
        next: (response) => {
          this.games.set(response.games);
          this.loading.set(false);
        },
        error: () => {
          this.errorMessage.set('Spieleliste konnte nicht geladen werden.');
          this.loading.set(false);
        },
      });
  }

  ngOnDestroy(): void {
    this.pollSubscription?.unsubscribe();
  }

  refresh(): void {
    this.loading.set(true);
    this.api.listGames().subscribe({
      next: (response) => {
        this.games.set(response.games);
        this.loading.set(false);
      },
      error: () => {
        this.errorMessage.set('Spieleliste konnte nicht geladen werden.');
        this.loading.set(false);
      },
    });
  }

  /** Fuehrt auf die Beitreten-Seite, mit dem gewaehlten Namen vorbelegt. */
  joinGame(name: string): void {
    void this.router.navigate(['/games/join'], { queryParams: { name } });
  }
}
