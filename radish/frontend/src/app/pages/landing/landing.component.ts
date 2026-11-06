import { Component, inject } from '@angular/core';
import { Router, RouterLink } from '@angular/router';

import { AuthService } from '../../core/auth.service';

@Component({
  selector: 'app-landing',
  standalone: true,
  imports: [RouterLink],
  template: `
    <header class="topbar">
      <a routerLink="/" class="brand">radish</a>
      <button class="secondary" (click)="primaryAction()">{{ primaryLabel() }}</button>
    </header>

    <main>
      <section class="hero">
        <div class="hero-text">
          <h1>Rundenbasierte Taktik im Browser</h1>
          <p class="lead muted">
            Stelle deine Armee zusammen, nutze das Gelaende und schlage deinen Gegner Zug um Zug --
            ohne Installation, direkt im Browser.
          </p>
          <div class="hero-actions">
            <button (click)="primaryAction()">{{ heroLabel() }}</button>
            <button class="secondary" (click)="scrollTo(howTo)">So funktioniert's</button>
          </div>
        </div>

        <img
          class="hero-map"
          src="landing_page.png"
          width="558"
          height="322"
          alt="Isometrisches Spielfeld mit Einheiten"
        />
      </section>

      <section class="features" aria-label="Was radish ausmacht">
        <article class="card">
          <h2>Armee bauen</h2>
          <p class="muted">
            Waehle Einheitentypen aus dem Katalog und ruestet sie mit Profilen und Waffen aus.
          </p>
        </article>
        <article class="card">
          <h2>Taktisch ziehen</h2>
          <p class="muted">
            Offenes Feld, Deckung oder unpassierbar: Das Gelaende bestimmt Bewegung und Treffer.
          </p>
        </article>
        <article class="card">
          <h2>Direkt im Browser</h2>
          <p class="muted">
            Der Spielclient laeuft als WebAssembly im Browser -- einloggen und losspielen.
          </p>
        </article>
      </section>

      <section class="how-to" #howTo>
        <h2>So funktioniert's</h2>
        <ol class="steps">
          <li>
            <span class="step-no">1</span>
            <div><strong>Einloggen</strong><p class="muted">Mit deinem radish-Konto anmelden.</p></div>
          </li>
          <li>
            <span class="step-no">2</span>
            <div>
              <strong>Armee zusammenstellen</strong>
              <p class="muted">Einheiten waehlen und ausruesten.</p>
            </div>
          </li>
          <li>
            <span class="step-no">3</span>
            <div>
              <strong>Spiel erstellen oder beitreten</strong>
              <p class="muted">Eine Lobby eroeffnen oder einem offenen Spiel beitreten.</p>
            </div>
          </li>
          <li>
            <span class="step-no">4</span>
            <div>
              <strong>Zug um Zug spielen</strong>
              <p class="muted">Der Host beginnt, dann wird abwechselnd gezogen.</p>
            </div>
          </li>
        </ol>
      </section>

      <section class="final-cta">
        <h2>Bereit fuer die erste Partie?</h2>
        <button (click)="primaryAction()">{{ heroLabel() }}</button>
      </section>
    </main>

    <footer class="footer muted">radish</footer>
  `,
  styles: [
    `
      :host {
        display: block;
        min-height: 100vh;
      }
      .topbar,
      main,
      .footer {
        max-width: 1040px;
        margin: 0 auto;
        padding-left: 1rem;
        padding-right: 1rem;
      }
      .topbar {
        display: flex;
        align-items: center;
        justify-content: space-between;
        padding-top: 1rem;
        padding-bottom: 1rem;
      }
      .brand {
        font-size: 1.25rem;
        font-weight: 700;
        color: var(--text);
        text-decoration: none;
      }

      .hero {
        display: grid;
        grid-template-columns: 1fr 1fr;
        align-items: center;
        gap: 2rem;
        padding: 3rem 0 4rem;
      }
      .hero h1 {
        font-size: clamp(2rem, 5vw, 3rem);
        line-height: 1.1;
        margin: 0 0 1rem;
      }
      .lead {
        font-size: 1.125rem;
        line-height: 1.5;
        margin: 0 0 1.5rem;
        max-width: 32rem;
      }
      .hero-actions {
        display: flex;
        flex-wrap: wrap;
        gap: 0.75rem;
      }
      .hero-map {
        width: 100%;
        height: auto;
      }

      .features {
        display: grid;
        grid-template-columns: repeat(3, 1fr);
        gap: 1rem;
      }
      .features h2 {
        font-size: 1.125rem;
        margin: 0 0 0.5rem;
      }
      .features p {
        margin: 0;
        line-height: 1.5;
      }

      .how-to {
        padding: 4rem 0 2rem;
        scroll-margin-top: 1rem;
      }
      .how-to h2,
      .final-cta h2 {
        font-size: 1.5rem;
        margin: 0 0 1.5rem;
      }
      .steps {
        list-style: none;
        margin: 0;
        padding: 0;
        display: grid;
        grid-template-columns: repeat(4, 1fr);
        gap: 1.5rem;
      }
      .steps li {
        display: flex;
        gap: 0.75rem;
      }
      .steps p {
        margin: 0.25rem 0 0;
        line-height: 1.4;
      }
      .step-no {
        flex: none;
        width: 2rem;
        height: 2rem;
        border-radius: 50%;
        border: 1px solid var(--accent);
        color: var(--accent);
        display: flex;
        align-items: center;
        justify-content: center;
        font-weight: 600;
      }

      .final-cta {
        text-align: center;
        padding: 3rem 0 4rem;
      }

      .footer {
        border-top: 1px solid var(--border);
        padding-top: 1.25rem;
        padding-bottom: 1.25rem;
        font-size: 0.875rem;
      }

      @media (max-width: 720px) {
        .hero {
          grid-template-columns: 1fr;
          padding: 1.5rem 0 3rem;
        }
        .features,
        .steps {
          grid-template-columns: 1fr;
        }
      }
      @media (min-width: 721px) and (max-width: 960px) {
        .steps {
          grid-template-columns: repeat(2, 1fr);
        }
      }
    `,
  ],
})
export class LandingComponent {
  private readonly auth = inject(AuthService);
  private readonly router = inject(Router);

  primaryLabel(): string {
    return this.auth.isAuthenticated() ? 'Zu meinen Spielen' : 'Einloggen';
  }

  heroLabel(): string {
    return this.auth.isAuthenticated() ? 'Zu meinen Spielen' : 'Jetzt spielen';
  }

  primaryAction(): void {
    if (this.auth.isAuthenticated()) {
      void this.router.navigateByUrl('/games');
    } else {
      void this.auth.login('/games');
    }
  }

  scrollTo(element: HTMLElement): void {
    element.scrollIntoView({ behavior: 'smooth' });
  }
}
