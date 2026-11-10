import { Component, EventEmitter, HostListener, Input, Output, signal } from '@angular/core';

/**
 * Ein Bild in Originalgroesse ueber der Seite: ungeskaliert, ein grosses
 * scrollt, darueber Name und Pixelgroesse. Schliesst mit "Schließen", einem
 * Klick daneben oder Escape (closed). Der Aufrufer zeigt die Komponente nur,
 * solange ein Bild offen ist.
 */
@Component({
  selector: 'app-image-lightbox',
  template: `
    <div class="overlay" role="dialog" aria-modal="true" [attr.aria-label]="name">
      <!-- Ein Klick neben das Bild schliesst; mit der Tastatur Escape. -->
      <button type="button" class="backdrop" aria-label="Schließen" tabindex="-1" (click)="closed.emit()"></button>
      <div class="bar">
        <span>
          {{ name }}
          @if (size(); as px) {
            <span class="muted">{{ px }}</span>
          }
        </span>
        <button type="button" class="secondary" (click)="closed.emit()">Schließen</button>
      </div>
      <div class="full checkerboard">
        <img [src]="src" [alt]="name" (load)="measure($event)" />
      </div>
    </div>
  `,
  styles: [
    `
      .overlay {
        position: fixed;
        inset: 0;
        z-index: 1000;
        display: flex;
        flex-direction: column;
        gap: 0.5rem;
        padding: 1rem;
        background: rgba(0, 0, 0, 0.85);
      }
      .backdrop {
        position: absolute;
        inset: 0;
        width: 100%;
        height: 100%;
        padding: 0;
        border: none;
        border-radius: 0;
        background: transparent;
        cursor: zoom-out;
      }
      .bar,
      .full {
        position: relative;
      }
      .bar {
        display: flex;
        justify-content: space-between;
        align-items: center;
        gap: 1rem;
        color: var(--text, #d8d8dc);
      }
      .bar .muted {
        margin-left: 0.5rem;
        font-size: 0.85rem;
      }
      /* Originalgroesse: das Bild wird nicht skaliert, ein grosses scrollt. */
      .full {
        flex: 1;
        overflow: auto;
        align-self: center;
        max-width: 100%;
      }
      .full img {
        display: block;
        max-width: none;
      }
      /* Schachbrett hinter dem Bild: so ist zu sehen, was durchsichtig ist. */
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
    `,
  ],
})
export class ImageLightboxComponent {
  @Input({ required: true }) src = '';
  @Input({ required: true }) name = '';
  @Output() readonly closed = new EventEmitter<void>();

  readonly size = signal<string | null>(null);

  measure(event: Event): void {
    const img = event.target as HTMLImageElement;
    this.size.set(`${img.naturalWidth} × ${img.naturalHeight} px`);
  }

  @HostListener('document:keydown.escape')
  onEscape(): void {
    this.closed.emit();
  }
}
