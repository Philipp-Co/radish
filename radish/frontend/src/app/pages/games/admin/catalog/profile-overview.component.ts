import { Component, Input, inject } from '@angular/core';
import { Router, RouterLink } from '@angular/router';

import { AdminEntityProfile, AdminUnitType } from '../../../../core/api.service';
import { CATALOG_BASE } from './catalog-shell.component';

/**
 * Uebersicht aller Entitaetsprofile eines Einheitentyps auf dessen
 * "Ändern"-Seite (siehe CatalogEditComponent): eine Zeile je Profil mit
 * allen Werten, ein Klick oeffnet das Profil zum Aendern.
 *
 * Darunter die Summen: wie viele Entitaeten die Profile mindestens
 * verlangen und hoechstens zulassen, gegen den Rahmen des Einheitentyps.
 * Das Backend verhindert nur, dass die Mindestzahlen zu hoch werden;
 * reichen die Hoechstzahlen nicht bis min_entities, weist erst diese
 * Uebersicht darauf hin.
 */
@Component({
  selector: 'app-profile-overview',
  standalone: true,
  imports: [RouterLink],
  template: `
    <div class="header">
      <h3>Entitätsprofile</h3>
      <a
        [routerLink]="base + '/profiles/new'"
        [queryParams]="{ species: unitType.species_id, unit_type: unitType.id }"
      >
        Profil anlegen
      </a>
    </div>

    @if (unitType.profiles.length === 0) {
      <p class="muted hint">
        Noch kein Profil -- ohne Profil lässt sich keine Einheit dieses Typs aufstellen.
      </p>
    } @else {
      <div class="table-wrap">
        <table>
          <thead>
            <tr>
              <th>Name</th>
              <th title="Lebenspunkte">LP</th>
              <th title="Rüstung">RP</th>
              <th title="Stärke">ST</th>
              <th title="Treffsicherheit">TS</th>
              <th title="Waffenslots">Waffen</th>
              <th title="Slots für schwere Waffen">Schw.</th>
              <th title="Slots für super-schwere Waffen">S.-schw.</th>
              <th title="Ausrüstungsslots">Ausr.</th>
              <th title="Anzahl je Einheit">je Einheit</th>
              <th>Kosten</th>
            </tr>
          </thead>
          <tbody>
            @for (profile of unitType.profiles; track profile.id) {
              <tr (click)="open(profile)">
                <td>
                  <a
                    [routerLink]="base + '/profiles/edit'"
                    [queryParams]="{ id: profile.id }"
                    (click)="$event.stopPropagation()"
                  >
                    {{ profile.name }}
                  </a>
                  @if (profile.in_use) {
                    <span class="badge" title="In Armeen verwendet">verwendet</span>
                  }
                </td>
                <td>{{ profile.health }}</td>
                <td>{{ profile.armor }}</td>
                <td>{{ profile.strength }}</td>
                <td>{{ profile.accuracy }}</td>
                <td>{{ profile.weapon_slots }}</td>
                <td>{{ profile.heavy_weapon_slots }}</td>
                <td>{{ profile.super_heavy_weapon_slots }}</td>
                <td>{{ profile.equipment_slots }}</td>
                <td>{{ countRange(profile) }}</td>
                <td>{{ profile.cost }}</td>
              </tr>
            }
          </tbody>
        </table>
      </div>

      <p class="muted hint">
        Die Profile verlangen zusammen mindestens {{ requiredEntities() }} und erlauben höchstens
        {{ allowedEntities() }} Entitäten; der Einheitentyp erlaubt
        {{ unitType.min_entities }}–{{ unitType.max_entities }}.
      </p>
      @if (allowedEntities() < unitType.min_entities) {
        <p class="error hint">
          Mit diesen Profilen lässt sich die Mindestzahl von {{ unitType.min_entities }}
          Entitäten nicht erreichen -- erhöhe die Höchstzahl eines Profils oder lege ein weiteres an.
        </p>
      }
    }
  `,
  styles: [
    `
      .header {
        display: flex;
        align-items: center;
        justify-content: space-between;
      }
      h3 {
        margin: 0 0 0.5rem;
        font-size: 1rem;
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
        padding: 0.35rem 0.4rem;
        border-bottom: 1px solid var(--border, #2a2a30);
        white-space: nowrap;
      }
      td {
        padding: 0.45rem 0.4rem;
        border-bottom: 1px solid var(--border, #2a2a30);
        white-space: nowrap;
      }
      tbody tr {
        cursor: pointer;
      }
      tbody tr:hover {
        background: var(--bg, #0b0b0d);
      }
      .badge {
        margin-left: 0.4rem;
        padding: 0.05rem 0.4rem;
        border: 1px solid var(--border, #2a2a30);
        border-radius: 999px;
        font-size: 0.7rem;
        color: var(--text-dim, #8a8a92);
      }
      .hint {
        font-size: 0.8rem;
      }
    `,
  ],
})
export class ProfileOverviewComponent {
  private readonly router = inject(Router);

  @Input({ required: true }) unitType!: AdminUnitType;
  readonly base = CATALOG_BASE;

  countRange(profile: AdminEntityProfile): string {
    return profile.min_count === profile.max_count
      ? `genau ${profile.min_count}`
      : `${profile.min_count}–${profile.max_count}`;
  }

  requiredEntities(): number {
    return this.unitType.profiles.reduce((sum, profile) => sum + profile.min_count, 0);
  }

  /** Hoechstens so viele, wie die Profile zulassen -- und nie mehr als der Typ. */
  allowedEntities(): number {
    const byProfiles = this.unitType.profiles.reduce((sum, profile) => sum + profile.max_count, 0);
    return Math.min(byProfiles, this.unitType.max_entities);
  }

  open(profile: AdminEntityProfile): void {
    void this.router.navigate([`${CATALOG_BASE}/profiles/edit`], {
      queryParams: { id: profile.id },
    });
  }
}
