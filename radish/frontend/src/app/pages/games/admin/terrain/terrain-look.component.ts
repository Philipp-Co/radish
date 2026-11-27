import { HttpErrorResponse } from '@angular/common/http';
import { Component, EventEmitter, Input, OnChanges, OnDestroy, Output, inject, signal } from '@angular/core';
import { FormsModule } from '@angular/forms';
import { Observable } from 'rxjs';

import { apiErrorMessage } from '../../../../core/api-error';
import {
  AdminSpriteImage,
  AdminTerrainDecor,
  AdminTerrainLook,
  AdminTerrainType,
  ApiService,
} from '../../../../core/api.service';
import { ImageLightboxComponent } from '../../../../shared/image-lightbox/image-lightbox.component';

/** Wie viele Dekorationen ein Feld tragen kann (TERRAIN_DECOR_MAX im Backend). */
const DECOR_MAX = 2;
/** Wie weit eine Dekoration verschoben sein darf, in jede Richtung (TERRAIN_DECOR_OFFSET_MAX). */
const OFFSET_MAX = 128;

/** Eine Dekoration im Formular; image_id ist null, solange keine PNG gewaehlt ist. */
interface DecorDraft {
  image_id: number | null;
  offset_x: number;
  offset_y: number;
  in_front: boolean;
}

/**
 * Wie ein Tile-Typ im Spiel aussieht (radish/backend/api/interfaces/admin/views/
 * admin_terrain_views.py): seine PNGs hochladen, ersetzen und löschen, das Bild
 * statt des Würfels und bis zu zwei Dekorationen wählen. Gespeichert wird es als
 * assets/tiles/<typ>/tile.json für den Client (radish/client/schema/
 * tile.schema.json); ab dem nächsten Laden des Spiels ist es zu sehen.
 *
 * Die Vorschau zeichnet wie der Client: jedes Bild gestreckt auf den Rahmen einer
 * Kachel (256 × 256), die Dekorationen darin verschoben. Die gestrichelte Raute
 * ist die Oberseite des Feldes. Die Farbe des Würfels ohne Bild führt der Client
 * selbst; sie fehlt hier.
 */
@Component({
  selector: 'app-terrain-look',
  imports: [FormsModule, ImageLightboxComponent],
  template: `
    <div class="header">
      <h3>Aussehen</h3>
      <span class="muted">assets/tiles/{{ terrain.code }}/</span>
    </div>

    @if (error(); as message) {
      <p class="error">{{ message }}</p>
    }
    @if (notice(); as message) {
      <p class="notice">{{ message }}</p>
    }

    <div class="layout">
      <div class="preview" title="Vorschau wie im Spiel, verkleinert">
        <div class="stage">
        @if (imageUrl(image_id); as url) {
          <img class="layer" [src]="url" alt="" [style.left.px]="inset" [style.top.px]="inset" />
        }
        @for (entry of backDecor(); track $index) {
          @if (imageUrl(entry.image_id); as url) {
            <img
              class="layer"
              [src]="url"
              alt=""
              [style.left.px]="inset + entry.offset_x"
              [style.top.px]="inset + entry.offset_y"
            />
          }
        }
        <svg
          class="layer outline"
          viewBox="0 0 256 256"
          [style.left.px]="inset"
          [style.top.px]="inset"
          aria-hidden="true"
        >
          <polygon points="128,0 256,64 128,128 0,64" />
        </svg>
        @for (entry of frontDecor(); track $index) {
          @if (imageUrl(entry.image_id); as url) {
            <img
              class="layer"
              [src]="url"
              alt=""
              [style.left.px]="inset + entry.offset_x"
              [style.top.px]="inset + entry.offset_y"
            />
          }
        }
        </div>
      </div>

      <div class="form">
        <label>
          Bild statt des Würfels
          <select [(ngModel)]="image_id" [disabled]="busy()">
            <option [ngValue]="null">kein Bild -- Würfel in der Farbe des Typs</option>
            @for (image of terrain.images; track image.id) {
              <option [ngValue]="image.id">{{ image.name }}</option>
            }
          </select>
        </label>

        <h4>Dekorationen</h4>
        @if (decor.length === 0) {
          <p class="muted hint">Keine -- auf dem Feld steht nichts.</p>
        }
        @for (entry of decor; track $index; let i = $index) {
          <div class="decor">
            <label>
              Bild
              <select [(ngModel)]="entry.image_id" [disabled]="busy()">
                <option [ngValue]="null" disabled>wählen ...</option>
                @for (image of terrain.images; track image.id) {
                  <option [ngValue]="image.id">{{ image.name }}</option>
                }
              </select>
            </label>
            <label>
              Versatz x
              <input type="number" [min]="-offsetMax" [max]="offsetMax" [(ngModel)]="entry.offset_x" [disabled]="busy()" />
            </label>
            <label>
              Versatz y
              <input type="number" [min]="-offsetMax" [max]="offsetMax" [(ngModel)]="entry.offset_y" [disabled]="busy()" />
            </label>
            <label class="check">
              <input type="checkbox" [(ngModel)]="entry.in_front" [disabled]="busy()" />
              vor den Figuren
            </label>
            <button type="button" class="secondary" [disabled]="busy()" (click)="removeDecor(i)">Entfernen</button>
          </div>
        }
        @if (decor.length < decorMax) {
          <button type="button" class="secondary" [disabled]="busy() || terrain.images.length === 0" (click)="addDecor()">
            Dekoration hinzufügen
          </button>
        }
        <p class="muted hint">
          Versatz in Pixeln gegen das Feld, je höchstens {{ offsetMax }}: x nach rechts, y nach unten
          (negativ nach links und oben).
        </p>

        <div class="actions">
          <button type="button" [disabled]="busy() || !changed() || !complete()" (click)="saveLook()">
            Aussehen speichern
          </button>
          @if (changed()) {
            <button type="button" class="secondary" [disabled]="busy()" (click)="reset()">Verwerfen</button>
          }
        </div>
      </div>
    </div>

    <h4>PNGs</h4>
    <label class="upload">
      PNGs hochladen
      <input type="file" accept="image/png" multiple [disabled]="busy()" (change)="upload($event)" />
    </label>
    @if (terrain.images.length === 0) {
      <p class="muted hint">Noch keine PNG für diesen Typ.</p>
    } @else {
      <ul class="grid">
        @for (image of terrain.images; track image.id) {
          <li>
            <button type="button" class="thumb" [title]="image.name" (click)="open(image)">
              @if (previews()[previewKey(image)]; as url) {
                <img [src]="url" [alt]="image.name" />
              }
            </button>
            <span class="name">{{ image.name }}</span>
            <span class="muted size">{{ image.width }} × {{ image.height }}</span>
            <div class="image-actions">
              <label class="replace">
                Ersetzen
                <input type="file" accept="image/png" [disabled]="busy()" (change)="replace(image, $event)" />
              </label>
              <button type="button" class="secondary" [disabled]="busy()" (click)="deleteImage(image)">Löschen</button>
            </div>
          </li>
        }
      </ul>
    }

    @if (opened(); as image) {
      <app-image-lightbox [src]="image.src" [name]="image.name" (closed)="opened.set(null)" />
    }
  `,
  styles: [
    `
      :host {
        display: block;
      }
      .header {
        display: flex;
        align-items: baseline;
        gap: 0.75rem;
      }
      .header h3 {
        margin: 0 0 0.75rem;
      }
      .header .muted {
        font-size: 0.8rem;
      }
      h4 {
        margin: 1rem 0 0.5rem;
      }
      .layout {
        display: flex;
        flex-wrap: wrap;
        gap: 1.5rem;
        align-items: flex-start;
      }
      /* Die Buehne ist eine Kachel (256 x 256) mit Platz fuer den groessten Versatz
         rundherum, verkleinert auf drei Viertel. */
      .preview {
        width: 384px;
        height: 384px;
        flex: none;
        overflow: hidden;
        background: var(--surface-2, #1b1b20);
        border: 1px solid var(--border, #2a2a30);
      }
      .stage {
        position: relative;
        width: 512px;
        height: 512px;
        transform: scale(0.75);
        transform-origin: top left;
      }
      .layer {
        position: absolute;
        width: 256px;
        height: 256px;
      }
      .outline {
        pointer-events: none;
      }
      .outline polygon {
        fill: none;
        stroke: var(--text-dim, #8a8a92);
        stroke-width: 1;
        stroke-dasharray: 4 3;
      }
      .form {
        flex: 1;
        min-width: 18rem;
      }
      label {
        display: flex;
        flex-direction: column;
        gap: 0.2rem;
        font-size: 0.8rem;
        color: var(--text-dim, #8a8a92);
      }
      .decor {
        display: flex;
        flex-wrap: wrap;
        align-items: flex-end;
        gap: 0.5rem;
        margin-bottom: 0.5rem;
      }
      .decor input[type='number'] {
        width: 6rem;
      }
      label.check {
        flex-direction: row;
        align-items: center;
        gap: 0.35rem;
      }
      .actions {
        display: flex;
        gap: 0.5rem;
        margin-top: 0.75rem;
      }
      .hint {
        font-size: 0.85rem;
      }
      .upload {
        margin-bottom: 0.75rem;
      }
      .grid {
        list-style: none;
        margin: 0;
        padding: 0;
        display: grid;
        grid-template-columns: repeat(auto-fill, minmax(9rem, 1fr));
        gap: 0.75rem;
      }
      .grid li {
        display: flex;
        flex-direction: column;
        gap: 0.2rem;
      }
      .thumb {
        padding: 0;
        aspect-ratio: 1;
        background: var(--surface-2, #1b1b20);
        border: 1px solid var(--border, #2a2a30);
        cursor: zoom-in;
      }
      .thumb img {
        width: 100%;
        height: 100%;
        object-fit: contain;
      }
      .name {
        font-size: 0.85rem;
        word-break: break-all;
      }
      .size {
        font-size: 0.75rem;
      }
      .image-actions {
        display: flex;
        gap: 0.35rem;
        align-items: center;
      }
      .replace {
        cursor: pointer;
        text-decoration: underline;
      }
      .replace input {
        display: none;
      }
      .notice {
        color: var(--success, #6fbf73);
      }
    `,
  ],
})
export class TerrainLookComponent implements OnChanges, OnDestroy {
  private readonly api = inject(ApiService);

  /** Der Typ, wie das Backend ihn zuletzt geliefert hat. */
  @Input({ required: true }) terrain!: AdminTerrainType;
  /** Der Typ nach jeder Änderung, damit die Seite ihn übernimmt. */
  @Output() readonly updated = new EventEmitter<AdminTerrainType>();

  readonly decorMax = DECOR_MAX;
  readonly offsetMax = OFFSET_MAX;
  /** Wo der Rahmen des Feldes in der Vorschau liegt: rundherum Platz für den größten Versatz. */
  readonly inset = OFFSET_MAX;

  image_id: number | null = null;
  decor: DecorDraft[] = [];

  readonly busy = signal(false);
  readonly error = signal<string | null>(null);
  readonly notice = signal<string | null>(null);
  /** Object-URLs der Vorschauen, je PNG und Fassung (previewKey). */
  readonly previews = signal<Record<string, string>>({});
  readonly opened = signal<{ src: string; name: string } | null>(null);

  private loadedCode: string | null = null;
  /** Vorschauen, die gerade laden -- nicht ein zweites Mal anfragen. */
  private readonly pending = new Set<string>();

  ngOnChanges(): void {
    // Ein anderer Typ: Formular und Meldungen neu; derselbe nach dem Speichern: nur die Vorschauen.
    if (this.terrain.code !== this.loadedCode) {
      this.loadedCode = this.terrain.code;
      this.revokeAll();
      this.error.set(null);
      this.notice.set(null);
      this.reset();
    }
    this.loadPreviews();
  }

  ngOnDestroy(): void {
    this.revokeAll();
  }

  previewKey(image: AdminSpriteImage): string {
    return `${image.id}:${image.sha256}`;
  }

  imageUrl(imageId: number | null): string | null {
    const image = this.terrain.images.find((entry) => entry.id === imageId);
    return image ? (this.previews()[this.previewKey(image)] ?? null) : null;
  }

  backDecor(): DecorDraft[] {
    return this.decor.filter((entry) => !entry.in_front);
  }

  frontDecor(): DecorDraft[] {
    return this.decor.filter((entry) => entry.in_front);
  }

  open(image: AdminSpriteImage): void {
    const src = this.previews()[this.previewKey(image)];
    if (src) {
      this.opened.set({ src, name: image.name });
    }
  }

  addDecor(): void {
    this.decor = [...this.decor, { image_id: null, offset_x: 0, offset_y: 0, in_front: false }];
  }

  removeDecor(index: number): void {
    this.decor = this.decor.filter((_, i) => i !== index);
  }

  /** Ob das Formular vom gespeicherten Stand abweicht. */
  changed(): boolean {
    return JSON.stringify(this.look()) !== JSON.stringify(savedLook(this.terrain));
  }

  /** Ob jede Dekoration eine PNG hat und die Versätze im Rahmen sind. */
  complete(): boolean {
    return this.decor.every(
      (entry) => entry.image_id !== null && inRange(entry.offset_x) && inRange(entry.offset_y),
    );
  }

  /** Formular auf den gespeicherten Stand von "terrain" (Vorgabe: der aktuelle Typ). */
  reset(terrain: AdminTerrainType = this.terrain): void {
    const saved = savedLook(terrain);
    this.image_id = saved.image_id;
    this.decor = saved.decor.map((entry) => ({ ...entry }));
  }

  saveLook(): void {
    // Aus der Antwort fuellen, nicht aus this.terrain: die Detailseite reicht
    // den neuen Typ per Template-Bindung zurueck, er kommt hier also erst beim
    // naechsten Neuzeichnen an -- bis dahin ist this.terrain noch der alte
    // Stand, und das Formular spraenge auf die Werte vor dem Speichern zurueck.
    this.run(this.api.setTerrainLook(this.terrain.code, this.look()), 'Aussehen gespeichert.', 'Aussehen nicht gespeichert.', (saved) =>
      this.reset(saved),
    );
  }

  /** Lädt die gewählten PNGs nacheinander hoch -- bei einem Fehler endet es dort. */
  upload(event: Event): void {
    const input = event.target as HTMLInputElement;
    const files = Array.from(input.files ?? []);
    input.value = '';
    const next = (index: number): void => {
      if (index < files.length) {
        this.run(
          this.api.uploadTerrainImage(this.terrain.code, files[index]),
          `${files.length === 1 ? files[index].name : `${index + 1} von ${files.length} PNGs`} hochgeladen.`,
          `${files[index].name} nicht hochgeladen.`,
          () => next(index + 1),
        );
      }
    };
    next(0);
  }

  replace(image: AdminSpriteImage, event: Event): void {
    const input = event.target as HTMLInputElement;
    const file = input.files?.[0];
    input.value = '';
    if (file) {
      this.run(
        this.api.replaceTerrainImage(this.terrain.code, image.id, file),
        `${image.name} ersetzt.`,
        `${image.name} nicht ersetzt.`,
      );
    }
  }

  deleteImage(image: AdminSpriteImage): void {
    if (!confirm(`${image.name} wirklich löschen?`)) {
      return;
    }
    const code = this.terrain.code;
    this.start();
    this.api.deleteTerrainImage(code, image.id).subscribe({
      next: () =>
        // Die Antwort ist leer: den Typ ohne die PNG weiterreichen.
        this.done({ ...this.terrain, images: this.terrain.images.filter((entry) => entry.id !== image.id) }, `${image.name} gelöscht.`),
      error: (err: HttpErrorResponse) => this.fail(err, `${image.name} nicht gelöscht.`),
    });
  }

  private look(): AdminTerrainLook {
    return {
      image_id: this.image_id,
      decor: this.decor.map((entry) => ({ ...entry, image_id: entry.image_id ?? 0 })),
    };
  }

  private run(
    request: Observable<AdminTerrainType>,
    success: string,
    failure: string,
    after?: (terrain: AdminTerrainType) => void,
  ): void {
    this.start();
    request.subscribe({
      next: (terrain) => {
        this.done(terrain, success);
        after?.(terrain);
      },
      error: (err: HttpErrorResponse) => this.fail(err, failure),
    });
  }

  private start(): void {
    this.busy.set(true);
    this.error.set(null);
    this.notice.set(null);
  }

  private done(terrain: AdminTerrainType, message: string): void {
    this.busy.set(false);
    this.notice.set(message);
    // Die Elternkomponente reicht den Typ zurück, ngOnChanges lädt die Vorschauen.
    this.updated.emit(terrain);
  }

  private fail(err: HttpErrorResponse, fallback: string): void {
    this.busy.set(false);
    this.error.set(apiErrorMessage(err, fallback));
  }

  /** Lädt fehlende Vorschauen und gibt die von gelöschten oder ersetzten PNGs frei. */
  private loadPreviews(): void {
    const wanted = new Set(this.terrain.images.map((image) => this.previewKey(image)));
    const current = { ...this.previews() };
    for (const [key, url] of Object.entries(current)) {
      if (!wanted.has(key)) {
        URL.revokeObjectURL(url);
        delete current[key];
      }
    }
    this.previews.set(current);
    const code = this.terrain.code;
    for (const image of this.terrain.images) {
      const key = this.previewKey(image);
      if (!(key in current) && !this.pending.has(key)) {
        this.pending.add(key);
        this.api.terrainImageBlob(code, image.id).subscribe({
          next: (blob) => {
            if (code === this.loadedCode) {
              this.previews.update((all) => ({ ...all, [key]: URL.createObjectURL(blob) }));
            }
          },
          complete: () => this.pending.delete(key),
          error: () => this.pending.delete(key),
        });
      }
    }
  }

  private revokeAll(): void {
    this.pending.clear();
    Object.values(this.previews()).forEach((url) => URL.revokeObjectURL(url));
    this.previews.set({});
  }
}

function savedLook(terrain: AdminTerrainType): { image_id: number | null; decor: AdminTerrainDecor[] } {
  return {
    image_id: terrain.image_id,
    decor: terrain.decor.map(({ image_id, offset_x, offset_y, in_front }) => ({ image_id, offset_x, offset_y, in_front })),
  };
}

function inRange(offset: number): boolean {
  return Number.isInteger(offset) && Math.abs(offset) <= OFFSET_MAX;
}
