import { Component, EventEmitter, Input, OnChanges, OnInit, Output, SimpleChanges, inject, signal } from '@angular/core';
import { FormsModule } from '@angular/forms';

import {
  UnitSpriteImage,
  UnitSpriteOption,
  UnitSpriteService,
  unitSpriteOptions,
} from '../../../../core/unit-sprite.service';
import { ImageLightboxComponent } from '../../../../shared/image-lightbox/image-lightbox.component';

/**
 * Die Bilder eines Einheitentyps auf dessen "Ändern"-Seite (siehe
 * CatalogEditComponent), unter den Entitaetsprofilen: jede PNG-Datei seines
 * Bild-Schluessels (UnitType.sprite) als Vorschau, ein Klick zeigt sie in
 * Originalgroesse. Welche Dateien es gibt, sagt das Asset-Manifest des Clients
 * (core/unit-sprite.service.ts) -- dieselben, die das Spiel laedt.
 *
 * Die Vorschau ist die Datei selbst, verkleinert; ein Atlas kann 4096 x 4096
 * Pixel gross sein. Der Browser laedt sie erst, wenn sie ins Bild kommt.
 *
 * **Hier wird das Bild gewaehlt** -- auf der "Ändern"-Seite steht es nicht im
 * Formular darueber (CatalogEditComponent.formFields): die Vorschau folgt der
 * Auswahl sofort, "Bild speichern" aendert nur dieses eine Feld (save, die
 * Elternkomponente schickt es per PATCH).
 */
@Component({
  selector: 'app-sprite-images',
  imports: [FormsModule, ImageLightboxComponent],
  template: `
    <div class="header">
      <h3>Bilder</h3>
      @if (chosen()) {
        <span class="muted">assets/units/{{ chosen() }}/</span>
      }
    </div>

    <div class="choose">
      <label>
        Bild
        <select [ngModel]="chosen()" (ngModelChange)="choose($event)" [disabled]="busy">
          @for (option of options(); track option.value) {
            <option [ngValue]="option.value">{{ option.label }}</option>
          }
        </select>
      </label>
      @if (chosen() !== spriteKey) {
        <button type="button" [disabled]="busy" (click)="save.emit(chosen())">Bild speichern</button>
        <button type="button" class="secondary" [disabled]="busy" (click)="choose(spriteKey)">Verwerfen</button>
        <span class="muted hint">Noch nicht gespeichert -- gespeichert ist {{ spriteKey || 'kein Bild' }}.</span>
      }
    </div>

    @if (!chosen()) {
      <p class="muted hint">Kein Bild gewählt -- im Spiel erscheint die Einheit als Rechteck.</p>
    } @else if (error(); as message) {
      <p class="error">{{ message }}</p>
    } @else if (images(); as found) {
      @if (found.length === 0) {
        <p class="error">„{{ chosen() }}" gibt es nicht in den Assets -- im Spiel erscheint ein Rechteck.</p>
      } @else {
        <ul class="grid">
          @for (image of found; track image.url) {
            <li>
              <button type="button" class="thumb" [title]="image.name" (click)="open(image)">
                <img [src]="image.url" [alt]="image.name" loading="lazy" />
              </button>
              <span class="name">{{ image.name }}</span>
            </li>
          }
        </ul>
      }
    } @else {
      <p class="muted hint">Lade Bilder ...</p>
    }

    @if (opened(); as image) {
      <app-image-lightbox [src]="image.url" [name]="image.name" (closed)="close()" />
    }
  `,
  styles: [
    `
      .header {
        display: flex;
        align-items: baseline;
        gap: 0.75rem;
      }
      h3 {
        margin: 0 0 0.5rem;
      }
      .header .muted {
        font-size: 0.8rem;
      }
      .choose {
        display: flex;
        flex-wrap: wrap;
        align-items: flex-end;
        gap: 0.5rem;
        margin-bottom: 0.75rem;
      }
      .choose label {
        display: flex;
        flex-direction: column;
        gap: 0.2rem;
        font-size: 0.8rem;
        color: var(--text-dim, #8a8a92);
        min-width: 12rem;
      }
      .choose .hint {
        flex-basis: 100%;
      }
      .hint {
        font-size: 0.85rem;
      }
      .grid {
        list-style: none;
        margin: 0;
        padding: 0;
        display: grid;
        grid-template-columns: repeat(auto-fill, minmax(7.5rem, 1fr));
        gap: 0.75rem;
      }
      .grid li {
        display: flex;
        flex-direction: column;
        gap: 0.25rem;
        min-width: 0;
      }
      /* Schachbrett hinter den Bildern: so ist zu sehen, was durchsichtig ist. */
      .thumb {
        background-color: #1a1a1e;
        background-image:
          linear-gradient(45deg, #24242a 25%, transparent 25%),
          linear-gradient(-45deg, #24242a 25%, transparent 25%),
          linear-gradient(45deg, transparent 75%, #24242a 75%),
          linear-gradient(-45deg, transparent 75%, #24242a 75%);
        background-size: 16px 16px;
        background-position: 0 0, 0 8px, 8px -8px, -8px 0;
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
      .name {
        font-size: 0.75rem;
        color: var(--text-dim, #8a8a92);
        overflow: hidden;
        text-overflow: ellipsis;
        white-space: nowrap;
      }
    `,
  ],
})
export class SpriteImagesComponent implements OnInit, OnChanges {
  private readonly unitSprites = inject(UnitSpriteService);

  /** Der gespeicherte Bild-Schluessel des Einheitentyps; leer heisst kein Bild. */
  @Input({ required: true }) spriteKey = '';
  /**
   * Welcher Einheitentyp -- ein anderer verwirft die Auswahl, auch wenn er
   * denselben Schluessel hat.
   */
  @Input({ required: true }) unitTypeId = 0;
  /** Waehrend gespeichert wird, ist die Auswahl gesperrt. */
  @Input() busy = false;

  /** "Bild speichern" mit dem gewaehlten Schluessel. */
  @Output() readonly save = new EventEmitter<string>();

  /** Was gerade gewaehlt ist -- bis zum Speichern nur hier. */
  readonly chosen = signal('');
  readonly keys = signal<string[]>([]);
  readonly options = signal<UnitSpriteOption[]>(unitSpriteOptions([], null));

  /** Die Dateien, sobald geladen; null waehrend des Ladens. */
  readonly images = signal<UnitSpriteImage[] | null>(null);
  readonly error = signal<string | null>(null);

  /** Die Datei in Originalgroesse (app-image-lightbox). */
  readonly opened = signal<UnitSpriteImage | null>(null);

  /** Wofuer zuletzt geladen wurde -- eine spaete Antwort fuer einen alten Schluessel zaehlt nicht. */
  private loadingKey = '';

  ngOnInit(): void {
    this.unitSprites.keys().then(
      (keys) => {
        this.keys.set(keys);
        this.updateOptions();
      },
      () => this.error.set('Bilder nicht ladbar -- das Asset-Manifest des Clients antwortet nicht.'),
    );
  }

  ngOnChanges(changes: SimpleChanges): void {
    // Ein neuer gespeicherter Wert -- anderer Einheitentyp oder gerade
    // gespeichert -- gilt als Auswahl; "busy" allein aendert sie nicht.
    if (changes['spriteKey'] || changes['unitTypeId']) {
      this.choose(this.spriteKey);
    }
  }

  /** Waehlt "key" und zeigt seine Bilder -- gespeichert wird erst mit save. */
  choose(key: string): void {
    this.chosen.set(key);
    this.updateOptions();
    this.opened.set(null);
    this.images.set(null);
    this.error.set(null);
    this.loadingKey = key;
    if (!key) {
      return;
    }
    this.unitSprites.images(key).then(
      (images) => {
        if (this.loadingKey === key) {
          this.images.set(images);
        }
      },
      () => {
        if (this.loadingKey === key) {
          this.error.set('Bilder nicht ladbar -- das Asset-Manifest des Clients antwortet nicht.');
        }
      },
    );
  }

  private updateOptions(): void {
    // Der gespeicherte Wert bleibt waehlbar, auch wenn er nicht in den Assets ist.
    this.options.set(unitSpriteOptions(this.keys(), this.spriteKey));
  }

  open(image: UnitSpriteImage): void {
    this.opened.set(image);
  }

  close(): void {
    this.opened.set(null);
  }
}
