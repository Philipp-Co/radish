import { Component } from '@angular/core';

interface HowToStep {
  image: string;
  /** Groesse des Bildes in Pixeln -- reserviert den Platz, bevor es geladen ist. */
  width: number;
  height: number;
  alt: string;
  text: string;
}

interface HowToSection {
  id: string;
  title: string;
  intro: string;
  steps: HowToStep[];
}

/**
 * Die Schritte je Kommando, mit Screenshots aus dem Spiel (public/how-to/).
 * Symbole im Kontextmenue: ">>" ziehen, Fadenkreuz angreifen, "X" schliessen.
 */
const SECTIONS: HowToSection[] = [
  {
    id: 'move',
    title: 'Ziehen',
    intro: 'So bewegst du eine eigene Einheit ueber die Karte.',
    steps: [
      {
        image: 'how-to/move-1.jpg',
        width: 834,
        height: 602,
        alt: 'Kontextmenue um eine Einheit mit den Symbolen Ziehen und Schliessen',
        text:
          'Klicke mit der linken Maustaste auf eine eigene Einheit. Um sie herum oeffnet sich ' +
          'das Menue: ">>" zum Ziehen, "X" zum Schliessen.',
      },
      {
        image: 'how-to/move-2.jpg',
        width: 978,
        height: 600,
        alt: 'Blau markierte Felder rund um die Einheit',
        text: 'Nach einem Klick auf ">>" sind die Felder blau, die die Einheit in diesem Zug erreicht.',
      },
      {
        image: 'how-to/move-3.jpg',
        width: 978,
        height: 600,
        alt: 'Gelb markierter Weg mit kraeftig gelbem Zielfeld',
        text:
          'Klicke auf das Zielfeld. Der Weg wird gelb markiert, das Ziel kraeftiger; was von ' +
          'dort aus noch erreichbar ist, bleibt blau.',
      },
      {
        image: 'how-to/move-4.jpg',
        width: 978,
        height: 630,
        alt: 'Kontextmenue ueber dem Zielfeld mit hervorgehobenem Ziehen-Symbol',
        text:
          'Ueber dem Ziel oeffnet sich das Menue wieder, ">>" ist hervorgehoben. Ein Klick ' +
          'darauf bestaetigt den Zug, "X" verwirft ihn.',
      },
      {
        image: 'how-to/move-5.jpg',
        width: 978,
        height: 630,
        alt: 'Die Einheit steht auf dem Zielfeld',
        text: 'Die Einheit faehrt den Weg ab und steht auf dem Ziel.',
      },
    ],
  },
  {
    id: 'attack',
    title: 'Angreifen',
    intro: 'So greifst du mit einer eigenen Einheit ein Feld an.',
    steps: [
      {
        image: 'how-to/attack-1.jpg',
        width: 978,
        height: 630,
        alt: 'Kontextmenue um eine Einheit mit den Symbolen Angreifen und Schliessen',
        text: 'Klicke auf eine eigene Einheit und waehle im Menue das Fadenkreuz.',
      },
      {
        image: 'how-to/attack-2.jpg',
        width: 978,
        height: 630,
        alt: 'Rot getoente Felder in Reichweite der Einheit',
        text: 'Rot getoent sind die Felder, die die Waffen der Einheit erreichen.',
      },
      {
        image: 'how-to/attack-3.jpg',
        width: 934,
        height: 710,
        alt: 'Kraeftig rot markiertes Ziel mit Kontextmenue und hervorgehobenem Fadenkreuz',
        text:
          'Klicke auf das Ziel. Es wird kraeftig rot markiert, darueber oeffnet sich das Menue ' +
          'mit hervorgehobenem Fadenkreuz. Ein Klick darauf greift an, "X" bricht ab.',
      },
      {
        image: 'how-to/attack-4.jpg',
        width: 934,
        height: 710,
        alt: 'Die Karte nach dem Angriff',
        text: 'Der Angriff ist ausgefuehrt.',
      },
    ],
  },
];

/**
 * Erklaert die Kommandos im Spiel -- Ziehen und Angreifen --, Schritt fuer
 * Schritt mit Screenshots. Reine Anzeige, ohne Aufrufe ans Backend; erreichbar
 * ueber die Kachel "How To" auf der Startseite (HomeComponent); zurueck geht es
 * ueber "Zum Hub" der GamesShellComponent.
 */
@Component({
  selector: 'app-how-to',
  standalone: true,
  template: `
    <section class="intro">
      <h2>How To</h2>
      <p class="muted">
        Alle Kommandos laufen ueber das Menue, das sich mit einem Linksklick auf eine eigene
        Einheit oeffnet.
      </p>
    </section>

    @for (section of sections; track section.id) {
      <section class="section" [attr.aria-labelledby]="'how-to-' + section.id">
        <h3 [id]="'how-to-' + section.id">{{ section.title }}</h3>
        <p class="muted">{{ section.intro }}</p>
        <ol class="steps">
          @for (step of section.steps; track step.image) {
            <li class="card step">
              <img [src]="step.image" [alt]="step.alt" [width]="step.width" [height]="step.height" loading="lazy" />
              <p>{{ step.text }}</p>
            </li>
          }
        </ol>
      </section>
    }

  `,
  styles: [
    `
      :host {
        display: flex;
        flex-direction: column;
        gap: 1.5rem;
      }
      h2,
      h3 {
        margin: 0 0 0.25rem;
      }
      h2 {
        font-size: 1.5rem;
      }
      .intro p,
      .section > p {
        margin: 0;
      }
      .steps {
        list-style: none;
        margin: 0.75rem 0 0;
        padding: 0;
        display: flex;
        flex-direction: column;
        gap: 0.75rem;
        counter-reset: step;
      }
      .step {
        display: flex;
        flex-direction: column;
        gap: 0.75rem;
        counter-increment: step;
      }
      .step img {
        display: block;
        width: 100%;
        height: auto;
        border-radius: 6px;
      }
      .step p {
        margin: 0;
        line-height: 1.5;
      }
      .step p::before {
        content: counter(step) '. ';
        color: var(--heading);
        font-weight: 600;
      }
    `,
  ],
})
export class HowToComponent {
  protected readonly sections = SECTIONS;
}
