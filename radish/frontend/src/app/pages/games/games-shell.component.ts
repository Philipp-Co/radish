import { Component, inject } from '@angular/core';
import { RouterLink, RouterLinkActive, RouterOutlet } from '@angular/router';

import { AuthService } from '../../core/auth.service';

/**
 * Rahmen um die Unterseiten "Aktuelles Spiel" (current/), "Laufende Spiele"
 * (list/), "Spiel erstellen" (create/), "Spiel beitreten" (join/) und,
 * nur fuer Nutzer mit der Keycloak-Realm-Rolle "Radish-Admin", "Admin"
 * (admin/) -- siehe app.routes.ts, dort als Kind-Routen von "games"
 * eingehaengt. Der authGuard sitzt an dieser Elternroute und gilt damit
 * fuer alle Unterseiten mit; admin/ hat zusaetzlich noch adminGuard.
 */
@Component({
  selector: 'app-games-shell',
  standalone: true,
  imports: [RouterLink, RouterLinkActive, RouterOutlet],
  template: `
    <main class="page">
      <header class="page-header">
        <h1>radish</h1>
        <button class="secondary" (click)="logout()">Logout</button>
      </header>

      <nav class="tabs">
        <a routerLink="current" routerLinkActive="active">Aktuelles Spiel</a>
        <a routerLink="list" routerLinkActive="active">Laufende Spiele</a>
        <a routerLink="create" routerLinkActive="active">Spiel erstellen</a>
        <a routerLink="join" routerLinkActive="active">Spiel beitreten</a>
        @if (auth.isAdmin()) {
          <a routerLink="admin" routerLinkActive="active">Admin</a>
        }
      </nav>

      <router-outlet />
    </main>
  `,
  styles: [
    `
      .page {
        max-width: 640px;
        margin: 0 auto;
        padding: 1.5rem 1rem;
        display: flex;
        flex-direction: column;
        gap: 1.25rem;
      }
      .page-header {
        display: flex;
        align-items: center;
        justify-content: space-between;
      }
      .tabs {
        display: flex;
        gap: 0.5rem;
        border-bottom: 1px solid var(--border, #2a2a30);
      }
      .tabs a {
        padding: 0.6rem 1rem;
        color: var(--text-dim, #8a8a92);
        text-decoration: none;
        border-bottom: 2px solid transparent;
        font-weight: 600;
      }
      .tabs a.active {
        color: var(--text, #d8d8dc);
        border-bottom-color: var(--accent, #5fb9a3);
      }
    `,
  ],
})
export class GamesShellComponent {
  // Nicht mehr private: das Template liest auth.isAdmin() direkt, um
  // den Admin-Tab nur fuer die Keycloak-Realm-Rolle "Radish-Admin"
  // anzuzeigen (siehe AuthService.isAdmin).
  protected readonly auth = inject(AuthService);

  logout(): void {
    this.auth.logout();
  }
}
