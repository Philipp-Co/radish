import { HttpErrorResponse } from '@angular/common/http';
import { Component, OnInit, inject, signal } from '@angular/core';
import { ActivatedRoute, Router, RouterLink } from '@angular/router';

import { apiErrorMessage } from '../../../../core/api-error';
import { ApiService, ArmyListEntry } from '../../../../core/api.service';

/**
 * Die eigenen Armeen (api/client/armies/, siehe radish/backend/api/
 * army_views.py): anlegen und bearbeiten laufen ueber ArmyEditComponent
 * (armies/new bzw. armies/:id), geloescht wird direkt hier.
 */
@Component({
  selector: 'app-army-list',
  standalone: true,
  imports: [RouterLink],
  template: `
    <section class="card">
      <div class="list-header">
        <h2>Meine Armeen</h2>
        <button (click)="createArmy()">Neue Armee</button>
      </div>

      @if (errorMessage(); as message) {
        <p class="error">{{ message }}</p>
      }

      @if (loading()) {
        <p class="muted">Lade...</p>
      } @else if (armies().length === 0) {
        <p class="muted">Du hast noch keine Armee.</p>
      } @else {
        <ul class="army-list">
          @for (army of armies(); track army.id) {
            <li>
              <div>
                <a [routerLink]="[army.id]" class="army-name">{{ army.name }}</a>
                <div class="muted">
                  {{ army.species_name }} · {{ army.unit_count }}
                  {{ army.unit_count === 1 ? 'Einheit' : 'Einheiten' }} ·
                  {{ army.total_cost }} Punkte
                </div>
              </div>
              <button
                class="secondary danger"
                [disabled]="deletingId() === army.id"
                (click)="deleteArmy(army)"
              >
                Löschen
              </button>
            </li>
          }
        </ul>
      }
    </section>
  `,
  styles: [
    `
      .list-header {
        display: flex;
        align-items: center;
        justify-content: space-between;
        margin-bottom: 0.75rem;
      }
      .army-list {
        list-style: none;
        margin: 0;
        padding: 0;
      }
      .army-list li {
        display: flex;
        align-items: center;
        justify-content: space-between;
        gap: 1rem;
        padding: 0.6rem 0;
        border-bottom: 1px solid var(--border, #2a2a30);
      }
      .army-name {
        font-weight: 600;
        text-decoration: none;
      }
      .danger {
        color: var(--danger, #d97a7a);
      }
    `,
  ],
})
export class ArmyListComponent implements OnInit {
  private readonly api = inject(ApiService);
  private readonly router = inject(Router);
  private readonly route = inject(ActivatedRoute);

  readonly armies = signal<ArmyListEntry[]>([]);
  readonly loading = signal(true);
  readonly deletingId = signal<number | null>(null);
  readonly errorMessage = signal<string | null>(null);

  ngOnInit(): void {
    this.load();
  }

  createArmy(): void {
    void this.router.navigate(['new'], { relativeTo: this.route });
  }

  deleteArmy(army: ArmyListEntry): void {
    if (!confirm(`Armee "${army.name}" wirklich löschen?`)) {
      return;
    }
    this.deletingId.set(army.id);
    this.errorMessage.set(null);
    this.api.deleteArmy(army.id).subscribe({
      next: () => {
        this.deletingId.set(null);
        this.armies.update((armies) => armies.filter((entry) => entry.id !== army.id));
      },
      error: (err: HttpErrorResponse) => {
        this.deletingId.set(null);
        this.errorMessage.set(apiErrorMessage(err, 'Armee konnte nicht gelöscht werden.'));
      },
    });
  }

  private load(): void {
    this.api.listArmies().subscribe({
      next: (response) => {
        this.armies.set(response.armies);
        this.loading.set(false);
      },
      error: (err: HttpErrorResponse) => {
        this.errorMessage.set(apiErrorMessage(err, 'Armeen konnten nicht geladen werden.'));
        this.loading.set(false);
      },
    });
  }
}
