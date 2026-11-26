import { HttpErrorResponse } from '@angular/common/http';
import { Component, OnInit, inject, signal } from '@angular/core';
import { FormsModule } from '@angular/forms';
import { Router, RouterLink } from '@angular/router';

import { apiErrorMessage } from '../../../../core/api-error';
import {
  AdminEffect,
  AdminEffectValues,
  ApiService,
  EFFECT_KINDS,
  effectKindLabel,
} from '../../../../core/api.service';

/**
 * Adminseite "Effekte": alle Effekte (bisher Explosionen) mit ihrem Stand --
 * vollstaendig (im Spiel) oder Entwurf. Ein neuer Effekt bekommt hier seinen
 * Schluessel, seine Art und erste Werte; alles Weitere (PNGs, Atlanten) auf
 * seiner eigenen Seite (EffectEditComponent).
 */
@Component({
  selector: 'app-effect-list',
  imports: [FormsModule, RouterLink],
  template: `
    <section class="card">
      <h2>Effekte</h2>
      <p class="muted">
        Bilder, die das Spiel bei Ereignissen zeichnet, etwa Explosionen beim Einschlag. Ein Effekt
        braucht je einen Atlas für alle vier Richtungen – erst dann ist er vollständig und erscheint
        ab dem nächsten Laden im Spiel. Bei jedem Angriff wählt der Client zufällig eine der
        vollständigen Explosionen.
      </p>

      <form class="create" (ngSubmit)="create()">
        <label class="grow">
          Neuer Effekt – Schlüssel
          <input type="text" name="key" placeholder="z.B. explosion_small" [(ngModel)]="newKey" />
        </label>
        <label>
          Art
          <select name="kind" [(ngModel)]="newValues.kind">
            @for (kind of kinds; track kind.kind) {
              <option [ngValue]="kind.kind">{{ kind.label }}</option>
            }
          </select>
        </label>
        <label title="Breite einer Zelle auf dem Bildschirm in Pixeln (1–1024).">
          Zeichenbreite
          <input type="number" name="draw_width" min="1" max="1024" [(ngModel)]="newValues.draw_width" />
        </label>
        <label title="Wie lange der Effekt läuft, in Millisekunden (bis 10000).">
          Dauer (ms)
          <input type="number" name="duration_ms" min="1" max="10000" [(ngModel)]="newValues.duration_ms" />
        </label>
        <button type="submit" [disabled]="busy() || !newKey.trim()">Anlegen</button>
      </form>
      <p class="muted hint">Nur a–z, 0–9 und _, höchstens 31 Zeichen. Der Schlüssel lässt sich später nicht ändern.</p>
      @if (error(); as message) {
        <p class="error">{{ message }}</p>
      }
    </section>

    <section class="card">
      @if (effects(); as list) {
        @if (list.length === 0) {
          <p class="muted">Noch kein Effekt angelegt.</p>
        } @else {
          <ul class="list">
            @for (effect of list; track effect.id) {
              <li>
                <a [routerLink]="[effect.id]">{{ effect.key }}</a>
                @if (effect.complete) {
                  <span class="badge ok">vollständig</span>
                } @else {
                  <span class="badge draft">Entwurf – {{ effect.problems.length }} offen</span>
                }
                <span class="muted">
                  {{ kindLabel(effect.kind) }} · {{ effect.images.length }} PNG · {{ effect.duration_ms }} ms
                </span>
              </li>
            }
          </ul>
        }
      } @else if (loadError(); as message) {
        <p class="error">{{ message }}</p>
      } @else {
        <p class="muted">Lade ...</p>
      }
    </section>
  `,
  styles: [
    `
      :host {
        display: flex;
        flex-direction: column;
        gap: 1rem;
      }
      .create {
        display: flex;
        flex-wrap: wrap;
        align-items: flex-end;
        gap: 0.5rem;
      }
      .create label {
        display: flex;
        flex-direction: column;
        gap: 0.25rem;
        font-size: 0.9rem;
      }
      .create label.grow {
        flex: 1;
        min-width: 12rem;
      }
      .create input[type='number'] {
        width: 6rem;
      }
      .hint {
        font-size: 0.8rem;
      }
      .list {
        list-style: none;
        margin: 0;
        padding: 0;
      }
      .list li {
        display: flex;
        flex-wrap: wrap;
        align-items: baseline;
        gap: 0.75rem;
        padding: 0.5rem 0;
        border-bottom: 1px solid var(--border, #2a2a30);
      }
      .list .muted {
        font-size: 0.85rem;
      }
      .badge {
        font-size: 0.75rem;
        padding: 0.05rem 0.4rem;
        border-radius: 4px;
        border: 1px solid currentColor;
      }
      .badge.ok {
        color: var(--accent, #5fb9a3);
      }
      .badge.draft {
        color: var(--text-dim, #8a8a92);
      }
    `,
  ],
})
export class EffectListComponent implements OnInit {
  private readonly api = inject(ApiService);
  private readonly router = inject(Router);

  readonly kinds = EFFECT_KINDS;
  readonly kindLabel = effectKindLabel;

  readonly effects = signal<AdminEffect[] | null>(null);
  readonly loadError = signal<string | null>(null);
  readonly error = signal<string | null>(null);
  readonly busy = signal(false);

  newKey = '';
  newValues: AdminEffectValues = { kind: 'explosion', draw_width: 256, duration_ms: 2500 };

  ngOnInit(): void {
    this.api.listEffects().subscribe({
      next: (effects) => this.effects.set(effects),
      error: (err: HttpErrorResponse) =>
        this.loadError.set(apiErrorMessage(err, 'Effekte konnten nicht geladen werden.')),
    });
  }

  create(): void {
    this.busy.set(true);
    this.error.set(null);
    this.api
      .createEffect(this.newKey.trim(), {
        kind: this.newValues.kind,
        draw_width: Number(this.newValues.draw_width),
        duration_ms: Number(this.newValues.duration_ms),
      })
      .subscribe({
        next: (effect) => {
          this.busy.set(false);
          void this.router.navigate(['/games/admin/effects', effect.id]);
        },
        error: (err: HttpErrorResponse) => {
          this.busy.set(false);
          this.error.set(
            apiErrorMessage(err, 'Effekt nicht angelegt.', {
              key: 'Schlüssel',
              kind: 'Art',
              draw_width: 'Zeichenbreite',
              duration_ms: 'Dauer',
            }),
          );
        },
      });
  }
}
