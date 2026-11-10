import { HttpErrorResponse } from '@angular/common/http';
import { Component, OnDestroy, OnInit, inject, signal } from '@angular/core';
import { FormsModule } from '@angular/forms';
import { ActivatedRoute, Router, RouterLink } from '@angular/router';
import { Observable } from 'rxjs';

import { apiErrorMessage } from '../../../../core/api-error';
import {
  AdminSprite,
  AdminSpriteAtlas,
  AdminSpriteImage,
  AdminSpriteValues,
  ApiService,
  SpriteAnimation,
  SpriteFacing,
} from '../../../../core/api.service';
import { UnitSpriteService } from '../../../../core/unit-sprite.service';
import { ImageLightboxComponent } from '../../../../shared/image-lightbox/image-lightbox.component';

/** Die Animationen in der Reihenfolge der sprite.json, mit Bezeichnung. */
const ANIMATIONS: { animation: SpriteAnimation; label: string; rotation: boolean }[] = [
  { animation: 'turn', label: 'Drehen', rotation: true },
  { animation: 'aim', label: 'Zielen', rotation: true },
  { animation: 'move', label: 'Ziehen', rotation: false },
  { animation: 'fire', label: 'Schießen', rotation: false },
  { animation: 'destroyed', label: 'Zerstört', rotation: false },
];

/** Die Blickrichtungen auf dem Bildschirm, wie in der sprite.json. */
const FACINGS: { facing: SpriteFacing; label: string }[] = [
  { facing: 'sw', label: 'links unten (sw)' },
  { facing: 'se', label: 'rechts unten (se)' },
  { facing: 'ne', label: 'rechts oben (ne)' },
  { facing: 'nw', label: 'links oben (nw)' },
];

/** Die Werte, wie das Formular sie bearbeitet -- Zeiten leer statt null. */
type ValuesDraft = Omit<AdminSpriteValues, 'step_ms' | 'turn_ms' | 'aim_ms' | 'aim_hold_ms'> & {
  step_ms: number | null;
  turn_ms: number | null;
  aim_ms: number | null;
  aim_hold_ms: number | null;
};

/** Eine Zeile der Atlas-Tabelle: ein Platz, belegt oder nicht. */
interface AtlasRow {
  animation: SpriteAnimation;
  facing: SpriteFacing;
  facingLabel: string;
  rotation: boolean;
  /** Erste Zeile ihrer Animation -- dort steht "für alle Richtungen übernehmen". */
  first: boolean;
  animationLabel: string;
  image_id: number | null;
  frame_width: number;
  frame_height: number;
  frame_count: number;
  key_frames: number[];
  fps: number;
}

/**
 * Ein hochgeladenes Einheitenbild bearbeiten: seine Werte (Anker,
 * Zeichenbreite, Schatten, Zeiten), seine PNGs (hochladen, ersetzen,
 * loeschen, in Originalgroesse ansehen) und seine Atlanten -- je Animation
 * und Blickrichtung, welche PNG sie zeigt und wie sie aufgeteilt ist. Die
 * sprite.json erzeugt das Backend daraus (radish/backend/api/domain/sprites/).
 *
 * Was noch fehlt, steht oben unter "Offen"; ein vollstaendiges Bild erscheint
 * ohne Neustart in der Bild-Auswahl der Einheitentypen und im Spiel.
 *
 * Die PNGs kommen fuer die Vorschau ueber den Client mit Token
 * (ApiService.spriteImageBlob) und stehen als Object-URL im Bild; beim Wechsel
 * und am Ende werden sie freigegeben.
 */
@Component({
  selector: 'app-sprite-edit',
  imports: [FormsModule, RouterLink, ImageLightboxComponent],
  template: `
    <p><a routerLink="/games/admin/sprites">← Alle Einheitenbilder</a></p>

    @if (loadError(); as message) {
      <p class="error">{{ message }}</p>
    }
    @if (message(); as text) {
      <p class="success">{{ text }}</p>
    }
    @if (error(); as text) {
      <p class="error">{{ text }}</p>
    }

    @if (sprite(); as s) {
      <section class="card">
        <div class="header">
          <h2>{{ s.key }}</h2>
          @if (s.complete) {
            <span class="badge ok">vollständig – im Spiel</span>
          } @else {
            <span class="badge draft">Entwurf</span>
          }
          <button
            type="button"
            class="secondary danger"
            [disabled]="busy() || s.used_by.length > 0"
            [title]="s.used_by.length ? 'Wird verwendet: ' + s.used_by.join(', ') : ''"
            (click)="deleteSprite(s)"
          >
            Löschen
          </button>
        </div>
        <p class="muted">
          {{ s.used_by.length ? 'Verwendet von ' + s.used_by.join(', ') + '.' : 'Noch von keinem Einheitentyp verwendet.' }}
        </p>
        @if (s.problems.length) {
          <details [open]="s.problems.length <= 6">
            <summary>Offen: {{ s.problems.length }}</summary>
            <ul class="problems">
              @for (problem of s.problems; track problem) {
                <li>{{ problem }}</li>
              }
            </ul>
          </details>
        }
      </section>

      <section class="card">
        <h3>Werte</h3>
        <div class="values">
          <label title="Wo die Figur in ihrer Zelle auf dem Boden steht, Anteil an der Breite (0–1).">
            Anker x
            <input type="number" min="0" max="1" step="0.01" [(ngModel)]="values.anchor_x" />
          </label>
          <label title="Wo die Figur in ihrer Zelle auf dem Boden steht, Anteil an der Höhe (0–1).">
            Anker y
            <input type="number" min="0" max="1" step="0.01" [(ngModel)]="values.anchor_y" />
          </label>
          <label title="Breite einer Zelle auf dem Bildschirm in Pixeln (1–1024).">
            Zeichenbreite
            <input type="number" min="1" max="1024" [(ngModel)]="values.draw_width" />
          </label>
          <label title="Leer: Standard des Clients.">
            Schritt (ms)
            <input type="number" min="1" max="10000" [(ngModel)]="values.step_ms" />
          </label>
          <label title="Eine Vierteldrehung. Leer: Standard des Clients.">
            Drehen (ms)
            <input type="number" min="1" max="10000" [(ngModel)]="values.turn_ms" />
          </label>
          <label title="Eine Vierteldrehung beim Zielen. Leer: Standard des Clients.">
            Zielen (ms)
            <input type="number" min="1" max="10000" [(ngModel)]="values.aim_ms" />
          </label>
          <label title="Wie lange nach dem Schuss auf das Ziel gerichtet. Leer: Standard des Clients.">
            Halten (ms)
            <input type="number" min="1" max="10000" [(ngModel)]="values.aim_hold_ms" />
          </label>
          <label class="checkbox">
            <input type="checkbox" [(ngModel)]="values.own_shadow" />
            Schatten in den Bildern
          </label>
        </div>
        <div class="actions">
          <button type="button" [disabled]="busy()" (click)="saveValues(s)">Werte speichern</button>
        </div>
      </section>

      <section class="card">
        <h3>PNG-Dateien</h3>
        <label class="upload">
          PNGs hochladen (mehrere möglich)
          <input
            type="file"
            accept="image/png"
            multiple
            [disabled]="busy()"
            (change)="upload(s, $event)"
          />
        </label>
        <p class="muted hint">
          Der Dateiname wird klein geschrieben übernommen: a–z, 0–9 und _, mit .png, höchstens 31 Zeichen.
        </p>
        @if (s.images.length === 0) {
          <p class="muted">Noch keine PNG.</p>
        } @else {
          <ul class="grid">
            @for (image of s.images; track image.id) {
              <li>
                <button type="button" class="thumb checkerboard" [title]="image.name" (click)="open(image)">
                  @if (previews()[previewKey(image)]; as url) {
                    <img [src]="url" [alt]="image.name" />
                  }
                </button>
                <span class="name">{{ image.name }}</span>
                <span class="muted size">{{ image.width }} × {{ image.height }}</span>
                <div class="image-actions">
                  <label class="file-button">
                    Ersetzen
                    <input type="file" accept="image/png" [disabled]="busy()" (change)="replace(s, image, $event)" />
                  </label>
                  <button type="button" class="secondary danger small" [disabled]="busy()" (click)="deleteImage(s, image)">
                    Löschen
                  </button>
                </div>
              </li>
            }
          </ul>
        }
      </section>

      <section class="card">
        <h3>Atlanten</h3>
        <p class="muted hint">
          Je Animation und Richtung eine PNG, zeilenweise von links oben in Zellen geteilt. Drehen und
          Zielen sind volle Drehungen mit den Bildern bei 0, 90, 180 und 270 Grad; die übrigen laufen mit
          Bildern je Sekunde ab. Ein Platz ohne PNG bleibt leer.
        </p>
        <div class="table-wrap">
          <table>
            <thead>
              <tr>
                <th>Animation</th>
                <th>Richtung</th>
                <th>PNG</th>
                <th title="Breite × Höhe einer Zelle in Pixeln">Zelle</th>
                <th title="Wie viele Zellen belegt sind">Bilder</th>
                <th>Schlüsselbilder / fps</th>
              </tr>
            </thead>
            <tbody>
              @for (row of rows; track row.animation + row.facing) {
                <tr [class.group]="row.first">
                  <td>
                    @if (row.first) {
                      <strong>{{ row.animationLabel }}</strong>
                      @if (!row.rotation || row.animation === 'aim') {
                        <button type="button" class="link" (click)="copyToAll(row)" title="Diese Zeile in die übrigen Richtungen übernehmen">
                          für alle Richtungen
                        </button>
                      }
                    }
                  </td>
                  <td class="muted">{{ row.facingLabel }}</td>
                  <td>
                    <select [(ngModel)]="row.image_id">
                      <option [ngValue]="null">–</option>
                      @for (image of s.images; track image.id) {
                        <option [ngValue]="image.id">{{ image.name }}</option>
                      }
                    </select>
                  </td>
                  <td class="cell">
                    <input type="number" min="1" max="1024" [(ngModel)]="row.frame_width" aria-label="Zellbreite" />
                    ×
                    <input type="number" min="1" max="1024" [(ngModel)]="row.frame_height" aria-label="Zellhöhe" />
                  </td>
                  <td>
                    <input type="number" min="1" max="1024" [(ngModel)]="row.frame_count" aria-label="Bilder" />
                  </td>
                  <td class="cell">
                    @if (row.rotation) {
                      @for (index of keyIndexes; track index) {
                        <input
                          type="number"
                          min="0"
                          max="1023"
                          [(ngModel)]="row.key_frames[index]"
                          [attr.aria-label]="'Schlüsselbild ' + index * 90 + ' Grad'"
                          [title]="index * 90 + ' Grad'"
                        />
                      }
                    } @else {
                      <input type="number" min="0.1" max="60" step="0.5" [(ngModel)]="row.fps" aria-label="Bilder je Sekunde" />
                      fps
                    }
                  </td>
                </tr>
              }
            </tbody>
          </table>
        </div>
        <div class="actions">
          <button type="button" [disabled]="busy()" (click)="saveAtlases(s)">Atlanten speichern</button>
        </div>
      </section>
    } @else if (!loadError()) {
      <p class="muted">Lade ...</p>
    }

    @if (opened(); as image) {
      <app-image-lightbox [src]="image.src" [name]="image.name" (closed)="opened.set(null)" />
    }
  `,
  styles: [
    `
      :host {
        display: flex;
        flex-direction: column;
        gap: 1rem;
      }
      p {
        margin: 0;
      }
      .header {
        display: flex;
        align-items: center;
        gap: 0.75rem;
      }
      .header h2 {
        margin: 0;
      }
      .header button {
        margin-left: auto;
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
      .problems {
        margin: 0.5rem 0 0;
        font-size: 0.85rem;
      }
      h3 {
        margin: 0 0 0.5rem;
      }
      .values {
        display: grid;
        grid-template-columns: repeat(auto-fill, minmax(8rem, 1fr));
        gap: 0.5rem;
      }
      label {
        display: flex;
        flex-direction: column;
        gap: 0.2rem;
        font-size: 0.8rem;
        color: var(--text-dim, #8a8a92);
      }
      label.checkbox {
        flex-direction: row;
        align-items: center;
        grid-column: span 2;
      }
      .actions {
        display: flex;
        justify-content: flex-end;
        margin-top: 0.75rem;
      }
      .hint {
        font-size: 0.8rem;
        margin: 0.25rem 0 0.75rem;
      }
      .grid {
        list-style: none;
        margin: 0;
        padding: 0;
        display: grid;
        grid-template-columns: repeat(auto-fill, minmax(8.5rem, 1fr));
        gap: 0.75rem;
      }
      .grid li {
        display: flex;
        flex-direction: column;
        gap: 0.2rem;
        min-width: 0;
      }
      .thumb {
        aspect-ratio: 1;
        width: 100%;
        padding: 0.25rem;
        border: 1px solid var(--border, #2a2a30);
        border-radius: 4px;
        cursor: zoom-in;
      }
      .thumb:hover {
        border-color: var(--accent, #5fb9a3);
      }
      .thumb img {
        width: 100%;
        height: 100%;
        object-fit: contain;
      }
      .checkerboard {
        background-color: #1a1a1e;
        background-image:
          linear-gradient(45deg, #24242a 25%, transparent 25%),
          linear-gradient(-45deg, #24242a 25%, transparent 25%),
          linear-gradient(45deg, transparent 75%, #24242a 75%),
          linear-gradient(-45deg, transparent 75%, #24242a 75%);
        background-size: 16px 16px;
        background-position: 0 0, 0 8px, 8px -8px, -8px 0;
      }
      .name {
        font-size: 0.8rem;
        overflow: hidden;
        text-overflow: ellipsis;
        white-space: nowrap;
      }
      .size {
        font-size: 0.75rem;
      }
      .image-actions {
        display: flex;
        gap: 0.25rem;
        align-items: center;
      }
      /* Ein Datei-Input als Knopf: das Input selbst ist unsichtbar im Label. */
      .file-button {
        flex-direction: row;
        cursor: pointer;
        font-size: 0.75rem;
        padding: 0.15rem 0.4rem;
        border: 1px solid var(--border, #2a2a30);
        border-radius: 4px;
        color: var(--text, #d8d8dc);
      }
      .file-button input {
        display: none;
      }
      .small {
        font-size: 0.75rem;
        padding: 0.15rem 0.4rem;
      }
      .danger {
        color: var(--danger, #d97a7a);
      }
      .table-wrap {
        overflow-x: auto;
      }
      table {
        border-collapse: collapse;
        width: 100%;
        font-size: 0.85rem;
      }
      th,
      td {
        text-align: left;
        padding: 0.25rem 0.4rem;
        border-bottom: 1px solid var(--border, #2a2a30);
        vertical-align: middle;
      }
      tr.group td {
        border-top: 2px solid var(--border, #2a2a30);
      }
      td input[type='number'] {
        width: 4.5rem;
      }
      td.cell {
        white-space: nowrap;
      }
      .link {
        display: block;
        background: none;
        border: none;
        padding: 0;
        color: var(--accent, #5fb9a3);
        font-size: 0.75rem;
        font-weight: 400;
        cursor: pointer;
        text-decoration: underline;
      }
      .success {
        color: var(--accent, #5fb9a3);
      }
    `,
  ],
})
export class SpriteEditComponent implements OnInit, OnDestroy {
  private readonly api = inject(ApiService);
  private readonly route = inject(ActivatedRoute);
  private readonly router = inject(Router);
  private readonly unitSprites = inject(UnitSpriteService);

  readonly keyIndexes = [0, 1, 2, 3];

  readonly sprite = signal<AdminSprite | null>(null);
  readonly loadError = signal<string | null>(null);
  readonly busy = signal(false);
  readonly message = signal<string | null>(null);
  readonly error = signal<string | null>(null);

  /** Object-URLs der Vorschauen, je PNG und Fassung (previewKey). */
  readonly previews = signal<Record<string, string>>({});
  readonly opened = signal<{ src: string; name: string } | null>(null);

  values: ValuesDraft = {
    anchor_x: 0.5,
    anchor_y: 0.8,
    draw_width: 128,
    own_shadow: false,
    step_ms: null,
    turn_ms: null,
    aim_ms: null,
    aim_hold_ms: null,
  };
  rows: AtlasRow[] = [];

  private spriteId = 0;

  ngOnInit(): void {
    this.spriteId = Number(this.route.snapshot.paramMap.get('id'));
    this.api.getSprite(this.spriteId).subscribe({
      next: (sprite) => {
        this.show(sprite, true);
      },
      error: (err: HttpErrorResponse) =>
        this.loadError.set(apiErrorMessage(err, 'Einheitenbild konnte nicht geladen werden.')),
    });
  }

  ngOnDestroy(): void {
    Object.values(this.previews()).forEach((url) => URL.revokeObjectURL(url));
  }

  previewKey(image: AdminSpriteImage): string {
    return `${image.id}:${image.sha256}`;
  }

  open(image: AdminSpriteImage): void {
    const src = this.previews()[this.previewKey(image)];
    if (src) {
      this.opened.set({ src, name: `${image.name} – ${image.width} × ${image.height}` });
    }
  }

  saveValues(sprite: AdminSprite): void {
    const blankToNull = (value: number | null): number | null =>
      value === null || (value as unknown) === '' ? null : Number(value);
    this.run(
      this.api.updateSprite(sprite.id, {
        anchor_x: Number(this.values.anchor_x),
        anchor_y: Number(this.values.anchor_y),
        draw_width: Number(this.values.draw_width),
        own_shadow: this.values.own_shadow,
        step_ms: blankToNull(this.values.step_ms),
        turn_ms: blankToNull(this.values.turn_ms),
        aim_ms: blankToNull(this.values.aim_ms),
        aim_hold_ms: blankToNull(this.values.aim_hold_ms),
      }),
      'Werte gespeichert.',
      'Werte nicht gespeichert.',
    );
  }

  /** Laedt die gewaehlten PNGs nacheinander hoch -- bei einem Fehler endet es dort. */
  upload(sprite: AdminSprite, event: Event): void {
    const input = event.target as HTMLInputElement;
    const files = Array.from(input.files ?? []);
    input.value = '';
    const next = (index: number): void => {
      if (index >= files.length) {
        return;
      }
      this.run(
        this.api.uploadSpriteImage(sprite.id, files[index]),
        `${files.length === 1 ? files[index].name : `${index + 1} von ${files.length} PNGs`} hochgeladen.`,
        `${files[index].name} nicht hochgeladen.`,
        () => next(index + 1),
      );
    };
    next(0);
  }

  replace(sprite: AdminSprite, image: AdminSpriteImage, event: Event): void {
    const input = event.target as HTMLInputElement;
    const file = input.files?.[0];
    input.value = '';
    if (file) {
      this.run(
        this.api.replaceSpriteImage(sprite.id, image.id, file),
        `${image.name} ersetzt.`,
        `${image.name} nicht ersetzt.`,
      );
    }
  }

  deleteImage(sprite: AdminSprite, image: AdminSpriteImage): void {
    if (!confirm(`${image.name} wirklich löschen?`)) {
      return;
    }
    this.busy.set(true);
    this.clearMessages();
    this.api.deleteSpriteImage(sprite.id, image.id).subscribe({
      next: () => this.reload(`${image.name} gelöscht.`),
      error: (err: HttpErrorResponse) => this.fail(err, `${image.name} nicht gelöscht.`),
    });
  }

  deleteSprite(sprite: AdminSprite): void {
    if (!confirm(`Einheitenbild „${sprite.key}“ mit allen PNGs wirklich löschen?`)) {
      return;
    }
    this.busy.set(true);
    this.clearMessages();
    this.api.deleteSprite(sprite.id).subscribe({
      next: () => {
        this.unitSprites.refresh();
        void this.router.navigate(['/games/admin/sprites']);
      },
      error: (err: HttpErrorResponse) => this.fail(err, 'Bild nicht gelöscht.'),
    });
  }

  /** Uebernimmt PNG, Zelle, Bilder und Schluesselbilder/fps in die uebrigen Richtungen. */
  copyToAll(source: AtlasRow): void {
    for (const row of this.rows) {
      if (row !== source && row.animation === source.animation) {
        row.image_id = source.image_id;
        row.frame_width = source.frame_width;
        row.frame_height = source.frame_height;
        row.frame_count = source.frame_count;
        row.key_frames = [...source.key_frames];
        row.fps = source.fps;
      }
    }
  }

  saveAtlases(sprite: AdminSprite): void {
    const atlases: AdminSpriteAtlas[] = this.rows
      .filter((row) => row.image_id !== null)
      .map((row) => ({
        animation: row.animation,
        facing: row.facing,
        image_id: row.image_id as number,
        frame_width: Number(row.frame_width),
        frame_height: Number(row.frame_height),
        frame_count: Number(row.frame_count),
        key_frames: row.rotation ? row.key_frames.map(Number) : null,
        fps: row.rotation ? null : Number(row.fps),
      }));
    this.run(this.api.putSpriteAtlases(sprite.id, atlases), 'Atlanten gespeichert.', 'Atlanten nicht gespeichert.', undefined, true);
  }

  /**
   * Fuehrt eine Aenderung aus und zeigt danach das Bild, wie das Backend es
   * zurueckgibt. Die Atlas-Tabelle wird nur nach dem Speichern der Atlanten neu
   * gefuellt -- sonst gingen dort Eingaben verloren, etwa beim Hochladen.
   */
  private run(
    request: Observable<AdminSprite>,
    done: string,
    failed: string,
    then?: () => void,
    resetRows = false,
  ): void {
    this.busy.set(true);
    this.clearMessages();
    request.subscribe({
      next: (sprite) => {
        this.busy.set(false);
        this.message.set(done);
        this.unitSprites.refresh();
        this.show(sprite, resetRows);
        then?.();
      },
      error: (err: HttpErrorResponse) => this.fail(err, failed),
    });
  }

  private reload(done: string): void {
    this.api.getSprite(this.spriteId).subscribe({
      next: (sprite) => {
        this.busy.set(false);
        this.message.set(done);
        this.unitSprites.refresh();
        this.show(sprite, false);
      },
      error: (err: HttpErrorResponse) => this.fail(err, 'Einheitenbild konnte nicht neu geladen werden.'),
    });
  }

  private fail(err: HttpErrorResponse, fallback: string): void {
    this.busy.set(false);
    this.error.set(apiErrorMessage(err, fallback));
  }

  private clearMessages(): void {
    this.message.set(null);
    this.error.set(null);
  }

  /** Zeigt "sprite": Werte, Vorschauen und -- mit "resetRows" -- die Atlas-Tabelle. */
  private show(sprite: AdminSprite, resetRows: boolean): void {
    const first = this.sprite() === null;
    this.sprite.set(sprite);
    if (first) {
      this.values = {
        anchor_x: sprite.anchor_x,
        anchor_y: sprite.anchor_y,
        draw_width: sprite.draw_width,
        own_shadow: sprite.own_shadow,
        step_ms: sprite.step_ms,
        turn_ms: sprite.turn_ms,
        aim_ms: sprite.aim_ms,
        aim_hold_ms: sprite.aim_hold_ms,
      };
    }
    if (first || resetRows) {
      this.rows = buildRows(sprite);
    }
    this.loadPreviews(sprite);
  }

  /** Laedt fehlende Vorschauen und gibt die von geloeschten oder ersetzten PNGs frei. */
  private loadPreviews(sprite: AdminSprite): void {
    const wanted = new Set(sprite.images.map((image) => this.previewKey(image)));
    const current = { ...this.previews() };
    for (const [key, url] of Object.entries(current)) {
      if (!wanted.has(key)) {
        URL.revokeObjectURL(url);
        delete current[key];
      }
    }
    this.previews.set(current);
    for (const image of sprite.images) {
      const key = this.previewKey(image);
      if (!(key in current)) {
        this.api.spriteImageBlob(sprite.id, image.id).subscribe({
          next: (blob) => this.previews.update((all) => ({ ...all, [key]: URL.createObjectURL(blob) })),
        });
      }
    }
  }
}

/** Die 17 Zeilen der Atlas-Tabelle, belegt aus den gespeicherten Atlanten. */
function buildRows(sprite: AdminSprite): AtlasRow[] {
  const saved = new Map(sprite.atlases.map((atlas) => [`${atlas.animation}:${atlas.facing}`, atlas]));
  const rows: AtlasRow[] = [];
  for (const { animation, label, rotation } of ANIMATIONS) {
    const facings = animation === 'turn' ? [{ facing: '' as SpriteFacing, label: 'alle' }] : FACINGS;
    facings.forEach(({ facing, label: facingLabel }, index) => {
      const atlas = saved.get(`${animation}:${facing}`);
      rows.push({
        animation,
        facing,
        facingLabel,
        rotation,
        first: index === 0,
        animationLabel: label,
        image_id: atlas?.image_id ?? null,
        frame_width: atlas?.frame_width ?? 128,
        frame_height: atlas?.frame_height ?? 128,
        frame_count: atlas?.frame_count ?? (rotation ? 4 : 1),
        key_frames: [...(atlas?.key_frames ?? [0, 1, 2, 3])],
        fps: atlas?.fps ?? 10,
      });
    });
  }
  return rows;
}
