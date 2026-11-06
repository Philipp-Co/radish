import { Component, OnInit, inject, signal } from '@angular/core';
import { RouterLink } from '@angular/router';

import { ApiService, GameDetail } from '../../../core/api.service';
import { AuthService } from '../../../core/auth.service';

interface HubTile {
  link: string;
  title: string;
  description: string;
}

const TILES: HubTile[] = [
  {
    link: '/games/current',
    title: 'Aktuelles Spiel',
    description: 'Zurueck ins eigene laufende Spiel.',
  },
  {
    link: '/games/list',
    title: 'Laufende Spiele',
    description: 'Offene Partien im Ueberblick -- und dort einer beitreten.',
  },
  {
    link: '/games/create',
    title: 'Spiel erstellen',
    description: 'Eine neue Lobby eroeffnen und auf einen Gegner warten.',
  },
  {
    link: '/games/armies',
    title: 'Armee',
    description: 'Armeen zusammenstellen und ausruesten.',
  },
];

const ADMIN_TILE: HubTile = {
  link: '/games/admin',
  title: 'Admin',
  description: 'Game-Server, Katalog und Gelaende pflegen.',
};

/**
 * Startseite nach dem Login (siehe app.routes.ts, "games" leitet hierher
 * um) und Hub zu allen Bereichen unter /games. Begruesst den Spieler mit
 * seinem Namen aus dem Access-Token (siehe AuthService.displayName) und
 * hebt ein eigenes Spiel hervor, falls es eins gibt (siehe
 * api/client/game/current/) -- laufend geht es von dort zu "current", in
 * der Lobby zu "lobby". Ohne Spiel steht an derselben Stelle ein gleich
 * aufgebauter Platzhalter, damit die Kacheln darunter nicht springen.
 *
 * Die Kacheln sind die einzige Navigation zwischen den Bereichen
 * (GamesShellComponent hat kein Menue), Admin nur mit der Rolle
 * "Radish-Admin". "Spiel beitreten" fehlt bewusst: das erreicht man nur
 * ueber ein Spiel in "Laufende Spiele".
 */
@Component({
  selector: 'app-home',
  standalone: true,
  imports: [RouterLink],
  template: `
    <section class="welcome">
      <h2>
        @if (auth.displayName(); as name) {
          Willkommen zurueck, {{ name }}
        } @else {
          Willkommen zurueck
        }
      </h2>
      <p class="muted">Was moechtest du als Naechstes tun?</p>
    </section>

    @if (game(); as current) {
      <section class="card current">
        <div>
          <p class="label muted">
            {{ current.status === 'running' ? 'Laufendes Spiel' : 'Wartet in der Lobby' }}
          </p>
          <p class="name"><strong>{{ current.name }}</strong></p>
          <p class="muted">
            {{ current.points_limit }} Punkte ·
            {{ current.host_army_name ?? '–' }} gegen
            {{ current.second_player_army_name ?? 'noch offen' }}
          </p>
        </div>
        <a class="button" [routerLink]="current.status === 'running' ? '/games/current' : '/games/lobby'">
          {{ current.status === 'running' ? 'Weiterspielen' : 'Zur Lobby' }}
        </a>
      </section>
    } @else {
      <!-- Gleich aufgebauter Platzhalter, damit die Kacheln darunter nicht
           springen, wenn ein Spiel dazukommt oder wegfaellt. Steht auch
           waehrend des Ladens da. -->
      <section class="card current placeholder">
        <div>
          <p class="label muted">Kein aktives Spiel</p>
          <p class="name"><strong>Gerade keine Partie</strong></p>
          <p class="muted">Erstelle ein Spiel oder tritt einem offenen bei.</p>
        </div>
        <a class="button" routerLink="/games/list">Spiel suchen</a>
      </section>
    }

    <nav class="tiles" aria-label="Bereiche">
      @for (tile of tiles(); track tile.link) {
        <a class="card tile" [routerLink]="tile.link">
          <strong>{{ tile.title }}</strong>
          <span class="muted">{{ tile.description }}</span>
        </a>
      }
    </nav>
  `,
  styles: [
    `
      /* Ein Abstand fuer alles: zwischen Begruessung, Spielkasten und
         Kacheln derselbe wie zwischen den Kacheln selbst (.tiles). */
      :host {
        --hub-gap: 0.75rem;
        display: flex;
        flex-direction: column;
        gap: var(--hub-gap);
      }
      .welcome h2 {
        margin: 0 0 0.25rem;
        font-size: 1.5rem;
      }
      .welcome p {
        margin: 0;
      }
      .current {
        display: flex;
        align-items: center;
        justify-content: space-between;
        gap: 1rem;
        border-color: var(--tile-hover);
      }
      .current p {
        margin: 0;
      }
      .current .label {
        font-size: 0.8rem;
        text-transform: uppercase;
        letter-spacing: 0.05em;
      }
      .current .name {
        margin: 0.25rem 0;
        font-size: 1.125rem;
      }
      .current.placeholder {
        border-style: dashed;
      }
      /* Link im Look der globalen Buttons (siehe styles.css). */
      .button {
        flex: none;
        padding: 0.6rem 1.1rem;
        border: 1px solid var(--button-hover);
        border-radius: 6px;
        background: var(--button-bg);
        color: var(--button-text);
        font-weight: 600;
        text-decoration: none;
        transition: background 0.15s ease;
      }
      .button:hover {
        background: var(--button-hover);
      }
      .tiles {
        display: grid;
        grid-template-columns: repeat(2, 1fr);
        gap: var(--hub-gap);
      }
      .tile {
        display: flex;
        flex-direction: column;
        gap: 0.35rem;
        color: var(--text, #d8d8dc);
        text-decoration: none;
        transition: border-color 0.15s ease;
      }
      .tile:hover,
      .tile:focus-visible {
        border-color: var(--tile-hover);
        outline: none;
      }
      .tile span {
        font-size: 0.875rem;
        line-height: 1.4;
      }
      @media (max-width: 480px) {
        .tiles {
          grid-template-columns: 1fr;
        }
        .current {
          flex-direction: column;
          align-items: flex-start;
        }
      }
    `,
  ],
})
export class HomeComponent implements OnInit {
  private readonly api = inject(ApiService);
  protected readonly auth = inject(AuthService);

  readonly game = signal<GameDetail | null>(null);

  tiles(): HubTile[] {
    return this.auth.isAdmin() ? [...TILES, ADMIN_TILE] : TILES;
  }

  ngOnInit(): void {
    // Schlaegt die Abfrage fehl, bleibt der Hinweis einfach weg -- die
    // Kacheln funktionieren auch ohne, und "Aktuelles Spiel" zeigt den
    // Fehler dann selbst an.
    this.api.getCurrentGame().subscribe({
      next: (response) => this.game.set(response.game),
      error: () => this.game.set(null),
    });
  }
}
