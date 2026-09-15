import { Component, inject } from '@angular/core';
import { ActivatedRoute } from '@angular/router';

import { AuthService } from '../../core/auth.service';

@Component({
  selector: 'app-login',
  standalone: true,
  template: `
    <main class="login-page">
      <div class="card">
        <h1>radish</h1>
        <p class="muted">
          Melde dich an, um Spiele zu erstellen, ihnen beizutreten und zu spielen.
        </p>
        <button (click)="login()">Mit Keycloak einloggen</button>
      </div>
    </main>
  `,
  styles: [
    `
      .login-page {
        min-height: 100vh;
        display: flex;
        align-items: center;
        justify-content: center;
        padding: 1rem;
      }
      .card {
        max-width: 360px;
        width: 100%;
        text-align: center;
      }
      button {
        width: 100%;
        margin-top: 1rem;
      }
    `,
  ],
})
export class LoginComponent {
  private readonly auth = inject(AuthService);
  private readonly route = inject(ActivatedRoute);

  login(): void {
    // "redirect" wird vom authGuard gesetzt, wenn ein Aufruf einer
    // geschuetzten Route (z.B. /games) hierher umgeleitet hat.
    const redirect = this.route.snapshot.queryParamMap.get('redirect') ?? '/games';
    void this.auth.login(redirect);
  }
}
