import { Component, inject } from '@angular/core';
import { toSignal } from '@angular/core/rxjs-interop';
import { NavigationEnd, Router, RouterLink, RouterOutlet } from '@angular/router';
import { filter, map } from 'rxjs';

import { AuthService } from '../../core/auth.service';

/**
 * Rahmen um die Unterseiten "Start" (home/, der Hub nach dem Login),
 * "Aktuelles Spiel" (current/), "Laufende Spiele"
 * (list/), "Spiel erstellen" (create/), "Spiel beitreten" (join/), "Armee"
 * (armies/, samt armies/new und armies/:id) und,
 * nur fuer Nutzer mit der Keycloak-Realm-Rolle "Radish-Admin", "Admin"
 * (admin/) -- siehe app.routes.ts, dort als Kind-Routen von "games"
 * eingehaengt. Der authGuard sitzt an dieser Elternroute und gilt damit
 * fuer alle Unterseiten mit; admin/ hat zusaetzlich noch adminGuard.
 *
 * Ein eigenes Menue gibt es nicht: navigiert wird ueber die Kacheln im Hub
 * (siehe HomeComponent). In der Kopfleiste steht links der Logout, rechts
 * auf allen Unterseiten ausser dem Hub selbst ein Zurueck-Button: meist
 * "Zum Hub" (home/), auf Unterseiten zweiter Ebene dagegen zu ihrer Liste:
 * "Spiel beitreten" (join/) zu "Laufende Spiele" (list/), eine einzelne
 * Armee (armies/:id, armies/new) zu "Armeen" (armies/) -- siehe backLink.
 */
@Component({
  selector: 'app-games-shell',
  imports: [RouterLink, RouterOutlet],
  template: `
    <header class="topbar">
      <button class="secondary logout" type="button" (click)="logout()">Logout</button>
      <h1>radish</h1>
      @if (backLink(); as back) {
        <a class="hub-link" [routerLink]="back.link">← {{ back.label }}</a>
      }
    </header>

    <main class="page">
      <router-outlet />
    </main>
  `,
  styles: [
    `
      .topbar {
        position: sticky;
        top: 0;
        z-index: 10;
        display: flex;
        align-items: center;
        gap: 0.75rem;
        padding: 0.5rem 1rem;
        background: var(--bg, #0b0b0d);
        border-bottom: 1px solid var(--border, #2a2a30);
      }
      .topbar h1 {
        margin: 0;
        font-size: 1.25rem;
      }
      /* Kompakter als die globalen Buttons, gleich gross wie .hub-link. */
      .logout {
        padding: 0.45rem 0.9rem;
      }
      /* Link im Look der globalen Buttons (siehe styles.css). */
      .hub-link {
        margin-left: auto;
        padding: 0.45rem 0.9rem;
        border: 1px solid var(--button-hover);
        border-radius: 6px;
        background: var(--button-bg);
        color: var(--button-text);
        font-weight: 600;
        text-decoration: none;
        transition: background 0.15s ease;
      }
      .hub-link:hover {
        background: var(--button-hover);
      }
      .page {
        max-width: 640px;
        margin: 0 auto;
        padding: 1.5rem 1rem;
        display: flex;
        flex-direction: column;
        gap: 1.25rem;
      }
    `,
  ],
})
export class GamesShellComponent {
  private readonly auth = inject(AuthService);
  private readonly router = inject(Router);

  /**
   * Ziel des Zurueck-Buttons oben rechts: auf dem Hub (home/) keiner,
   * auf "Spiel beitreten" (join/) zurueck zur Spieleliste und in einer
   * einzelnen Armee (armies/...) zurueck zur Armeeliste -- jeweils dorthin,
   * von wo man die Seite geoeffnet hat --, sonst zum Hub.
   */
  protected readonly backLink = toSignal(
    this.router.events.pipe(
      filter((event): event is NavigationEnd => event instanceof NavigationEnd),
      map((event) => this.backLinkFor(event.urlAfterRedirects)),
    ),
    { initialValue: this.backLinkFor(this.router.url) },
  );

  private backLinkFor(url: string): { link: string; label: string } | null {
    const path = this.router.parseUrl(url).root.children['primary']?.toString();
    if (path === 'games/home') {
      return null;
    }
    if (path === 'games/join') {
      return { link: 'list', label: 'Laufende Spiele' };
    }
    // Armee-Detail (armies/:id) und neue Armee (armies/new) -- zurueck zur
    // Armeeliste, von der aus man sie geoeffnet hat.
    if (path?.startsWith('games/armies/')) {
      return { link: 'armies', label: 'Armeen' };
    }
    return { link: 'home', label: 'Zum Hub' };
  }

  logout(): void {
    this.auth.logout();
  }
}
