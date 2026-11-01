import { Component, inject } from '@angular/core';
import { Router, RouterLink } from '@angular/router';

import { AuthService } from '../../core/auth.service';

interface IsoTile {
  points: string;
  kind: 'open' | 'cover' | 'blocked';
}

interface IsoUnit {
  x: number;
  y: number;
  side: 'own' | 'enemy';
}

// Kantenlaengen einer Raute im isometrischen Raster (halbe Breite/Hoehe).
const TILE_HW = 32;
const TILE_HH = 16;
const GRID_SIZE = 6;
const ORIGIN_X = GRID_SIZE * TILE_HW;
const ORIGIN_Y = 8;

// Rein dekorative Gelaendeverteilung fuer das Hero-Bild.
const COVER_TILES = new Set(['1,3', '2,3', '4,1']);
const BLOCKED_TILES = new Set(['3,0', '0,5', '5,4']);

function tileCenter(col: number, row: number): { x: number; y: number } {
  return {
    x: ORIGIN_X + (col - row) * TILE_HW,
    y: ORIGIN_Y + (col + row) * TILE_HH + TILE_HH,
  };
}

function buildTiles(): IsoTile[] {
  const tiles: IsoTile[] = [];
  for (let row = 0; row < GRID_SIZE; row++) {
    for (let col = 0; col < GRID_SIZE; col++) {
      const { x, y } = tileCenter(col, row);
      const key = `${col},${row}`;
      tiles.push({
        points: `${x},${y - TILE_HH} ${x + TILE_HW},${y} ${x},${y + TILE_HH} ${x - TILE_HW},${y}`,
        kind: BLOCKED_TILES.has(key) ? 'blocked' : COVER_TILES.has(key) ? 'cover' : 'open',
      });
    }
  }
  return tiles;
}

function buildUnits(): IsoUnit[] {
  return [
    { ...tileCenter(1, 1), side: 'own' },
    { ...tileCenter(2, 0), side: 'own' },
    { ...tileCenter(4, 4), side: 'enemy' },
  ];
}

@Component({
  selector: 'app-landing',
  standalone: true,
  imports: [RouterLink],
  template: `
    <header class="topbar">
      <a routerLink="/" class="brand">radish</a>
      <button class="secondary" (click)="primaryAction()">{{ primaryLabel() }}</button>
    </header>

    <main>
      <section class="hero">
        <div class="hero-text">
          <h1>Rundenbasierte Taktik im Browser</h1>
          <p class="lead muted">
            Stelle deine Armee zusammen, nutze das Gelaende und schlage deinen Gegner Zug um Zug --
            ohne Installation, direkt im Browser.
          </p>
          <div class="hero-actions">
            <button (click)="primaryAction()">{{ heroLabel() }}</button>
            <button class="secondary" (click)="scrollTo(howTo)">So funktioniert's</button>
          </div>
        </div>

        <svg
          class="hero-map"
          viewBox="0 0 384 216"
          role="img"
          aria-label="Isometrisches Spielfeld mit Einheiten"
        >
          @for (tile of tiles; track $index) {
            <polygon [attr.points]="tile.points" [class]="'tile tile-' + tile.kind" />
          }
          @for (unit of units; track $index) {
            <g [class]="'unit unit-' + unit.side">
              <ellipse [attr.cx]="unit.x" [attr.cy]="unit.y + 4" rx="12" ry="5" class="unit-shadow" />
              <rect [attr.x]="unit.x - 7" [attr.y]="unit.y - 20" width="14" height="22" rx="4" />
            </g>
          }
        </svg>
      </section>

      <section class="features" aria-label="Was radish ausmacht">
        <article class="card">
          <h2>Armee bauen</h2>
          <p class="muted">
            Waehle Einheitentypen aus dem Katalog und ruestet sie mit Profilen und Waffen aus.
          </p>
        </article>
        <article class="card">
          <h2>Taktisch ziehen</h2>
          <p class="muted">
            Offenes Feld, Deckung oder unpassierbar: Das Gelaende bestimmt Bewegung und Treffer.
          </p>
        </article>
        <article class="card">
          <h2>Direkt im Browser</h2>
          <p class="muted">
            Der Spielclient laeuft als WebAssembly im Browser -- einloggen und losspielen.
          </p>
        </article>
      </section>

      <section class="how-to" #howTo>
        <h2>So funktioniert's</h2>
        <ol class="steps">
          <li>
            <span class="step-no">1</span>
            <div><strong>Einloggen</strong><p class="muted">Mit deinem radish-Konto anmelden.</p></div>
          </li>
          <li>
            <span class="step-no">2</span>
            <div>
              <strong>Armee zusammenstellen</strong>
              <p class="muted">Einheiten waehlen und ausruesten.</p>
            </div>
          </li>
          <li>
            <span class="step-no">3</span>
            <div>
              <strong>Spiel erstellen oder beitreten</strong>
              <p class="muted">Eine Lobby eroeffnen oder einem offenen Spiel beitreten.</p>
            </div>
          </li>
          <li>
            <span class="step-no">4</span>
            <div>
              <strong>Zug um Zug spielen</strong>
              <p class="muted">Der Host beginnt, dann wird abwechselnd gezogen.</p>
            </div>
          </li>
        </ol>
      </section>

      <section class="final-cta">
        <h2>Bereit fuer die erste Partie?</h2>
        <button (click)="primaryAction()">{{ heroLabel() }}</button>
      </section>
    </main>

    <footer class="footer muted">radish</footer>
  `,
  styles: [
    `
      :host {
        display: block;
        min-height: 100vh;
      }
      .topbar,
      main,
      .footer {
        max-width: 1040px;
        margin: 0 auto;
        padding-left: 1rem;
        padding-right: 1rem;
      }
      .topbar {
        display: flex;
        align-items: center;
        justify-content: space-between;
        padding-top: 1rem;
        padding-bottom: 1rem;
      }
      .brand {
        font-size: 1.25rem;
        font-weight: 700;
        color: var(--text);
        text-decoration: none;
      }

      .hero {
        display: grid;
        grid-template-columns: 1fr 1fr;
        align-items: center;
        gap: 2rem;
        padding: 3rem 0 4rem;
      }
      .hero h1 {
        font-size: clamp(2rem, 5vw, 3rem);
        line-height: 1.1;
        margin: 0 0 1rem;
      }
      .lead {
        font-size: 1.125rem;
        line-height: 1.5;
        margin: 0 0 1.5rem;
        max-width: 32rem;
      }
      .hero-actions {
        display: flex;
        flex-wrap: wrap;
        gap: 0.75rem;
      }
      .hero-map {
        width: 100%;
        height: auto;
      }
      .tile {
        stroke: var(--bg);
        stroke-width: 1.5;
      }
      .tile-open {
        fill: color-mix(in srgb, var(--accent) 22%, var(--bg-elevated));
      }
      .tile-cover {
        fill: color-mix(in srgb, var(--accent) 45%, var(--bg-elevated));
      }
      .tile-blocked {
        fill: var(--border);
      }
      .unit-shadow {
        fill: rgba(0, 0, 0, 0.45);
      }
      .unit-own rect {
        fill: var(--accent-hover);
      }
      .unit-enemy rect {
        fill: var(--danger);
      }

      .features {
        display: grid;
        grid-template-columns: repeat(3, 1fr);
        gap: 1rem;
      }
      .features h2 {
        font-size: 1.125rem;
        margin: 0 0 0.5rem;
      }
      .features p {
        margin: 0;
        line-height: 1.5;
      }

      .how-to {
        padding: 4rem 0 2rem;
        scroll-margin-top: 1rem;
      }
      .how-to h2,
      .final-cta h2 {
        font-size: 1.5rem;
        margin: 0 0 1.5rem;
      }
      .steps {
        list-style: none;
        margin: 0;
        padding: 0;
        display: grid;
        grid-template-columns: repeat(4, 1fr);
        gap: 1.5rem;
      }
      .steps li {
        display: flex;
        gap: 0.75rem;
      }
      .steps p {
        margin: 0.25rem 0 0;
        line-height: 1.4;
      }
      .step-no {
        flex: none;
        width: 2rem;
        height: 2rem;
        border-radius: 50%;
        border: 1px solid var(--accent);
        color: var(--accent);
        display: flex;
        align-items: center;
        justify-content: center;
        font-weight: 600;
      }

      .final-cta {
        text-align: center;
        padding: 3rem 0 4rem;
      }

      .footer {
        border-top: 1px solid var(--border);
        padding-top: 1.25rem;
        padding-bottom: 1.25rem;
        font-size: 0.875rem;
      }

      @media (max-width: 720px) {
        .hero {
          grid-template-columns: 1fr;
          padding: 1.5rem 0 3rem;
        }
        .features,
        .steps {
          grid-template-columns: 1fr;
        }
      }
      @media (min-width: 721px) and (max-width: 960px) {
        .steps {
          grid-template-columns: repeat(2, 1fr);
        }
      }
    `,
  ],
})
export class LandingComponent {
  private readonly auth = inject(AuthService);
  private readonly router = inject(Router);

  readonly tiles = buildTiles();
  readonly units = buildUnits();

  primaryLabel(): string {
    return this.auth.isAuthenticated() ? 'Zu meinen Spielen' : 'Einloggen';
  }

  heroLabel(): string {
    return this.auth.isAuthenticated() ? 'Zu meinen Spielen' : 'Jetzt spielen';
  }

  primaryAction(): void {
    if (this.auth.isAuthenticated()) {
      void this.router.navigateByUrl('/games');
    } else {
      void this.auth.login('/games');
    }
  }

  scrollTo(element: HTMLElement): void {
    element.scrollIntoView({ behavior: 'smooth' });
  }
}
