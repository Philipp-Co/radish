import { HttpErrorResponse } from '@angular/common/http';
import { Component, OnInit, inject, signal } from '@angular/core';
import { FormsModule } from '@angular/forms';
import { ActivatedRoute, RouterLink } from '@angular/router';

import { apiErrorMessage } from '../../../../core/api-error';
import { AdminSpecies, AdminUnitType, ApiService } from '../../../../core/api.service';
import {
  CATALOG_FIELD_LABELS,
  CatalogEntry,
  CatalogModel,
  catalogModel,
} from './catalog-config';
import { CatalogDraft, CatalogEntryFormComponent } from './catalog-entry-form.component';
import { CATALOG_BASE, CatalogPageHeaderComponent } from './catalog-shell.component';

/**
 * "Neu anlegen" fuer ein Katalog-Modell (welches, steht in den Routendaten,
 * siehe app.routes.ts). Zeigt nur das leere Formular und -- je nach Modell
 * -- die Auswahl von Spezies bzw. Einheitentyp, unter dem der Eintrag
 * entsteht. ?species= und ?unit_type= belegen diese Auswahl vor (z.B. vom
 * Link "Profil anlegen" auf der Seite eines Einheitentyps).
 */
@Component({
  selector: 'app-catalog-create',
  standalone: true,
  imports: [FormsModule, RouterLink, CatalogEntryFormComponent, CatalogPageHeaderComponent],
  template: `
    <section class="card">
      <app-catalog-page-header [model]="model" />

      @if (created(); as entry) {
        <p class="success">
          {{ model.singular }} „{{ entry.name }}“ angelegt.
          <a [routerLink]="editLink" [queryParams]="{ id: entry.id }">Bearbeiten</a>
        </p>
      }

      @if (model.parent !== null) {
        @if (species().length === 0) {
          <p class="muted">
            Es gibt noch keine Spezies.
            <a [routerLink]="base + '/species/new'">Zuerst eine Spezies anlegen.</a>
          </p>
        } @else {
          <div class="parents">
            <label>
              Spezies
              <select [ngModel]="speciesId()" (ngModelChange)="selectSpecies($event)">
                @for (entry of species(); track entry.id) {
                  <option [ngValue]="entry.id">{{ entry.name }}</option>
                }
              </select>
            </label>
            @if (model.parent === 'unit-type') {
              <label>
                Einheitentyp
                <select [(ngModel)]="unitTypeId" [disabled]="unitTypes().length === 0">
                  @for (type of unitTypes(); track type.id) {
                    <option [ngValue]="type.id">{{ type.name }}</option>
                  }
                </select>
              </label>
            }
          </div>
          @if (model.parent === 'unit-type' && unitTypes().length === 0) {
            <p class="muted">
              Für diese Spezies gibt es noch keine Einheitentypen.
              <a [routerLink]="base + '/unit-types/new'" [queryParams]="{ species: speciesId() }">
                Zuerst einen Einheitentyp anlegen.
              </a>
            </p>
          }
        }
      }

      <app-catalog-entry-form
        [fields]="model.fields"
        [value]="blank()"
        [isNew]="true"
        [busy]="saving()"
        [error]="errorMessage()"
        (save)="create($event)"
      />
    </section>
  `,
  styles: [
    `
      .parents {
        display: grid;
        grid-template-columns: 1fr 1fr;
        gap: 0.75rem;
        margin-bottom: 0.75rem;
      }
      label {
        display: flex;
        flex-direction: column;
        gap: 0.25rem;
        font-size: 0.9rem;
      }
      .success {
        color: var(--accent, #5fb9a3);
      }
    `,
  ],
})
export class CatalogCreateComponent implements OnInit {
  private readonly api = inject(ApiService);
  private readonly route = inject(ActivatedRoute);

  readonly model: CatalogModel = catalogModel(this.route.snapshot.data['resource']);
  readonly base = CATALOG_BASE;
  readonly editLink = `${CATALOG_BASE}/${this.model.resource}/edit`;

  readonly species = signal<AdminSpecies[]>([]);
  readonly speciesId = signal<number | null>(null);
  readonly unitTypes = signal<AdminUnitType[]>([]);
  unitTypeId: number | null = null;

  /** Ein frisches Objekt setzt das Formular zurueck (siehe CatalogEntryFormComponent). */
  readonly blank = signal<object>(this.model.blank());
  readonly created = signal<CatalogEntry | null>(null);
  readonly saving = signal(false);
  readonly errorMessage = signal<string | null>(null);

  ngOnInit(): void {
    if (this.model.parent === null) {
      return;
    }
    const params = this.route.snapshot.queryParamMap;
    const wantedSpecies = Number(params.get('species'));
    const wantedUnitType = Number(params.get('unit_type'));

    this.api.listCatalogEntries<AdminSpecies>('species').subscribe({
      next: (species) => {
        this.species.set(species);
        const initial = species.find((entry) => entry.id === wantedSpecies) ?? species[0];
        if (initial) {
          this.selectSpecies(initial.id, wantedUnitType);
        }
      },
      error: (err: HttpErrorResponse) =>
        this.errorMessage.set(apiErrorMessage(err, 'Spezies konnten nicht geladen werden.')),
    });
  }

  selectSpecies(speciesId: number, wantedUnitType?: number): void {
    this.speciesId.set(speciesId);
    if (this.model.parent !== 'unit-type') {
      return;
    }
    this.unitTypes.set([]);
    this.unitTypeId = null;
    this.api.listCatalogEntries<AdminUnitType>('unit-types', { species: speciesId }).subscribe({
      next: (types) => {
        this.unitTypes.set(types);
        this.unitTypeId = (types.find((type) => type.id === wantedUnitType) ?? types[0])?.id ?? null;
      },
      error: (err: HttpErrorResponse) =>
        this.errorMessage.set(apiErrorMessage(err, 'Einheitentypen konnten nicht geladen werden.')),
    });
  }

  create(draft: CatalogDraft): void {
    const entry: Record<string, unknown> = { ...draft };
    if (this.model.parent === 'species') {
      entry['species_id'] = this.speciesId();
    } else if (this.model.parent === 'unit-type') {
      if (this.unitTypeId === null) {
        this.errorMessage.set('Bitte zuerst einen Einheitentyp wählen.');
        return;
      }
      entry['unit_type_id'] = this.unitTypeId;
    }

    this.saving.set(true);
    this.errorMessage.set(null);
    this.created.set(null);
    this.api.createCatalogEntry<CatalogEntry>(this.model.resource, entry).subscribe({
      next: (saved) => {
        this.saving.set(false);
        this.created.set(saved);
        this.blank.set(this.model.blank());
      },
      error: (err: HttpErrorResponse) => {
        this.saving.set(false);
        this.errorMessage.set(
          apiErrorMessage(err, `${this.model.singular} konnte nicht angelegt werden.`, CATALOG_FIELD_LABELS),
        );
      },
    });
  }
}
