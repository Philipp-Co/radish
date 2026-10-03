import { Routes } from '@angular/router';

import { adminGuard, authGuard } from './core/auth.guard';
import { CATALOG_MODELS } from './pages/games/admin/catalog/catalog-config';

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
    // authGuard steht hier an der Elternroute und gilt damit fuer alle
    // Unterseiten (current/lobby/list/create/join/armies/admin) mit -- siehe
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
        // Wartebereich nach Erstellen/Beitreten, bis die Spielinstanz bereit
        // ist -- danach geht es weiter auf "current" (siehe LobbyComponent).
        path: 'lobby',
        loadComponent: () =>
          import('./pages/games/lobby/lobby.component').then((m) => m.LobbyComponent),
        title: 'radish -- Lobby',
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
        path: 'armies',
        loadComponent: () =>
          import('./pages/games/armies/list/army-list.component').then(
            (m) => m.ArmyListComponent,
          ),
        title: 'radish -- Armeen',
      },
      {
        // Vor armies/:id, sonst wuerde "new" als id gelesen.
        path: 'armies/new',
        loadComponent: () =>
          import('./pages/games/armies/edit/army-edit.component').then(
            (m) => m.ArmyEditComponent,
          ),
        title: 'radish -- Neue Armee',
      },
      {
        path: 'armies/:id',
        loadComponent: () =>
          import('./pages/games/armies/edit/army-edit.component').then(
            (m) => m.ArmyEditComponent,
          ),
        title: 'radish -- Armee bearbeiten',
      },
      {
        // Zusaetzlich zu authGuard an der Elternroute "games" noch
        // adminGuard: nur mit der Keycloak-Realm-Rolle "Radish-Admin"
        // erreichbar (siehe core/auth.guard.ts). Der Tab dorthin ist in
        // GamesShellComponent ebenfalls an diese Rolle geknuepft, aber wer
        // die URL direkt aufruft, soll trotzdem nicht durchkommen.
        // adminGuard sitzt am Rahmen und gilt damit fuer servers/ und catalog/.
        path: 'admin',
        loadComponent: () =>
          import('./pages/games/admin/admin-shell.component').then((m) => m.AdminShellComponent),
        canActivate: [adminGuard],
        children: [
          { path: '', pathMatch: 'full', redirectTo: 'servers' },
          {
            path: 'servers',
            loadComponent: () =>
              import('./pages/games/admin/admin.component').then((m) => m.AdminComponent),
            title: 'radish -- Admin: Game-Server',
          },
          {
            // Je Katalog-Modell (siehe catalog-config.ts) eine Seite zum
            // Anlegen und eine zum Aendern; welches Modell, steht in den
            // Routendaten (von der komponentenlosen Elternroute geerbt).
            path: 'catalog',
            loadComponent: () =>
              import('./pages/games/admin/catalog/catalog-shell.component').then(
                (m) => m.CatalogShellComponent,
              ),
            children: [
              { path: '', pathMatch: 'full', redirectTo: 'unit-types' },
              ...CATALOG_MODELS.map((model) => ({
                path: model.resource,
                data: { resource: model.resource },
                children: [
                  { path: '', pathMatch: 'full' as const, redirectTo: 'edit' },
                  {
                    path: 'new',
                    loadComponent: () =>
                      import('./pages/games/admin/catalog/catalog-create.component').then(
                        (m) => m.CatalogCreateComponent,
                      ),
                    title: `radish -- Katalog: ${model.singular} anlegen`,
                  },
                  {
                    path: 'edit',
                    loadComponent: () =>
                      import('./pages/games/admin/catalog/catalog-edit.component').then(
                        (m) => m.CatalogEditComponent,
                      ),
                    title: `radish -- Katalog: ${model.title} ändern`,
                  },
                ],
              })),
            ],
          },
        ],
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
