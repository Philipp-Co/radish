import { AfterViewInit, Component, ElementRef, Input, OnDestroy, ViewChild, inject } from '@angular/core';

import { environment } from '../../../environments/environment';
import { GameSocketService } from '../../core/game-socket.service';

/*
 * Emscripten legt beim Laden von client.js eine globale Fabrikfunktion an
 * (Name durch "-s EXPORT_NAME=..." in radish/client/CMakeLists.txt
 * festgelegt). Es gibt dafuer keine offizielle Typdeklaration -- das
 * zurueckgegebene Modul (Module.ccall/._malloc/.HEAPU8/...) kommt aus
 * Emscriptens eigener Laufzeit, deshalb hier bewusst als `any` belassen statt
 * einer selbst ausgedachten, moeglicherweise falschen Typdefinition.
 */
declare global {
  interface Window {
    // eslint-disable-next-line @typescript-eslint/no-explicit-any
    createZucchiniModule?: (options: Record<string, unknown>) => Promise<any>;
  }
}

/**
 * Das eigentliche Spiel-Canvas samt WASM-Client -- ausgelagert aus dem, was
 * bis vor Kurzem GameComponent alleine war (siehe deren Docstring-Historie),
 * damit dieselbe Logik an mehreren Stellen eingebettet werden kann, ohne sie
 * zu verlassen bzw. neu zu laden: einerseits weiterhin unter der
 * eigenstaendigen Route "/game/:name" (siehe GameComponent, z.B. nach dem
 * Erstellen/Beitreten eines Spiels), andererseits direkt eingebettet auf
 * "Aktuelles Spiel" (siehe CurrentGameComponent) -- dort ausdruecklich ohne
 * Umleitung auf eine andere Seite.
 *
 * Laedt aktuell nur den WASM-Client und haengt ihn ans Canvas -- die
 * bisherige WebRTC/Relay-Verbindung (radish/relay/server.py) ist entfernt,
 * weil der Relay entfaellt (die UDP-Bruecke laeuft stattdessen ueber den
 * Django-Backend-WebSocket, siehe radish/backend/api/interfaces/player/views/consumers.py,
 * EchoConsumer). Der WASM-Client bekommt dadurch aktuell KEINE Verbindung
 * zu einem Spielserver mehr -- das Verdrahten mit dem WebSocket (siehe
 * core/game-socket.service.ts) ist ein spaeterer Schritt.
 *
 * gameName ist weiterhin rein informativ (fuer eine optionale Anzeige durch
 * die einbettende Seite).
 */
/**
 * Die Werte von ZucConnectionState in client/src/main.c -- zuc_on_connection_state()
 * nimmt sie als int entgegen. Bei einer Aenderung dort auch hier nachziehen.
 */
const ZUC_STATE_OPEN = 1;
const ZUC_STATE_CLOSED = 2;

@Component({
  selector: 'app-game-canvas',
  standalone: true,
  template: `
    <div class="game-canvas">
      <!-- Die rechte Maustaste verschiebt die Kamera (client/src/io/camera_control.c):
           das Kontextmenue des Browsers darf dabei nicht aufgehen. -->
      <canvas id="canvas" #canvas width="640" height="420" (contextmenu)="$event.preventDefault()"></canvas>
    </div>
  `,
  styles: [
    `
      .game-canvas {
        display: flex;
        flex-direction: column;
        gap: 0.5rem;
      }
      canvas {
        border: 1px solid var(--border, #2a2a30);
        display: block;
        background: #000;
        max-width: 100%;
      }
    `,
  ],
})
export class GameCanvasComponent implements AfterViewInit, OnDestroy {
  @ViewChild('canvas') private readonly canvasRef!: ElementRef<HTMLCanvasElement>;

  /** Nur fuer eine moegliche Anzeige durch die einbettende Seite, siehe Klassen-Docstring. */
  @Input() gameName = '';

  private readonly gameSocket = inject(GameSocketService);
  private scriptEl: HTMLScriptElement | null = null;
  /*
   * Ergebnis von loadWasmModule() (Module.ccall/._malloc/.HEAPU8/... aus der
   * Emscripten-Laufzeit, siehe Kommentar bei Window.createZucchiniModule
   * oben) -- wird in forwardToWasm() gebraucht, um eingehende "event"-Bytes
   * von GameSocketService an RAD_OnMessageReceived() (client/src/main.c)
   * weiterzureichen. Deshalb bewusst als `any` belassen statt einer selbst
   * ausgedachten Typdefinition.
   */
  // eslint-disable-next-line @typescript-eslint/no-explicit-any
  private wasmModule: any = null;

  async ngAfterViewInit(): Promise<void> {
    try {
      this.wasmModule = await this.loadWasmModule();
      // Wird von zuc_js_send() (client/src/main.c) aufgerufen, wenn der
      // WASM-Client ein Kommando ans Backend schicken will.
      this.wasmModule.sendToChannel = (bytes: Uint8Array | ArrayBuffer) => this.sendToBackend(bytes);
      this.gameSocket.setWasmEventHandler((bytes) => this.forwardToWasm(bytes));
      // Die eigene, oeffentliche Spieler-Id -- damit der Client erkennt, was
      // ihm gehoert (RAD_ClientSetPlayerId, client/src/main.c).
      this.gameSocket.setPlayerIdHandler((playerId) => this.forwardPlayerId(playerId));
      // Nach sendToChannel: sobald der Client "offen" hoert, schickt er seine
      // erste Nachricht (Discover, client/src/main.c).
      this.gameSocket.setConnectionStateHandler((open) => this.forwardConnectionState(open));
    } catch (err) {
      console.error('[game-canvas] WASM-Client konnte nicht geladen werden:', err);
    }
  }

  ngOnDestroy(): void {
    this.gameSocket.setWasmEventHandler(null);
    this.gameSocket.setPlayerIdHandler(null);
    this.gameSocket.setConnectionStateHandler(null);
    this.scriptEl?.remove();
  }

  /**
   * Kopiert die von GameSocketService dekodierten "event"-Bytes ins
   * WASM-Heap und ruft RAD_OnMessageReceived() (siehe client/src/main.c) --
   * gleiches Muster wie zuvor in web/index.html (forwardResponseToWasm)
   * fuer die inzwischen entfernte WebRTC/Relay-Verbindung.
   */
  private forwardToWasm(bytes: Uint8Array): void {
    const module = this.wasmModule;
    if (!module) {
      return;
    }
    const ptr = module._malloc(bytes.length);
    module.HEAPU8.set(bytes, ptr);
    module.ccall('RAD_OnMessageReceived', null, ['number', 'number'], [ptr, bytes.length]);
    module._free(ptr);
  }

  /**
   * Meldet dem WASM-Client, ob die Verbindung zum Backend offen ist
   * (zuc_on_connection_state() in client/src/main.c). Ohne diese Meldung bleibt
   * er im Zustand "verbinde..." und schickt nach dem Beitritt nichts von sich aus.
   */
  private forwardPlayerId(playerId: string): void {
    const module = this.wasmModule;
    if (!module) {
      return;
    }
    module.ccall('RAD_ClientSetPlayerId', null, ['string'], [playerId]);
  }

  private forwardConnectionState(open: boolean): void {
    const module = this.wasmModule;
    if (!module) {
      return;
    }
    module.ccall('zuc_on_connection_state', null, ['number'], [open ? ZUC_STATE_OPEN : ZUC_STATE_CLOSED]);
  }

  /**
   * Umgekehrte Richtung: reicht ein vom WASM-Client gesendetes Kommando
   * (Module.sendToChannel, siehe ngAfterViewInit) an
   * GameSocketService.sendCommand() weiter, das es Base64-kodiert als
   * {"type":"command","data":...} ans Backend schickt. HEAPU8.slice() in
   * zuc_js_send() (client/src/main.c) liefert bereits ein Uint8Array --
   * der ArrayBuffer-Fall bleibt trotzdem als Absicherung erhalten, falls
   * eine kuenftige client.js-Version das anders macht.
   */
  private sendToBackend(bytes: Uint8Array | ArrayBuffer): void {
    const payload = bytes instanceof ArrayBuffer ? new Uint8Array(bytes) : bytes;
    this.gameSocket.sendCommand(payload);
  }

  /**
   * Laedt Bilder und Schrift des Clients. Sie stecken nicht im Wasm, sondern
   * liegen einzeln mit Hash im Namen unter wasmBaseUrl/assets/ (siehe
   * radish/client/publish_assets.cmake); manifest.json bildet den Pfad, unter
   * dem der C-Code sie oeffnet ("/assets/..."), auf diesen Namen ab. Weil ein
   * geaendertes Bild einen neuen Namen bekommt, darf der Browser jede Datei
   * dauerhaft cachen (radish/backend/web/interfaces/serving.py) -- nur das Manifest wird
   * jedes Mal neu geprueft.
   */
  private async loadAssets(): Promise<[string, Uint8Array][]> {
    const baseUrl = `${environment.wasmBaseUrl}/assets`;
    const manifestResponse = await fetch(`${baseUrl}/manifest.json`);
    if (!manifestResponse.ok) {
      throw new Error(`${baseUrl}/manifest.json: HTTP ${manifestResponse.status}`);
    }
    const manifest: Record<string, string> = await manifestResponse.json();
    return Promise.all(
      Object.entries(manifest).map(async ([path, file]): Promise<[string, Uint8Array]> => {
        const response = await fetch(`${baseUrl}/${file}`);
        if (!response.ok) {
          throw new Error(`${baseUrl}/${file}: HTTP ${response.status}`);
        }
        return [path, new Uint8Array(await response.arrayBuffer())];
      }),
    );
  }

  /**
   * Legt die geladenen Assets ins Emscripten-Dateisystem (Module.FS, in
   * radish/client/CMakeLists.txt exportiert). Laeuft als preRun, also vor
   * main() -- der C-Code findet sie dort, wo frueher --embed-file sie ablegte.
   */
  // eslint-disable-next-line @typescript-eslint/no-explicit-any
  private writeAssets(module: any, assets: [string, Uint8Array][]): void {
    for (const [path, data] of assets) {
      module.FS.mkdirTree(path.substring(0, path.lastIndexOf('/')));
      module.FS.writeFile(path, data);
    }
  }

  // eslint-disable-next-line @typescript-eslint/no-explicit-any
  private loadWasmModule(): Promise<any> {
    // Parallel zu client.js, nicht erst danach.
    const assets = this.loadAssets();
    // Ohne das gaelte ein Fehler hier als unbehandelt, solange client.js noch
    // laedt; gemeldet wird er unten ueber reject.
    assets.catch(() => undefined);

    return new Promise((resolve, reject) => {
      // Ohne tabindex und Fokus: der Client wertet keine Tastatur aus, und
      // SDL meldet dafuer keine Listener an (client/src/view/view.c).
      const canvas = this.canvasRef.nativeElement;

      const script = document.createElement('script');
      // Von radish/client/CMakeLists.txt (RUNTIME_OUTPUT_DIRECTORY) nach
      // radish/web/ gebaut -- unter environment.wasmBaseUrl eingehaengt,
      // siehe radish/backend/web/ (liefert es unter /wasm/ aus, sowohl im
      // Container als auch bei lokalem `manage.py runserver`) und
      // proxy.conf.json (`ng serve`, reicht /wasm ebenfalls dorthin durch).
      script.src = `${environment.wasmBaseUrl}/client.js`;
      script.onload = () => {
        if (!window.createZucchiniModule) {
          reject(
            new Error('createZucchiniModule steht nach dem Laden von client.js nicht zur Verfuegung.'),
          );
          return;
        }
        const createModule = window.createZucchiniModule;
        assets
          .then((loaded) =>
            createModule({
              canvas,
              print: console.log,
              printErr: console.error,
              // eslint-disable-next-line @typescript-eslint/no-explicit-any
              preRun: [(module: any) => this.writeAssets(module, loaded)],
            }),
          )
          .then(resolve, reject);
      };
      script.onerror = () => reject(new Error(`${script.src} konnte nicht geladen werden.`));
      document.body.appendChild(script);
      this.scriptEl = script;
    });
  }
}
