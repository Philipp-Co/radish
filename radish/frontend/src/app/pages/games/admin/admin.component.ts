import { Component, OnDestroy, OnInit, inject, signal } from '@angular/core';
import { Subscription, interval, startWith, switchMap } from 'rxjs';

import { ApiService, GameServerEntry } from '../../../core/api.service';

/**
 * Adminseite: listet alle aktuell angemeldeten Game-Server auf (siehe
 * radish/backend/api/admin_views.py, AdminServerListView). Nur erreichbar
 * mit der Keycloak-Realm-Rolle "Radish-Admin" -- der Tab dorthin ist in
 * games-shell.component.ts an AuthService.isAdmin() geknuepft, die Route
 * selbst zusaetzlich an adminGuard (siehe app.routes.ts, core/auth.guard.ts),
 * und serverseitig durchgesetzt ueber permissions.HasAdminRole. Ohne
 * Server-Push (siehe Docstring von GamesListComponent) deshalb, wie dort,
 * einfaches Polling alle 5s statt eines Live-Updates.
 */
@Component({
  selector: 'app-admin',
  standalone: true,
  template: `
    <section class="card">
      <div class="list-header">
        <h2>Angemeldete Game-Server</h2>
        <button class="secondary" (click)="refresh()">Aktualisieren</button>
      </div>

      @if (errorMessage(); as message) {
        <p class="error">{{ message }}</p>
      }

      @if (loading()) {
        <p class="muted">Lade...</p>
      } @else if (servers().length === 0) {
        <p class="muted">Aktuell kein angemeldeter Game-Server.</p>
      } @else {
        <table class="server-table">
          <thead>
            <tr>
              <th>Name</th>
              <th>IP</th>
              <th>Port</th>
              <th>Status</th>
            </tr>
          </thead>
          <tbody>
            @for (server of servers(); track server.id) {
              <tr>
                <td>{{ server.name }}</td>
                <td>{{ server.ip_address }}</td>
                <td>{{ server.port }}</td>
                <td>{{ server.is_occupied ? 'belegt' : 'frei' }}</td>
              </tr>
            }
          </tbody>
        </table>
      }
    </section>
  `,
  styles: [
    `
      .list-header {
        display: flex;
        align-items: center;
        justify-content: space-between;
        margin-bottom: 0.75rem;
      }
      .server-table {
        width: 100%;
        border-collapse: collapse;
      }
      .server-table th {
        text-align: left;
        color: var(--text-dim, #8a8a92);
        font-weight: 600;
        font-size: 0.85rem;
        padding: 0.4rem 0.5rem;
        border-bottom: 1px solid var(--border, #2a2a30);
      }
      .server-table td {
        padding: 0.5rem;
        border-bottom: 1px solid var(--border, #2a2a30);
      }
    `,
  ],
})
export class AdminComponent implements OnInit, OnDestroy {
  private readonly api = inject(ApiService);

  readonly servers = signal<GameServerEntry[]>([]);
  readonly loading = signal(true);
  readonly errorMessage = signal<string | null>(null);

  private pollSubscription: Subscription | null = null;

  ngOnInit(): void {
    this.pollSubscription = interval(5000)
      .pipe(
        startWith(0),
        switchMap(() => this.api.listServers()),
      )
      .subscribe({
        next: (response) => {
          this.servers.set(response.servers);
          this.loading.set(false);
        },
        error: () => {
          this.errorMessage.set('Serverliste konnte nicht geladen werden.');
          this.loading.set(false);
        },
      });
  }

  ngOnDestroy(): void {
    this.pollSubscription?.unsubscribe();
  }

  refresh(): void {
    this.loading.set(true);
    this.api.listServers().subscribe({
      next: (response) => {
        this.servers.set(response.servers);
        this.loading.set(false);
      },
      error: () => {
        this.errorMessage.set('Serverliste konnte nicht geladen werden.');
        this.loading.set(false);
      },
    });
  }
}
