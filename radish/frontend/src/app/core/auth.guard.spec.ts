import { signal } from '@angular/core';
import { TestBed } from '@angular/core/testing';
import { ActivatedRouteSnapshot, RouterStateSnapshot, UrlTree, provideRouter } from '@angular/router';
import { Router } from '@angular/router';

import { adminGuard, authGuard } from './auth.guard';
import { AuthService } from './auth.service';

describe('Guards', () => {
  const isAuthenticated = signal(false);
  const isAdmin = signal(false);

  beforeEach(() => {
    isAuthenticated.set(false);
    isAdmin.set(false);
    TestBed.configureTestingModule({
      providers: [provideRouter([]), { provide: AuthService, useValue: { isAuthenticated, isAdmin } }],
    });
  });

  const run = (guard: typeof authGuard, url = '/games/armies') =>
    TestBed.runInInjectionContext(() =>
      guard({} as ActivatedRouteSnapshot, { url } as RouterStateSnapshot),
    );
  const serialize = (result: unknown) => TestBed.inject(Router).serializeUrl(result as UrlTree);

  describe('authGuard', () => {
    it('laesst Angemeldete durch', () => {
      isAuthenticated.set(true);
      expect(run(authGuard)).toBe(true);
    });

    it('schickt Abgemeldete zu /login und merkt sich das Ziel', () => {
      expect(serialize(run(authGuard, '/game/alpha'))).toBe('/login?redirect=%2Fgame%2Falpha');
    });
  });

  describe('adminGuard', () => {
    it('laesst Admins durch', () => {
      isAdmin.set(true);
      expect(run(adminGuard)).toBe(true);
    });

    it('schickt Angemeldete ohne Rolle zum Hub statt zu /login', () => {
      isAuthenticated.set(true);
      expect(serialize(run(adminGuard))).toBe('/games/home');
    });
  });
});
