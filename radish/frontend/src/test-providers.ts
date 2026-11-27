import { provideZonelessChangeDetection } from '@angular/core';

/**
 * Provider fuer jeden TestBed-Test (angular.json, test.options.providersFile):
 * die App laeuft ohne zone.js (siehe app.config.ts), die Tests deshalb auch.
 */
export default [provideZonelessChangeDetection()];
