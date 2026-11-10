import { Injectable } from '@angular/core';

import { environment } from '../../environments/environment';

/**
 * Pfad einer Datei eines Einheitenbildes im Asset-Manifest des Clients: je Bild
 * ein Ordner unter assets/units/ mit seiner sprite.json und seinen PNG-Atlanten
 * (radish/client/publish_assets.cmake, radish/client/schema/sprite.schema.json).
 * Der Ordnername ist der Schluessel, den ein Einheitentyp im Katalog nennt
 * (UnitType.sprite) -- dasselbe Muster wie dort: [a-z0-9_], hoechstens 31
 * Zeichen.
 */
const SPRITE_PATH = /^\/assets\/units\/([a-z0-9_]{1,31})\/sprite\.json$/;
const SPRITE_IMAGE_PATH = /^\/assets\/units\/([a-z0-9_]{1,31})\/([^/]+\.png)$/;

/** Eine Option einer Auswahl der Einheitenbilder. */
export interface UnitSpriteOption {
  value: string;
  label: string;
}

/**
 * Die Optionen einer Auswahl der Einheitenbilder: zuerst "kein Bild" (leer),
 * dann jeder Schluessel aus "keys" -- und "current", auch wenn es ihn dort nicht
 * (mehr) gibt: sonst saehe die Auswahl leer aus, und Speichern wuerde das Bild
 * still entfernen.
 */
export function unitSpriteOptions(keys: string[], current: string | null): UnitSpriteOption[] {
  const options: UnitSpriteOption[] = [
    { value: '', label: 'Kein Bild (Rechteck)' },
    ...keys.map((key) => ({ value: key, label: key })),
  ];
  if (current && !keys.includes(current)) {
    options.push({ value: current, label: `${current} (nicht in den Assets)` });
  }
  return options;
}

/** Eine PNG-Datei eines Einheitenbildes. */
export interface UnitSpriteImage {
  /** Dateiname im Ordner des Bildes, z.B. "turn.png". */
  name: string;
  /** Wo der Browser sie laedt -- mit Hash im Namen, dauerhaft cachebar. */
  url: string;
}

/**
 * Welche Einheitenbilder es gibt und welche Dateien zu ihnen gehoeren: aus dem
 * Asset-Manifest, das der Wasm-Client beim Start laedt
 * (shared/game-canvas/game-canvas.component.ts). So zeigt der Admin-Katalog nur
 * Bilder an, die der Client auch findet.
 *
 * Das Manifest wird einmal je Sitzung geladen; schlaegt es fehl, beim naechsten
 * Aufruf neu.
 */
@Injectable({ providedIn: 'root' })
export class UnitSpriteService {
  private readonly baseUrl = `${environment.wasmBaseUrl}/assets`;
  private manifestPromise: Promise<Record<string, string>> | null = null;

  /** Die Schluessel aller Bilder mit sprite.json, alphabetisch. */
  async keys(): Promise<string[]> {
    const manifest = await this.manifest();
    return Object.keys(manifest)
      .map((path) => SPRITE_PATH.exec(path)?.[1])
      .filter((key): key is string => key !== undefined)
      .sort();
  }

  /**
   * Die PNG-Dateien des Bildes "key", nach Namen sortiert -- leer, wenn es
   * keine gibt.
   */
  async images(key: string): Promise<UnitSpriteImage[]> {
    const manifest = await this.manifest();
    return Object.entries(manifest)
      .map(([path, file]): UnitSpriteImage | null => {
        const match = SPRITE_IMAGE_PATH.exec(path);
        return match !== null && match[1] === key ? { name: match[2], url: `${this.baseUrl}/${file}` } : null;
      })
      .filter((image): image is UnitSpriteImage => image !== null)
      .sort((a, b) => a.name.localeCompare(b.name));
  }

  /**
   * Vergisst das geladene Manifest -- nach einer Aenderung an einem
   * hochgeladenen Bild, damit Auswahl und Vorschau den neuen Stand zeigen.
   */
  refresh(): void {
    this.manifestPromise = null;
  }

  private manifest(): Promise<Record<string, string>> {
    if (this.manifestPromise === null) {
      this.manifestPromise = this.loadManifest().catch((error: unknown) => {
        this.manifestPromise = null;
        throw error;
      });
    }
    return this.manifestPromise;
  }

  private async loadManifest(): Promise<Record<string, string>> {
    const url = `${this.baseUrl}/manifest.json`;
    // no-cache: das Manifest wechselt mit jedem hochgeladenen Bild.
    const response = await fetch(url, { cache: 'no-cache' });
    if (!response.ok) {
      throw new Error(`${url}: HTTP ${response.status}`);
    }
    return response.json();
  }
}
