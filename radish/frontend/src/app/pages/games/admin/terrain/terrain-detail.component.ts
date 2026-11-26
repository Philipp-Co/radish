import { HttpErrorResponse } from '@angular/common/http';
import { Component, OnInit, inject, signal } from '@angular/core';
import { FormsModule } from '@angular/forms';
import { ActivatedRoute, RouterLink } from '@angular/router';

import { apiErrorMessage } from '../../../../core/api-error';
import { AdminTerrainType, AdminTerrainValues, ApiService } from '../../../../core/api.service';
import { TerrainLookComponent } from './terrain-look.component';
import { terrainLookSummary } from './terrain-list.component';

const FIELD_LABELS: Record<string, string> = {
  movement_modifier: 'MV',
  cover: 'CV',
  impassable: 'Unbegehbar',
  blocks_sight: 'Sichtblockierend',
};

/**
 * Die Seite eines Tile-Typs (admin/terrain/<code>), erreicht über die Tabelle
 * (TerrainListComponent): oben die Werte für das Spiel (MV, CV, unbegehbar,
 * sichtblockierend), darunter das Aussehen -- PNGs, Grundbild und Dekorationen
 * mit Vorschau (TerrainLookComponent).
 */
@Component({
  selector: 'app-terrain-detail',
  imports: [FormsModule, RouterLink, TerrainLookComponent],
  template: `
    <p class="back"><a routerLink="/games/admin/terrain">← Gelände</a></p>

    @if (loadError(); as message) {
      <p class="error">{{ message }}</p>
    }

    @if (terrain(); as entry) {
      <section class="card">
        <h2>
          {{ entry.label }}
          <span class="muted">{{ entry.code }} · {{ lookSummary(entry) }}</span>
        </h2>

        <h3>Werte</h3>
        <p class="muted hint">
          MV ändert die Bewegungsreichweite: ein Feld zu betreten kostet 1 − MV, mindestens 1. CV
          erschwert den Treffer auf eine Einheit darauf (W10 ≥ Genauigkeit + CV). Ein
          sichtblockierendes Feld zwischen Schütze und Ziel verhindert den Angriff; das Feld selbst
          bleibt angreifbar. Änderungen gelten ab dem nächsten Spielstart.
        </p>

        <form class="values" (ngSubmit)="save(entry)">
          <label>
            MV
            <input type="number" name="movement_modifier" min="-10" max="10" [(ngModel)]="draft.movement_modifier" />
          </label>
          <label>
            CV
            <input type="number" name="cover" min="0" max="10" [(ngModel)]="draft.cover" />
          </label>
          <label class="check">
            <input type="checkbox" name="impassable" [(ngModel)]="draft.impassable" />
            Unbegehbar
          </label>
          <label class="check">
            <input type="checkbox" name="blocks_sight" [(ngModel)]="draft.blocks_sight" />
            Sichtblockierend
          </label>
          <div class="actions">
            <button type="submit" [disabled]="!changed(entry) || saving()">
              {{ saving() ? 'Speichere...' : 'Werte speichern' }}
            </button>
            @if (changed(entry)) {
              <button type="button" class="secondary" [disabled]="saving()" (click)="reset(entry)">Verwerfen</button>
            }
          </div>
        </form>
        @if (error(); as message) {
          <p class="error">{{ message }}</p>
        }
        @if (notice(); as message) {
          <p class="success">{{ message }}</p>
        }
      </section>

      <section class="card">
        <app-terrain-look [terrain]="entry" (updated)="lookUpdated($event)" />
      </section>
    } @else if (!loadError()) {
      <p class="muted">Lade...</p>
    }
  `,
  styles: [
    `
      :host {
        display: flex;
        flex-direction: column;
        gap: 1rem;
      }
      .back {
        margin: 0;
        font-size: 0.9rem;
      }
      h2 .muted {
        font-size: 0.85rem;
        font-weight: 400;
        margin-left: 0.5rem;
      }
      h3 {
        margin: 1rem 0 0.25rem;
      }
      .hint {
        font-size: 0.85rem;
        margin-top: 0;
      }
      .values {
        display: flex;
        flex-wrap: wrap;
        align-items: flex-end;
        gap: 1rem;
      }
      .values label {
        display: flex;
        flex-direction: column;
        gap: 0.25rem;
        font-size: 0.9rem;
      }
      .values input[type='number'] {
        width: 5rem;
      }
      .values label.check {
        flex-direction: row;
        align-items: center;
        gap: 0.35rem;
      }
      .actions {
        display: flex;
        gap: 0.5rem;
      }
      .success {
        color: var(--accent, #5fb9a3);
      }
    `,
  ],
})
export class TerrainDetailComponent implements OnInit {
  private readonly api = inject(ApiService);
  private readonly route = inject(ActivatedRoute);

  readonly terrain = signal<AdminTerrainType | null>(null);
  readonly loadError = signal<string | null>(null);
  readonly saving = signal(false);
  readonly error = signal<string | null>(null);
  readonly notice = signal<string | null>(null);

  /** Was in den Feldern der Werte steht. */
  draft: AdminTerrainValues = { movement_modifier: 0, cover: 0, impassable: false, blocks_sight: false };

  readonly lookSummary = terrainLookSummary;

  ngOnInit(): void {
    const code = this.route.snapshot.paramMap.get('code') ?? '';
    this.api.getTerrainType(code).subscribe({
      next: (terrain) => {
        this.terrain.set(terrain);
        this.reset(terrain);
      },
      error: (err: HttpErrorResponse) =>
        this.loadError.set(apiErrorMessage(err, `Geländetyp „${code}“ konnte nicht geladen werden.`)),
    });
  }

  changed(terrain: AdminTerrainType): boolean {
    const saved = values(terrain);
    return (Object.keys(saved) as (keyof AdminTerrainValues)[]).some((key) => this.draft[key] !== saved[key]);
  }

  reset(terrain: AdminTerrainType): void {
    this.draft = values(terrain);
  }

  save(terrain: AdminTerrainType): void {
    this.saving.set(true);
    this.error.set(null);
    this.notice.set(null);
    this.api.updateTerrainType(terrain.code, this.draft).subscribe({
      next: (saved) => {
        this.saving.set(false);
        this.terrain.set(saved);
        this.reset(saved);
        this.notice.set('Werte gespeichert.');
      },
      error: (err: HttpErrorResponse) => {
        this.saving.set(false);
        this.error.set(apiErrorMessage(err, 'Werte nicht gespeichert.', FIELD_LABELS));
      },
    });
  }

  /** Das Aussehen hat sich geändert: die Werte im Formular bleiben, wie sie stehen. */
  lookUpdated(terrain: AdminTerrainType): void {
    this.terrain.set(terrain);
  }
}

function values(terrain: AdminTerrainType): AdminTerrainValues {
  return {
    movement_modifier: terrain.movement_modifier,
    cover: terrain.cover,
    impassable: terrain.impassable,
    blocks_sight: terrain.blocks_sight,
  };
}
