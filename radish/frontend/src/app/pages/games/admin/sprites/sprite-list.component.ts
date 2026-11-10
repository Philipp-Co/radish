import { HttpErrorResponse } from '@angular/common/http';
import { Component, OnInit, inject, signal } from '@angular/core';
import { FormsModule } from '@angular/forms';
import { Router, RouterLink } from '@angular/router';

import { apiErrorMessage } from '../../../../core/api-error';
import { AdminSprite, ApiService } from '../../../../core/api.service';

/**
 * Adminseite "Einheitenbilder": alle hochgeladenen Bilder mit ihrem Stand --
 * vollstaendig (im Spiel) oder Entwurf -- und den Einheitentypen, die sie
 * nennen. Ein neues Bild bekommt hier seinen Schluessel; alles Weitere
 * (Werte, PNGs, Atlanten) auf seiner eigenen Seite (SpriteEditComponent).
 *
 * Die Bilder aus dem Build (radish/assets/units/) stehen nicht hier: sie
 * kommen mit dem Image und lassen sich nicht hochladen oder aendern.
 */
@Component({
  selector: 'app-sprite-list',
  imports: [FormsModule, RouterLink],
  template: `
    <section class="card">
      <h2>Einheitenbilder</h2>
      <p class="muted">
        Bilder, die das Spiel für Einheiten zeichnet. Ein Bild braucht alle Animationen
        (Drehen, Zielen, Ziehen, Schießen, Zerstört) – erst dann ist es vollständig, steht beim
        Einheitentyp zur Wahl und erscheint im Spiel.
      </p>

      <form class="create" (ngSubmit)="create()">
        <label>
          Neues Bild – Schlüssel
          <input type="text" name="key" placeholder="z.B. infantry" [(ngModel)]="newKey" />
        </label>
        <button type="submit" [disabled]="busy() || !newKey.trim()">Anlegen</button>
      </form>
      <p class="muted hint">Nur a–z, 0–9 und _, höchstens 31 Zeichen. Der Schlüssel lässt sich später nicht ändern.</p>
      @if (error(); as message) {
        <p class="error">{{ message }}</p>
      }
    </section>

    <section class="card">
      @if (sprites(); as list) {
        @if (list.length === 0) {
          <p class="muted">Noch kein Bild hochgeladen.</p>
        } @else {
          <ul class="list">
            @for (sprite of list; track sprite.id) {
              <li>
                <a [routerLink]="[sprite.id]">{{ sprite.key }}</a>
                @if (sprite.complete) {
                  <span class="badge ok">vollständig</span>
                } @else {
                  <span class="badge draft">Entwurf – {{ sprite.problems.length }} offen</span>
                }
                <span class="muted">
                  {{ sprite.images.length }} PNG ·
                  {{ sprite.used_by.length ? 'verwendet von ' + sprite.used_by.join(', ') : 'nicht verwendet' }}
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
        align-items: flex-end;
        gap: 0.5rem;
      }
      .create label {
        display: flex;
        flex-direction: column;
        gap: 0.25rem;
        font-size: 0.9rem;
        flex: 1;
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
export class SpriteListComponent implements OnInit {
  private readonly api = inject(ApiService);
  private readonly router = inject(Router);

  readonly sprites = signal<AdminSprite[] | null>(null);
  readonly loadError = signal<string | null>(null);
  readonly error = signal<string | null>(null);
  readonly busy = signal(false);

  newKey = '';

  ngOnInit(): void {
    this.api.listSprites().subscribe({
      next: (sprites) => this.sprites.set(sprites),
      error: (err: HttpErrorResponse) =>
        this.loadError.set(apiErrorMessage(err, 'Einheitenbilder konnten nicht geladen werden.')),
    });
  }

  create(): void {
    this.busy.set(true);
    this.error.set(null);
    this.api.createSprite(this.newKey.trim()).subscribe({
      next: (sprite) => {
        this.busy.set(false);
        void this.router.navigate(['/games/admin/sprites', sprite.id]);
      },
      error: (err: HttpErrorResponse) => {
        this.busy.set(false);
        this.error.set(apiErrorMessage(err, 'Bild nicht angelegt.', { key: 'Schlüssel' }));
      },
    });
  }
}
