import { Component, EventEmitter, Input, Output } from '@angular/core';
import { FormsModule } from '@angular/forms';

import {
  SpeciesCatalog,
  UnitType,
  WEAPON_CLASS_LABELS,
  Weapon,
  WeaponClass,
} from '../../../../core/api.service';
import {
  DraftEntity,
  DraftUnit,
  ENTITY_WEAPON_CLASSES,
  applyProfile,
  emptyEntity,
  entityCost,
  profileOf,
  profileWithRoom,
  unitCost,
  unitProblems,
  unitWeaponCount,
  weaponStats,
  weaponsOfClass,
} from './army-draft';

/**
 * Detailansicht einer Einheit im Armee-Editor (siehe ArmyEditComponent,
 * ?unit=<Index>): Anzahl der Entitaeten und je Entitaet eine Tabellenzeile
 * mit Profil und Slots -- auch schwere und super-schwere Waffen traegt eine
 * einzelne Entitaet. Arbeitet direkt auf dem DraftUnit der
 * Elternkomponente -- gespeichert wird erst dort, fuer die ganze Armee.
 */
@Component({
  selector: 'app-unit-detail',
  standalone: true,
  imports: [FormsModule],
  template: `
    <div class="header">
      <button class="secondary" (click)="back.emit()">← Übersicht</button>
      <h3>
        {{ index + 1 }}. {{ type.name }}
        <span class="muted transport">Bewegung {{ type.movement }} Felder</span>
        @if (type.transport_capacity > 0) {
          <span class="muted transport">
            transportiert bis zu {{ type.transport_capacity }}
            {{ type.transport_capacity === 1 ? 'Einheit' : 'Einheiten' }}
          </span>
        }
        @if (type.can_capture_objectives) {
          <span class="muted transport">· kann Ziele einnehmen</span>
        }
      </h3>
      <span class="cost">{{ cost() }} Punkte</span>
    </div>

    @for (problem of problems(); track problem) {
      <p class="error hint">⚠ {{ problem }}</p>
    }

    <div class="unit-settings">
      @if (limitedClasses().length > 0) {
        <div class="limits">
          @for (weaponClass of limitedClasses(); track weaponClass.weaponClass) {
            <span
              class="limit"
              [class.over]="usedOf(weaponClass) > (weaponClass.unitLimit(type) ?? 0)"
            >
              {{ weaponClass.label }}n: {{ usedOf(weaponClass) }} / {{ weaponClass.unitLimit(type) }}
            </span>
          }
        </div>
      }
      <div class="count">
        <span class="muted">Entitäten ({{ type.min_entities }}–{{ type.max_entities }})</span>
        <div class="stepper">
          <button
            class="secondary"
            [disabled]="unit.entities.length <= type.min_entities"
            (click)="removeEntity(unit.entities.length - 1)"
          >
            −
          </button>
          <strong>{{ unit.entities.length }}</strong>
          <button
            class="secondary"
            [disabled]="unit.entities.length >= type.max_entities"
            (click)="addEntity()"
          >
            +
          </button>
        </div>
      </div>
    </div>

    <div class="table-wrap">
      <table>
        <thead>
          <tr>
            <th>#</th>
            <th>Profil</th>
            <th>Waffen</th>
            <th>Ausrüstung</th>
            <th class="num">P</th>
            <th></th>
          </tr>
        </thead>
        <tbody>
          @for (entity of unit.entities; track entity; let i = $index) {
            <tr>
              <td class="muted">{{ i + 1 }}</td>
              <td>
                <select [ngModel]="entity.profileId" (ngModelChange)="changeProfile(entity, $event)">
                  @for (profile of type.profiles; track profile.id) {
                    <option [ngValue]="profile.id">{{ profile.name }}</option>
                  }
                </select>
                @if (profile(entity); as p) {
                  <div
                    class="muted stats"
                    title="Lebenspunkte · Rüstungspunkte · Stärke · Treffsicherheit"
                  >
                    LP {{ p.health }} · RP {{ p.armor }} · ST {{ p.strength }} · TS {{ p.accuracy }}
                  </div>
                }
              </td>
              <td>
                @for (weaponClass of weaponClasses; track weaponClass.weaponClass) {
                  @for (slot of weaponClass.ids(entity); track $index; let s = $index) {
                    @if (weaponClass.weaponClass !== 'standard') {
                      <span class="slot-label">{{ weaponClass.label }}</span>
                    }
                    <select
                      [(ngModel)]="weaponClass.ids(entity)[s]"
                      [class.heavy]="weaponClass.weaponClass !== 'standard'"
                    >
                      <option [ngValue]="null">– leer –</option>
                      @for (weapon of weaponsOf(weaponClass.weaponClass); track weapon.id) {
                        <option [ngValue]="weapon.id">{{ weapon.name }} · {{ weapon.cost }} P</option>
                      }
                    </select>
                  }
                }
                @if (!hasWeaponSlots(entity)) {
                  <span class="muted">–</span>
                }
              </td>
              <td>
                @for (slot of entity.equipmentIds; track $index; let s = $index) {
                  <select [(ngModel)]="entity.equipmentIds[s]">
                    <option [ngValue]="null">– leer –</option>
                    @for (item of species.equipment; track item.id) {
                      <option [ngValue]="item.id">{{ item.name }} · {{ item.cost }} P</option>
                    }
                  </select>
                } @empty {
                  <span class="muted">–</span>
                }
              </td>
              <td class="num">{{ entityCost(entity) }}</td>
              <td class="actions">
                @if (sameProfileCount(entity) > 1) {
                  <button
                    class="secondary small"
                    [title]="'Ausstattung auf alle ' + profile(entity)?.name + ' übertragen'"
                    (click)="copyToSameProfile(entity)"
                  >
                    ⇉ alle
                  </button>
                }
                <button
                  class="secondary small"
                  title="Entität entfernen"
                  [disabled]="unit.entities.length <= type.min_entities"
                  (click)="removeEntity(i)"
                >
                  ✕
                </button>
              </td>
            </tr>
          }
        </tbody>
      </table>
    </div>

    @if (species.weapons.length > 0) {
      <details>
        <summary class="muted">Waffenwerte</summary>
        <table class="reference">
          @for (group of weaponGroups(); track group.label) {
            <tbody>
              <tr>
                <th colspan="3">{{ group.label }}</th>
              </tr>
              @for (weapon of group.weapons; track weapon.id) {
                <tr>
                  <td>{{ weapon.name }}</td>
                  <td class="muted">{{ stats(weapon) }}</td>
                  <td class="num">{{ weapon.cost }} P</td>
                </tr>
              }
            </tbody>
          }
        </table>
        <p class="muted hint">
          S = Schüsse, ST = Stärke, RW = Reichweite (min–max), DK = Durchschlagskraft
        </p>
      </details>
    }

    <div class="footer">
      <button class="secondary danger" (click)="remove.emit()">Einheit entfernen</button>
      <button (click)="back.emit()">Fertig</button>
    </div>
  `,
  styles: [
    `
      :host {
        display: flex;
        flex-direction: column;
        gap: 0.75rem;
      }
      .header {
        display: flex;
        align-items: center;
        gap: 0.75rem;
      }
      .header h3 {
        margin: 0;
        flex: 1;
        font-size: 1.05rem;
      }
      .cost {
        font-weight: 600;
      }
      .transport {
        font-size: 0.8rem;
        font-weight: 400;
        margin-left: 0.5rem;
      }
      .hint {
        font-size: 0.8rem;
        margin: 0;
      }
      .unit-settings {
        display: flex;
        align-items: flex-end;
        justify-content: space-between;
        gap: 1rem;
        flex-wrap: wrap;
      }
      label,
      .count {
        display: flex;
        flex-direction: column;
        gap: 0.25rem;
        font-size: 0.9rem;
      }
      .stepper {
        display: flex;
        align-items: center;
        gap: 0.6rem;
      }
      .stepper button {
        padding: 0.3rem 0.75rem;
      }
      .table-wrap {
        overflow-x: auto;
      }
      table {
        width: 100%;
        border-collapse: collapse;
        font-size: 0.85rem;
      }
      th {
        text-align: left;
        color: var(--text-dim, #8a8a92);
        font-weight: 600;
        padding: 0.35rem 0.3rem;
        border-bottom: 1px solid var(--border, #2a2a30);
      }
      td {
        padding: 0.4rem 0.3rem;
        border-bottom: 1px solid var(--border, #2a2a30);
        vertical-align: top;
      }
      td select {
        display: block;
        width: 100%;
        min-width: 7rem;
        font-size: 0.8rem;
        padding: 0.25rem 0.35rem;
        margin-bottom: 0.25rem;
      }
      .limits {
        display: flex;
        flex-direction: column;
        gap: 0.2rem;
        font-size: 0.85rem;
      }
      .limit.over {
        color: var(--danger, #d97a7a);
      }
      .slot-label {
        display: block;
        font-size: 0.7rem;
        color: var(--text-dim, #8a8a92);
      }
      select.heavy {
        border-color: var(--accent, #5fb9a3);
      }
      .stats {
        font-size: 0.75rem;
        white-space: nowrap;
      }
      .num {
        text-align: right;
        white-space: nowrap;
      }
      .actions {
        white-space: nowrap;
        text-align: right;
      }
      button.small {
        padding: 0.2rem 0.45rem;
        font-size: 0.75rem;
      }
      details summary {
        cursor: pointer;
        font-size: 0.85rem;
      }
      .reference td {
        border-bottom: none;
        padding: 0.2rem 0.3rem;
      }
      .reference th {
        padding-top: 0.6rem;
        border-bottom: none;
      }
      .footer {
        display: flex;
        justify-content: space-between;
      }
      .danger {
        color: var(--danger, #d97a7a);
      }
    `,
  ],
})
export class UnitDetailComponent {
  @Input({ required: true }) species!: SpeciesCatalog;
  @Input({ required: true }) type!: UnitType;
  @Input({ required: true }) unit!: DraftUnit;
  @Input({ required: true }) index!: number;

  @Output() readonly back = new EventEmitter<void>();
  @Output() readonly remove = new EventEmitter<void>();

  readonly stats = weaponStats;
  readonly weaponClasses = ENTITY_WEAPON_CLASSES;

  cost(): number {
    return unitCost(this.species, this.type, this.unit);
  }

  problems(): string[] {
    return unitProblems(this.species, this.type, this.unit);
  }

  profile(entity: DraftEntity) {
    return profileOf(this.type, entity.profileId);
  }

  entityCost(entity: DraftEntity): number {
    return entityCost(this.species, this.type, entity);
  }

  weaponsOf(weaponClass: WeaponClass): Weapon[] {
    return weaponsOfClass(this.species, weaponClass);
  }

  /** Waffen der Spezies nach Klasse, fuer die Referenz "Waffenwerte". */
  weaponGroups(): { label: string; weapons: Weapon[] }[] {
    return (Object.keys(WEAPON_CLASS_LABELS) as WeaponClass[])
      .map((weaponClass) => ({
        label: WEAPON_CLASS_LABELS[weaponClass],
        weapons: weaponsOfClass(this.species, weaponClass),
      }))
      .filter((group) => group.weapons.length > 0);
  }

  addEntity(): void {
    if (this.unit.entities.length < this.type.max_entities) {
      this.unit.entities.push(emptyEntity(profileWithRoom(this.type, this.unit)));
    }
  }

  removeEntity(index: number): void {
    if (this.unit.entities.length > this.type.min_entities) {
      this.unit.entities.splice(index, 1);
    }
  }

  /** Profilwechsel: Slots an das neue Profil anpassen, Belegtes bleibt soweit moeglich. */
  changeProfile(entity: DraftEntity, profileId: number): void {
    const profile = profileOf(this.type, profileId);
    if (!profile) {
      return;
    }
    applyProfile(entity, profile);
  }

  /** Die schweren Klassen, fuer die der Einheitentyp eine Hoechstzahl setzt. */
  limitedClasses() {
    return ENTITY_WEAPON_CLASSES.filter((weaponClass) => weaponClass.unitLimit(this.type) !== null);
  }

  usedOf(weaponClass: (typeof ENTITY_WEAPON_CLASSES)[number]): number {
    return unitWeaponCount(this.unit, weaponClass);
  }

  hasWeaponSlots(entity: DraftEntity): boolean {
    return ENTITY_WEAPON_CLASSES.some((weaponClass) => weaponClass.ids(entity).length > 0);
  }

  sameProfileCount(entity: DraftEntity): number {
    return this.unit.entities.filter((other) => other.profileId === entity.profileId).length;
  }

  /** Waffen und Ausruestung dieser Entitaet auf alle mit demselben Profil kopieren. */
  copyToSameProfile(source: DraftEntity): void {
    for (const entity of this.unit.entities) {
      if (entity !== source && entity.profileId === source.profileId) {
        for (const weaponClass of ENTITY_WEAPON_CLASSES) {
          weaponClass.setIds(entity, [...weaponClass.ids(source)]);
        }
        entity.equipmentIds = [...source.equipmentIds];
      }
    }
  }
}
