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
 * Django-Backend-WebSocket, siehe radish/backend/api/consumers.py,
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
      <canvas id="canvas" #canvas width="640" height="420" tabindex="0" (contextmenu)="$event.preventDefault()"></canvas>
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
   * von GameSocketService an zuc_on_response() (client/src/main.c)
   * weiterzureichen. Deshalb bewusst als `any` belassen statt einer selbst
   * ausgedachten Typdefinition.
   */
  // eslint-disable-next-line @typescript-eslint/no-explicit-any
  private wasmModule: any = null;

  /*
   * Der Emscripten/SDL2-Client haengt seine Tastatur-Handler (keydown/
   * keypress/keyup) global an `window` an -- nicht an das <canvas> --, weil
   * SDL2s HTML5-Backend das absichtlich so macht, damit Tasteneingaben auch
   * ohne zuverlaessigen Canvas-Fokus ankommen (per Browser-DevTools verifiziert:
   * beim Aufruf von createZucchiniModule() registriert client.js u.a.
   * "window:keydown"/"window:keyup"/"window:keypress", nichts davon an
   * `document`). client.js exportiert dafuer kein Aufraeum-Hook
   * (`JSEvents.removeAllEventListeners` ist Teil der Emscripten-Runtime,
   * aber nicht in EXPORTED_RUNTIME_METHODS enthalten -- siehe
   * radish/client/CMakeLists.txt), und ohne eigenes Nachrunterfahren blieben
   * diese `window`-Listener nach dem Verlassen dieser Komponente fuer den
   * Rest der SPA aktiv: jede Taste, die das Spiel waehrend einer laufenden
   * Partie fuer Steuerung nutzt (z.B. WASD/Pfeile), wuerde dann per
   * `e.preventDefault()` global geschluckt -- inklusive in ganz normalen
   * Text-/Passwort-Feldern auf anderen Seiten (siehe games-shell/join).
   * Deshalb werden hier `window.addEventListener` UND (vorsichtshalber,
   * falls eine kuenftige client.js-Version doch an `document` registriert)
   * `document.addEventListener` waehrend des WASM-Ladens abgefangen, um
   * genau die von client.js registrierten Tastatur-Listener zu merken und
   * sie in ngOnDestroy() wieder zu entfernen.
   */
  private readonly capturedKeyListeners: Array<{
    target: Document | Window;
    type: string;
    listener: EventListenerOrEventListenerObject;
    options?: boolean | AddEventListenerOptions;
  }> = [];
  private restoreAddEventListeners: (() => void) | null = null;

  async ngAfterViewInit(): Promise<void> {
    try {
      this.wasmModule = await this.loadWasmModule();
      // Wird von zuc_js_send() (client/src/main.c) aufgerufen, wenn der
      // WASM-Client ein Kommando ans Backend schicken will.
      this.wasmModule.sendToChannel = (bytes: Uint8Array | ArrayBuffer) => this.sendToBackend(bytes);
      this.gameSocket.setWasmEventHandler((bytes) => this.forwardToWasm(bytes));
      // Nach sendToChannel: sobald der Client "offen" hoert, schickt er seine
      // erste Nachricht (Discover, client/src/main.c).
      this.gameSocket.setConnectionStateHandler((open) => this.forwardConnectionState(open));
    } catch (err) {
      console.error('[game-canvas] WASM-Client konnte nicht geladen werden:', err);
    }
  }

  ngOnDestroy(): void {
    this.gameSocket.setWasmEventHandler(null);
    this.gameSocket.setConnectionStateHandler(null);
    this.scriptEl?.remove();
    this.releaseGlobalKeyListeners();
  }

  /**
   * Kopiert die von GameSocketService dekodierten "event"-Bytes ins
   * WASM-Heap und ruft zuc_on_response() (siehe client/src/main.c) --
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
    module.ccall('zuc_on_response', null, ['number', 'number'], [ptr, bytes.length]);
    module._free(ptr);
  }

  /**
   * Meldet dem WASM-Client, ob die Verbindung zum Backend offen ist
   * (zuc_on_connection_state() in client/src/main.c). Ohne diese Meldung bleibt
   * er im Zustand "verbinde..." und schickt nach dem Beitritt nichts von sich aus.
   */
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

  // eslint-disable-next-line @typescript-eslint/no-explicit-any
  private loadWasmModule(): Promise<any> {
    return new Promise((resolve, reject) => {
      const canvas = this.canvasRef.nativeElement;
      canvas.addEventListener('click', () => canvas.focus());

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
        window
          .createZucchiniModule({ canvas, print: console.log, printErr: console.error })
          .then(resolve, reject);
      };
      script.onerror = () => reject(new Error(`${script.src} konnte nicht geladen werden.`));
      this.interceptGlobalKeyListeners();
      document.body.appendChild(script);
      this.scriptEl = script;
    });
  }

  /**
   * Faengt window.addEventListener(...)/document.addEventListener(...) fuer
   * "keydown"/"keypress"/"keyup" ab, solange der WASM-Client laedt bzw.
   * laeuft, und merkt sich jeden so registrierten Listener, um ihn in
   * releaseGlobalKeyListeners() gezielt wieder zu entfernen (siehe Kommentar
   * bei capturedKeyListeners oben).
   */
  private interceptGlobalKeyListeners(): void {
    const captured = this.capturedKeyListeners;
    const KEY_EVENT_TYPES = new Set(['keydown', 'keypress', 'keyup']);
    const restoreFns: Array<() => void> = [];

    for (const target of [window, document] as Array<Window | Document>) {
      const original = target.addEventListener.bind(target);
      target.addEventListener = ((
        type: string,
        listener: EventListenerOrEventListenerObject,
        options?: boolean | AddEventListenerOptions,
      ) => {
        if (KEY_EVENT_TYPES.has(type)) {
          captured.push({ target, type, listener, options });
        }
        return original(type, listener, options);
      }) as typeof target.addEventListener;
      restoreFns.push(() => {
        target.addEventListener = original;
      });
    }

    this.restoreAddEventListeners = () => restoreFns.forEach((restore) => restore());
  }

  private releaseGlobalKeyListeners(): void {
    for (const { target, type, listener, options } of this.capturedKeyListeners) {
      target.removeEventListener(type, listener, options);
    }
    this.capturedKeyListeners.length = 0;
    this.restoreAddEventListeners?.();
    this.restoreAddEventListeners = null;
  }
}
