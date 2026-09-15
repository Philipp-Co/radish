import { Routes } from '@angular/router';

import { adminGuard, authGuard } from './core/auth.guard';

export const routes: Routes = [
  {
    path: 'login',
    loadComponent: () => import('./pages/login/login.component').then((m) => m.LoginComponent),
    title: 'radish -- Login',
  },
  {
    // Redirect-Ziel von Keycloak nach dem Login (siehe environment.keycloak.redirectUri
    // und docker/keycloak/realm-radish.json, Client "radish-web").
    path: 'callback',
    loadComponent: () =>
      import('./pages/callback/callback.component').then((m) => m.CallbackComponent),
    title: 'radish -- Anmeldung...',
  },
  {
    // authGuard steht hier an der Elternroute und gilt damit fuer alle fuenf
    // Unterseiten (current/list/create/join/admin) mit -- siehe
    // GamesShellComponent, das das Menue zwischen ihnen rendert. admin/ hat
    // zusaetzlich noch adminGuard (siehe dort).
    path: 'games',
    loadComponent: () =>
      import('./pages/games/games-shell.component').then((m) => m.GamesShellComponent),
    canActivate: [authGuard],
    children: [
      { path: '', pathMatch: 'full', redirectTo: 'list' },
      {
        path: 'current',
        loadComponent: () =>
          import('./pages/games/current/current-game.component').then(
            (m) => m.CurrentGameComponent,
          ),
        title: 'radish -- Aktuelles Spiel',
      },
      {
        path: 'list',
        loadComponent: () =>
          import('./pages/games/list/games-list.component').then((m) => m.GamesListComponent),
        title: 'radish -- Laufende Spiele',
      },
      {
        path: 'create',
        loadComponent: () =>
          import('./pages/games/create/game-create.component').then((m) => m.GameCreateComponent),
        title: 'radish -- Spiel erstellen',
      },
      {
        path: 'join',
        loadComponent: () =>
          import('./pages/games/join/game-join.component').then((m) => m.GameJoinComponent),
        title: 'radish -- Spiel beitreten',
      },
      {
        // Zusaetzlich zu authGuard an der Elternroute "games" noch
        // adminGuard: nur mit der Keycloak-Realm-Rolle "Radish-Admin"
        // erreichbar (siehe core/auth.guard.ts). Der Tab dorthin ist in
        // GamesShellComponent ebenfalls an diese Rolle geknuepft, aber wer
        // die URL direkt aufruft, soll trotzdem nicht durchkommen.
        path: 'admin',
        loadComponent: () =>
          import('./pages/games/admin/admin.component').then((m) => m.AdminComponent),
        canActivate: [adminGuard],
        title: 'radish -- Admin',
      },
    ],
  },
  {
    path: 'game/:name',
    loadComponent: () => import('./pages/game/game.component').then((m) => m.GameComponent),
    canActivate: [authGuard],
    title: 'radish -- Spiel',
  },
  { path: '', pathMatch: 'full', redirectTo: 'games' },
  { path: '**', redirectTo: 'games' },
];
