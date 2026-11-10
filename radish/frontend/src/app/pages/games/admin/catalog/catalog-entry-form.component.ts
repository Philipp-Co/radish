import { Component, EventEmitter, Input, OnChanges, Output, SimpleChanges, inject, signal } from '@angular/core';
import { FormsModule } from '@angular/forms';

import { UnitSpriteService, unitSpriteOptions } from '../../../../core/unit-sprite.service';

export type CatalogFieldValue = string | number | boolean | null;
export type CatalogDraft = Record<string, CatalogFieldValue>;

export interface CatalogFieldOption {
  value: string;
  label: string;
}

export interface CatalogField {
  key: string;
  label: string;
  type: 'text' | 'number' | 'checkbox' | 'select';
  /** Nur bei type 'select'. */
  options?: CatalogFieldOption[];
  /**
   * Nur bei type 'select': die Optionen kommen nicht aus "options", sondern
   * werden geladen -- 'unit-sprites' sind die Einheitenbilder aus dem
   * Asset-Manifest (core/unit-sprite.service.ts), davor "kein Bild".
   */
  optionsFrom?: 'unit-sprites';
  /** Erklaerung als Tooltip, z.B. was ein leeres Feld bedeutet. */
  hint?: string;
}

/**
 * Formular fuer einen Katalogeintrag, beschrieben ueber seine Felder --
 * dieselbe Komponente fuer Spezies, Einheitentypen, Profile, Waffen und
 * Ausruestung. Arbeitet auf einer Kopie von value; save liefert nur die in
 * fields genannten Schluessel, anlegen/speichern/loeschen uebernimmt die
 * Elternkomponente.
 */
@Component({
  selector: 'app-catalog-entry-form',
  imports: [FormsModule],
  template: `
    <div class="grid">
      @for (field of fields; track field.key) {
        <!-- Das Eingabefeld steckt im Label, nur innerhalb von @switch -- das
             erkennt die Regel nicht. -->
        <!-- eslint-disable-next-line @angular-eslint/template/label-has-associated-control -->
        <label
          [class.wide]="field.type === 'text'"
          [class.checkbox]="field.type === 'checkbox'"
          [class.select]="field.type === 'select'"
          [title]="field.hint ?? ''"
        >
          @switch (field.type) {
            @case ('text') {
              {{ field.label }}
              <input type="text" [(ngModel)]="draft[field.key]" />
            }
            @case ('number') {
              {{ field.label }}
              <input
                type="number"
                min="0"
                [(ngModel)]="draft[field.key]"
              />
            }
            @case ('select') {
              {{ field.label }}
              <select [(ngModel)]="draft[field.key]">
                @for (option of optionsOf(field); track option.value) {
                  <option [ngValue]="option.value">{{ option.label }}</option>
                }
              </select>
              @if (field.optionsFrom && optionsError()) {
                <span class="error">{{ optionsError() }}</span>
              }
            }
            @case ('checkbox') {
              <input type="checkbox" [(ngModel)]="draft[field.key]" />
              {{ field.label }}
            }
          }
        </label>
      }
    </div>

    @if (error) {
      <p class="error">{{ error }}</p>
    }

    <div class="actions">
      @if (inUse) {
        <span class="muted hint">In Armeen verwendet: nicht löschbar.</span>
      }
      <button [class.secondary]="!isNew" [disabled]="busy" (click)="submit()">
        {{ isNew ? 'Anlegen' : 'Speichern' }}
      </button>
      @if (!isNew) {
        <button class="secondary danger" [disabled]="busy || inUse" (click)="remove.emit()">
          Löschen
        </button>
      }
    </div>
  `,
  styles: [
    `
      .grid {
        display: grid;
        grid-template-columns: repeat(auto-fill, minmax(6.5rem, 1fr));
        gap: 0.5rem;
      }
      label {
        display: flex;
        flex-direction: column;
        gap: 0.2rem;
        font-size: 0.8rem;
        color: var(--text-dim, #8a8a92);
      }
      label.wide {
        grid-column: 1 / -1;
      }
      label.select {
        grid-column: span 2;
      }
      label.checkbox {
        flex-direction: row;
        align-items: center;
        grid-column: span 2;
      }
      input[type='number'],
      input[type='text'] {
        width: 100%;
      }
      .actions {
        display: flex;
        align-items: center;
        justify-content: flex-end;
        gap: 0.5rem;
        margin-top: 0.5rem;
      }
      .actions .hint {
        flex: 1;
        font-size: 0.8rem;
      }
      .danger {
        color: var(--danger, #d97a7a);
      }
      label .error {
        font-size: 0.75rem;
        color: var(--danger, #d97a7a);
      }
    `,
  ],
})
export class CatalogEntryFormComponent implements OnChanges {
  private readonly unitSprites = inject(UnitSpriteService);

  @Input({ required: true }) fields: CatalogField[] = [];
  @Input({ required: true }) value: object = {};
  @Input() inUse = false;
  @Input() isNew = false;
  @Input() busy = false;
  @Input() error: string | null = null;

  @Output() readonly save = new EventEmitter<CatalogDraft>();
  @Output() readonly remove = new EventEmitter<void>();

  draft: CatalogDraft = {};

  /** Die Schluessel der Einheitenbilder, sobald geladen (optionsFrom). */
  readonly spriteKeys = signal<string[]>([]);
  readonly optionsError = signal<string | null>(null);

  ngOnChanges(changes: SimpleChanges): void {
    // Nur ein neuer value (nach Laden oder nach erfolgreichem Anlegen) setzt
    // die Eingaben zurueck -- busy/error wechseln auch waehrend des
    // Speicherns, und nach einer Fehlermeldung sollen die Eingaben bleiben.
    if (!changes['value'] && !changes['fields']) {
      return;
    }
    const source = this.value as Record<string, CatalogFieldValue>;
    // Ein Bild ohne Wert ist "kein Bild" (leer) -- null nimmt das Backend dort nicht.
    this.draft = Object.fromEntries(
      this.fields.map((field) => [field.key, source[field.key] ?? (field.optionsFrom ? '' : null)]),
    );

    if (changes['fields'] && this.fields.some((field) => field.optionsFrom === 'unit-sprites')) {
      this.loadSpriteKeys();
    }
  }

  /** Die Optionen eines Auswahlfeldes; bei den Einheitenbildern aus unitSpriteOptions. */
  optionsOf(field: CatalogField): CatalogFieldOption[] {
    if (field.optionsFrom !== 'unit-sprites') {
      return field.options ?? [];
    }
    const current = this.draft[field.key];
    return unitSpriteOptions(this.spriteKeys(), typeof current === 'string' ? current : null);
  }

  private loadSpriteKeys(): void {
    this.unitSprites.keys().then(
      (keys) => {
        this.spriteKeys.set(keys);
        this.optionsError.set(null);
      },
      () => this.optionsError.set('Bilder nicht ladbar -- nur der gespeicherte Wert steht zur Wahl.'),
    );
  }

  submit(): void {
    this.save.emit({ ...this.draft });
  }
}
