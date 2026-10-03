import { Component, Input } from '@angular/core';
import { RouterLink, RouterLinkActive, RouterOutlet } from '@angular/router';

import { CATALOG_MODELS, CatalogModel } from './catalog-config';

export const CATALOG_BASE = '/games/admin/catalog';

/**
 * Rahmen um die Katalog-Pflege: je Modell ein Tab, darunter dessen Seite
 * "Neu anlegen" oder "Ändern" (siehe CatalogCreateComponent,
 * CatalogEditComponent). Bewusst keine Seite mit allen Eintraegen auf
 * einmal -- geaendert wird ueber die Suche.
 */
@Component({
  selector: 'app-catalog-shell',
  standalone: true,
  imports: [RouterLink, RouterLinkActive, RouterOutlet],
  template: `
    <nav class="model-tabs">
      @for (model of models; track model.resource) {
        <a [routerLink]="base + '/' + model.resource" routerLinkActive="active">
          {{ model.title }}
        </a>
      }
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
      .model-tabs {
        display: flex;
        flex-wrap: wrap;
        gap: 0.4rem;
      }
      .model-tabs a {
        padding: 0.3rem 0.7rem;
        border: 1px solid var(--border, #2a2a30);
        border-radius: 999px;
        color: var(--text-dim, #8a8a92);
        text-decoration: none;
        font-size: 0.85rem;
      }
      .model-tabs a.active {
        color: var(--text, #d8d8dc);
        border-color: var(--accent, #5fb9a3);
      }
    `,
  ],
})
export class CatalogShellComponent {
  readonly models = CATALOG_MODELS;
  readonly base = CATALOG_BASE;
}

/** Kopf einer Katalogseite: Titel des Modells und Wechsel zwischen Neu/Ändern. */
@Component({
  selector: 'app-catalog-page-header',
  standalone: true,
  imports: [RouterLink, RouterLinkActive],
  template: `
    <div class="header">
      <h2>{{ model.title }}</h2>
      <nav>
        <a [routerLink]="base + '/' + model.resource + '/new'" routerLinkActive="active">
          Neu anlegen
        </a>
        <a [routerLink]="base + '/' + model.resource + '/edit'" routerLinkActive="active">
          Ändern
        </a>
      </nav>
    </div>
  `,
  styles: [
    `
      .header {
        display: flex;
        align-items: center;
        justify-content: space-between;
        margin-bottom: 0.75rem;
      }
      h2 {
        margin: 0;
      }
      nav {
        display: flex;
        gap: 0.75rem;
      }
      nav a {
        color: var(--text-dim, #8a8a92);
        text-decoration: none;
        font-weight: 600;
        font-size: 0.9rem;
      }
      nav a.active {
        color: var(--accent, #5fb9a3);
      }
    `,
  ],
})
export class CatalogPageHeaderComponent {
  @Input({ required: true }) model!: CatalogModel;
  readonly base = CATALOG_BASE;
}
