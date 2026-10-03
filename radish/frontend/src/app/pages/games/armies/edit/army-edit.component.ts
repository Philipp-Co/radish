import { HttpErrorResponse } from '@angular/common/http';
import { Component, DestroyRef, OnInit, inject, signal } from '@angular/core';
import { takeUntilDestroyed } from '@angular/core/rxjs-interop';
import { FormsModule } from '@angular/forms';
import { ActivatedRoute, Router, RouterLink } from '@angular/router';
import { forkJoin, of } from 'rxjs';

import { apiErrorMessage } from '../../../../core/api-error';
import {
  ApiService,
  ArmyDetail,
  ArmyInput,
  SpeciesCatalog,
  UnitType,
} from '../../../../core/api.service';
import {
  DraftUnit,
  composition,
  newUnit,
  profileOf,
  slotsForSaved,
  unitCost,
  unitProblems,
  heavyWeaponsSummary,
} from './army-draft';
import { UnitDetailComponent } from './unit-detail.component';

/**
 * Anlegen (armies/new) und Bearbeiten (armies/:id) einer Armee, in zwei
 * Stufen: die Uebersicht zeigt je Einheit eine Zeile (Zusammensetzung,
 * schwere Waffe, Kosten, Regelverstoesse), die Detailansicht einer Einheit
 * (?unit=<Nr.>, siehe UnitDetailComponent) die Feineinstellung je Entitaet.
 * Beides auf einer Seite, damit der ungespeicherte Stand beim Wechsel
 * erhalten bleibt; gespeichert wird immer die ganze Armee.
 *
 * Waehlt nur aus dem Katalog (api/client/catalog/) aus -- die Werte selbst
 * stehen dort. Massgeblich fuer die Regeln ist die Pruefung im Backend
 * (serializers.ArmyWriteSerializer), deren Meldung hier angezeigt wird.
 */
@Component({
  selector: 'app-army-edit',
  standalone: true,
  imports: [FormsModule, RouterLink, UnitDetailComponent],
  template: `
    <section class="card">
      @if (loading()) {
        <p class="muted">Lade...</p>
      } @else {
        <div class="top">
          <h2>{{ armyId === null ? 'Neue Armee' : 'Armee bearbeiten' }}</h2>
          <a routerLink="/games/armies">Zur Armeeliste</a>
        </div>

        <div class="fields">
          <label>
            Name
            <input type="text" [(ngModel)]="name" required />
          </label>
          <label>
            Spezies
            <select
              [ngModel]="speciesId"
              (ngModelChange)="changeSpecies($event)"
              [disabled]="units.length > 0"
              [title]="units.length > 0 ? 'Nur änderbar, solange die Armee keine Einheiten hat.' : ''"
            >
              @for (species of catalog(); track species.id) {
                <option [ngValue]="species.id">{{ species.name }}</option>
              }
            </select>
          </label>
        </div>

        <div class="save-bar">
          @if (currentSpecies(); as species) {
            <span class="total">{{ armyCost(species) }} Punkte</span>
            <span class="muted">
              · {{ units.length }} {{ units.length === 1 ? 'Einheit' : 'Einheiten' }}
            </span>
          }
          <span class="status">
            @if (dirty()) {
              <span class="muted">Nicht gespeicherte Änderungen</span>
            } @else if (savedMessage()) {
              <span class="success">{{ savedMessage() }}</span>
            }
          </span>
          <button [disabled]="saving() || !name || speciesId === null" (click)="save()">
            Speichern
          </button>
        </div>

        @if (errorMessage(); as message) {
          <p class="error">{{ message }}</p>
        }
      }
    </section>

    @if (!loading() && currentSpecies(); as species) {
      <section class="card">
        @if (selectedUnit(); as selected) {
          <app-unit-detail
            [species]="species"
            [type]="selected.type"
            [unit]="selected.unit"
            [index]="selected.index"
            (back)="openUnit(null)"
            (remove)="removeUnit(selected.index)"
          />
        } @else {
          <h3>Einheiten</h3>
          @if (units.length === 0) {
            <p class="muted">Noch keine Einheit.</p>
          } @else {
            <table class="units">
              <thead>
                <tr>
                  <th>#</th>
                  <th>Einheit</th>
                  <th>Schwere Waffen</th>
                  <th class="num">Punkte</th>
                  <th></th>
                </tr>
              </thead>
              <tbody>
                @for (unit of units; track unit; let i = $index) {
                  @if (unitType(unit.unitTypeId); as type) {
                    <tr (click)="openUnit(i)">
                      <td class="muted">{{ i + 1 }}</td>
                      <td>
                        <strong>{{ type.name }}</strong>
                        <div class="muted small">{{ composition(type, unit) }}</div>
                        <div class="muted small">Bewegung: {{ type.movement }} Felder</div>
                        @if (type.transport_capacity > 0) {
                          <div class="muted small">
                            Transport: {{ type.transport_capacity }}
                            {{ type.transport_capacity === 1 ? 'Einheit' : 'Einheiten' }}
                          </div>
                        }
                        @if (type.can_capture_objectives) {
                          <div class="muted small">Kann Ziele einnehmen</div>
                        }
                        @for (problem of problems(species, type, unit); track problem) {
                          <div class="error small">⚠ {{ problem }}</div>
                        }
                      </td>
                      <td>{{ heavyWeapons(species, unit) }}</td>
                      <td class="num">{{ unitCost(species, type, unit) }}</td>
                      <td class="actions">
                        <button class="secondary small" (click)="openUnit(i); $event.stopPropagation()">
                          Details
                        </button>
                        <button
                          class="secondary small"
                          title="Einheit entfernen"
                          (click)="removeUnit(i); $event.stopPropagation()"
                        >
                          ✕
                        </button>
                      </td>
                    </tr>
                  }
                }
              </tbody>
            </table>
          }

          @if (species.unit_types.length === 0) {
            <p class="muted">Für {{ species.name }} gibt es noch keine Einheitentypen.</p>
          } @else {
            <div class="add-unit">
              <select [(ngModel)]="newUnitTypeId">
                @for (type of species.unit_types; track type.id) {
                  <option [ngValue]="type.id" [disabled]="type.profiles.length === 0">
                    {{ type.name }}{{ type.profiles.length === 0 ? ' (noch ohne Profil)' : '' }}
                  </option>
                }
              </select>
              <button class="secondary" [disabled]="newUnitTypeId === null" (click)="addUnit()">
                Einheit hinzufügen
              </button>
            </div>
          }
        }
      </section>
    }
  `,
  styles: [
    `
      :host {
        display: flex;
        flex-direction: column;
        gap: 1rem;
      }
      .top,
      .add-unit {
        display: flex;
        align-items: center;
        justify-content: space-between;
        gap: 0.75rem;
      }
      .top h2 {
        margin: 0 0 0.75rem;
      }
      .fields {
        display: grid;
        grid-template-columns: 2fr 1fr;
        gap: 0.75rem;
      }
      label {
        display: flex;
        flex-direction: column;
        gap: 0.25rem;
        font-size: 0.9rem;
      }
      .save-bar {
        display: flex;
        align-items: center;
        gap: 0.4rem;
        margin-top: 1rem;
      }
      .total {
        font-weight: 600;
        font-size: 1.1rem;
      }
      .status {
        flex: 1;
        text-align: right;
        font-size: 0.85rem;
        margin-right: 0.5rem;
      }
      .success {
        color: var(--accent, #5fb9a3);
      }
      h3 {
        margin: 0 0 0.5rem;
        font-size: 1rem;
      }
      .units {
        width: 100%;
        border-collapse: collapse;
        font-size: 0.9rem;
      }
      .units th {
        text-align: left;
        color: var(--text-dim, #8a8a92);
        font-weight: 600;
        font-size: 0.8rem;
        padding: 0.35rem 0.4rem;
        border-bottom: 1px solid var(--border, #2a2a30);
      }
      .units td {
        padding: 0.5rem 0.4rem;
        border-bottom: 1px solid var(--border, #2a2a30);
        vertical-align: top;
      }
      .units tbody tr {
        cursor: pointer;
      }
      .units tbody tr:hover {
        background: var(--bg, #0b0b0d);
      }
      .small {
        font-size: 0.78rem;
      }
      .num {
        text-align: right;
        white-space: nowrap;
      }
      .actions {
        text-align: right;
        white-space: nowrap;
      }
      button.small {
        padding: 0.2rem 0.5rem;
        font-size: 0.75rem;
      }
      .add-unit {
        margin-top: 0.75rem;
      }
      .add-unit select {
        flex: 1;
      }
    `,
  ],
})
export class ArmyEditComponent implements OnInit {
  private readonly api = inject(ApiService);
  private readonly router = inject(Router);
  private readonly route = inject(ActivatedRoute);
  private readonly destroyRef = inject(DestroyRef);

  readonly catalog = signal<SpeciesCatalog[]>([]);
  readonly loading = signal(true);
  readonly saving = signal(false);
  readonly errorMessage = signal<string | null>(null);
  readonly savedMessage = signal<string | null>(null);
  /** Index der Einheit in der Detailansicht (aus ?unit=, 1-basiert), sonst null. */
  readonly openIndex = signal<number | null>(null);

  readonly composition = composition;
  readonly problems = unitProblems;
  readonly unitCost = unitCost;

  /** null = neue Armee (Route armies/new). */
  armyId: number | null = null;
  name = '';
  speciesId: number | null = null;
  units: DraftUnit[] = [];
  newUnitTypeId: number | null = null;
  /** Zuletzt gespeicherter Stand, fuer "Nicht gespeicherte Änderungen". */
  private savedSnapshot = '';

  ngOnInit(): void {
    const idParam = this.route.snapshot.paramMap.get('id');
    this.armyId = idParam === null ? null : Number(idParam);
    // Nach dem ersten Speichern einer neuen Armee kommt man hierher zurueck
    // (siehe save()) -- die Bestaetigung reist im History-State mit.
    if ((history.state as { saved?: boolean } | null)?.saved) {
      this.savedMessage.set('Gespeichert.');
    }

    forkJoin({
      catalog: this.api.getCatalog(),
      army: this.armyId === null ? of(null) : this.api.getArmy(this.armyId),
    }).subscribe({
      next: ({ catalog, army }) => {
        this.catalog.set(catalog.species);
        if (army) {
          this.applyArmy(army);
        } else if (catalog.species.length > 0) {
          this.speciesId = catalog.species[0].id;
        }
        this.resetNewUnitType();
        this.savedSnapshot = army ? this.snapshot() : '';
        this.loading.set(false);
      },
      error: (err: HttpErrorResponse) => {
        this.errorMessage.set(apiErrorMessage(err, 'Armee konnte nicht geladen werden.'));
        this.loading.set(false);
      },
    });

    this.route.queryParamMap.pipe(takeUntilDestroyed(this.destroyRef)).subscribe((params) => {
      const number = Number(params.get('unit'));
      this.openIndex.set(number > 0 ? number - 1 : null);
    });
  }

  currentSpecies(): SpeciesCatalog | undefined {
    return this.catalog().find((species) => species.id === this.speciesId);
  }

  unitType(id: number): UnitType | undefined {
    return this.currentSpecies()?.unit_types.find((type) => type.id === id);
  }

  /** Die Einheit der Detailansicht samt Typ, oder null fuer die Uebersicht. */
  selectedUnit(): { unit: DraftUnit; type: UnitType; index: number } | null {
    const index = this.openIndex();
    const unit = index === null ? undefined : this.units[index];
    const type = unit && this.unitType(unit.unitTypeId);
    return unit && type && index !== null ? { unit, type, index } : null;
  }

  openUnit(index: number | null): void {
    void this.router.navigate([], {
      relativeTo: this.route,
      queryParams: { unit: index === null ? null : index + 1 },
    });
  }

  readonly heavyWeapons = heavyWeaponsSummary;

  armyCost(species: SpeciesCatalog): number {
    return this.units.reduce((sum, unit) => {
      const type = this.unitType(unit.unitTypeId);
      return sum + (type ? unitCost(species, type, unit) : 0);
    }, 0);
  }

  dirty(): boolean {
    return this.snapshot() !== this.savedSnapshot;
  }

  changeSpecies(speciesId: number): void {
    this.speciesId = speciesId;
    this.resetNewUnitType();
  }

  addUnit(): void {
    const type = this.newUnitTypeId === null ? undefined : this.unitType(this.newUnitTypeId);
    if (type && type.profiles.length > 0) {
      this.units.push(newUnit(type));
    }
  }

  removeUnit(index: number): void {
    const type = this.unitType(this.units[index]?.unitTypeId);
    if (!confirm(`Einheit ${index + 1} (${type?.name ?? '?'}) entfernen?`)) {
      return;
    }
    this.units.splice(index, 1);
    if (this.openIndex() !== null) {
      this.openUnit(null);
    }
  }

  save(): void {
    if (!this.name || this.speciesId === null) {
      return;
    }
    const army = this.buildInput();
    this.saving.set(true);
    this.errorMessage.set(null);
    this.savedMessage.set(null);
    const request =
      this.armyId === null ? this.api.createArmy(army) : this.api.updateArmy(this.armyId, army);
    request.subscribe({
      next: (saved) => {
        this.saving.set(false);
        if (this.armyId === null) {
          // Neue Armee: auf ihre eigene URL wechseln, damit weiteres
          // Speichern sie aendert statt eine zweite anzulegen.
          void this.router.navigate(['/games/armies', saved.id], {
            replaceUrl: true,
            state: { saved: true },
          });
          return;
        }
        this.savedSnapshot = this.snapshot();
        this.savedMessage.set('Gespeichert.');
      },
      error: (err: HttpErrorResponse) => {
        this.saving.set(false);
        this.errorMessage.set(apiErrorMessage(err, 'Armee konnte nicht gespeichert werden.'));
      },
    });
  }

  private buildInput(): ArmyInput {
    return {
      name: this.name,
      species_id: this.speciesId ?? 0,
      units: this.units.map((unit) => ({
        unit_type_id: unit.unitTypeId,
        entities: unit.entities.map((entity) => ({
          profile_id: entity.profileId,
          weapon_ids: filled(entity.weaponIds),
          heavy_weapon_ids: filled(entity.heavyWeaponIds),
          super_heavy_weapon_ids: filled(entity.superHeavyWeaponIds),
          equipment_ids: filled(entity.equipmentIds),
        })),
      })),
    };
  }

  private snapshot(): string {
    return JSON.stringify(this.buildInput());
  }

  private applyArmy(army: ArmyDetail): void {
    this.name = army.name;
    this.speciesId = army.species_id;
    this.units = army.units.map((unit) => {
      const type = this.unitType(unit.unit_type_id);
      return {
        unitTypeId: unit.unit_type_id,
        entities: unit.entities.map((entity) => {
          const profile = type && profileOf(type, entity.profile_id);
          return {
            profileId: entity.profile_id,
            weaponIds: slotsForSaved(entity.weapon_ids, profile?.weapon_slots ?? 0),
            heavyWeaponIds: slotsForSaved(
              entity.heavy_weapon_ids,
              profile?.heavy_weapon_slots ?? 0,
            ),
            superHeavyWeaponIds: slotsForSaved(
              entity.super_heavy_weapon_ids,
              profile?.super_heavy_weapon_slots ?? 0,
            ),
            equipmentIds: slotsForSaved(entity.equipment_ids, profile?.equipment_slots ?? 0),
          };
        }),
      };
    });
  }

  private resetNewUnitType(): void {
    const types = this.currentSpecies()?.unit_types ?? [];
    this.newUnitTypeId = (types.find((type) => type.profiles.length > 0) ?? types[0])?.id ?? null;
  }
}

function filled(ids: (number | null)[]): number[] {
  return ids.filter((id): id is number => id !== null);
}
