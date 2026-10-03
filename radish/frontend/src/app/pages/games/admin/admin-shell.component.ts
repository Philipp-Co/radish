import { Component } from '@angular/core';
import { RouterLink, RouterLinkActive, RouterOutlet } from '@angular/router';

/**
 * Rahmen um den Adminbereich: "Game-Server" (servers/, siehe
 * AdminComponent) und "Armee-Katalog" (catalog/, siehe
 * CatalogAdminComponent). Eine Unternavigation statt weiterer Tabs im
 * Hauptmenue -- dort ist neben den Spieler-Tabs kein Platz mehr.
 */
@Component({
  selector: 'app-admin-shell',
  standalone: true,
  imports: [RouterLink, RouterLinkActive, RouterOutlet],
  template: `
    <nav class="subnav">
      <a routerLink="servers" routerLinkActive="active">Game-Server</a>
      <a routerLink="catalog" routerLinkActive="active">Armee-Katalog</a>
    </nav>
    <router-outlet />
  `,
  styles: [
    `
      :host {
        display: flex;
        flex-direction: column;
        gap: 1rem;
      }
      .subnav {
        display: flex;
        gap: 1rem;
      }
      .subnav a {
        color: var(--text-dim, #8a8a92);
        text-decoration: none;
        font-weight: 600;
        font-size: 0.9rem;
      }
      .subnav a.active {
        color: var(--accent, #5fb9a3);
      }
    `,
  ],
})
export class AdminShellComponent {}
