import { Component, HostListener, inject, signal } from '@angular/core';
import { RouterLink, RouterLinkActive, RouterOutlet } from '@angular/router';

import { AuthService } from '../../core/auth.service';

/**
 * Rahmen um die Unterseiten "Aktuelles Spiel" (current/), "Laufende Spiele"
 * (list/), "Spiel erstellen" (create/), "Spiel beitreten" (join/), "Armee"
 * (armies/, samt armies/new und armies/:id) und,
 * nur fuer Nutzer mit der Keycloak-Realm-Rolle "Radish-Admin", "Admin"
 * (admin/) -- siehe app.routes.ts, dort als Kind-Routen von "games"
 * eingehaengt. Der authGuard sitzt an dieser Elternroute und gilt damit
 * fuer alle Unterseiten mit; admin/ hat zusaetzlich noch adminGuard.
 *
 * Die Navigation sitzt in einem Burgermenue oben links, das als Leiste
 * von links hereinfaehrt. Es schliesst sich nach einem Klick auf einen
 * Eintrag, auf den abgedunkelten Hintergrund oder mit Escape.
 */
@Component({
  selector: 'app-games-shell',
  standalone: true,
  imports: [RouterLink, RouterLinkActive, RouterOutlet],
  template: `
    <header class="topbar">
      <button
        class="secondary burger"
        type="button"
        aria-label="Menü"
        aria-controls="main-menu"
        [attr.aria-expanded]="menuOpen()"
        (click)="toggleMenu()"
      >
        <span></span>
        <span></span>
        <span></span>
      </button>
      <h1>radish</h1>
    </header>

    @if (menuOpen()) {
      <div class="backdrop" (click)="closeMenu()"></div>
    }

    <nav id="main-menu" class="drawer" [class.open]="menuOpen()" [attr.inert]="menuOpen() ? null : ''">
      <a routerLink="current" routerLinkActive="active" (click)="closeMenu()">Aktuelles Spiel</a>
      <a routerLink="list" routerLinkActive="active" (click)="closeMenu()">Laufende Spiele</a>
      <a routerLink="create" routerLinkActive="active" (click)="closeMenu()">Spiel erstellen</a>
      <a routerLink="join" routerLinkActive="active" (click)="closeMenu()">Spiel beitreten</a>
      <a routerLink="armies" routerLinkActive="active" (click)="closeMenu()">Armee</a>
      @if (auth.isAdmin()) {
        <a routerLink="admin" routerLinkActive="active" (click)="closeMenu()">Admin</a>
      }
      <button class="secondary logout" type="button" (click)="logout()">Logout</button>
    </nav>

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
      .burger {
        display: flex;
        flex-direction: column;
        justify-content: center;
        gap: 5px;
        width: 2.5rem;
        height: 2.5rem;
        padding: 0 0.55rem;
      }
      .burger span {
        display: block;
        height: 2px;
        background: var(--text, #d8d8dc);
        border-radius: 1px;
      }
      .burger:hover span {
        background: var(--accent, #5fb9a3);
      }
      .backdrop {
        position: fixed;
        inset: 0;
        z-index: 20;
        background: rgba(0, 0, 0, 0.5);
      }
      .drawer {
        position: fixed;
        top: 0;
        bottom: 0;
        left: 0;
        z-index: 30;
        width: min(260px, 80vw);
        display: flex;
        flex-direction: column;
        gap: 0.25rem;
        padding: 1rem 0.75rem;
        background: var(--bg-elevated, #17171b);
        border-right: 1px solid var(--border, #2a2a30);
        transform: translateX(-100%);
        transition: transform 0.2s ease;
      }
      .drawer.open {
        transform: translateX(0);
      }
      .drawer a {
        padding: 0.6rem 0.75rem;
        color: var(--text-dim, #8a8a92);
        text-decoration: none;
        font-weight: 600;
        border-left: 2px solid transparent;
        border-radius: 4px;
      }
      .drawer a:hover {
        color: var(--text, #d8d8dc);
      }
      .drawer a.active {
        color: var(--text, #d8d8dc);
        border-left-color: var(--accent, #5fb9a3);
      }
      .drawer .logout {
        margin-top: auto;
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
  // Nicht mehr private: das Template liest auth.isAdmin() direkt, um
  // den Admin-Eintrag nur fuer die Keycloak-Realm-Rolle "Radish-Admin"
  // anzuzeigen (siehe AuthService.isAdmin).
  protected readonly auth = inject(AuthService);

  protected readonly menuOpen = signal(false);

  toggleMenu(): void {
    this.menuOpen.update((open) => !open);
  }

  closeMenu(): void {
    this.menuOpen.set(false);
  }

  @HostListener('document:keydown.escape')
  onEscape(): void {
    this.closeMenu();
  }

  logout(): void {
    this.auth.logout();
  }
}
