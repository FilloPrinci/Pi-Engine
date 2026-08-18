# Progettazione — Engine 3D Open Source per Raspberry Pi 4 (e Pi 5)

**Fase:** Progettazione
**Stato:** Bozza v8 — copre hardware, scelte tecnologiche, architettura moduli, Platform Layer, Editor e Script System, portabilità multi-piattaforma, rendering pipeline (TBDR, profili Low-Poly/PBR, bloom), fisica (Jolt, parallelizzazione, API scripting), audio (miniaudio, thread dedicato, bus), input/gamepad (Action Mapping, hot-plug), Asset Pipeline (Source/Cooked, GUID, Cooker), Prefab (istanziazione, nested, scope one-way). Base per le fasi successive (analisi, analisi tecnica, contesto Claude Code)

---

## 1. Obiettivo del progetto

Un engine 3D **open source**, **accessibile** (documentazione chiara, curva di apprendimento ragionevole per sviluppatori indie) e **ben ottimizzato per l'hardware Raspberry Pi 4**, con compatibilità forward verso il Raspberry Pi 5. Target primario: giochi 3D low-poly in stile retro. Target secondario (opzionale, scalabile): 3D più realistico per progetti più ambiziosi.

---

## 2. Analisi hardware target (dati verificati, agosto 2026)

### 2.1 Raspberry Pi 4 (BCM2711) — target primario

| Componente | Specifica |
|---|---|
| CPU | 4× ARM Cortex-A72, nessun SMT, 1 MB cache L2 condivisa, nessuna L3 |
| GPU | Broadcom VideoCore VI, tile-based deferred renderer (TBDR), 500–600 MHz, ~4.4 GFLOPS |
| API grafiche | OpenGL ES 3.1 conformante (driver V3D), **Vulkan 1.3 conformante** (driver V3DV, da Mesa 24.3) |
| RAM | LPDDR4-3200, condivisa CPU/GPU (nessuna VRAM dedicata) |
| Banda di memoria | fino a ~13 GB/s |

### 2.2 Raspberry Pi 5 (BCM2712) — target di compatibilità forward

| Componente | Specifica |
|---|---|
| CPU | 4× ARM Cortex-A76, 2 MB cache L2, 2 MB cache L3, IPC ~35% superiore all'A72 a parità di clock |
| GPU | Broadcom VideoCore VII, TBDR, 800 MHz–1 GHz, ~10–19 GFLOPS a seconda del clock |
| API grafiche | Stesso driver V3DV, stessa conformità Vulkan 1.3; OpenGL ES 3.1 |
| RAM | LPDDR4X-4267, condivisa CPU/GPU |
| Banda di memoria | fino a ~17 GB/s |
| Extra | PCIe 2.0 (NVMe), utile per streaming asset più rapido in progetti futuri |

### 2.3 Implicazioni architetturali (il punto più importante di questo documento)

1. **La GPU è il collo di bottiglia principale, non la CPU.** Il Cortex-A76 del Pi5 da solo fa più FLOPS FP32 della sua stessa GPU VideoCore VII. Questo è l'opposto di un PC desktop, dove la GPU domina. Conseguenza diretta: l'engine deve spostare quanto più lavoro possibile sulla CPU (culling, batching, LOD, animazione) e trattare i cicli GPU come la risorsa più preziosa.
2. **Entrambe le GPU sono TBDR** (come le GPU mobile: Mali, Adreno, PowerVR), non immediate-mode come una GPU desktop. Questo cambia le regole di un renderer efficiente: i render pass e i subpass Vulkan vanno progettati per sfruttare il tiling, va evitato ogni readback framebuffer non necessario, e i cambi di render target vanno minimizzati.
3. **La banda di memoria (13–17 GB/s, condivisa CPU+GPU) è la seconda risorsa più scarsa.** Texture compresse obbligatorie (ETC2 come baseline mobile-class), overdraw da ridurre attivamente, niente buffer ridondanti.
4. **Nessuna VRAM dedicata**: ogni allocazione GPU consuma la stessa RAM di sistema usata dalla logica di gioco. Il resource manager deve avere budget di memoria espliciti e configurabili, non assunzioni "infinite" come su desktop.
5. **CPU quad-core senza SMT**: il job system / task graph va progettato nel core dell'engine fin dal primo giorno, non aggiunto in un secondo momento. Culling, skinning, fisica, streaming asset devono girare in parallelo su thread worker.
6. **Differenza di potenza Pi4→Pi5 (~2-3× su CPU, ~2-4× su GPU)**: l'engine deve avere un sistema di *hardware profile* che rilevi il chip a runtime e scali automaticamente budget poligoni, risoluzione texture e distanza LOD, invece di richiedere retuning manuale per ogni target.

---

## 3. Scelte tecnologiche

### 3.1 Linguaggio: **C++20**

Motivazione (non preferenza soggettiva):
- Ecosistema Vulkan su Linux/ARM (validation layers, RenderDoc, SDK, documentazione, sample) è nativamente C/C++.
- Librerie open source essenziali per un engine (loader glTF, fisica — Bullet/Jolt, audio — miniaudio, tooling — Dear ImGui) sono C/C++ con binding maturi.
- Controllo diretto e prevedibile del layout di memoria e della cache: critico con 1 MB di L2 condivisa su 4 core (Pi4).
- Coerente con l'obiettivo "accessibile": la maggioranza degli sviluppatori indie/hobbisti di engine conosce C++.
- Rust resta valido in astratto ma qui introdurrebbe attrito nel tooling ARM/embedded specifico senza vantaggio decisivo, dato che comunque gran parte del codice richiede gestione manuale della memoria per motivi di performance.

### 3.2 API grafica: **Vulkan (driver V3DV)**

- Target baseline: **Vulkan 1.2 core**, con feature detection per estensioni 1.3 dove disponibili (garantisce compatibilità anche su installazioni con Mesa meno recente).
- Nessun backend OpenGL ES nella v1. Può essere aggiunto in futuro come backend alternativo dietro un layer di astrazione (RHI), se emergerà necessità di compatibilità con sistemi datati.

### 3.2bis Creazione superficie e input: doppio backend (vedi sezione 5)

- **SDL2** per il backend a finestra (Wayland/X11) — non SDL3, per bug noti attuali nella combinazione KMSDRM+Vulkan.
- **libdrm + libinput** per il backend a schermo diretto (KMS/DRM, nessun compositor), implementato a mano via estensione `VK_KHR_display`.
- Entrambi dietro l'interfaccia comune `IDisplayBackend`, dettagliata in sezione 5.

### 3.3 Sistema di build e dipendenze

- CMake (standard de facto per progetti C++ cross-platform/cross-compiler, buon supporto per cross-compilazione ARM64).
- Gestione dipendenze: da valutare in fase di analisi tecnica (vcpkg vs Conan vs sottomoduli git) — punto da approfondire nella prossima fase.
- Target di compilazione: AArch64 (Raspberry Pi OS 64-bit), con possibilità di cross-compilazione da x86_64 per sviluppo più rapido.

---

## 4. Architettura software dell'engine (moduli)

```
┌─────────────────────────────────────────────────────┐
│                     Applicazione / Gioco              │
├─────────────────────────────────────────────────────┤
│  Scripting layer (opzionale, da valutare fase succ.)  │
├───────────────┬───────────────┬───────────────────────┤
│   ECS (scene   │  Gameplay     │   Audio               │
│   & entità)    │  systems      │   engine               │
├───────────────┴───────────────┴───────────────────────┤
│              Job System / Task Graph                   │
│         (parallelizzazione su 4 core, no SMT)          │
├─────────────────────────────────────────────────────┤
│  Renderer (RHI astratto)  │  Resource Manager           │
│  - Culling (frustum+occl.)│  - Budget memoria esplicito │
│  - Batching / instancing  │  - Streaming asset          │
│  - LOD                    │  - Compressione texture     │
│  - Hardware profile       │                              │
├─────────────────────────────────────────────────────┤
│              Backend Vulkan (V3DV) — RHI concreto       │
├─────────────────────────────────────────────────────┤
│   Platform Layer (IDisplayBackend)                      │
│   - SDL2Backend (finestra, Wayland/X11)                 │
│   - DirectDRMBackend (KMS/DRM diretto, VK_KHR_display)   │
├─────────────────────────────────────────────────────┤
│         Raspberry Pi OS (Linux, kernel DRM/KMS)         │
└─────────────────────────────────────────────────────┘
```

**Moduli principali:**

0. **Platform Layer** — interfaccia `IDisplayBackend` che disaccoppia tutto l'engine (RHI, renderer, gameplay) dal modo in cui viene creata la superficie di rendering e raccolto l'input. Vedi sezione 5 per il dettaglio completo: due implementazioni intercambiabili (finestra via SDL2, oppure KMS/DRM diretto), stesso binario, nessuna differenza per il codice sopra questo layer.
1. **RHI (Render Hardware Interface)** — layer di astrazione sottile sopra Vulkan. Non un'astrazione multi-backend enorme fin da subito (sovraingegnerizzazione da evitare), ma una separazione pulita che permette di isolare le specificità V3DV/TBDR e — in futuro — aggiungere un backend GLES se necessario.
2. **ECS (Entity Component System)** — data-oriented per natura, si sposa bene con i vincoli di cache ridotta del Cortex-A72/A76 (layout contiguo, cache-friendly iteration).
3. **Job System** — task graph con work-stealing su 4 worker thread, usato da culling, animazione, fisica, streaming.
4. **Resource Manager** — gestione asset (mesh, texture, materiali, audio) con budget di memoria espliciti per profilo hardware (Pi4 vs Pi5), streaming asincrono.
5. **Renderer** — culling (frustum + occlusione), batching/instancing per minimizzare draw call, sistema LOD, gestione consapevole del TBDR (render pass/subpass ottimizzati, minimo overdraw).
6. **Hardware Profile System** — rilevamento a runtime (Pi4/VideoCore VI vs Pi5/VideoCore VII) con scaling automatico di: budget poligoni, risoluzione texture, distanza/bias LOD, qualità post-processing.
7. **Asset Pipeline** — tool offline per conversione/ottimizzazione asset (compressione texture ETC2, ottimizzazione mesh, generazione LOD), separato dal runtime.
8. **Audio Engine** — **miniaudio** (scelta motivata in sezione 10), thread dedicato fuori dal Job System per evitare buffer underrun.
9. **Fisica** — **Jolt Physics** (scelta motivata in sezione 9), integrato nel Job System dell'engine via adapter invece di un thread pool interno separato.
10. **Script System** — `ScriptComponent`, `ComponentHandle<T>`, macro `REGISTER_SCRIPT`/`EXPOSE`; permette al gameplay in C++ compilato di agganciarsi a entity/componenti con sintassi stile Unity. Dettaglio completo in sezione 6.
11. **Editor** — applicazione separata, client dell'Engine Core (stesso RHI/Vulkan del gioco finale): Scene View, Asset Browser, Inspector, Console, Project Hub, pipeline Build/Play/Debug. Nessun editor di codice integrato — si appoggia a IDE esterni (VSCode) generando automaticamente `compile_commands.json` e configurazioni `.vscode/`. Dettaglio completo in sezione 6.

---

## 5. Platform Layer: display duale e modalità di esecuzione

### 5.1 Contesto

Il Pi gira di default con desktop **Wayland/labwc**. Per un engine di giochi, però, il compositor introduce overhead — piccolo ma non trascurabile quando la GPU è già la risorsa più scarsa. La soluzione non è scegliere *un* backend di display, ma progettare l'engine perché ne supporti **due, intercambiabili, nello stesso binario**, così lo sviluppatore/giocatore sceglie senza compromessi sul codice.

### 5.2 Interfaccia comune

Vulkan non ha bisogno di sapere *come* è stata creata la superficie su cui disegna — solo che `VkSurfaceKHR` sia valida. Questo rende possibile isolare la differenza in un'unica interfaccia:

```cpp
class IDisplayBackend {
public:
    virtual bool Init() = 0;
    virtual VkSurfaceKHR CreateVulkanSurface(VkInstance instance) = 0;
    virtual std::vector<const char*> GetRequiredVulkanExtensions() = 0;
    virtual void PollEvents(InputState& out) = 0;
    virtual Extent2D GetDrawableSize() = 0;
    virtual void Shutdown() = 0;
};
```

**Tutto** il resto dell'engine (RHI, renderer, ECS, gameplay) dipende solo da `IDisplayBackend`, mai dalle implementazioni concrete.

### 5.3 Le due implementazioni

| Backend | Come funziona | Quando si usa |
|---|---|---|
| **`SDL2DisplayBackend`** | Finestra via SDL2 su Wayland/X11 (via XWayland). Superficie Vulkan via `SDL_Vulkan_CreateSurface`. Input tradotto direttamente da SDL2 (tastiera, mouse, gamepad). | Default per lo sviluppo e per la build distribuita ai giocatori. Funziona identico su Pi e su PC desktop, utile per iterare senza dover testare solo su hardware target. |
| **`DirectDRMDisplayBackend`** | Nessuna finestra: prende il DRM master su un VT libero, crea la superficie via estensione `VK_KHR_display` (`vkCreateDisplayPlaneSurfaceKHR`), enumera risoluzioni disponibili via `vkGetDisplayModePropertiesKHR`. Input via **libinput** (stessa libreria usata da Wayland/labwc sotto il cofano — matura, gestisce già gamepad/tastiera/mouse via evdev). | "Modalità Performance" opzionale — nessun compositor in mezzo, massime prestazioni disponibili. |

Entrambi i backend alimentano la stessa struttura `InputState`: il codice di gameplay non sa (né deve sapere) quale dei due è attivo.

### 5.4 Come avviene il passaggio (switch) tra i due

Non in-process (smontare SDL/Wayland e rimontare DRM a runtime nello stesso processo è fragile — race condition su chi possiede il DRM master). Il meccanismo robusto è per **rilancio del processo**:

1. Il giocatore attiva "Modalità Performance" nel menu → il gioco salva la preferenza e si rilancia da sé con un flag (`--display=drm`).
2. Il nuovo processo parte su un VT libero (via `logind`/`systemd-run --scope`, meccanismo già usato dal sistema per far coesistere sessioni grafiche — **nessun permesso speciale o root richiesto**, l'utente normale di Raspberry Pi OS è già nei gruppi `video`/`render`/`input` necessari) e istanzia `DirectDRMDisplayBackend` al posto di `SDL2DisplayBackend`.
3. Il desktop resta vivo sul suo VT originale, in pausa, pronto a riprendere quando il gioco termina o l'utente torna indietro.
4. Alla chiusura, la preferenza torna a default e il prossimo avvio riparte in finestra.

Due processi puliti in sequenza, mai due backend attivi contemporaneamente — niente stato ibrido da gestire.

### 5.5 Modalità di esecuzione per il gioco distribuito

| Livello | Come si presenta | Note |
|---|---|---|
| **Finestra (default)** | Il gioco compilato gira come qualsiasi app Linux: si lancia, appare fullscreen via SDL2 sopra il desktop normale. Zero configurazione, zero permessi speciali. | Build distribuita di default. Per la maggior parte dei giochi low-poly target di questo engine, la perdita di prestazioni dovuta al compositor è trascurabile. |
| **Performance (opt-in)** | Opzione nel menu del gioco stesso. Internamente esegue lo switch descritto in 5.4, in modo trasparente per il giocatore (nessun terminale, nessuna password). | Stesso binario del Livello 1 — nessuna build separata da mantenere. |

*(Nota: l'opzione "immagine SD dedicata standalone" è stata volutamente esclusa dallo scope — l'engine punta a girare su installazioni normali di Raspberry Pi OS, non a fornire un sistema operativo/distribuzione a sé.)*

### 5.6 Conseguenze pratiche per lo sviluppo

- **Un solo binario**: entrambi i backend compilati sempre dentro (le dipendenze — SDL2, libdrm, libinput — sono leggere, nessun motivo di escluderle a compile-time).
- **Un solo set di test** per gameplay/renderer, perché la logica sopra `IDisplayBackend` è identica tra i due backend; il codice da testare *specificamente* per ciascun backend si riduce al sottile layer di creazione superficie + input, isolato e piccolo.
- **Nota tecnica su SDL**: usare **SDL2**, non SDL3, per il backend a finestra — SDL3 ha attualmente bug noti e ricorrenti proprio nella combinazione KMSDRM+Vulkan (schermo nero, display non rilevati, frame corrotti su Pi5); per questo il backend DRM diretto va comunque implementato a mano via `VK_KHR_display`, non delegato a SDL.

---

## 6. Editor e Script System

### 6.1 Filosofia dell'Editor: organizzare, non scrivere codice

L'Editor **non** è un IDE (niente editor di codice integrato, niente hot-reload runtime — scelta esplicita per restare semplici e coerenti con "solo C++ compilato, massime prestazioni"). L'Editor è dove si **vede, organizza, configura e lancia** — la scrittura di codice avviene sempre in un IDE esterno (VSCode di default, configurabile).

**Scope dell'Editor v1:**
- **Scene View** — visualizzazione/navigazione 3D della scena, usa lo stesso RHI del gioco (nessuna discrepanza tra editor e build finale).
- **Asset Browser** — import, organizzazione asset, creazione script da template.
- **Inspector** — posizionare/configurare game object (entity), impostare attributi, assegnare script.
- **Console** — log, errori di build.
- **Project Hub** — gestione multi-progetto: engine installato una volta, ogni progetto referenzia l'installazione condivisa (nessuna copia dell'engine per progetto).
- **Build/Play/Debug pipeline** — vedi 6.4.

**Esplicitamente escluso dallo scope v1:** editor di codice integrato, hot-reload C++ a runtime.

### 6.2 Script System: come uno script accede ai componenti

Un game object (entity ECS) con uno script attaccato accede agli altri componenti della stessa entity (es. Transform) con una sintassi allo stile Unity:

```cpp
class PlayerScript : public ScriptComponent {
public:
    void OnStart() override {
        transform = GetComponent<TransformComponent>();
    }
    void OnUpdate(float dt) override {
        transform->position += glm::vec3(0, 0, 1) * speed * dt;
    }

    EXPOSE(speed) float speed = 3.0f; // visibile e modificabile nell'Inspector
};
```

- **`ScriptComponent`** — classe base con `OnStart()`, `OnUpdate(float dt)`, `OnDestroy()`, più `GetComponent<T>()`/`GetEntity()` ereditati.
- **`ComponentHandle<T>`** — non un puntatore raw permanente: l'ECS data-oriented (sezione 2.3, 4) può spostare i dati dei componenti in memoria tra un frame e l'altro (array contigui, cache-friendly). `ComponentHandle<T>` è un handle leggero che si risolve in modo sicuro a ogni accesso, così `transform->position` resta valido anche dopo riarrangiamenti interni — stesso pattern usato da ECS seri (es. Unity DOTS). Per lo sviluppatore, la sintassi resta identica a un puntatore normale.
- **`REGISTER_SCRIPT(NomeClasse)`** — macro che iscrive la classe script in una factory a startup, così l'Editor sa quali script esistono nel binario compilato e può elencarli/assegnarli nell'Inspector (nessuna reflection C++ complessa richiesta).
- **`EXPOSE(campo)`** — macro "intrusiva" che si espande a codice che registra il campo (nome, offset, tipo) in una tabella statica, **senza cambiare la dichiarazione del campo stesso** (resta un `float` normale). Scelta deliberata rispetto a un tool di code-generation esterno (stile Unreal Header Tool): niente file `.generated.h` da rigenerare, l'IDE vede sempre il codice reale così com'è scritto — autocomplete corretto sempre, senza passi di build intermedi.

### 6.3 Creazione script e integrazione IDE esterno

- **Da template**: nell'Asset Browser, "Nuovo Script" genera una coppia `.h`/`.cpp` da un template con gli stub (`OnStart`, `OnUpdate`, `OnDestroy`) già presenti — come "Create > Script" in Unity. Il file nuovo viene **registrato automaticamente nel sistema di build** (CMake `file(GLOB)` sulla cartella script del progetto, o manifest generato dall'Editor) — lo sviluppatore non tocca mai `CMakeLists.txt` a mano.
- **Apertura in IDE**: doppio click sullo script nell'Asset Browser lancia l'IDE esterno configurato (default VSCode) sul file specifico.
- **Autocomplete accurato senza configurazione manuale**: l'Editor genera automaticamente, per ogni progetto:
  - `compile_commands.json` (via `CMAKE_EXPORT_COMPILE_COMMANDS=ON`) — necessario perché clangd (motore dietro l'IntelliSense C/C++ di VSCode) sappia esattamente include, flag e standard C++ del progetto.
  - `.vscode/tasks.json` e `.vscode/launch.json` — task di build e configurazione di debug (GDB) già pronti puntati al progetto corretto.
  - Risultato: lo sviluppatore apre VSCode sul progetto generato dall'Editor e ha autocomplete e debug funzionanti **da subito**, zero setup manuale.
- **Assegnazione a game object**: nell'Inspector, componente "Script" con uno slot — si trascina lo script dall'Asset Browser, l'Editor salva un riferimento per nome classe (non path, sopravvive a spostamenti file) nella scena serializzata.

### 6.4 Pipeline Build / Play / Debug

Bottone "Play" sulla scena aperta nell'Editor:
1. Trigger di una **build incrementale** (CMake ricompila solo ciò che è cambiato, non un full rebuild) **e cook incrementale degli asset modificati** (vedi sezione 12) — stesso flusso unico, non due passi separati da gestire a mano.
2. Errori di compilazione o di cook mostrati nella Console dell'Editor (niente terminale esterno da controllare).
3. Al successo, l'Editor lancia il gioco con quella scena caricata (usa lo stesso Platform Layer/backend finestra descritto in sezione 5).
4. **"Play in Debug"**: lancia sotto GDB, oppure lo sviluppatore si aggancia da VSCode tramite il `launch.json` già generato — breakpoint e step reali disponibili senza configurazione aggiuntiva.

Nessun hot-reload runtime richiesto per questo flusso: "Play" è compila-e-lancia, reso comunque fluido dalla build incrementale.

---

## 7. Portabilità multi-piattaforma (Pi4 / Pi5 / PC Windows / PC Linux)

### 7.1 Principio

Le ottimizzazioni restano pensate primariamente per Pi4 (target primario del progetto, sezione 2), ma le scelte architetturali fatte finora (Vulkan invece di API proprietarie, `IDisplayBackend`, Hardware Profile System) sono già l'astrazione giusta per estendersi a PC desktop con pochi aggiustamenti mirati — non un secondo motore da mantenere in parallelo.

### 7.2 Cosa è già portabile senza modifiche

- **Vulkan** è nativamente multi-piattaforma: su PC gira sui driver NVIDIA/AMD/Intel. Stesso RHI, stesso codice di rendering.
- **Shader in SPIR-V** (formato intermedio Vulkan) — compilati offline, identici su VideoCore, NVIDIA, AMD, Intel.
- **`SDL2DisplayBackend`** (sezione 5) — SDL2 supporta Win32 nativamente, nessuna riscrittura richiesta, solo ricompilazione per la piattaforma target.

### 7.3 Cosa va gestito esplicitamente

1. **`DirectDRMDisplayBackend` è Linux-only** (KMS/DRM è un'API del kernel Linux, non esiste su Windows) — escluso a compile-time nelle build Windows. Su PC desktop resta solo `SDL2DisplayBackend`: la "Modalità Performance" (bypassare il compositor) non ha lo stesso valore su desktop, dove la GPU non è il collo di bottiglia come sul Pi.
2. **Codice NEON (ARM) non compila su x86** — qualunque intrinsics NEON scritto a mano (skinning, culling, matematica vettoriale) richiede un equivalente SSE/AVX dietro la stessa interfaccia, oppure l'uso di una libreria matematica con supporto SIMD multi-target integrato (es. GLM in modalità SIMD). Punto tecnico da strutturare bene in fase di analisi tecnica — se rimandato, rischia di produrre codice ARM-only sparso nel codebase.
3. **Hardware Profile System** (sezione 4) esteso con un profilo **"Desktop"** (rilevato quando la GPU non è Broadcom): stesso meccanismo già esistente per Pi4/Pi5, ma che rimuove i limiti di budget poligoni/texture/LOD invece di stringerli — nessun nuovo concetto, solo un terzo profilo.
4. **Preset di build CMake per target**, ognuno seleziona toolchain, profilo hardware di default, e quali backend `IDisplayBackend` includere nel binario:

```
cmake --preset pi4      # toolchain aarch64, profilo Pi4, backend SDL2+DirectDRM
cmake --preset pi5      # toolchain aarch64, profilo Pi5, backend SDL2+DirectDRM
cmake --preset windows  # toolchain MSVC/MinGW, profilo Desktop, solo backend SDL2
cmake --preset linux-pc # toolchain nativa, profilo Desktop, solo backend SDL2
```

### 7.4 Costo reale da tenere presente

Ogni piattaforma aggiunta è superficie di test in più (input, packaging, differenze tra driver Vulkan dei vari vendor) — non è gratuito solo perché l'architettura lo permette. Resta comunque molto meno costoso rispetto a mantenere un renderer o un editor separato per piattaforma, proprio perché RHI e Platform Layer sono stati progettati come layer sottili e sostituibili fin dall'inizio.

---

## 8. Rendering Pipeline: tecniche TBDR e profili per progetto

### 8.1 Tecniche di ottimizzazione TBDR (valgono per entrambi i profili)

Regole di scrittura del renderer specifiche per l'architettura tile-based deferred renderer (VideoCore VI/VII, sezione 2), diverse da un renderer pensato per GPU desktop immediate-mode:

1. **Sfruttare l'Hidden Surface Removal hardware nativo** — le GPU TBDR eliminano già da sole le superfici nascoste dentro ogni tile *prima* di eseguire il fragment shader sui pixel coperti. Per ottenerlo davvero: evitare alpha blending non necessario (disattiva l'HSR per gli oggetti trasparenti), evitare `discard`/alpha-test nel fragment shader dove possibile (rompe l'early-depth-test).
2. **Render pass/subpass progettati per restare in tile memory** — ogni cambio di render target o `LOAD_OP_LOAD`/store non necessario forza un giro extra in RAM (la risorsa più scarsa, sezione 2.3). Regola pratica: `VK_ATTACHMENT_LOAD_OP_DONT_CARE` per ogni buffer non da caricare da un frame precedente, attachment `TRANSIENT` per depth/buffer intermedi che non devono mai lasciare la tile.
3. **Shader semplici e a precisione ridotta** — meno istruzioni ALU per pixel, `mediump`/fp16 dove la precisione lo permette (V3DV supporta le estensioni fp16).
4. **Culling CPU aggressivo prima della sottomissione geometria** (sezione 4) — su TBDR ogni tile comunque processa i triangoli che la toccano, quindi ridurre draw call e triangoli resta il modo più efficace di risparmiare.

### 8.2 Perché due pipeline separate invece di un uber-shader

Un uber-shader generico che gestisce con branching sia lo stile low-poly sia il PBR sprecherebbe cicli GPU su feature non usate in ogni singolo pixel — inaccettabile su hardware così limitato. Scelta architetturale: **due pipeline concrete, separate e specializzate**, ognuna compilata solo con ciò che le serve — implementazioni di classi diverse nel Renderer (`ForwardLitPipeline` vs `ForwardPlusPBRPipeline`), non un sistema unico configurabile a runtime.

### 8.3 Profilo "Low-Poly Retro" (target primario, sezione 1)

- **Forward rendering semplice**, single pass, nessun G-buffer.
- Illuminazione: vertex lighting o Blinn-Phong minimale, budget indicativo 2-4 luci dinamiche simultanee (valore da validare su hardware reale in fase di test, sezione 9).
- Ombre: preferibilmente **baked** (precalcolate offline nell'Asset Pipeline) invece di shadow map real-time — coerente con lo stile retro, quasi gratis a runtime.
- Materiali: template "Unlit" e "Simple Lit" pronti nell'Editor.

**Bloom leggero (post-process):**

Tecnica *soglia + downsample chain + Dual Kawase Blur* (usata nei motori mobile su GPU TBDR), non il Gaussian bloom multi-pass da tutorial desktop — quest'ultimo costa troppa banda per il nostro budget (sezione 2.3):

1. Estrazione soglia di luminosità **fusa nel primo downsample** (nessun pass a piena risoluzione dedicato).
2. Catena di downsample progressivo (es. 1/2 → 1/4 → 1/8 → 1/16, tipicamente 3-4 livelli) — ogni livello economico perché già piccolo.
3. **Dual Kawase Blur** invece di Gaussian — costo costante indipendentemente dal raggio (4-5 texture fetch per pass), niente kernel largo; il "morbido" viene dalla catena downsample/upsample stessa.
4. Upsample e accumulo verso la risoluzione piena con blending additivo.
5. Composito finale **fuso nel pass di tonemapping/color grading esistente** — niente pass extra dedicato, resta in tile memory (regola 8.1.2).

Parametri scalati per profilo hardware (Hardware Profile System, sezione 4):

| Parametro | Pi4 | Pi5 | Desktop |
|---|---|---|---|
| Risoluzione di partenza catena | 1/4 | 1/2 | 1/2 o piena |
| Livelli downsample/upsample | 3 | 4 | 4-5 |
| Precisione | fp16 | fp16 | fp16/fp32 |

Su hardware ancora più limitato, il sistema può disattivare il bloom del tutto (`bloom_enabled: false` nel profilo) senza toccare il codice del gioco.

### 8.4 Profilo "PBR" (target secondario, opzionale, sezione 1)

- **Non deferred pieno** — anche se il TBDR rende il G-buffer teoricamente economico via subpass/input attachment, su Pi4 ALU e bandwidth restano troppo scarse in assoluto per giustificare la complessità aggiuntiva.
- **Forward+ (clustered forward)**: singolo pass come il profilo retro, ma con culling delle luci per cluster — più luci dinamiche del forward semplice senza i costi di un deferred completo.
- PBR metallic-roughness, con **IBL precalcolata** offline nell'Asset Pipeline (non in tempo reale).
- Shadow mapping a budget contenuto (risoluzione/cascate ridotte rispetto a uno standard desktop).
- Materiali: template "PBR Standard" nell'Editor.
- Bloom: stessa tecnica Dual Kawase di 8.3, parametri leggermente più generosi di default.

### 8.5 Integrazione nel Project Hub

Nel **Project Hub** ("Nuovo Progetto"), oltre a nome/percorso, si sceglie lo **stile di rendering** (`render_pipeline: lowpoly_forward | pbr_forwardplus`), scritto nel manifest di progetto. Questa scelta determina a cascata:

- quale classe pipeline concreta usa il Renderer;
- quali template di materiale/shader sono disponibili nell'Editor per quel progetto;
- i budget di default nell'Hardware Profile System (un progetto PBR parte più conservativo di uno low-poly, a parità di hardware, per il costo per pixel più alto);
- quali tool dell'Asset Pipeline si attivano (es. il bake IBL ha senso solo per progetti PBR).

La scelta non è bloccante per sempre (un progetto avanzato può sbloccare controllo manuale), ma il default guidato evita che uno sviluppatore alle prime armi finisca per sbaglio su una pipeline più costosa del necessario — coerente con l'obiettivo "accessibile" (sezione 1).

---

## 9. Fisica: motore, parallelizzazione, integrazione con lo scripting

### 9.1 Scelta motore fisico: Jolt Physics

**Jolt Physics** (open source, MIT, usato da Guerrilla Games per Horizon Forbidden West e Death Stranding 2) invece di Bullet. Motivazione specifica per questo progetto: <cite index="46-1">Guerrilla è passata a Jolt proprio per i problemi che il loro precedente motore fisico causava durante l'interazione con l'aggiornamento multithreaded dei game object — Jolt è stato architettato specificamente per risolvere questo</cite>, grazie a <cite index="46-1">una broadphase lock-free e un algoritmo lock-free di costruzione delle simulation island</cite>. È esattamente la nostra situazione: abbiamo già un Job System nostro (sezione 4) e non vogliamo un secondo thread pool interno in concorrenza per gli stessi 4 core.

### 9.2 Come si parallelizza la pipeline fisica

1. **Broadphase** (aggiornamento AABB, coppie potenzialmente in collisione) — imbarazzantemente parallela, per corpo.
2. **Costruzione delle Island** — <cite index="43-1">i corpi vengono divisi in "island": ogni island è un insieme di corpi dinamici in contatto tra loro o connessi tramite un vincolo.</cite> Island diverse sono completamente indipendenti nello stesso step — confine naturale di parallelismo per il solver.
3. **Solver dei vincoli** — <cite index="39-1">diversi job vengono eseguiti in parallelo: ognuno prende la prossima island non ancora processata ed esegue il solver iterativo dei vincoli per quella island.</cite> Il parallelismo avviene *tra* island, non dentro (il solver a impulsi sequenziale è intrinsecamente iterativo all'interno di una singola island).
4. **Island grandi (caso critico)**: <cite index="41-1">le island il cui numero di vincoli e contatti supera una soglia vengono affidate al `LargeIslandSplitter`, così che più job possano processare parti diverse della stessa island in parallelo</cite> — evita che una singola pila di oggetti fisici lasci 3 core inattivi.
5. **Integrazione finale** (velocità/posizione) — di nuovo imbarazzantemente parallela per corpo.

### 9.3 Integrazione con il Job System dell'engine

Jolt espone `JobSystem` come interfaccia sostituibile. Scelta architetturale: **adapter che inietta i job di Jolt nel nostro Job System esistente** (sezione 4), invece di due scheduler paralleli sugli stessi 4 core — su Cortex-A72 senza SMT, due thread pool indipendenti sarebbero puro spreco per contesa/context switching. Un unico scheduler, una coda condivisa: culling, animazione e fisica competono equamente per gli stessi worker thread nello stesso frame.

### 9.4 Fasi e barriere di sincronizzazione (sicurezza per lo scripting, senza lock espliciti)

```
Frame:
 ├─ Fase Script (OnUpdate) — legge stato, applica forze/impulsi ai corpi
 ├─ ═══ barriera di sincronizzazione ═══
 ├─ Fase Fisica (parallela: broadphase → island → solve → integrate)
 ├─ ═══ barriera di sincronizzazione ═══
 ├─ Fase Callback Collisioni (single-thread, vedi 9.6)
 └─ Fase Post-Fisica — script leggono le nuove Transform, il renderer legge per disegnare
```

Nessuno script legge o scrive un `RigidBodyComponent` mentre il solver ci lavora sopra — impedito strutturalmente dalla barriera, non per disciplina del programmatore. Nessun mutex da spiegare allo sviluppatore indie, nessuna race condition possibile per costruzione — coerente con l'obiettivo "accessibile" (sezione 1).

**Timestep fisso** (accumulator pattern): la fisica gira a passo fisso (es. 60 Hz) disaccoppiato dal framerate di rendering variabile — dà anche un carico di lavoro prevedibile da distribuire tra i worker a ogni step.

### 9.5 Nota sulla cache (1 MB L2 condivisa, sezione 2)

I corpi in una stessa island sono spesso vicini spazialmente (sono in contatto tra loro) — processarli come unità di lavoro coesa mantiene un working set piccolo e cache-friendly. Da riflettere anche nel layout dati dei `RigidBodyComponent` nell'ECS (SoA, sezione 4): allineamento/padding per evitare false sharing tra thread che scrivono corpi adiacenti in memoria.

### 9.6 API di scripting: collisioni, trigger, raycast

```cpp
class BallScript : public ScriptComponent {
public:
    void OnCollisionEnter(const CollisionInfo& collision) override {
        Entity other = collision.otherEntity;
        glm::vec3 point  = collision.contactPoint;
        glm::vec3 normal = collision.contactNormal;
        float impulse    = collision.impulseMagnitude;
    }
    void OnCollisionStay(const CollisionInfo& collision) override { /* ogni frame di contatto continuo */ }
    void OnCollisionExit(Entity other) override { /* fine contatto */ }
    void OnTriggerEnter(Entity other) override { /* per collider con IsTrigger */ }
    void OnTriggerExit(Entity other) override { }
};
```

Sintassi allo stile Unity per lo sviluppatore; meccanismo sotto, necessario per via del solver parallelo (9.2):

1. Durante la fase fisica, Jolt notifica i contatti tramite un `ContactListener` — ma questi callback arrivano **sui thread worker del solver**, non sicuri per eseguire codice script arbitrario.
2. L'engine intercetta gli eventi e li scrive in **buffer per-thread lock-free** (una coda per worker, nessuna contesa tra thread) — solo raccolta dati grezzi, zero logica di gioco eseguita a questo punto.
3. Nella **Fase Callback Collisioni** dedicata (single-threaded, dopo la barriera di fine fisica), i buffer vengono uniti e dispatchati come `OnCollisionEnter/Stay/Exit` sugli script coinvolti — sicuro leggere/scrivere qualunque cosa, come in `OnUpdate`.
4. Enter/Stay/Exit non arrivano così direttamente da Jolt (che dà solo "contatto aggiunto/persistito/rimosso" per coppia di shape) — l'engine mantiene una tabella per-entity dei contatti attivi tra frame per derivare la transizione corretta.

```cpp
class PlayerScript : public ScriptComponent {
public:
    void OnStart() override { rigidbody = GetComponent<RigidbodyComponent>(); }
    void OnUpdate(float dt) override {
        if (Input::IsKeyPressed(Key::Space))
            rigidbody->AddImpulse(glm::vec3(0, 5, 0));

        RaycastHit hit;
        if (Physics::Raycast(transform->position, glm::vec3(0,-1,0), 1.5f, hit))
            isGrounded = true;
    }
    EXPOSE(rigidbody) ComponentHandle<RigidbodyComponent> rigidbody;
    bool isGrounded = false;
};
```

- **`RigidbodyComponent`** — wrapper sottile sul `BodyInterface` di Jolt (`AddForce`, `AddImpulse`, `SetVelocity`/`GetVelocity`, `SetKinematic`). Chiamate da script in `OnUpdate` (fase pre-fisica) accodate e applicate all'inizio dello step fisico successivo — sicure per costruzione, nessun lock gestito dallo sviluppatore.
- **`Physics::Raycast` / `Physics::OverlapSphere`** — query di sola lettura contro lo stato risultante dalla fine dello step fisico precedente, sicure in `OnUpdate` senza sincronizzazione aggiuntiva.
- **`ColliderComponent`** — forma di collisione (box/sfera/capsula/mesh), flag `IsTrigger`, **layer** — mappa diretta sul filtro a layer/maschera nativo di Jolt, esposto nell'Editor come matrice di collisione per-layer (Project Settings → Physics), stessa idea della collision matrix di Unity.

---

## 10. Audio: motore, thread dedicato, integrazione con lo scripting

### 10.1 Cosa deve fare il sistema audio (concetti base)

1. **Riprodurre suoni** — musica, effetti sonori, voci: partono, si fermano, si ripetono in loop.
2. **Mixare** — più suoni simultanei sommati in un unico segnale senza distorcere.
3. **Audio spaziale (3D)** — un suono a destra deve "sentirsi" a destra e attenuarsi con la distanza, equivalente sonoro del modello di illuminazione per il rendering.
4. **Bus per categoria** — gruppi con volume indipendente (Master → Musica, Effetti, UI, Voci), come l'Audio Mixer di Unity.

### 10.2 Vincolo specifico: thread dedicato, fuori dal Job System

Decodificare audio compresso costa CPU, e la riproduzione ha un vincolo che il rendering non ha: se il thread che genera il segnale arriva in ritardo anche di un millisecondo, si sente un click/scatto (buffer underrun) — l'orecchio nota le interruzioni più dell'occhio un frame perso. Per questo l'audio **non condivide il Job System** (culling/fisica/animazione, sezioni 4 e 9): merita un **thread dedicato**, sempre attivo, priorità stabile, che non compete con job di durata variabile.

### 10.3 Stack di sistema (Raspberry Pi OS)

<cite index="47-1">Su Raspberry Pi OS Desktop, PipeWire ha sostituito PulseAudio come server audio di default</cite>, con compatibilità ALSA sotto.

### 10.4 Libreria: **miniaudio**

- Singolo header C, zero dipendenze pesanti — coerente con la filosofia "dipendenze minime" già seguita per gli altri sistemi.
- Supporta nativamente ALSA/PulseAudio su Linux (funziona sotto PipeWire via compatibilità), WASAPI su Windows, CoreAudio su macOS — stesso codice su tutti i target della sezione 7, nessun backend audio da scrivere per piattaforma.
- Decodifica nativamente WAV, MP3, FLAC, Ogg Vorbis.
- Motore di spazializzazione 3D base integrato (`ma_engine`/`ma_sound`: attenuazione per distanza, panning stereo) — nessuna dipendenza da soluzioni proprietarie chiuse (FMOD/Wwise), incompatibili con l'obiettivo open source.

### 10.5 Formati e scelte pratiche per restare leggeri sul Pi4

- **SFX brevi**: WAV non compresso — zero decodifica a runtime, ideale per suoni ripetuti spesso (passi, spari), dove il costo CPU di decompressione ripetuta peserebbe più dello storage extra.
- **Musica/suoni lunghi**: Ogg Vorbis, **streaming** dal disco (non tutto in RAM, coerente col budget di memoria esplicito già impostato per le texture, sezione 2.3), decodificato a pezzi sul thread audio dedicato.
- **Niente HRTF/audio binaurale** — troppo costoso per il budget CPU del Pi4. Attenuazione per distanza + panning stereo semplice: compromesso corretto, quasi gratis.

### 10.6 Integrazione con ECS e scripting

```cpp
AudioSourceComponent {
    AudioClip clip;
    bool loop;
    bool spatial;       // 2D (musica/UI) o 3D (posizionato nel mondo)
    float volume;
    AudioBus bus;        // Music | SFX | UI | Voice
};

AudioListenerComponent { }; // di solito su camera/player, unico per scena
```

```cpp
class ExplosionScript : public ScriptComponent {
public:
    void OnCollisionEnter(const CollisionInfo& c) override {
        Audio::PlayOneShot(explosionClip, transform->position, /*volume*/ 1.0f);
    }
    EXPOSE(explosionClip) AudioClip explosionClip;
};
```

`Audio::PlayOneShot` per suoni "spara e dimentica" (non serve un `AudioSourceComponent` persistente per ogni effetto); musica/ambienti in loop usano `AudioSourceComponent` con `Play()`/`Stop()`/`FadeTo()` chiamati dallo script.

### 10.7 Bus audio nell'Editor

Project Settings → Audio: gerarchia fissa e semplice (non un grafo nodi complesso) **Master → Music, SFX, UI, Voice**, ognuno con proprio volume — permette ai giocatori di regolare Music/SFX separatamente nelle opzioni del gioco finale, esperienza standard attesa.

---

## 11. Input System: tastiera, mouse, gamepad

### 11.1 Principio: stessa logica del Platform Layer (sezione 5)

L'input segue la stessa filosofia già stabilita per il display: **backend fisici diversi, un'unica interfaccia che il resto dell'engine vede**. Tastiera e mouse arrivano dal backend video attivo (`SDL2DisplayBackend` o `DirectDRMDisplayBackend`/libinput, sezione 5).

### 11.2 Il caso specifico dei gamepad

Ogni controller ha un layout fisico diverso — serve un database di mapping che traduca i byte grezzi del dispositivo in un layout canonico standard (stile Xbox: A/B/X/Y, stick sinistro/destro, grilletti). Soluzione: il sottosistema joystick/game controller di SDL2 **non richiede il sottosistema video** — quindi si usa **sempre** `SDL_INIT_GAMECONTROLLER`, indipendentemente da quale dei due backend video (sezione 5) è attivo. Un solo codice di gestione gamepad in entrambe le modalità, appoggiato al database di mapping controller community-maintained di SDL2 (Xbox, PlayStation, Switch Pro, generici) — nessun database da scrivere e mantenere internamente.

### 11.3 Architettura a livelli

```
Script (Input::GetAction("Jump").WasPressed())
        ↑
Action Mapping Layer  — decoupled dal dispositivo fisico
        ↑
InputState unificato  — stesso formato indipendentemente dal backend
        ↑
   ┌────┴─────┐
Tastiera/Mouse   Gamepad (sempre via SDL_GameController,
(backend video    indipendente da quale backend video è attivo)
attivo, sez. 5)
```

### 11.4 Action Mapping

Lo script non legge direttamente il tasto fisico (altrimenti ogni remapping richiederebbe modifiche al codice C++) — chiede un'**azione logica**, mappata verso i tasti/bottoni fisici in un asset di configurazione modificabile dall'Editor (stesso principio del Input System di Unity / InputMap di Godot):

```cpp
class PlayerScript : public ScriptComponent {
public:
    void OnUpdate(float dt) override {
        glm::vec2 move = Input::GetAxis2D("Move");           // WASD o stick sinistro, stesso codice
        if (Input::GetAction("Jump").WasPressedThisFrame())  // Spazio o tasto A del pad
            rigidbody->AddImpulse(glm::vec3(0, 5, 0));
    }
};
```

Nell'Editor, pannello **Input Manager** (Project Settings → Input): ogni azione definita e collegabile a più input fisici insieme come alternative (es. "Jump" → tastiera Spazio **e** pad tasto A).

### 11.5 Deadzone e curva di risposta analogica

Deadzone **radiale** per stick analogici (non per singolo asse, altrimenti il movimento diagonale risulta scorretto), configurabile per azione/asse, più curva di risposta (lineare o esponenziale) — parametri esposti nell'Input Manager, non hardcoded.

### 11.6 Hot-plug e multiplayer locale

Eventi `OnGamepadConnected(int slot)` / `OnGamepadDisconnected(int slot)`, stesso pattern delle altre callback dell'engine (es. `OnCollisionEnter`, sezione 9) — uno script "Player Manager" assegna i pad connessi a slot giocatore, gestisce la disconnessione senza crash. Rilevante per il couch co-op, caso d'uso comune per giochi low-poly indie (sezione 1).

### 11.7 Timing nel frame

Coerente con la struttura a fasi già stabilita per la fisica (sezione 9): l'input viene letto **una volta all'inizio del frame**, prima della Fase Script — tutti gli script nello stesso frame vedono lo stesso identico stato di input.

```
Frame:
 ├─ Poll Input (tastiera/mouse dal backend video attivo + gamepad via SDL_GameController)
 ├─ Fase Script (OnUpdate) — legge Input::GetAction/GetAxis2D, stato coerente per tutto il frame
 ├─ ... (fisica, sezione 9)
```

### 11.8 Rumble/vibrazione

Supportato da SDL2 sui pad compatibili. Non essenziale per l'MVP — feature a bassa priorità da confermare in fase di analisi, non impatta l'architettura sopra.

---

## 12. Asset Pipeline

### 12.1 Principio guida: Source Assets vs Cooked Assets

Due mondi separati:

- **Source Assets** — quello che sviluppatore/artista produce e mette sotto controllo versione (glTF esportato da Blender, PNG, WAV sorgente ad alta qualità).
- **Cooked Assets** — versione ottimizzata, compressa, specifica per hardware, generata **offline** da un tool separato (il "Cooker"), mai a runtime sul Pi. Il gioco spedito carica solo i Cooked Assets — nessuna libreria di conversione/decodifica pesante (encoder ETC2, parser glTF completo, ecc.) finisce nel binario finale.

### 12.2 Pipeline per tipo di asset

- **Mesh** — sorgente glTF/GLB. Il Cooker ottimizza l'ordine dei vertici per la cache GPU (es. via meshoptimizer), genera automaticamente i livelli di **LOD** richiesti dall'Hardware Profile System (sezione 4) decimando la mesh, impacchetta in formato binario nativo per caricamento veloce (niente parsing JSON a runtime).
- **Texture** — sorgente PNG/TGA non compresso. Il Cooker comprime **per profilo hardware**: ETC2 per Pi4/Pi5 (sezione 8), formato più adatto (es. BC7) per il profilo Desktop (sezione 7) — due varianti cooked della stessa sorgente, il Resource Manager carica quella giusta in base al profilo attivo. Mipmap generate offline.
- **Audio** — coerente con sezione 10: SFX brevi restano WAV (zero decode a runtime), musica/suoni lunghi transcodificati in Ogg Vorbis a bitrate target durante il cook.
- **Shader** — sorgente GLSL, compilato offline in **SPIR-V** (via glslang/shaderc). Il Cooker compila solo le varianti rilevanti per la pipeline di rendering scelta al momento della creazione del progetto (Low-Poly Retro o PBR, sezione 8) — un progetto low-poly non si porta dietro shader PBR mai usati.
- **Collisioni fisiche** — forme di collisione (hull convessi, mesh semplificate per Jolt, sezione 9) generate al cook time dalla mesh sorgente, con i tool già offerti da Jolt, non calcolate a ogni avvio.
- **IBL / illuminazione precalcolata** (solo profilo PBR, sezione 8.4) e **ombre baked** (profilo Low-Poly, sezione 8.3) — bake offline, mai in tempo reale.
- **Scene e Prefab** (sezione 13) — durante lo sviluppo salvate in formato testuale leggibile (JSON/YAML), non binario: git-friendly, diffabile e mergeabile in team. Il Cooker le converte in binario ottimizzato solo nella build finale.

### 12.3 Asset GUID

Stesso principio già usato per gli script (sezione 6: riferimento per nome classe, non per path), esteso a tutti gli asset: ogni asset sorgente riceve un **GUID stabile** al primo import, salvato in un file sidecar (es. `player_mesh.gltf.meta`) accanto al sorgente. Scene, materiali, componenti e Prefab referenziano asset per GUID, non per percorso file — rinominare/spostare una cartella nell'Asset Browser non rompe nulla.

### 12.4 Architettura del Cooker

- **Tool CLI separato dal runtime dell'engine** — condivide codice dove sensato (es. libreria matematica), ma è un eseguibile a sé: le librerie pesanti di conversione non finiscono mai nel gioco spedito.
- **Cook incrementale**: hash/timestamp per asset sorgente, si ricompila solo ciò che è cambiato — stesso principio della build C++ incrementale (sezione 6.4).
- **Output organizzato per profilo hardware**: cache cooked separata per `pi4/`, `pi5/`, `desktop/` — coerente con la sezione 7.
- **Integrazione nel bottone "Play"** (sezione 6.4): il cook incrementale degli asset modificati è parte dello stesso flusso della ricompilazione C++ — premendo Play, l'Editor ricompila codice e ricuoce asset cambiati insieme, prima di lanciare.

---

## 13. Prefab

### 13.1 Cosa è, tecnicamente

Un Prefab è semplicemente **un asset con GUID** (sezione 12.3) il cui contenuto è un frammento di scena (un'entity e i suoi figli) — non un concetto a parte da costruire da zero. Riusa lo stesso formato di serializzazione delle scene (testuale, JSON/YAML, sezione 12.2): un albero di entity/componenti salvato come file `.prefab` con GUID stabile.

```cpp
class SpawnerScript : public ScriptComponent {
public:
    void OnUpdate(float dt) override {
        if (shouldSpawn) {
            Entity e = Prefab::Instantiate(enemyPrefab, spawnPosition, spawnRotation);
            shouldSpawn = false;
        }
    }
    EXPOSE(enemyPrefab) PrefabRef enemyPrefab;   // trascinato dall'Asset Browser nell'Inspector
    glm::vec3 spawnPosition;
    glm::quat spawnRotation;
};
```

### 13.2 Riferimenti interni all'albero: ID locali rimappati

Uno script dentro un Prefab potrebbe voler referenziare un figlio specifico (es. il punto di spawn dei proiettili su un'entity Player). Gli ID delle entity sono generati a runtime dall'ECS — non fissabili nel file. Soluzione standard (Unity, Godot): all'interno del Prefab si usano **ID locali stabili** (assegnati al salvataggio, validi solo dentro quel file); all'istanziazione (`Prefab::Instantiate`) l'engine li **rimappa** a nuovi ID reali dell'ECS, ricostruendo i riferimenti interni coerentemente — trasparente per lo script (`GetChild("MuzzlePoint")` funziona normalmente).

### 13.3 Nested Prefab

Arriva "gratis" dall'architettura a GUID: poiché un Prefab è un asset come un altro, referenziabile per GUID, **un Prefab può contenere un altro Prefab come figlio** semplicemente referenziandolo — nessun caso speciale da scrivere, stesso meccanismo già previsto per mesh/texture/script. Utile per composizione (es. Prefab "Auto" che contiene 4 Prefab "Ruota" come figli).

### 13.4 Scope v1: sincronizzazione one-way

Il sistema di **override annidati** di Unity moderno (modifichi un'istanza, il prefab si aggiorna per quella proprietà, propagando alle altre istanze tranne dove sovrascritte) è complesso e va costruito con cura — **escluso dalla v1**, coerente con l'approccio già usato per Editor (sezione 6) e networking (nota di scope in sezione 15). Per la prima versione:

- `Prefab::Instantiate` clona l'albero: da quel momento l'istanza è un'entity normale, indipendente.
- Nell'Editor, azioni esplicite **"Aggiorna Prefab da questa istanza"** (one-way, sovrascrive il file `.prefab` con lo stato dell'istanza selezionata) e **"Ripristina istanza da Prefab"** (rilegge il file, sovrascrive l'istanza) — coprono il caso d'uso pratico principale senza la complessità di un sistema di override diff-based.
- Le istanze di prefab nella Scene View sono marcate visivamente (icona/colore diverso nella gerarchia) per essere distinguibili dalle entity normali.

### 13.5 Integrazione con l'Asset Pipeline

Coerente con 12.2: durante lo sviluppo il `.prefab` resta testuale (git-friendly), il Cooker lo converte in binario ottimizzato solo nella build finale — stesso trattamento delle scene.

---

## 14. Vincoli di design da rispettare (checklist per ogni feature futura)

Ogni nuova feature dell'engine, in fase di sviluppo, dovrà rispondere a queste domande:

- [ ] Riduce o aumenta il numero di draw call?
- [ ] Riduce o aumenta l'overdraw?
- [ ] È compatibile con l'architettura TBDR (evita readback framebuffer non necessari)?
- [ ] Rispetta il budget di banda di memoria del profilo hardware attivo?
- [ ] Può essere parallelizzata sul job system, o è forzatamente single-thread?
- [ ] Scala correttamente tra profilo Pi4 e profilo Pi5 senza retuning manuale?

---

## 15. Prossimi passi (fasi successive del progetto)

**Nota di scope — Networking:** esplicitamente escluso dalla progettazione v1. Il target primario (single-player / multiplayer locale via gamepad, sezione 11) non lo richiede, e un vero sistema di netcode è un progetto a sé (come l'Editor, sezione 6) che merita una fase di progettazione dedicata se/quando diventa una priorità. Le scelte architetturali già fatte (ECS data-oriented sezione 4, `ComponentHandle<T>` sezione 6, frame a fasi/barriere sezioni 9/11) non chiudono questa porta per il futuro.

1. **Analisi** — definire più nel dettaglio: feature set del renderer per la v1 (cosa è "must have" per un MVP low-poly), scelta librerie di supporto (fisica, audio, loader asset, ImGui per tool), formato asset pipeline.
2. **Analisi tecnica in preparazione a Claude Code** — spec tecniche dettagliate: struttura repository, convenzioni di codice, interfacce dei moduli principali, milestone incrementali testabili.
3. **Contesto per Claude Code** — documento di contesto sintetico (architettura + vincoli + convenzioni) da fornire come riferimento durante lo sviluppo assistito.
4. **Sviluppo su Claude Code** — implementazione incrementale a partire da un MVP (triangolo renderizzato → mesh statica → scena con culling → ECS minimo → primo demo giocabile).
5. **Test e miglioramenti** — profiling reale su hardware Pi4 fisico, benchmark GPU/CPU/banda memoria, iterazione sui budget del profilo hardware.
