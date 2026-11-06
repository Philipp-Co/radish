import { Component, OnDestroy, OnInit, computed, inject, signal } from '@angular/core';
import { Router } from '@angular/router';
import {
  EMPTY,
  Subject,
  Subscription,
  catchError,
  interval,
  map,
  merge,
  startWith,
  switchMap,
} from 'rxjs';

import { ApiService, GameListEntry, GameListPage } from '../../../core/api.service';

/**
 * Offene Spiele in der Lobby, seitenweise (siehe radish/backend/api/
 * client_views.py, GameListView -- dort auch die Seitengroesse). Unten
 * blaettert man mit "Zurueck"/"Weiter"; das Polling laedt immer die gerade
 * gewaehlte Seite neu.
 */
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
              <span class="muted">{{ game.points_limit }} P</span>
              <span class="muted">{{ game.player_count }}/2</span>
              <button class="secondary" type="button" (click)="joinGame(game.name)">
                Beitreten
              </button>
            </li>
          }
        </ul>

        <nav class="pagination" aria-label="Seiten">
          <button
            class="secondary"
            type="button"
            [disabled]="page() <= 1"
            (click)="goToPage(page() - 1)"
          >
            ← Zurueck
          </button>
          <span class="muted">Seite {{ page() }} von {{ pageCount() }}</span>
          <button
            class="secondary"
            type="button"
            [disabled]="page() >= pageCount()"
            (click)="goToPage(page() + 1)"
          >
            Weiter →
          </button>
        </nav>
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
      .pagination {
        display: flex;
        align-items: center;
        justify-content: space-between;
        gap: 0.75rem;
        margin-top: 1rem;
        padding-top: 0.75rem;
        border-top: 1px solid var(--border);
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
  readonly page = signal(1);
  private readonly count = signal(0);
  private readonly pageSize = signal(1);
  readonly pageCount = computed(() => Math.max(1, Math.ceil(this.count() / this.pageSize())));

  /** Seitenwechsel per Button -- laedt sofort statt erst beim naechsten Poll. */
  private readonly pageRequests = new Subject<number>();
  private pollSubscription: Subscription | null = null;

  ngOnInit(): void {
    // Kein Server-Push fuer die Spieleliste (anders als das aktuelle Spiel,
    // siehe CurrentGameComponent/GameSocketService, die ueber den WebSocket
    // laufen) -- deshalb einfaches Polling alle 5s statt eines Live-Updates.
    // switchMap verwirft dabei eine noch laufende Abfrage fuer eine alte Seite.
    this.pollSubscription = merge(
      interval(5000).pipe(
        startWith(0),
        map(() => this.page()),
      ),
      this.pageRequests,
    )
      .pipe(
        switchMap((page) =>
          // Fehler je Abfrage abfangen, damit das Polling weiterlaeuft.
          this.api.listGames(page).pipe(
            catchError(() => {
              this.errorMessage.set('Spieleliste konnte nicht geladen werden.');
              this.loading.set(false);
              return EMPTY;
            }),
          ),
        ),
      )
      .subscribe((response) => this.applyPage(response));
  }

  ngOnDestroy(): void {
    this.pollSubscription?.unsubscribe();
  }

  refresh(): void {
    this.loading.set(true);
    this.errorMessage.set(null);
    this.pageRequests.next(this.page());
  }

  goToPage(page: number): void {
    this.page.set(page);
    this.pageRequests.next(page);
  }

  private applyPage(response: GameListPage): void {
    this.games.set(response.games);
    this.count.set(response.count);
    this.pageSize.set(response.page_size);
    // Das Backend liefert bei einer weggefallenen Seite die letzte vorhandene.
    this.page.set(response.page);
    this.errorMessage.set(null);
    this.loading.set(false);
  }

  /** Fuehrt auf die Beitreten-Seite, mit dem gewaehlten Namen vorbelegt. */
  joinGame(name: string): void {
    void this.router.navigate(['/games/join'], { queryParams: { name } });
  }
}
