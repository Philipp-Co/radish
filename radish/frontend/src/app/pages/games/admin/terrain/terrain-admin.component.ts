import { HttpErrorResponse } from '@angular/common/http';
import { Component, OnInit, inject, signal } from '@angular/core';
import { FormsModule } from '@angular/forms';

import { apiErrorMessage } from '../../../../core/api-error';
import { AdminTerrainType, AdminTerrainValues, ApiService } from '../../../../core/api.service';

/** Ein Tile-Typ in der Tabelle: was gespeichert ist und was gerade eingetragen steht. */
interface TerrainRow {
  saved: AdminTerrainType;
  draft: AdminTerrainValues;
  saving: boolean;
  error: string | null;
}

const FIELD_LABELS: Record<string, string> = {
  movement_modifier: 'MV',
  cover: 'CV',
  impassable: 'Unbegehbar',
};

/**
 * Adminseite "Gelände": die Werte der Tile-Typen (radish/backend/api/
 * admin_views.py, AdminTerrainTypeListView). Anders als der Armee-Katalog
 * gibt es nichts anzulegen oder zu löschen -- die Typen gehören zum Spiel,
 * jeder steht genau einmal da. Geändert wird je Zeile; die Werte gehen mit
 * dem nächsten Spielstart an die Instanz, laufende Spiele behalten ihre.
 */
@Component({
  selector: 'app-terrain-admin',
  standalone: true,
  imports: [FormsModule],
  template: `
    <section class="card">
      <h2>Gelände</h2>
      <p class="muted">
        MV ändert die Bewegungsreichweite: ein Feld zu betreten kostet 1 − MV, mindestens 1.
        CV erschwert den Treffer auf eine Einheit darauf (W10 ≥ Genauigkeit + CV).
        Änderungen gelten ab dem nächsten Spielstart.
      </p>

      @if (loadError(); as message) {
        <p class="error">{{ message }}</p>
      }

      @if (loading()) {
        <p class="muted">Lade...</p>
      } @else {
        <table class="terrain-table">
          <thead>
            <tr>
              <th>Typ</th>
              <th>MV</th>
              <th>CV</th>
              <th>Unbegehbar</th>
              <th></th>
            </tr>
          </thead>
          <tbody>
            @for (row of rows(); track row.saved.code) {
              <tr>
                <td>{{ row.saved.label }}</td>
                <td>
                  <input type="number" min="-10" max="10" [(ngModel)]="row.draft.movement_modifier" />
                </td>
                <td>
                  <input type="number" min="0" max="10" [(ngModel)]="row.draft.cover" />
                </td>
                <td>
                  <input type="checkbox" [(ngModel)]="row.draft.impassable" />
                </td>
                <td class="actions">
                  <button [disabled]="!changed(row) || row.saving" (click)="save(row)">
                    {{ row.saving ? 'Speichere...' : 'Speichern' }}
                  </button>
                </td>
              </tr>
              @if (row.error) {
                <tr>
                  <td colspan="5" class="error">{{ row.error }}</td>
                </tr>
              }
            }
          </tbody>
        </table>
      }
    </section>
  `,
  styles: [
    `
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
        padding: 0.4rem 0.5rem;
        border-bottom: 1px solid var(--border, #2a2a30);
      }
      .terrain-table input[type='number'] {
        width: 4.5rem;
      }
      .actions {
        text-align: right;
      }
    `,
  ],
})
export class TerrainAdminComponent implements OnInit {
  private readonly api = inject(ApiService);

  readonly rows = signal<TerrainRow[]>([]);
  readonly loading = signal(true);
  readonly loadError = signal<string | null>(null);

  ngOnInit(): void {
    this.api.listTerrainTypes().subscribe({
      next: (terrain) => {
        this.rows.set(terrain.map((saved) => ({ saved, draft: values(saved), saving: false, error: null })));
        this.loading.set(false);
      },
      error: (err: HttpErrorResponse) => {
        this.loadError.set(apiErrorMessage(err, 'Gelände konnte nicht geladen werden.'));
        this.loading.set(false);
      },
    });
  }

  changed(row: TerrainRow): boolean {
    const saved = values(row.saved);
    return (
      row.draft.movement_modifier !== saved.movement_modifier ||
      row.draft.cover !== saved.cover ||
      row.draft.impassable !== saved.impassable
    );
  }

  save(row: TerrainRow): void {
    this.update(row, { saving: true, error: null });
    this.api.updateTerrainType(row.saved.code, row.draft).subscribe({
      next: (saved) => this.update(row, { saved, draft: values(saved), saving: false }),
      error: (err: HttpErrorResponse) =>
        this.update(row, {
          saving: false,
          error: apiErrorMessage(err, 'Speichern fehlgeschlagen.', FIELD_LABELS),
        }),
    });
  }

  /** Ersetzt die Zeile, damit das Signal die Aenderung meldet. */
  private update(row: TerrainRow, changes: Partial<TerrainRow>): void {
    this.rows.update((rows) =>
      rows.map((entry) => (entry.saved.code === row.saved.code ? { ...entry, ...changes } : entry)),
    );
  }
}

function values(terrain: AdminTerrainType): AdminTerrainValues {
  return {
    movement_modifier: terrain.movement_modifier,
    cover: terrain.cover,
    impassable: terrain.impassable,
  };
}
