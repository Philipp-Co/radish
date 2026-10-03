import { HttpClient } from '@angular/common/http';
import { Injectable } from '@angular/core';
import { Observable, map } from 'rxjs';

export interface GameListEntry {
  name: string;
  player_count: number;
  /** Keine der beiden Armeen darf mehr Punkte kosten. */
  points_limit: number;
}

/**
 * Wo ein Spiel steht, siehe radish/backend/api/models.py, GameStatus: in
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

/** Ein registrierter Game-Server, siehe radish/backend/api/admin_views.py. */
export interface GameServerEntry {
  id: number;
  name: string;
  /** Muss keine IP sein -- siehe radish/backend/api/models.py, GameServer.address. */
  address: string;
  port: number;
  is_occupied: boolean;
}

/**
 * Katalog, aus dem Armeen zusammengestellt werden -- siehe
 * radish/backend/api/army_views.py, CatalogView. Nur lesend; gepflegt wird
 * er spaeter ueber eigene Admin-Schnittstellen.
 */
export interface EntityProfile {
  id: number;
  name: string;
  health: number;
  armor: number;
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
  /** Summe aus den Katalogpreisen, siehe radish/backend/api/models.py, Army.total_cost. */
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
 * api/admin_views.py). in_use: der Eintrag steckt in mindestens einer Armee
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

/** Filter fuer listCatalogEntries; search sucht im Namen (ohne Gross-/Kleinschreibung). */
export interface CatalogFilter {
  species?: number;
  unit_type?: number;
  weapon_class?: WeaponClass;
  search?: string;
}

/**
 * Duenner Wrapper um die Matchmaking-Endpunkte unter api/client/ (siehe
 * radish/backend/api/client_views.py, urls.py). stream/ und state/
 * existieren serverseitig zwar schon (teils noch als Platzhalter, siehe
 * dortige Docstrings), sind hier aber bewusst noch nicht angebunden. Das
 * eigentliche Ingame-Geschehen (frueher: command/) laeuft inzwischen ueber
 * den WebSocket (siehe core/game-socket.service.ts, radish/backend/api/
 * consumers.py).
 */
@Injectable({ providedIn: 'root' })
export class ApiService {
  constructor(private readonly http: HttpClient) {}

  listGames(): Observable<{ games: GameListEntry[] }> {
    return this.http.get<{ games: GameListEntry[] }>('/api/client/games/');
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
   * zweitem Spieler (siehe radish/backend/api/client_views.py, GameStartView).
   * Synchron: die Antwort kommt erst, wenn die Spielinstanz das Spiel aufgesetzt
   * hat, und liefert es dann schon als "running".
   */
  startGame(): Observable<GameDetail> {
    return this.http.post<GameDetail>('/api/client/game/start/', {});
  }

  /** Verlaesst das aktuelle Spiel, siehe radish/backend/api/client_views.py, LeaveGameView. */
  leaveGame(): Observable<{ status: string }> {
    return this.http.post<{ status: string }>('/api/client/game/leave/', {});
  }

  /**
   * Alle aktuell angemeldeten Game-Server (Adminbereich, siehe
   * radish/backend/api/admin_views.py, AdminServerListView -- erfordert die
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

  deleteCatalogEntry(resource: CatalogResource, id: number): Observable<void> {
    return this.http.delete<void>(`/api/admin/catalog/${resource}/${id}/`);
  }
}
