import { Component, EventEmitter, Input, OnChanges, OnInit, Output, inject, signal } from '@angular/core';
import { RouterLink } from '@angular/router';

import { ApiService, ArmyListEntry } from '../../core/api.service';

/**
 * Auswahl der eigenen Armee fuer "Spiel erstellen" und "Spiel beitreten".
 * Laedt die Armeen selbst (api/client/armies/) und zeigt je Armee Spezies,
 * Einheiten und Kosten. Armeen ohne Einheiten oder ueber pointsLimit sind
 * nicht waehlbar -- dieselben Regeln prueft das Backend beim Erstellen/
 * Beitreten (client_views._army_for_game). Ist pointsLimit null (z.B. beim
 * Beitreten, solange das Spiel unbekannt ist), gilt nur die erste Regel.
 */
@Component({
  selector: 'app-army-picker',
  standalone: true,
  imports: [RouterLink],
  template: `
    <fieldset>
      <legend>Armee</legend>
      @if (loading()) {
        <p class="muted">Lade Armeen...</p>
      } @else if (error()) {
        <p class="error">{{ error() }}</p>
      } @else if (armies().length === 0) {
        <p class="muted">
          Du hast noch keine Armee.
          <a routerLink="/games/armies/new">Armee erstellen</a>
        </p>
      } @else {
        <ul>
          @for (army of armies(); track army.id) {
            <li [class.disabled]="reason(army) !== null">
              <label>
                <input
                  type="radio"
                  name="army"
                  [value]="army.id"
                  [checked]="army.id === selectedId"
                  [disabled]="reason(army) !== null"
                  (change)="select(army.id)"
                />
                <span class="name">{{ army.name }}</span>
                <span class="muted">
                  {{ army.species_name }} · {{ army.unit_count }}
                  {{ army.unit_count === 1 ? 'Einheit' : 'Einheiten' }}
                </span>
                <span class="cost">{{ army.total_cost }} P</span>
              </label>
              @if (reason(army); as text) {
                <span class="reason">{{ text }}</span>
              }
            </li>
          }
        </ul>
      }
    </fieldset>
  `,
  styles: [
    `
      fieldset {
        border: none;
        padding: 0;
        margin: 0;
      }
      legend {
        font-size: 0.9rem;
        margin-bottom: 0.35rem;
      }
      ul {
        list-style: none;
        margin: 0;
        padding: 0;
        border: 1px solid var(--border, #2a2a30);
        border-radius: 8px;
      }
      li {
        padding: 0.45rem 0.6rem;
        border-bottom: 1px solid var(--border, #2a2a30);
      }
      li:last-child {
        border-bottom: none;
      }
      li.disabled {
        opacity: 0.55;
      }
      label {
        display: flex;
        align-items: center;
        gap: 0.6rem;
        cursor: pointer;
        font-size: 0.9rem;
      }
      li.disabled label {
        cursor: not-allowed;
      }
      .name {
        font-weight: 600;
      }
      .muted {
        flex: 1;
        font-size: 0.8rem;
      }
      .cost {
        font-weight: 600;
        white-space: nowrap;
      }
      .reason {
        display: block;
        margin-left: 1.6rem;
        font-size: 0.75rem;
        color: var(--danger, #d97a7a);
      }
    `,
  ],
})
export class ArmyPickerComponent implements OnInit, OnChanges {
  private readonly api = inject(ApiService);

  @Input() pointsLimit: number | null = null;
  @Input() selectedId: number | null = null;
  @Output() readonly selectedIdChange = new EventEmitter<number | null>();

  readonly armies = signal<ArmyListEntry[]>([]);
  readonly loading = signal(true);
  readonly error = signal<string | null>(null);

  ngOnInit(): void {
    this.api.listArmies().subscribe({
      next: (response) => {
        this.armies.set(response.armies);
        this.loading.set(false);
        this.dropInvalidSelection();
      },
      error: () => {
        this.error.set('Armeen konnten nicht geladen werden.');
        this.loading.set(false);
      },
    });
  }

  ngOnChanges(): void {
    // Ein geaendertes Punktelimit kann die gewaehlte Armee zu teuer machen.
    this.dropInvalidSelection();
  }

  /** Warum die Armee nicht waehlbar ist, oder null. */
  reason(army: ArmyListEntry): string | null {
    if (army.unit_count === 0) {
      return 'Hat noch keine Einheiten.';
    }
    if (this.pointsLimit !== null && army.total_cost > this.pointsLimit) {
      return `Kostet ${army.total_cost} Punkte, erlaubt sind ${this.pointsLimit}.`;
    }
    return null;
  }

  select(id: number): void {
    this.selectedId = id;
    this.selectedIdChange.emit(id);
  }

  private dropInvalidSelection(): void {
    if (this.loading() || this.selectedId === null) {
      return;
    }
    const selected = this.armies().find((army) => army.id === this.selectedId);
    if (!selected || this.reason(selected) !== null) {
      this.selectedId = null;
      // Nicht synchron aus ngOnChanges heraus in die Elternkomponente
      // schreiben -- das waere eine Aenderung waehrend ihrer eigenen
      // Change Detection.
      queueMicrotask(() => this.selectedIdChange.emit(null));
    }
  }
}
