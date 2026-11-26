import { HttpErrorResponse } from '@angular/common/http';
import { Component, OnInit, computed, inject, signal } from '@angular/core';
import { FormsModule } from '@angular/forms';
import { Router, RouterLink } from '@angular/router';

import { apiErrorMessage } from '../../../../core/api-error';
import { AdminTerrainType, ApiService } from '../../../../core/api.service';

/**
 * Adminseite "Gelände": alle Tile-Typen in einer Tabelle, darüber eine Suchzeile
 * wie im Armee-Katalog (CatalogEditComponent) -- sie filtert beim Tippen in
 * Bezeichnung und Typname, ohne neue Anfrage. Ein Klick auf eine Zeile führt
 * zur Seite des Typs (TerrainDetailComponent), auf der sich Werte und Aussehen
 * ändern lassen.
 *
 * Anzulegen oder zu löschen gibt es nichts: die Typen gehören zum Spiel, jeder
 * steht genau einmal da (radish/backend/api/interfaces/admin/views/
 * admin_views.py, AdminTerrainTypeListView).
 */
@Component({
  selector: 'app-terrain-list',
  imports: [FormsModule, RouterLink],
  template: `
    <section class="card">
      <h2>Gelände</h2>
      <p class="muted">
        Was jeder Tile-Typ im Spiel bedeutet und wie er aussieht. Werte gelten ab dem nächsten
        Spielstart, das Aussehen ab dem nächsten Laden des Spiels.
      </p>

      <label class="search">
        Suche
        <input
          type="text"
          placeholder="Typ eingeben..."
          [ngModel]="searchTerm()"
          (ngModelChange)="searchTerm.set($event)"
        />
      </label>

      @if (loadError(); as message) {
        <p class="error">{{ message }}</p>
      } @else if (filtered(); as found) {
        <p class="muted count">
          @if (searchTerm().trim()) {
            {{ found.length }} von {{ terrain()?.length ?? 0 }} Typen
          } @else {
            {{ found.length }} Typen
          }
        </p>
        @if (found.length === 0) {
          <p class="muted">Keine Treffer.</p>
        } @else {
          <div class="table-wrap">
          <table class="terrain-table">
            <thead>
              <tr>
                <th>Typ</th>
                <th class="num" title="Bewegung: Betreten kostet 1 − MV, mindestens 1">MV</th>
                <th class="num" title="Deckung">CV</th>
                <th>Eigenschaften</th>
                <th>Aussehen</th>
              </tr>
            </thead>
            <tbody>
              @for (entry of found; track entry.code) {
                <tr (click)="open(entry)">
                  <td>
                    <a [routerLink]="[entry.code]" (click)="$event.stopPropagation()">{{ entry.label }}</a>
                    <div class="muted code">{{ entry.code }}</div>
                  </td>
                  <td class="num">{{ entry.movement_modifier }}</td>
                  <td class="num">{{ entry.cover }}</td>
                  <td class="muted">{{ properties(entry) }}</td>
                  <td class="muted">{{ lookSummary(entry) }}</td>
                </tr>
              }
            </tbody>
          </table>
          </div>
        }
      } @else {
        <p class="muted">Lade...</p>
      }
    </section>
  `,
  styles: [
    `
      .search {
        display: flex;
        flex-direction: column;
        gap: 0.25rem;
        font-size: 0.9rem;
      }
      .count {
        margin: 0.75rem 0 0.25rem;
        font-size: 0.8rem;
      }
      /* Bei schmalem Fenster scrollt die Tabelle in sich statt aus der Karte heraus. */
      .table-wrap {
        overflow-x: auto;
      }
      .terrain-table {
        width: 100%;
        border-collapse: collapse;
      }
      .terrain-table th {
        text-align: left;
        color: var(--text-dim, #8a8a92);
        font-weight: 600;
        font-size: 0.85rem;
        padding: 0.4rem 0.5rem;
        border-bottom: 1px solid var(--border, #2a2a30);
      }
      .terrain-table td {
        padding: 0.5rem;
        border-bottom: 1px solid var(--border, #2a2a30);
      }
      .terrain-table tbody tr {
        cursor: pointer;
      }
      .terrain-table tbody tr:hover {
        background: var(--bg, #0b0b0d);
      }
      .num {
        text-align: right;
        width: 3.5rem;
      }
      .code {
        font-size: 0.75rem;
      }
      td.muted {
        font-size: 0.85rem;
      }
    `,
  ],
})
export class TerrainListComponent implements OnInit {
  private readonly api = inject(ApiService);
  private readonly router = inject(Router);

  /** Alle Typen in der Reihenfolge des Spiels; null, solange sie laden. */
  readonly terrain = signal<AdminTerrainType[] | null>(null);
  readonly loadError = signal<string | null>(null);
  /** Was in der Suchzeile steht -- filtert in Bezeichnung und Typname. */
  readonly searchTerm = signal('');

  readonly filtered = computed(() => {
    const all = this.terrain();
    const term = this.searchTerm().trim().toLowerCase();
    if (all === null || !term) {
      return all;
    }
    return all.filter((entry) => entry.label.toLowerCase().includes(term) || entry.code.includes(term));
  });

  ngOnInit(): void {
    this.api.listTerrainTypes().subscribe({
      next: (terrain) => this.terrain.set(terrain),
      error: (err: HttpErrorResponse) =>
        this.loadError.set(apiErrorMessage(err, 'Gelände konnte nicht geladen werden.')),
    });
  }

  open(entry: AdminTerrainType): void {
    void this.router.navigate(['/games/admin/terrain', entry.code]);
  }

  lookSummary(entry: AdminTerrainType): string {
    return terrainLookSummary(entry);
  }

  /** Unbegehbar und sichtblockierend in einem Wort -- spart zwei Spalten. */
  properties(entry: AdminTerrainType): string {
    const flags = [entry.impassable ? 'unbegehbar' : '', entry.blocks_sight ? 'sichtblockierend' : ''];
    return flags.filter(Boolean).join(', ') || '–';
  }
}

/** Kurz, was ein Typ zeigt -- für Tabelle und Detailseite. */
export function terrainLookSummary(terrain: AdminTerrainType): string {
  const decor = terrain.decor.length === 1 ? '1 Dekoration' : `${terrain.decor.length} Dekorationen`;
  if (terrain.image_id === null) {
    return terrain.decor.length === 0 ? 'Würfel in Typfarbe' : decor;
  }
  return terrain.decor.length === 0 ? 'Grundbild' : `Grundbild, ${decor}`;
}
