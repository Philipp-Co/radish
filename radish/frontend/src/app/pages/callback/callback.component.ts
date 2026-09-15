import { Component, OnInit, inject, signal } from '@angular/core';
import { Router, RouterLink } from '@angular/router';

import { AuthService } from '../../core/auth.service';

@Component({
  selector: 'app-callback',
  standalone: true,
  imports: [RouterLink],
  template: `
    <main class="callback-page">
      @if (error(); as message) {
        <div class="card">
          <p class="error">{{ message }}</p>
          <a routerLink="/login">Zurueck zum Login</a>
        </div>
      } @else {
        <p class="muted">Anmeldung wird verarbeitet...</p>
      }
    </main>
  `,
  styles: [
    `
      .callback-page {
        min-height: 100vh;
        display: flex;
        align-items: center;
        justify-content: center;
        padding: 1rem;
        text-align: center;
      }
    `,
  ],
})
export class CallbackComponent implements OnInit {
  private readonly auth = inject(AuthService);
  private readonly router = inject(Router);

  readonly error = signal<string | null>(null);

  async ngOnInit(): Promise<void> {
    // window.location.search statt ActivatedRoute.queryParamMap: Keycloak
    // haengt code/state/error als echte Query-Parameter an, das deckt sich,
    // aber so bleibt handleCallback() unabhaengig vom Router testbar.
    const queryParams = new URLSearchParams(window.location.search);
    try {
      const redirectTo = await this.auth.handleCallback(queryParams);
      await this.router.navigateByUrl(redirectTo);
    } catch (err) {
      this.error.set(err instanceof Error ? err.message : 'Anmeldung fehlgeschlagen.');
    }
  }
}
