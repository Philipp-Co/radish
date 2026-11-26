import { HttpErrorResponse } from '@angular/common/http';
import {
  Component,
  EventEmitter,
  Input,
  OnChanges,
  Output,
  SimpleChanges,
  inject,
  signal,
} from '@angular/core';

import { apiErrorMessage } from '../../../../core/api-error';
import { AdminUnitType, ApiService, ProfileRestricted } from '../../../../core/api.service';

/** Ein Katalogeintrag mit Einschraenkung (Waffe, Ausruestung), wie die Admin-API ihn liefert. */
export interface RestrictedEntry extends ProfileRestricted {
  species_id: number;
}

/**
 * Wer eine Waffe bzw. einen Ausruestungsgegenstand tragen darf, auf dessen
 * "Ändern"-Seite (siehe CatalogEditComponent): die Einheitentypen seiner
 * Spezies mit ihren Profilen als Haken. Ein Einheitentyp erlaubt alle seine
 * Profile, sonst einzelne Profile; ganz ohne Haken jede Entitaet der Spezies
 * (siehe radish/backend/api/models/profile_restricted.py). Gespeichert wird
 * getrennt vom Formular darueber -- per PATCH in der Elternkomponente.
 */
@Component({
  selector: 'app-carrier-restrictions',
  template: `
    <h3>Wer darf tragen?</h3>
    <p class="muted hint">{{ summary() }}</p>

    @if (loadError(); as message) {
      <p class="error">{{ message }}</p>
    }

    @if (unitTypes(); as types) {
      @if (types.length === 0) {
        <p class="muted hint">Die Spezies hat noch keine Einheitentypen.</p>
      }
      <ul class="tree">
        @for (type of types; track type.id) {
          <li>
            <label>
              <input
                type="checkbox"
                [checked]="unitTypeIds().has(type.id)"
                (change)="toggleUnitType(type, checkedOf($event))"
              />
              <strong>{{ type.name }}</strong>
              <span class="muted">alle Profile</span>
            </label>
            <ul>
              @for (profile of type.profiles; track profile.id) {
                <li>
                  <label [class.muted]="unitTypeIds().has(type.id)">
                    <input
                      type="checkbox"
                      [checked]="unitTypeIds().has(type.id) || profileIds().has(profile.id)"
                      [disabled]="unitTypeIds().has(type.id)"
                      (change)="toggleProfile(profile.id, checkedOf($event))"
                    />
                    {{ profile.name }}
                  </label>
                </li>
              }
            </ul>
          </li>
        }
      </ul>
    }

    <div class="actions">
      <button class="secondary" [disabled]="!dirty()" (click)="reset()">Verwerfen</button>
      <button [disabled]="busy || !dirty()" (click)="submit()">Speichern</button>
    </div>
  `,
  styles: [
    `
      h3 {
        margin-top: 0;
      }
      .hint {
        font-size: 0.8rem;
      }
      .tree,
      .tree ul {
        list-style: none;
        margin: 0;
        padding: 0;
      }
      .tree > li {
        padding: 0.35rem 0;
        border-bottom: 1px solid var(--border, #2a2a30);
      }
      .tree ul {
        display: flex;
        flex-wrap: wrap;
        gap: 0.2rem 1rem;
        padding-left: 1.6rem;
        margin-top: 0.2rem;
      }
      label {
        display: inline-flex;
        align-items: center;
        gap: 0.4rem;
        font-size: 0.85rem;
      }
      label .muted {
        font-size: 0.75rem;
      }
      .actions {
        display: flex;
        justify-content: flex-end;
        gap: 0.5rem;
        margin-top: 0.75rem;
      }
    `,
  ],
})
export class CarrierRestrictionsComponent implements OnChanges {
  private readonly api = inject(ApiService);

  @Input({ required: true }) entry!: RestrictedEntry;
  /** Wie der Eintrag in Saetzen heisst, z.B. "die Waffe". */
  @Input({ required: true }) noun!: string;
  @Input() busy = false;

  @Output() readonly save = new EventEmitter<ProfileRestricted>();

  /** Einheitentypen der Spezies samt Profilen; null, solange sie laden. */
  readonly unitTypes = signal<AdminUnitType[] | null>(null);
  readonly loadError = signal<string | null>(null);
  readonly unitTypeIds = signal(new Set<number>());
  readonly profileIds = signal(new Set<number>());

  ngOnChanges(changes: SimpleChanges): void {
    const previous = changes['entry']?.previousValue as RestrictedEntry | undefined;
    if (!changes['entry']) {
      return;
    }
    this.reset();
    if (previous?.species_id !== this.entry.species_id) {
      this.loadUnitTypes();
    }
  }

  /** Zurueck auf den gespeicherten Stand. */
  reset(): void {
    this.unitTypeIds.set(new Set(this.entry.unit_type_ids));
    this.profileIds.set(new Set(this.entry.profile_ids));
  }

  checkedOf(event: Event): boolean {
    return (event.target as HTMLInputElement).checked;
  }

  /** Ein Einheitentyp schliesst seine Profile ein -- deren einzelne Haken entfallen. */
  toggleUnitType(type: AdminUnitType, checked: boolean): void {
    this.unitTypeIds.update((ids) => toggled(ids, type.id, checked));
    if (checked) {
      this.profileIds.update(
        (ids) => new Set([...ids].filter((id) => !type.profiles.some((profile) => profile.id === id))),
      );
    }
  }

  toggleProfile(id: number, checked: boolean): void {
    this.profileIds.update((ids) => toggled(ids, id, checked));
  }

  summary(): string {
    const types = this.unitTypeIds().size;
    const profiles = this.profileIds().size;
    if (types === 0 && profiles === 0) {
      return `Keine Einschränkung: jede Entität der Spezies darf ${this.noun} tragen.`;
    }
    const parts = [
      types > 0 ? `${count(types, 'Einheitentyp', 'Einheitentypen')} (mit allen Profilen)` : '',
      profiles > 0 ? count(profiles, 'einzelnes Profil', 'einzelne Profile') : '',
    ];
    return `Nur für ${parts.filter(Boolean).join(' und ')}.`;
  }

  dirty(): boolean {
    return (
      !sameIds(this.unitTypeIds(), this.entry.unit_type_ids) ||
      !sameIds(this.profileIds(), this.entry.profile_ids)
    );
  }

  submit(): void {
    this.save.emit({ unit_type_ids: [...this.unitTypeIds()], profile_ids: [...this.profileIds()] });
  }

  private loadUnitTypes(): void {
    this.unitTypes.set(null);
    this.api
      .listCatalogEntries<AdminUnitType>('unit-types', { species: this.entry.species_id })
      .subscribe({
        next: (types) => {
          this.loadError.set(null);
          this.unitTypes.set(types);
        },
        error: (err: HttpErrorResponse) =>
          this.loadError.set(apiErrorMessage(err, 'Einheitentypen konnten nicht geladen werden.')),
      });
  }
}

function toggled(ids: Set<number>, id: number, checked: boolean): Set<number> {
  const next = new Set(ids);
  if (checked) {
    next.add(id);
  } else {
    next.delete(id);
  }
  return next;
}

function sameIds(ids: Set<number>, saved: number[]): boolean {
  return ids.size === saved.length && saved.every((id) => ids.has(id));
}

function count(n: number, one: string, many: string): string {
  return `${n} ${n === 1 ? one : many}`;
}
