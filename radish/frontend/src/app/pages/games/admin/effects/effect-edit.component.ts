import { DecimalPipe } from '@angular/common';
import { HttpErrorResponse } from '@angular/common/http';
import { Component, OnDestroy, OnInit, inject, signal } from '@angular/core';
import { FormsModule } from '@angular/forms';
import { ActivatedRoute, Router, RouterLink } from '@angular/router';
import { Observable } from 'rxjs';

import { apiErrorMessage } from '../../../../core/api-error';
import {
  AdminEffect,
  AdminEffectAtlas,
  AdminEffectValues,
  AdminSpriteImage,
  ApiService,
  EFFECT_KINDS,
  EffectFacing,
} from '../../../../core/api.service';
import { ImageLightboxComponent } from '../../../../shared/image-lightbox/image-lightbox.component';

/** Die Blickrichtungen auf dem Bildschirm, wie bei den Einheitenbildern. */
const FACINGS: { facing: EffectFacing; label: string }[] = [
  { facing: 'sw', label: 'links unten (sw)' },
  { facing: 'se', label: 'rechts unten (se)' },
  { facing: 'ne', label: 'rechts oben (ne)' },
  { facing: 'nw', label: 'links oben (nw)' },
];

/**
 * Kantenlaenge der Vorschau-Buehne in Pixeln. Der Einschlag liegt in ihrer
 * Mitte; groessere Effekte werden verkleinert, bis sie bei jedem Anker
 * hineinpassen.
 */
const STAGE_SIZE = 288;

/** Ein Iso-Feld im Client (RAD_ISO_TILE_WIDTH/-HEIGHT); der Einschlag liegt in seiner Mitte. */
const ISO_TILE_WIDTH = 256;
const ISO_TILE_HEIGHT = 128;

/** Pause zwischen zwei Durchlaeufen der Vorschau, in Millisekunden. */
const ANIMATION_PAUSE_MS = 600;

/** Der Atlas einer Richtung im Editor, belegt oder nicht. */
interface AtlasRow {
  facing: EffectFacing;
  facingLabel: string;
  image_id: number | null;
  frame_width: number;
  frame_height: number;
  first_frame: number;
  frame_count: number;
  anchor_x: number;
  anchor_y: number;
}

/** Wie eine Zelle einer Richtung in der Vorschau steht (siehe framePreview). */
interface CellPreview {
  url: string;
  width: number;
  height: number;
  size: string;
  position: string;
}

/**
 * Einen Effekt bearbeiten: seine Werte (Art, Zeichenbreite, Dauer), seine
 * PNGs (hochladen, ersetzen, loeschen, in Originalgroesse ansehen) und seine
 * Atlanten -- je Blickrichtung, welche PNG ihn zeigt, wie sie in Zellen
 * geteilt ist, ab welcher Zelle wie viele Bilder folgen und wo der Einschlag
 * in der Zelle liegt. Mehrere Richtungen koennen sich eine PNG teilen, ihre
 * Bilder stehen dann hintereinander.
 *
 * Was noch fehlt, steht oben unter "Offen"; ein vollstaendiger Effekt
 * erscheint ab dem naechsten Laden im Spiel.
 *
 * Die PNGs kommen fuer die Vorschau ueber den Client mit Token
 * (ApiService.effectImageBlob) und stehen als Object-URL im Bild; beim Wechsel
 * und am Ende werden sie freigegeben.
 */
@Component({
  selector: 'app-effect-edit',
  imports: [DecimalPipe, FormsModule, RouterLink, ImageLightboxComponent],
  template: `
    <p><a routerLink="/games/admin/effects">← Alle Effekte</a></p>

    @if (loadError(); as message) {
      <p class="error">{{ message }}</p>
    }
    @if (message(); as text) {
      <p class="success">{{ text }}</p>
    }
    @if (error(); as text) {
      <p class="error">{{ text }}</p>
    }

    @if (effect(); as e) {
      <section class="card">
        <div class="header">
          <h2>{{ e.key }}</h2>
          @if (e.complete) {
            <span class="badge ok">vollständig – im Spiel</span>
          } @else {
            <span class="badge draft">Entwurf</span>
          }
          <button type="button" class="secondary danger" [disabled]="busy()" (click)="deleteEffect(e)">
            Löschen
          </button>
        </div>
        <p class="muted">Änderungen erscheinen im Spiel ab dem nächsten Laden.</p>
        @if (e.problems.length) {
          <details [open]="e.problems.length <= 6">
            <summary>Offen: {{ e.problems.length }}</summary>
            <ul class="problems">
              @for (problem of e.problems; track problem) {
                <li>{{ problem }}</li>
              }
            </ul>
          </details>
        }
      </section>

      <section class="card">
        <h3>Werte</h3>
        <div class="values">
          <label>
            Art
            <select [(ngModel)]="values.kind">
              @for (kind of kinds; track kind.kind) {
                <option [ngValue]="kind.kind">{{ kind.label }}</option>
              }
            </select>
          </label>
          <label title="Breite einer Zelle auf dem Bildschirm in Pixeln (1–1024).">
            Zeichenbreite
            <input type="number" min="1" max="1024" [(ngModel)]="values.draw_width" />
          </label>
          <label title="Wie lange der Effekt einmal abläuft, in Millisekunden (bis 10000).">
            Dauer (ms)
            <input type="number" min="1" max="10000" [(ngModel)]="values.duration_ms" />
          </label>
        </div>
        <div class="actions">
          <button type="button" [disabled]="busy()" (click)="saveValues(e)">Werte speichern</button>
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
            (change)="upload(e, $event)"
          />
        </label>
        <p class="muted hint">
          Der Dateiname wird klein geschrieben übernommen: a–z, 0–9 und _, mit .png, höchstens 31 Zeichen.
        </p>
        @if (e.images.length === 0) {
          <p class="muted">Noch keine PNG.</p>
        } @else {
          <ul class="grid">
            @for (image of e.images; track image.id) {
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
                    <input type="file" accept="image/png" [disabled]="busy()" (change)="replace(e, image, $event)" />
                  </label>
                  <button type="button" class="secondary danger small" [disabled]="busy()" (click)="deleteImage(e, image)">
                    Löschen
                  </button>
                </div>
              </li>
            }
          </ul>
        }
      </section>

      <section class="card">
        <div class="header">
          <h3>Atlanten</h3>
          <button type="button" class="secondary small" (click)="togglePlaying()">
            {{ playing() ? 'Anhalten' : 'Abspielen' }}
          </button>
        </div>
        <p class="muted hint">
          Je Richtung eine PNG, zeilenweise von links oben in Zellen geteilt. „Ab Zelle“ ist die erste
          Zelle des Effekts in der PNG (0 = links oben) – teilen sich mehrere Richtungen eine PNG, stehen
          ihre Bilder hintereinander. Für einen vollständigen Effekt braucht es alle vier Richtungen.
        </p>
        <p class="muted hint">
          Die Vorschau spielt den Effekt, wie das Spiel ihn zeigt: {{ values.draw_width }} px breit, in
          {{ values.duration_ms }} ms, danach eine kurze Pause – schon mit ungespeicherten Eingaben. Die
          Raute ist das getroffene Feld, der Punkt in ihrer Mitte der Einschlag; dort liegt der Anker des
          Bildes. <strong>Die Animation mit der Maus verschieben</strong>, um den Anker zu setzen.
        </p>
        <div class="copy">
          <button type="button" class="link" (click)="copyToAll(false)" title="Werte der ersten Richtung in die übrigen übernehmen">
            Werte von {{ rows[0].facingLabel }} für alle Richtungen übernehmen
          </button>
          <button
            type="button"
            class="link"
            (click)="copyToAll(true)"
            title="Wie oben, aber jede Richtung beginnt hinter der vorigen in derselben PNG"
          >
            … hintereinander in derselben PNG
          </button>
        </div>
        <div class="facings">
          @for (row of rows; track row.facing) {
            <div class="facing">
              <figure>
                @if (animationFrame(e, row); as shown) {
                  <div
                    class="checkerboard stage"
                    [class.dragging]="anchorDrag?.row === row"
                    title="Ziehen verschiebt die Animation gegen den Einschlag"
                    (pointerdown)="startAnchor(row, shown.cell, $event)"
                    (pointermove)="moveAnchor(row, $event)"
                    (pointerup)="endAnchor($event)"
                    (pointercancel)="endAnchor($event)"
                  >
                    <svg class="tile" [attr.viewBox]="'0 0 ' + stageSize + ' ' + stageSize" aria-hidden="true">
                      <polygon [attr.points]="tilePoints(shown.scale)" />
                    </svg>
                    <div
                      class="frame"
                      [style.left.px]="stageSize / 2 - row.anchor_x * shown.cell.width"
                      [style.top.px]="stageSize / 2 - row.anchor_y * shown.cell.height"
                      [style.width.px]="shown.cell.width"
                      [style.height.px]="shown.cell.height"
                      [style.background-image]="shown.frame === null ? 'none' : 'url(' + shown.cell.url + ')'"
                      [style.background-size]="shown.cell.size"
                      [style.background-position]="shown.cell.position"
                    ></div>
                    @if (shown.frame === null) {
                      <span class="pause muted">Pause</span>
                    }
                    <span class="anchor"></span>
                  </div>
                  <figcaption class="muted">
                    Bild {{ shown.frame === null ? '–' : shown.frame + 1 }} / {{ row.frame_count }}
                    @if (shown.scale < 1) {
                      · {{ shown.scale * 100 | number: '1.0-0' }} %
                    }
                  </figcaption>
                } @else {
                  <div class="empty muted">Keine PNG gewählt oder die Bilder passen nicht in die PNG</div>
                }
              </figure>
              <div class="fields">
                <h4>{{ row.facingLabel }}</h4>
                <label class="wide">
                  PNG
                  <select [(ngModel)]="row.image_id">
                    <option [ngValue]="null">–</option>
                    @for (image of e.images; track image.id) {
                      <option [ngValue]="image.id">{{ image.name }} ({{ image.width }} × {{ image.height }})</option>
                    }
                  </select>
                </label>
                <label title="Breite einer Zelle in der PNG, in Pixeln">
                  Zellbreite
                  <input type="number" min="1" max="1024" [(ngModel)]="row.frame_width" />
                </label>
                <label title="Höhe einer Zelle in der PNG, in Pixeln">
                  Zellhöhe
                  <input type="number" min="1" max="1024" [(ngModel)]="row.frame_height" />
                </label>
                <label title="Erste Zelle in der PNG, zeilenweise von links oben, ab 0">
                  Ab Zelle
                  <input type="number" min="0" max="4095" [(ngModel)]="row.first_frame" />
                </label>
                <label title="Wie viele Zellen der Effekt belegt">
                  Bilder
                  <input type="number" min="1" max="1024" [(ngModel)]="row.frame_count" />
                </label>
                <label title="Anteil an der Zellbreite: 0 = links, 1 = rechts">
                  Anker x
                  <input type="number" min="0" max="1" step="0.01" [(ngModel)]="row.anchor_x" />
                </label>
                <label title="Anteil an der Zellhöhe: 0 = oben, 1 = unten">
                  Anker y
                  <input type="number" min="0" max="1" step="0.01" [(ngModel)]="row.anchor_y" />
                </label>
              </div>
            </div>
          }
        </div>
        <div class="actions">
          <button type="button" [disabled]="busy()" (click)="saveAtlases(e)">Atlanten speichern</button>
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
      .copy {
        display: flex;
        flex-wrap: wrap;
        gap: 1rem;
        margin-bottom: 0.5rem;
      }
      /* Je Richtung ein Block: links die Vorschau, rechts die Felder. */
      .facings {
        display: flex;
        flex-direction: column;
      }
      .facing {
        display: flex;
        flex-wrap: wrap;
        gap: 1rem;
        padding: 0.75rem 0;
        border-bottom: 1px solid var(--border, #2a2a30);
      }
      .facing figure {
        margin: 0;
        width: 290px;
        max-width: 100%;
        display: flex;
        flex-direction: column;
        align-items: center;
        gap: 0.35rem;
      }
      .facing figcaption {
        font-size: 0.75rem;
      }
      .fields {
        flex: 1;
        min-width: 14rem;
        display: grid;
        grid-template-columns: repeat(2, minmax(0, 1fr));
        gap: 0.5rem;
        align-content: start;
      }
      .fields h4 {
        grid-column: 1 / -1;
        margin: 0;
      }
      .fields .wide {
        grid-column: 1 / -1;
      }
      .fields input,
      .fields select {
        width: 100%;
        box-sizing: border-box;
      }
      /* Eine Zelle als Ausschnitt der PNG. */
      .frame {
        background-repeat: no-repeat;
      }
      /* Die Buehne: Einschlag in der Mitte, das Bild liegt mit seinem Anker darauf. */
      .stage {
        position: relative;
        width: 288px;
        height: 288px;
        overflow: hidden;
        border: 1px solid var(--border, #2a2a30);
        cursor: grab;
        touch-action: none;
      }
      .stage.dragging {
        cursor: grabbing;
      }
      .stage .frame {
        position: absolute;
        pointer-events: none;
      }
      .tile {
        position: absolute;
        inset: 0;
        width: 100%;
        height: 100%;
        pointer-events: none;
      }
      .tile polygon {
        fill: none;
        stroke: var(--accent, #5fb9a3);
        stroke-opacity: 0.6;
        stroke-dasharray: 4 3;
      }
      .anchor {
        position: absolute;
        left: 50%;
        top: 50%;
        width: 8px;
        height: 8px;
        margin: -4px 0 0 -4px;
        border-radius: 50%;
        background: var(--danger, #d97a7a);
        box-shadow: 0 0 0 1px #000;
        pointer-events: none;
      }
      .link {
        background: none;
        border: none;
        padding: 0;
        color: var(--accent, #5fb9a3);
        font-size: 0.8rem;
        font-weight: 400;
        cursor: pointer;
        text-decoration: underline;
      }
      .success {
        color: var(--accent, #5fb9a3);
      }
      .pause {
        position: absolute;
        inset: 0;
        display: flex;
        align-items: center;
        justify-content: center;
        font-size: 0.75rem;
        line-height: normal;
        pointer-events: none;
      }
      .empty {
        width: 100%;
        min-height: 8rem;
        padding: 0.5rem;
        box-sizing: border-box;
        display: flex;
        align-items: center;
        justify-content: center;
        text-align: center;
        border: 1px dashed var(--border, #2a2a30);
        font-size: 0.8rem;
      }
    `,
  ],
})
export class EffectEditComponent implements OnInit, OnDestroy {
  private readonly api = inject(ApiService);
  private readonly route = inject(ActivatedRoute);
  private readonly router = inject(Router);

  readonly kinds = EFFECT_KINDS;

  readonly effect = signal<AdminEffect | null>(null);
  readonly loadError = signal<string | null>(null);
  readonly busy = signal(false);
  readonly message = signal<string | null>(null);
  readonly error = signal<string | null>(null);

  /** Object-URLs der Vorschauen, je PNG und Fassung (previewKey). */
  readonly previews = signal<Record<string, string>>({});
  readonly opened = signal<{ src: string; name: string } | null>(null);

  readonly stageSize = STAGE_SIZE;
  /** Abgespielte Zeit der Vorschau in Millisekunden; steht beim Anhalten. */
  readonly elapsed = signal(0);
  readonly playing = signal(true);
  private animationHandle = 0;
  private lastTick: number | null = null;
  /** Die Richtung, deren Animation gerade gezogen wird, mit dem Stand beim Anfassen. */
  anchorDrag: {
    row: AtlasRow;
    x: number;
    y: number;
    anchorX: number;
    anchorY: number;
    width: number;
    height: number;
  } | null = null;

  values: AdminEffectValues = { kind: 'explosion', draw_width: 256, duration_ms: 2500 };
  rows: AtlasRow[] = buildRows(null);

  private effectId = 0;

  ngOnInit(): void {
    this.effectId = Number(this.route.snapshot.paramMap.get('id'));
    this.api.getEffect(this.effectId).subscribe({
      next: (effect) => this.show(effect, true),
      error: (err: HttpErrorResponse) =>
        this.loadError.set(apiErrorMessage(err, 'Effekt konnte nicht geladen werden.')),
    });
    this.animationHandle = requestAnimationFrame((now) => this.tick(now));
  }

  ngOnDestroy(): void {
    cancelAnimationFrame(this.animationHandle);
    Object.values(this.previews()).forEach((url) => URL.revokeObjectURL(url));
  }

  /** Ein Schritt der Vorschau: zaehlt die Zeit weiter, solange sie laeuft. */
  private tick(now: number): void {
    if (this.playing() && this.lastTick !== null) {
      // Hoechstens 100 ms je Schritt: war der Tab im Hintergrund, laeuft die
      // Vorschau dort weiter, wo sie stand.
      const delta = Math.min(now - this.lastTick, 100);
      this.elapsed.update((elapsed) => elapsed + delta);
    }
    this.lastTick = now;
    this.animationHandle = requestAnimationFrame((next) => this.tick(next));
  }

  private rowImage(effect: AdminEffect, row: AtlasRow): { image: AdminSpriteImage; url: string } | null {
    const image = effect.images.find((candidate) => candidate.id === row.image_id);
    const url = image ? this.previews()[this.previewKey(image)] : undefined;
    return image && url ? { image, url } : null;
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

  /**
   * Das Bild, das die Vorschau einer Richtung gerade zeigt -- gerechnet wie im
   * Client (RAD_IsoEffectFrameAt und RAD_IsoEffectFrameTarget): Bild =
   * Fortschritt * Bilder, gezeichnet draw_width breit, die Hoehe im
   * Seitenverhaeltnis der Zelle, der Anker auf dem Einschlag in der Mitte der
   * Buehne. "scale" verkleinert alles (Bild und Feld) so weit, dass das Bild bei
   * jedem Anker auf die Buehne passt. In der Pause zwischen zwei Durchlaeufen
   * ist "frame" null. null, solange der Atlas nicht passt -- wie die Pruefung
   * im Backend: alle Bilder muessen in die PNG passen.
   */
  animationFrame(
    effect: AdminEffect,
    row: AtlasRow,
  ): { cell: CellPreview; frame: number | null; scale: number } | null {
    const count = Math.floor(Number(row.frame_count));
    const duration = Number(this.values.duration_ms);
    const width = Number(this.values.draw_width);
    const height = (width * Number(row.frame_height)) / Number(row.frame_width);
    const image = this.rowImage(effect, row);
    if (!(count >= 1) || !(duration > 0) || !(width > 0) || !(height > 0) || !framePreview(image, row, count - 1, 1)) {
      return null;
    }
    const scale = Math.min(1, STAGE_SIZE / 2 / width, STAGE_SIZE / 2 / height, STAGE_SIZE / ISO_TILE_WIDTH);
    const elapsed = this.elapsed() % (duration + ANIMATION_PAUSE_MS);
    const frame = elapsed < duration ? Math.min(Math.floor((elapsed / duration) * count), count - 1) : null;
    const cell = framePreview(image, row, frame ?? 0, (width * scale) / Number(row.frame_width));
    return cell ? { cell, frame, scale } : null;
  }

  /** Das getroffene Iso-Feld um den Einschlag, als SVG-Punkte auf der Buehne. */
  tilePoints(scale: number): string {
    const center = STAGE_SIZE / 2;
    const x = (ISO_TILE_WIDTH / 2) * scale;
    const y = (ISO_TILE_HEIGHT / 2) * scale;
    return `${center},${center - y} ${center + x},${center} ${center},${center + y} ${center - x},${center}`;
  }

  /**
   * Ziehen in der Vorschau verschiebt die Animation; der Einschlag bleibt
   * stehen, der Anker wandert also gegenlaeufig ueber das Bild.
   */
  startAnchor(row: AtlasRow, cell: CellPreview, event: PointerEvent): void {
    (event.currentTarget as HTMLElement).setPointerCapture(event.pointerId);
    this.anchorDrag = {
      row,
      x: event.clientX,
      y: event.clientY,
      anchorX: Number(row.anchor_x),
      anchorY: Number(row.anchor_y),
      width: cell.width,
      height: cell.height,
    };
  }

  moveAnchor(row: AtlasRow, event: PointerEvent): void {
    const drag = this.anchorDrag;
    if (drag?.row !== row) {
      return;
    }
    const fraction = (value: number): number => Math.round(Math.min(Math.max(value, 0), 1) * 1000) / 1000;
    row.anchor_x = fraction(drag.anchorX - (event.clientX - drag.x) / drag.width);
    row.anchor_y = fraction(drag.anchorY - (event.clientY - drag.y) / drag.height);
  }

  endAnchor(event: PointerEvent): void {
    (event.currentTarget as HTMLElement).releasePointerCapture(event.pointerId);
    this.anchorDrag = null;
  }

  togglePlaying(): void {
    // Weiter ab dem naechsten Schritt -- nicht die Zeit seit dem letzten
    // Schritt nachholen, der im Hintergrund lange her sein kann.
    this.lastTick = null;
    this.playing.update((playing) => !playing);
  }

  saveValues(effect: AdminEffect): void {
    this.run(
      this.api.updateEffect(effect.id, {
        kind: this.values.kind,
        draw_width: Number(this.values.draw_width),
        duration_ms: Number(this.values.duration_ms),
      }),
      'Werte gespeichert.',
      'Werte nicht gespeichert.',
    );
  }

  /** Laedt die gewaehlten PNGs nacheinander hoch -- bei einem Fehler endet es dort. */
  upload(effect: AdminEffect, event: Event): void {
    const input = event.target as HTMLInputElement;
    const files = Array.from(input.files ?? []);
    input.value = '';
    const next = (index: number): void => {
      if (index >= files.length) {
        return;
      }
      this.run(
        this.api.uploadEffectImage(effect.id, files[index]),
        `${files.length === 1 ? files[index].name : `${index + 1} von ${files.length} PNGs`} hochgeladen.`,
        `${files[index].name} nicht hochgeladen.`,
        () => next(index + 1),
      );
    };
    next(0);
  }

  replace(effect: AdminEffect, image: AdminSpriteImage, event: Event): void {
    const input = event.target as HTMLInputElement;
    const file = input.files?.[0];
    input.value = '';
    if (file) {
      this.run(
        this.api.replaceEffectImage(effect.id, image.id, file),
        `${image.name} ersetzt.`,
        `${image.name} nicht ersetzt.`,
      );
    }
  }

  deleteImage(effect: AdminEffect, image: AdminSpriteImage): void {
    if (!confirm(`${image.name} wirklich löschen?`)) {
      return;
    }
    this.busy.set(true);
    this.clearMessages();
    this.api.deleteEffectImage(effect.id, image.id).subscribe({
      next: () => this.reload(`${image.name} gelöscht.`),
      error: (err: HttpErrorResponse) => this.fail(err, `${image.name} nicht gelöscht.`),
    });
  }

  deleteEffect(effect: AdminEffect): void {
    if (!confirm(`Effekt „${effect.key}“ mit allen PNGs wirklich löschen?`)) {
      return;
    }
    this.busy.set(true);
    this.clearMessages();
    this.api.deleteEffect(effect.id).subscribe({
      next: () => void this.router.navigate(['/games/admin/effects']),
      error: (err: HttpErrorResponse) => this.fail(err, 'Effekt nicht gelöscht.'),
    });
  }

  /**
   * Uebernimmt PNG, Zelle, Bilder, erste Zelle und Anker der ersten Richtung in
   * die uebrigen Richtungen. Mit "consecutive" beginnt jede Richtung hinter
   * der vorigen -- fuer eine PNG, in der alle vier hintereinander stehen.
   */
  copyToAll(consecutive: boolean): void {
    const [source, ...others] = this.rows;
    const count = Number(source.frame_count);
    others.forEach((row, index) => {
      row.image_id = source.image_id;
      row.frame_width = source.frame_width;
      row.frame_height = source.frame_height;
      row.frame_count = source.frame_count;
      row.first_frame = consecutive ? Number(source.first_frame) + (index + 1) * count : source.first_frame;
      row.anchor_x = source.anchor_x;
      row.anchor_y = source.anchor_y;
    });
  }

  saveAtlases(effect: AdminEffect): void {
    const atlases: AdminEffectAtlas[] = this.rows
      .filter((row) => row.image_id !== null)
      .map((row) => ({
        facing: row.facing,
        image_id: row.image_id as number,
        frame_width: Number(row.frame_width),
        frame_height: Number(row.frame_height),
        first_frame: Number(row.first_frame),
        frame_count: Number(row.frame_count),
        anchor_x: Number(row.anchor_x),
        anchor_y: Number(row.anchor_y),
      }));
    this.run(this.api.putEffectAtlases(effect.id, atlases), 'Atlanten gespeichert.', 'Atlanten nicht gespeichert.', undefined, true);
  }

  /**
   * Fuehrt eine Aenderung aus und zeigt danach den Effekt, wie das Backend ihn
   * zurueckgibt. Die Atlas-Felder werden nur nach dem Speichern der Atlanten neu
   * gefuellt -- sonst gingen dort Eingaben verloren, etwa beim Hochladen.
   */
  private run(
    request: Observable<AdminEffect>,
    done: string,
    failed: string,
    then?: () => void,
    resetRows = false,
  ): void {
    this.busy.set(true);
    this.clearMessages();
    request.subscribe({
      next: (effect) => {
        this.busy.set(false);
        this.message.set(done);
        this.show(effect, resetRows);
        then?.();
      },
      error: (err: HttpErrorResponse) => this.fail(err, failed),
    });
  }

  private reload(done: string): void {
    this.api.getEffect(this.effectId).subscribe({
      next: (effect) => {
        this.busy.set(false);
        this.message.set(done);
        this.show(effect, false);
      },
      error: (err: HttpErrorResponse) => this.fail(err, 'Effekt konnte nicht neu geladen werden.'),
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

  /** Zeigt "effect": Werte, Vorschauen und -- mit "resetRows" -- die Atlas-Felder. */
  private show(effect: AdminEffect, resetRows: boolean): void {
    const first = this.effect() === null;
    this.effect.set(effect);
    if (first) {
      this.values = { kind: effect.kind, draw_width: effect.draw_width, duration_ms: effect.duration_ms };
    }
    if (first || resetRows) {
      this.rows = buildRows(effect);
    }
    this.loadPreviews(effect);
  }

  /** Laedt fehlende Vorschauen und gibt die von geloeschten oder ersetzten PNGs frei. */
  private loadPreviews(effect: AdminEffect): void {
    const wanted = new Set(effect.images.map((image) => this.previewKey(image)));
    const current = { ...this.previews() };
    for (const [key, url] of Object.entries(current)) {
      if (!wanted.has(key)) {
        URL.revokeObjectURL(url);
        delete current[key];
      }
    }
    this.previews.set(current);
    for (const image of effect.images) {
      const key = this.previewKey(image);
      if (!(key in current)) {
        this.api.effectImageBlob(effect.id, image.id).subscribe({
          next: (blob) => this.previews.update((all) => ({ ...all, [key]: URL.createObjectURL(blob) })),
        });
      }
    }
  }
}

/**
 * Bild "frame" (ab 0) einer Richtung als Ausschnitt ihrer PNG, um "scale"
 * vergroessert oder verkleinert; null, solange PNG oder Zelle nicht passen.
 */
function framePreview(
  source: { image: AdminSpriteImage; url: string } | null,
  row: AtlasRow,
  frame: number,
  scale: number,
): CellPreview | null {
  const width = Number(row.frame_width);
  const height = Number(row.frame_height);
  const cell = Number(row.first_frame) + frame;
  if (!source || !(width > 0) || !(height > 0) || !(cell >= 0) || !(scale > 0)) {
    return null;
  }
  const { image, url } = source;
  const columns = Math.floor(image.width / width);
  if (columns < 1) {
    return null;
  }
  const column = cell % columns;
  const line = Math.floor(cell / columns);
  if ((line + 1) * height > image.height) {
    return null;
  }
  return {
    url,
    width: width * scale,
    height: height * scale,
    size: `${image.width * scale}px ${image.height * scale}px`,
    position: `${-column * width * scale}px ${-line * height * scale}px`,
  };
}

/** Die vier Richtungen im Editor, belegt aus den gespeicherten Atlanten. */
function buildRows(effect: AdminEffect | null): AtlasRow[] {
  const saved = new Map((effect?.atlases ?? []).map((atlas) => [atlas.facing, atlas]));
  return FACINGS.map(({ facing, label }) => {
    const atlas = saved.get(facing);
    return {
      facing,
      facingLabel: label,
      image_id: atlas?.image_id ?? null,
      frame_width: atlas?.frame_width ?? 128,
      frame_height: atlas?.frame_height ?? 128,
      first_frame: atlas?.first_frame ?? 0,
      frame_count: atlas?.frame_count ?? 1,
      anchor_x: atlas?.anchor_x ?? 0.5,
      anchor_y: atlas?.anchor_y ?? 0.5,
    };
  });
}
