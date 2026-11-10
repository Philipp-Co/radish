import { HttpErrorResponse } from '@angular/common/http';
import { Component, DestroyRef, OnInit, computed, inject, signal } from '@angular/core';
import { takeUntilDestroyed } from '@angular/core/rxjs-interop';
import { FormsModule } from '@angular/forms';
import { ActivatedRoute, Router, RouterLink } from '@angular/router';
import { Subject, catchError, of, switchMap } from 'rxjs';

import { apiErrorMessage } from '../../../../core/api-error';
import { AdminSpecies, AdminUnitType, ApiService } from '../../../../core/api.service';
import {
  CATALOG_FIELD_LABELS,
  CatalogEntry,
  CatalogModel,
  catalogModel,
  entryContext,
} from './catalog-config';
import { CatalogDraft, CatalogEntryFormComponent } from './catalog-entry-form.component';
import { CATALOG_BASE, CatalogPageHeaderComponent } from './catalog-shell.component';
import { ProfileOverviewComponent } from './profile-overview.component';
import { SpriteImagesComponent } from './sprite-images.component';

/**
 * "Ändern" fuer ein Katalog-Modell: unter der Suchzeile alle Eintraege
 * (optional auf eine Spezies oder einen Zusatzfilter eingeschraenkt), die
 * Suchzeile filtert sie beim Tippen im Namen -- ohne Gross-/Kleinschreibung
 * und ohne neue Anfrage. Geladen wird die Liste einmal und bei jedem
 * geaenderten Filter neu. Der gewaehlte Eintrag steht als ?id= in der URL,
 * damit "Bearbeiten" nach dem Anlegen direkt hierher fuehren kann.
 */
@Component({
  selector: 'app-catalog-edit',
  imports: [
    FormsModule,
    RouterLink,
    CatalogEntryFormComponent,
    CatalogPageHeaderComponent,
    ProfileOverviewComponent,
    SpriteImagesComponent,
  ],
  template: `
    <section class="card">
      <app-catalog-page-header [model]="model" />

      <div
        class="search"
        [class.with-filter]="model.parent !== null && !model.extraFilter"
        [class.with-two-filters]="model.parent !== null && !!model.extraFilter"
      >
        <label>
          Suche
          <input
            type="text"
            placeholder="Name eingeben..."
            [ngModel]="searchTerm()"
            (ngModelChange)="searchTerm.set($event)"
          />
        </label>
        @if (model.parent !== null) {
          <label>
            Spezies
            <select [ngModel]="speciesFilter" (ngModelChange)="filterSpecies($event)">
              <option [ngValue]="null">Alle</option>
              @for (entry of species(); track entry.id) {
                <option [ngValue]="entry.id">{{ entry.name }}</option>
              }
            </select>
          </label>
        }
        @if (model.extraFilter; as filter) {
          <label>
            {{ filter.label }}
            <select [ngModel]="extraFilterValue" (ngModelChange)="filterExtra($event)">
              <option [ngValue]="null">Alle</option>
              @for (option of filter.options; track option.value) {
                <option [ngValue]="option.value">{{ option.label }}</option>
              }
            </select>
          </label>
        }
      </div>

      @if (searchError(); as message) {
        <p class="error">{{ message }}</p>
      }

      @if (filtered(); as found) {
        <p class="muted count">
          @if (searchTerm().trim()) {
            {{ found.length }} von {{ results()?.length ?? 0 }} Einträgen
          } @else {
            {{ found.length }} Einträge
          }
        </p>
        @if (found.length === 0) {
          <p class="muted">Keine Treffer.</p>
        } @else {
          <ul class="results">
            @for (entry of found; track entry.id) {
              <li>
                <button
                  class="result"
                  [class.selected]="entry.id === selected()?.id"
                  (click)="select(entry.id)"
                >
                  <span>{{ entry.name }}</span>
                  <span class="muted">{{ context(entry) }}</span>
                </button>
              </li>
            }
          </ul>
        }
      }
    </section>

    @if (loadError(); as message) {
      <p class="error">{{ message }}</p>
    }

    @if (message(); as text) {
      <p class="success">{{ text }}</p>
    }

    @if (selected(); as entry) {
      <section class="card">
        <h3>
          {{ entry.name }}
          @if (context(entry); as ctx) {
            <span class="muted">{{ ctx }}</span>
          }
        </h3>

        <app-catalog-entry-form
          [fields]="formFields"
          [value]="entry"
          [inUse]="entry.in_use ?? false"
          [busy]="saving()"
          [error]="errorMessage()"
          (save)="save(entry, $event)"
          (remove)="remove(entry)"
        />

        @if (entry.unit_type_id !== undefined) {
          <p class="hint">
            Gehört zu
            <a [routerLink]="base + '/unit-types/edit'" [queryParams]="{ id: entry.unit_type_id }">
              {{ entry.unit_type_name }}
            </a>
          </p>
        }
      </section>

      @if (asUnitType(entry); as unitType) {
        <section class="card">
          <app-profile-overview [unitType]="unitType" />
        </section>
        <section class="card">
          <app-sprite-images
            [spriteKey]="unitType.sprite"
            [unitTypeId]="unitType.id"
            [busy]="saving()"
            (save)="saveSprite(entry, $event)"
          />
        </section>
      }
    }
  `,
  styles: [
    `
      :host {
        display: flex;
        flex-direction: column;
        gap: 1rem;
      }
      .search {
        display: grid;
        grid-template-columns: 1fr;
        gap: 0.75rem;
      }
      .search.with-filter {
        grid-template-columns: 2fr 1fr;
      }
      .search.with-two-filters {
        grid-template-columns: 2fr 1fr 1fr;
      }
      label {
        display: flex;
        flex-direction: column;
        gap: 0.25rem;
        font-size: 0.9rem;
      }
      .count {
        margin: 0.75rem 0 0;
        font-size: 0.8rem;
      }
      /* Die ganze Liste, aber nicht beliebig lang: sie scrollt in sich. */
      .results {
        list-style: none;
        margin: 0.25rem 0 0;
        padding: 0;
        max-height: 22rem;
        overflow-y: auto;
      }
      .result {
        width: 100%;
        display: flex;
        justify-content: space-between;
        gap: 1rem;
        background: transparent;
        color: var(--text, #d8d8dc);
        border: none;
        border-bottom: 1px solid var(--border, #2a2a30);
        border-radius: 0;
        padding: 0.5rem 0.25rem;
        font-weight: 400;
        text-align: left;
      }
      .result:hover:not(:disabled),
      .result.selected {
        background: var(--bg, #0b0b0d);
      }
      h3 {
        margin-top: 0;
      }
      h3 .muted {
        font-size: 0.85rem;
        font-weight: 400;
        margin-left: 0.5rem;
      }
      .hint {
        font-size: 0.8rem;
      }
      .success {
        color: var(--accent, #5fb9a3);
        margin: 0;
      }
    `,
  ],
})
export class CatalogEditComponent implements OnInit {
  private readonly api = inject(ApiService);
  private readonly route = inject(ActivatedRoute);
  private readonly router = inject(Router);
  private readonly destroyRef = inject(DestroyRef);

  readonly model: CatalogModel = catalogModel(this.route.snapshot.data['resource']);
  readonly base = CATALOG_BASE;
  readonly context = entryContext;

  /**
   * Die Felder des Formulars -- ohne das Bild eines Einheitentyps: das waehlt
   * und speichert hier die Bild-Sektion darunter (SpriteImagesComponent), mit
   * Vorschau. Beim Anlegen steht es weiter im Formular.
   */
  readonly formFields = this.model.fields.filter((field) => field.optionsFrom !== 'unit-sprites');

  readonly species = signal<AdminSpecies[]>([]);
  /** Alle Eintraege zu Spezies und Zusatzfilter; null, solange sie laden. */
  readonly results = signal<CatalogEntry[] | null>(null);
  /** Was in der Suchzeile steht -- filtert "results" im Namen. */
  readonly searchTerm = signal('');
  /** Die Eintraege, deren Name den Suchbegriff enthaelt. */
  readonly filtered = computed(() => {
    const term = this.searchTerm().trim().toLocaleLowerCase();
    const all = this.results();
    return all === null || !term ? all : all.filter((entry) => entry.name.toLocaleLowerCase().includes(term));
  });
  readonly selected = signal<CatalogEntry | null>(null);
  readonly saving = signal(false);
  readonly message = signal<string | null>(null);
  readonly errorMessage = signal<string | null>(null);
  readonly searchError = signal<string | null>(null);
  readonly loadError = signal<string | null>(null);

  speciesFilter: number | null = null;
  extraFilterValue: string | null = null;
  private readonly searches = new Subject<void>();

  ngOnInit(): void {
    if (this.model.parent !== null) {
      this.api
        .listCatalogEntries<AdminSpecies>('species')
        .subscribe({ next: (species) => this.species.set(species) });
    }

    this.searches
      .pipe(
        switchMap(() =>
          // Fehler hier abfangen: im aeusseren Stream wuerde er die Liste
          // fuer den Rest der Seite beenden.
          this.api
            .listCatalogEntries<CatalogEntry>(this.model.resource, {
              species: this.speciesFilter ?? undefined,
              ...(this.model.extraFilter && this.extraFilterValue !== null
                ? { [this.model.extraFilter.key]: this.extraFilterValue }
                : {}),
            })
            .pipe(
              catchError((err: HttpErrorResponse) => {
                this.searchError.set(apiErrorMessage(err, 'Liste konnte nicht geladen werden.'));
                return of(null);
              }),
            ),
        ),
        takeUntilDestroyed(this.destroyRef),
      )
      .subscribe((results) => {
        if (results !== null) {
          this.searchError.set(null);
        }
        this.results.set(results);
      });
    this.searches.next();

    // Dieselbe Komponente bleibt beim Wechsel von ?id=1 auf ?id=2 bestehen.
    this.route.queryParamMap.pipe(takeUntilDestroyed(this.destroyRef)).subscribe((params) => {
      const id = Number(params.get('id'));
      this.errorMessage.set(null);
      if (!id) {
        this.selected.set(null);
        return;
      }
      if (id !== this.selected()?.id) {
        this.message.set(null);
      }
      this.load(id);
    });
  }

  filterSpecies(speciesId: number | null): void {
    this.speciesFilter = speciesId;
    this.searches.next();
  }

  filterExtra(value: string | null): void {
    this.extraFilterValue = value;
    this.searches.next();
  }

  select(id: number): void {
    void this.router.navigate([], { relativeTo: this.route, queryParams: { id } });
  }

  /** Der Eintrag als Einheitentyp (samt Profilen), bei anderen Modellen null. */
  asUnitType(entry: CatalogEntry): AdminUnitType | null {
    return this.model.resource === 'unit-types' ? (entry as unknown as AdminUnitType) : null;
  }

  save(entry: CatalogEntry, draft: CatalogDraft): void {
    // Die Zuordnung aendert sich nie (das Backend lehnt es ab) -- sie muss
    // beim PUT aber mitgeschickt werden.
    const payload: Record<string, unknown> = { ...draft };
    // Das Bild steht nicht im Formular (formFields) -- gespeichert bleibt das bisherige.
    for (const field of this.model.fields) {
      if (field.optionsFrom === 'unit-sprites') {
        payload[field.key] = (entry as unknown as Record<string, unknown>)[field.key] ?? '';
      }
    }
    if (this.model.parent === 'species') {
      payload['species_id'] = entry.species_id;
    } else if (this.model.parent === 'unit-type') {
      payload['unit_type_id'] = entry.unit_type_id;
    }

    this.saving.set(true);
    this.errorMessage.set(null);
    this.message.set(null);
    this.api.updateCatalogEntry<CatalogEntry>(this.model.resource, entry.id, payload).subscribe({
      next: (saved) => {
        this.saving.set(false);
        this.selected.set(saved);
        this.results.update((list) =>
          list?.map((other) => (other.id === saved.id ? saved : other)) ?? null,
        );
        this.message.set(`„${saved.name}“ gespeichert.`);
      },
      error: (err: HttpErrorResponse) => {
        this.saving.set(false);
        this.errorMessage.set(
          apiErrorMessage(err, 'Speichern fehlgeschlagen.', CATALOG_FIELD_LABELS),
        );
      },
    });
  }

  /**
   * Speichert nur das Bild eines Einheitentyps (PATCH) -- aus der Bild-Sektion.
   * Das Formular darueber laedt danach den gespeicherten Stand neu; was dort
   * noch nicht gespeichert war, ist damit verworfen.
   */
  saveSprite(entry: CatalogEntry, sprite: string): void {
    this.saving.set(true);
    this.errorMessage.set(null);
    this.message.set(null);
    this.api.patchCatalogEntry<CatalogEntry>(this.model.resource, entry.id, { sprite }).subscribe({
      next: (saved) => {
        this.saving.set(false);
        this.selected.set(saved);
        this.results.update((list) =>
          list?.map((other) => (other.id === saved.id ? saved : other)) ?? null,
        );
        this.message.set(`Bild von „${saved.name}“ gespeichert.`);
      },
      error: (err: HttpErrorResponse) => {
        this.saving.set(false);
        this.errorMessage.set(apiErrorMessage(err, 'Bild nicht gespeichert.', CATALOG_FIELD_LABELS));
      },
    });
  }

  remove(entry: CatalogEntry): void {
    if (!confirm(`${this.model.singular} „${entry.name}“ wirklich löschen?`)) {
      return;
    }
    this.saving.set(true);
    this.errorMessage.set(null);
    this.api.deleteCatalogEntry(this.model.resource, entry.id).subscribe({
      next: () => {
        this.saving.set(false);
        this.results.update((list) => list?.filter((other) => other.id !== entry.id) ?? null);
        this.message.set(`„${entry.name}“ gelöscht.`);
        void this.router.navigate([], { relativeTo: this.route, queryParams: {} });
      },
      error: (err: HttpErrorResponse) => {
        this.saving.set(false);
        this.errorMessage.set(apiErrorMessage(err, 'Löschen fehlgeschlagen.'));
      },
    });
  }

  private load(id: number): void {
    this.api.getCatalogEntry<CatalogEntry>(this.model.resource, id).subscribe({
      next: (entry) => {
        this.loadError.set(null);
        this.selected.set(entry);
      },
      error: (err: HttpErrorResponse) => {
        this.selected.set(null);
        this.loadError.set(
          apiErrorMessage(err, `${this.model.singular} konnte nicht geladen werden.`),
        );
      },
    });
  }
}
