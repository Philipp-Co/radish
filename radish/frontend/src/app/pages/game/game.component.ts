import { Component, inject } from '@angular/core';
import { ActivatedRoute, RouterLink } from '@angular/router';

import { GameCanvasComponent } from '../../shared/game-canvas/game-canvas.component';

/**
 * Eigenstaendige Seite fuer "/game/:name" -- reine Chrome (Titel,
 * Zurueck-Link) um die eigentliche Canvas-Komponente herum (siehe
 * shared/game-canvas/, dort auch der Docstring zu deren Trennung von
 * dieser Seite). Wird z.B. nach dem Erstellen/Beitreten eines Spiels
 * angesteuert (siehe GameCreateComponent/GameJoinComponent). Fuer
 * "Aktuelles Spiel" (CurrentGameComponent) wird dieselbe Canvas-Komponente
 * dagegen direkt eingebettet, ohne hierher umzuleiten.
 */
@Component({
  selector: 'app-game',
  standalone: true,
  imports: [RouterLink, GameCanvasComponent],
  template: `
    <main class="page">
      <header class="page-header">
        <h1>Spiel: {{ gameName }}</h1>
        <a routerLink="/games">Zurueck zur Uebersicht</a>
      </header>
      <app-game-canvas [gameName]="gameName" />
    </main>
  `,
  styles: [
    `
      .page {
        max-width: 680px;
        margin: 0 auto;
        padding: 1.5rem 1rem;
        display: flex;
        flex-direction: column;
        gap: 0.75rem;
      }
      .page-header {
        display: flex;
        align-items: center;
        justify-content: space-between;
        gap: 1rem;
      }
    `,
  ],
})
export class GameComponent {
  private readonly route = inject(ActivatedRoute);

  readonly gameName = this.route.snapshot.paramMap.get('name') ?? '';
}
