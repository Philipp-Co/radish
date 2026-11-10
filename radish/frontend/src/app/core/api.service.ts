import { HttpClient } from '@angular/common/http';
import { Injectable, inject } from '@angular/core';
import { Observable, map } from 'rxjs';

export interface GameListEntry {
  name: string;
  player_count: number;
  /** Keine der beiden Armeen darf mehr Punkte kosten. */
  points_limit: number;
}

/**
 * Eine Seite der Spieleliste, siehe radish/backend/api/interfaces/player/views/client_views.py,
 * GameListView. page ist die tatsaechlich gelieferte Seite (ab 1) -- das
 * Backend korrigiert eine zu grosse oder ungueltige Seite selbst.
 */
export interface GameListPage {
  games: GameListEntry[];
  count: number;
  page: number;
  page_size: number;
}

/**
 * Wo ein Spiel steht, siehe radish/backend/api/models/game_status.py, GameStatus: in
 * der Lobby, oder laufend, sobald der Host es gestartet und die
 * Spielinstanz es aufgesetzt hat.
 */
export type GameStatus = 'lobby' | 'running';

export interface GameDetail {
  id: number;
  name: string;
  host_identifier: string;
  second_player_identifier: string | null;
  points_limit: number;
  host_army_name: string | null;
  second_player_army_name: string | null;
  status: GameStatus;
  /** Ob der angemeldete Nutzer Host ist -- nur der kann das Spiel starten. */
  is_host: boolean;
}

/** Ein registrierter Game-Server, siehe radish/backend/api/interfaces/admin/views/admin_views.py. */
export interface GameServerEntry {
  id: number;
  name: string;
  /** Muss keine IP sein -- siehe radish/backend/api/models/game_server.py, GameServer.address. */
  address: string;
  port: number;
  is_occupied: boolean;
}

/**
 * Katalog, aus dem Armeen zusammengestellt werden -- siehe
 * radish/backend/api/interfaces/player/views/army_views.py, CatalogView. Nur lesend; gepflegt wird
 * er spaeter ueber eigene Admin-Schnittstellen.
 */
export interface EntityProfile {
  id: number;
  name: string;
  health: number;
  armor: number;
  toughness: number;
  strength: number;
  accuracy: number;
  equipment_slots: number;
  /** Waffenslots je Klasse: Standard, schwer, super-schwer. */
  weapon_slots: number;
  heavy_weapon_slots: number;
  super_heavy_weapon_slots: number;
  cost: number;
  /** Wie viele Entitaeten einer Einheit dieses Profil mindestens/hoechstens haben. */
  min_count: number;
  max_count: number;
}

export interface UnitType {
  id: number;
  name: string;
  min_entities: number;
  max_entities: number;
  /**
   * Hoechstzahl schwerer bzw. super-schwerer Waffen je Einheit, ueber alle
   * Entitaeten zusammen; null = keine Grenze (nur die Slots der Profile).
   */
  max_heavy_weapons: number | null;
  max_super_heavy_weapons: number | null;
  /** Wie viele andere Einheiten dieser Typ transportieren kann (0 = keine). */
  transport_capacity: number;
  /** Ob Einheiten dieses Typs Ziele einnehmen koennen. */
  can_capture_objectives: boolean;
  /** Bewegungsradius in Feldern. */
  movement: number;
  /** Sichtweite in Feldern (waagerecht plus senkrecht), 0..255. */
  sight_range: number;
  /**
   * Bild im Spiel: der Ordner unter radish/assets/units/, z.B. "heavy_tank".
   * Leer, solange es keins gibt -- dann zeichnet der Client ein Rechteck.
   */
  sprite: string;
  profiles: EntityProfile[];
}

/**
 * Klasse einer Waffe -- bestimmt, welche Slots des Entitaetsprofils sie
 * belegt (weapon_slots, heavy_weapon_slots, super_heavy_weapon_slots).
 * Alle Waffen traegt eine einzelne Entitaet.
 */
export type WeaponClass = 'standard' | 'heavy' | 'super_heavy';

export const WEAPON_CLASS_LABELS: Record<WeaponClass, string> = {
  standard: 'Waffe',
  heavy: 'Schwere Waffe',
  super_heavy: 'Super-schwere Waffe',
};

export interface Weapon {
  id: number;
  name: string;
  weapon_class: WeaponClass;
  shots: number;
  strength: number;
  /** 0 = keine Mindestreichweite. */
  min_range: number;
  max_range: number;
  armor_penetration: number;
  cost: number;
}

export interface Equipment {
  id: number;
  name: string;
  cost: number;
}

export interface SpeciesCatalog {
  id: number;
  name: string;
  unit_types: UnitType[];
  weapons: Weapon[];
  equipment: Equipment[];
}

export interface ArmyListEntry {
  id: number;
  name: string;
  species_id: number;
  species_name: string;
  unit_count: number;
  /** Summe aus den Katalogpreisen, siehe radish/backend/api/models/army.py, Army.total_cost. */
  total_cost: number;
}

/** Eine Entitaet einer Einheit; Waffen je Klasse und Ausruestung nach Slot geordnet. */
export interface ArmyUnitEntity {
  profile_id: number;
  weapon_ids: number[];
  heavy_weapon_ids: number[];
  super_heavy_weapon_ids: number[];
  equipment_ids: number[];
}

/** Wie vom Backend geliefert: zusaetzlich mit den berechneten Kosten. */
export interface ArmyUnitEntityDetail extends ArmyUnitEntity {
  total_cost: number;
}

export interface ArmyUnitDetail extends ArmyUnit {
  entities: ArmyUnitEntityDetail[];
  total_cost: number;
}

export interface ArmyUnit {
  unit_type_id: number;
  entities: ArmyUnitEntity[];
}

/** Eingabe fuer Anlegen und Speichern -- immer der vollstaendige Stand. */
export interface ArmyInput {
  name: string;
  species_id: number;
  units: ArmyUnit[];
}

export interface ArmyDetail extends ArmyInput {
  id: number;
  species_name: string;
  units: ArmyUnitDetail[];
  total_cost: number;
}

/**
 * Katalog-Pflege fuer Admins unter api/admin/catalog/ (siehe radish/backend/
 * api/interfaces/admin/views/admin_views.py). in_use: der Eintrag steckt in mindestens einer Armee
 * -- dann sind seine Strukturfelder (Slots, Anzahlgrenzen, schwer) gesperrt
 * und er laesst sich nicht loeschen.
 */
export type CatalogResource = 'species' | 'unit-types' | 'profiles' | 'weapons' | 'equipment';

const CATALOG_LIST_KEYS: Record<CatalogResource, string> = {
  species: 'species',
  'unit-types': 'unit_types',
  profiles: 'profiles',
  weapons: 'weapons',
  equipment: 'equipment',
};

export interface AdminSpecies {
  id: number;
  name: string;
}

export interface AdminEntityProfile extends EntityProfile {
  unit_type_id: number;
  unit_type_name: string;
  species_id: number;
  species_name: string;
  in_use: boolean;
}

export interface AdminUnitType {
  id: number;
  species_id: number;
  species_name: string;
  name: string;
  min_entities: number;
  max_entities: number;
  /**
   * Hoechstzahl schwerer bzw. super-schwerer Waffen je Einheit, ueber alle
   * Entitaeten zusammen; null = keine Grenze (nur die Slots der Profile).
   */
  max_heavy_weapons: number | null;
  max_super_heavy_weapons: number | null;
  /** Wie viele andere Einheiten dieser Typ transportieren kann (0 = keine). */
  transport_capacity: number;
  /** Ob Einheiten dieses Typs Ziele einnehmen koennen. */
  can_capture_objectives: boolean;
  /** Bewegungsradius in Feldern. */
  movement: number;
  /** Sichtweite in Feldern (waagerecht plus senkrecht), 0..255. */
  sight_range: number;
  /**
   * Bild im Spiel: der Ordner unter radish/assets/units/, z.B. "heavy_tank".
   * Leer, solange es keins gibt -- dann zeichnet der Client ein Rechteck.
   */
  sprite: string;
  in_use: boolean;
  profiles: AdminEntityProfile[];
}

export interface AdminWeapon extends Weapon {
  species_id: number;
  species_name: string;
  in_use: boolean;
}

export interface AdminEquipment extends Equipment {
  species_id: number;
  species_name: string;
  in_use: boolean;
}

/**
 * Ein Tile-Typ mit seinen Werten (api/admin/terrain/, siehe radish/backend/
 * api/interfaces/admin/views/admin_views.py, AdminTerrainTypeListView). Die Typen stehen fest --
 * geaendert werden nur die Werte; sie gelten ab dem naechsten Spielstart.
 */
export interface AdminTerrainType {
  /** Der Typ, wie ihn das Spiel kennt, z.B. "forest". Nicht aenderbar. */
  code: string;
  /** Bezeichnung fuer den Adminbereich, z.B. "Wald". */
  label: string;
  /** MV: um wie viele Felder das Feld die Bewegungsreichweite aendert. */
  movement_modifier: number;
  /** CV: um wie viel das Feld einen Treffer auf eine Einheit darauf erschwert. */
  cover: number;
  /** Ob eine Einheit das Feld betreten oder darauf aufgestellt werden darf. */
  impassable: boolean;
  /** Ob das Feld die direkte Sicht auf die Felder dahinter versperrt. */
  blocks_sight: boolean;
}

export type AdminTerrainValues = Pick<
  AdminTerrainType,
  'movement_modifier' | 'cover' | 'impassable' | 'blocks_sight'
>;

/** Filter fuer listCatalogEntries; search sucht im Namen (ohne Gross-/Kleinschreibung). */
export interface CatalogFilter {
  species?: number;
  unit_type?: number;
  weapon_class?: WeaponClass;
  search?: string;
}

/** Animationen eines Einheitenbildes, wie in der sprite.json. */
export type SpriteAnimation = 'turn' | 'aim' | 'move' | 'fire' | 'destroyed';
/** Blickrichtung auf dem Bildschirm; leer bei der Drehung (ein Atlas fuer alle). */
export type SpriteFacing = '' | 'sw' | 'se' | 'ne' | 'nw';

/** Eine hochgeladene PNG eines Einheitenbildes (ohne ihre Bytes). */
export interface AdminSpriteImage {
  id: number;
  /** Dateiname im Ordner des Bildes, z.B. "turn.png". */
  name: string;
  width: number;
  height: number;
  sha256: string;
}

/**
 * Ein Atlas: welche PNG eine Animation in einer Richtung zeigt und wie sie
 * aufgeteilt ist. Drehungen (turn, aim) nennen vier Schluesselbilder, Folgen
 * Bilder je Sekunde.
 */
export interface AdminSpriteAtlas {
  animation: SpriteAnimation;
  facing: SpriteFacing;
  image_id: number;
  frame_width: number;
  frame_height: number;
  frame_count: number;
  key_frames: number[] | null;
  fps: number | null;
}

/** Die Werte eines Bildes, die sich aendern lassen -- ohne Schluessel. */
export interface AdminSpriteValues {
  anchor_x: number;
  anchor_y: number;
  draw_width: number;
  own_shadow: boolean;
  step_ms: number | null;
  turn_ms: number | null;
  aim_ms: number | null;
  aim_hold_ms: number | null;
}

/** Ein hochgeladenes Einheitenbild (radish/backend/api/interfaces/admin/views/admin_sprite_views.py). */
export interface AdminSprite extends AdminSpriteValues {
  id: number;
  key: string;
  images: AdminSpriteImage[];
  atlases: AdminSpriteAtlas[];
  /** Ob alle Atlanten da und gueltig sind -- erst dann ist es im Spiel. */
  complete: boolean;
  /** Was fehlt oder nicht stimmt, je ein Satz. */
  problems: string[];
  /** Die Einheitentypen, die es nennen, als "Name (Spezies)". */
  used_by: string[];
  updated_at: string;
}

/**
 * Duenner Wrapper um die Matchmaking-Endpunkte unter api/client/ (siehe
 * radish/backend/api/interfaces/player/views/client_views.py, urls.py). stream/ und state/
 * existieren serverseitig zwar schon (teils noch als Platzhalter, siehe
 * dortige Docstrings), sind hier aber bewusst noch nicht angebunden. Das
 * eigentliche Ingame-Geschehen (frueher: command/) laeuft inzwischen ueber
 * den WebSocket (siehe core/game-socket.service.ts, radish/backend/api/
 * consumers.py).
 */
@Injectable({ providedIn: 'root' })
export class ApiService {
  private readonly http = inject(HttpClient);

  listGames(page = 1): Observable<GameListPage> {
    return this.http.get<GameListPage>('/api/client/games/', { params: { page } });
  }

  /** Das offene Spiel genau dieses Namens, egal auf welcher Seite es stuende. */
  findGame(name: string): Observable<GameListEntry | null> {
    return this.http
      .get<GameListPage>('/api/client/games/', { params: { name } })
      .pipe(map((response) => response.games[0] ?? null));
  }

  /** Die Armee muss dem Spieler gehoeren und darf hoechstens pointsLimit kosten. */
  createGame(
    name: string,
    password: string,
    pointsLimit: number,
    armyId: number,
  ): Observable<GameDetail> {
    return this.http.post<GameDetail>('/api/client/game/', {
      name,
      password,
      points_limit: pointsLimit,
      army_id: armyId,
    });
  }

  /** Die Armee darf hoechstens das Punktelimit des Spiels kosten. */
  joinGame(name: string, password: string, armyId: number): Observable<GameDetail> {
    return this.http.post<GameDetail>('/api/client/game/join/', {
      name,
      password,
      army_id: armyId,
    });
  }

  /** Das laufende Spiel des angemeldeten Nutzers, oder `null` wenn er gerade keins hat. */
  getCurrentGame(): Observable<{ game: GameDetail | null }> {
    return this.http.get<{ game: GameDetail | null }>('/api/client/game/current/');
  }

  /**
   * Startet das eigene Spiel aus der Lobby heraus -- nur als Host und erst mit
   * zweitem Spieler (siehe radish/backend/api/interfaces/player/views/client_views.py, GameStartView).
   * Synchron: die Antwort kommt erst, wenn die Spielinstanz das Spiel aufgesetzt
   * hat, und liefert es dann schon als "running".
   */
  startGame(): Observable<GameDetail> {
    return this.http.post<GameDetail>('/api/client/game/start/', {});
  }

  /** Verlaesst das aktuelle Spiel, siehe radish/backend/api/interfaces/player/views/client_views.py, LeaveGameView. */
  leaveGame(): Observable<{ status: string }> {
    return this.http.post<{ status: string }>('/api/client/game/leave/', {});
  }

  /**
   * Alle aktuell angemeldeten Game-Server (Adminbereich, siehe
   * radish/backend/api/interfaces/admin/views/admin_views.py, AdminServerListView -- erfordert die
   * Keycloak-Realm-Rolle "Radish-Admin").
   */
  listServers(): Observable<{ servers: GameServerEntry[] }> {
    return this.http.get<{ servers: GameServerEntry[] }>('/api/admin/servers/');
  }

  getCatalog(): Observable<{ species: SpeciesCatalog[] }> {
    return this.http.get<{ species: SpeciesCatalog[] }>('/api/client/catalog/');
  }

  listArmies(): Observable<{ armies: ArmyListEntry[] }> {
    return this.http.get<{ armies: ArmyListEntry[] }>('/api/client/armies/');
  }

  getArmy(id: number): Observable<ArmyDetail> {
    return this.http.get<ArmyDetail>(`/api/client/armies/${id}/`);
  }

  createArmy(army: ArmyInput): Observable<ArmyDetail> {
    return this.http.post<ArmyDetail>('/api/client/armies/', army);
  }

  updateArmy(id: number, army: ArmyInput): Observable<ArmyDetail> {
    return this.http.put<ArmyDetail>(`/api/client/armies/${id}/`, army);
  }

  deleteArmy(id: number): Observable<void> {
    return this.http.delete<void>(`/api/client/armies/${id}/`);
  }

  /** Eintraege einer Katalog-Ressource, optional gefiltert (siehe CatalogFilter). */
  listCatalogEntries<T>(resource: CatalogResource, filter: CatalogFilter = {}): Observable<T[]> {
    const params: Record<string, string | number> = {};
    for (const [key, value] of Object.entries(filter)) {
      if (value !== undefined && value !== '') {
        params[key] = value;
      }
    }
    return this.http
      .get<Record<string, T[]>>(`/api/admin/catalog/${resource}/`, { params })
      .pipe(map((response) => response[CATALOG_LIST_KEYS[resource]]));
  }

  getCatalogEntry<T>(resource: CatalogResource, id: number): Observable<T> {
    return this.http.get<T>(`/api/admin/catalog/${resource}/${id}/`);
  }

  createCatalogEntry<T>(resource: CatalogResource, entry: object): Observable<T> {
    return this.http.post<T>(`/api/admin/catalog/${resource}/`, entry);
  }

  updateCatalogEntry<T>(resource: CatalogResource, id: number, entry: object): Observable<T> {
    return this.http.put<T>(`/api/admin/catalog/${resource}/${id}/`, entry);
  }

  /** Aendert nur die genannten Felder (PATCH), etwa das Bild eines Einheitentyps. */
  patchCatalogEntry<T>(resource: CatalogResource, id: number, fields: object): Observable<T> {
    return this.http.patch<T>(`/api/admin/catalog/${resource}/${id}/`, fields);
  }

  deleteCatalogEntry(resource: CatalogResource, id: number): Observable<void> {
    return this.http.delete<void>(`/api/admin/catalog/${resource}/${id}/`);
  }

  /** Alle Tile-Typen mit ihren Werten, in der Reihenfolge des Spiels. */
  listTerrainTypes(): Observable<AdminTerrainType[]> {
    return this.http
      .get<{ terrain: AdminTerrainType[] }>('/api/admin/terrain/')
      .pipe(map((response) => response.terrain));
  }

  updateTerrainType(code: string, values: AdminTerrainValues): Observable<AdminTerrainType> {
    return this.http.put<AdminTerrainType>(`/api/admin/terrain/${code}/`, values);
  }

  // --- Einheitenbilder -------------------------------------------------------

  listSprites(): Observable<AdminSprite[]> {
    return this.http
      .get<{ sprites: AdminSprite[] }>('/api/admin/sprites/')
      .pipe(map((response) => response.sprites));
  }

  getSprite(id: number): Observable<AdminSprite> {
    return this.http.get<AdminSprite>(`/api/admin/sprites/${id}/`);
  }

  createSprite(key: string): Observable<AdminSprite> {
    return this.http.post<AdminSprite>('/api/admin/sprites/', { key });
  }

  updateSprite(id: number, values: Partial<AdminSpriteValues>): Observable<AdminSprite> {
    return this.http.patch<AdminSprite>(`/api/admin/sprites/${id}/`, values);
  }

  deleteSprite(id: number): Observable<void> {
    return this.http.delete<void>(`/api/admin/sprites/${id}/`);
  }

  /** Laedt eine PNG hoch; ohne "name" gilt der Dateiname, klein geschrieben. */
  uploadSpriteImage(id: number, file: File, name?: string): Observable<AdminSprite> {
    const form = new FormData();
    form.append('file', file);
    if (name) {
      form.append('name', name);
    }
    return this.http.post<AdminSprite>(`/api/admin/sprites/${id}/images/`, form);
  }

  /** Ersetzt den Inhalt einer PNG; ihr Name bleibt. */
  replaceSpriteImage(id: number, imageId: number, file: File): Observable<AdminSprite> {
    const form = new FormData();
    form.append('file', file);
    return this.http.put<AdminSprite>(`/api/admin/sprites/${id}/images/${imageId}/`, form);
  }

  deleteSpriteImage(id: number, imageId: number): Observable<void> {
    return this.http.delete<void>(`/api/admin/sprites/${id}/images/${imageId}/`);
  }

  /**
   * Die Bytes einer PNG fuer die Vorschau -- ueber den Client mit Token, denn
   * ein <img src> schickt keinen mit.
   */
  spriteImageBlob(id: number, imageId: number): Observable<Blob> {
    return this.http.get(`/api/admin/sprites/${id}/images/${imageId}/file/`, { responseType: 'blob' });
  }

  /** Setzt alle Atlanten auf einmal; was fehlt, gibt es danach nicht mehr. */
  putSpriteAtlases(id: number, atlases: AdminSpriteAtlas[]): Observable<AdminSprite> {
    return this.http.put<AdminSprite>(`/api/admin/sprites/${id}/atlases/`, atlases);
  }
}
