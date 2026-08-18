# Analisi Tecnica — Struttura, Convenzioni e Interfacce per Claude Code

**Fase:** Analisi tecnica (preparazione a Claude Code)
**Stato:** Bozza v1
**Documenti di riferimento:** `01-progettazione-engine-3d-rpi.md` (decisioni architetturali), `02-analisi-mvp-roadmap.md` (milestone M0-M5)

---

## 1. Obiettivo di questa fase

Rendere ogni milestone M0-M5 **eseguibile senza ambiguità**: struttura repository definitiva, convenzioni di codice, dipendenze esterne con motivazione e metodo di integrazione, e — per ciascuna milestone — i file/classi/interfacce esatti da creare. Questo documento è l'input diretto per il "Contesto per Claude Code" (fase successiva).

---

## 2. Struttura del repository

```
engine-repo/
├── CMakeLists.txt                  # top-level, aggrega i sottoprogetti
├── CMakePresets.json                # preset pi4 / pi5 / windows / linux-pc (progettazione sez. 7.3)
├── vcpkg.json                       # manifest dipendenze (sez. 3 di questo documento)
├── .clang-format                    # stile di formattazione unico
├── .clang-tidy                      # regole di lint statiche
├── cmake/
│   ├── toolchains/
│   │   └── aarch64-linux-gnu.cmake  # cross-compilazione da x86_64 verso Pi4/Pi5
│   └── CompilerWarnings.cmake       # warning level unico per tutti i target
├── engine/                          # Engine Core — libreria, target `engine_core`
│   ├── include/engine/               # header pubblici, un sottodirectory per modulo
│   │   ├── core/                     # logging, assert, math wrapper, Application/GameLoop
│   │   ├── platform/                  # IDisplayBackend, SDL2Backend, DirectDRMBackend, Input
│   │   ├── rhi/                       # wrapper Vulkan sottile
│   │   ├── ecs/                       # Entity, World, ComponentHandle
│   │   ├── jobs/                      # JobSystem
│   │   ├── renderer/                  # pipeline, culling, mesh loading
│   │   ├── script/                    # ScriptComponent, REGISTER_SCRIPT, EXPOSE
│   │   └── physics/                   # PhysicsWorld, adapter Jolt
│   └── src/                          # implementazione, stessa struttura di include/
├── tests/                            # unit test (ECS, math, Job System) — doctest
├── samples/                          # un eseguibile per milestone
│   ├── m0_hello_vulkan/
│   ├── m1_hello_mesh/
│   ├── m2_hello_scene/
│   ├── m3_hello_script/
│   ├── m4_hello_physics/
│   └── m5_vertical_slice/
├── assets/                           # asset sorgente grezzi usati dai sample (M0-M5, no Cooker ancora)
├── shaders/                          # sorgenti GLSL, compilati in SPIR-V a build time per ora
└── docs/                             # questo documento + 01/02, mantenuti nel repo
```

**Nota sui sample**: ogni milestone è un eseguibile a sé in `samples/`, non un ramo Git separato — così M0 resta compilabile e verificabile anche dopo aver costruito M5, utile per non "perdere" i criteri di uscita già raggiunti (regressioni catturate subito).

---

## 3. Gestione dipendenze: vcpkg in modalità manifest

Decisione (il documento di progettazione la lasciava aperta, sez. 3.3): **vcpkg in modalità manifest** (`vcpkg.json` al root, integrato via `CMAKE_TOOLCHAIN_FILE`).

Motivazione:
- Le due dipendenze più delicate del progetto — **SDL2** e **Jolt Physics** — sono entrambe pacchetti vcpkg maturi con supporto alla cross-compilazione verso `arm64-linux`, verificato prima di fissare questa scelta.
- Manifest mode fissa le versioni esatte nel repository (`vcpkg.json` + `vcpkg-configuration.json`) — build riproducibili tra sviluppatori diversi e tra le piattaforme della sezione 7 del documento di progettazione, senza gestione manuale di sottomoduli Git per ogni libreria.
- Si integra direttamente con i preset CMake già decisi (`cmake --preset pi4` ecc.) tramite triplet vcpkg dedicati (`arm64-linux`, `x64-linux`, `x64-windows`).

**Fallback esplicito**: se in fase di sviluppo emergessero problemi di cross-compilazione ARM64 con una specifica libreria via vcpkg (storicamente capitato con alcuni pacchetti SDL correlati, non SDL2 core), quella singola dipendenza passa a sottomodulo Git compilato direttamente nell'albero CMake — non è un fallimento dell'intera strategia, solo un'eccezione puntuale da gestire caso per caso.

### 3.1 Dipendenze esterne pinnate

| Libreria | Ruolo | Come si integra |
|---|---|---|
| **volk** | Meta-loader Vulkan — carica i puntatori a funzione senza link diretto contro `vulkan-1`, abilita il feature detection 1.2/1.3 (progettazione sez. 3.2) | vcpkg |
| **Vulkan Memory Allocator (VMA)** | Allocatore memoria GPU — sub-allocazione, tracking budget esplicito (progettazione sez. 2.3, punto 4) | vcpkg (header-only) |
| **SDL2** | Platform Layer, backend a finestra + gamepad (progettazione sez. 5, 11) | vcpkg |
| **GLM** | Matematica (vettori, matrici, quaternioni) — stessa API usata negli esempi di scripting del documento di progettazione | vcpkg (header-only) |
| **cgltf** | Loader glTF — single-header C, nessuna dipendenza JSON pesante (a differenza di tinygltf), coerente con la filosofia "dipendenze minime" già seguita per miniaudio | vendored (singolo header, non serve vcpkg) |
| **Jolt Physics** | Fisica (progettazione sez. 9) | vcpkg |
| **miniaudio** | Audio (progettazione sez. 10) — non serve prima della milestone audio post-vertical-slice, ma pinnata da subito | vendored (singolo header) |
| **Dear ImGui** | Overlay di debug (FPS, stato ECS, log) — utile già da M2 per ispezionare frustum culling e Job System, precursore leggero dell'Editor (sez. 6) | vcpkg |
| **doctest** | Unit test (ECS, math, Job System) | vcpkg |

---

## 4. Convenzioni di codice

- **Standard**: C++20, nessuna estensione compilatore non portabile (coerente col target multi-piattaforma, sez. 7).
- **Naming**:
  - Classi/tipi: `PascalCase` (`JobSystem`, `IDisplayBackend`, `ComponentHandle<T>`).
  - Metodi pubblici: `PascalCase` (`OnUpdate`, `GetComponent`, `AddImpulse`) — coerente con tutti gli esempi di scripting già mostrati nel documento di progettazione.
  - Membri privati: `m_camelCase` (`m_position`, `m_workerThreads`).
  - File: `snake_case` (`job_system.h` / `job_system.cpp`) — evita problemi di case-sensitivity tra Linux e Windows.
  - Namespace: `engine::<modulo>` (`engine::rhi`, `engine::ecs`, `engine::jobs`, `engine::renderer`, `engine::script`, `engine::physics`, `engine::platform`).
- **Formattazione**: un `.clang-format` unico al root del repo (stile base LLVM, indentazione 4 spazi, colonna 100) — applicato automaticamente, nessuna discussione di stile lasciata al singolo commit.
- **Gestione errori**: **niente eccezioni C++ nel codice hot-path dell'Engine Core** (renderer, fisica, job system) — coerente con l'obiettivo di prestazioni prevedibili su hardware limitato (progettazione sez. 2.3). Si usano codici di ritorno / `bool` + parametro di output, e un macro `ENGINE_ASSERT` per errori da programmatore (build debug: abort con stack trace; build release: no-op o log). Le eccezioni restano ammesse nel Cooker/tooling offline, dove le prestazioni non sono critiche.
- **Contenitori**: `std::` standard per l'MVP (M0-M5) — allocator custom e pool di memoria sono un'ottimizzazione da introdurre **dopo** che il vertical slice gira, non prima (evita di ottimizzare prematuramente codice che potrebbe ancora cambiare forma).
- **Lingua**: **inglese ovunque nel codice e in tutto ciò che è user/developer-facing** — codice, commenti, messaggi di log, testi di errore (`ENGINE_ASSERT`, log del Job System/RHI/Cooker), e in futuro tutto il testo dell'interfaccia dell'Editor (menu, tooltip, Console). Nessuna eccezione, coerente con l'obiettivo di un progetto open source accessibile a una platea internazionale, non solo italiana (sez. 1 progettazione). Fanno eccezione solo i documenti di progettazione/analisi (questi tre documenti), che restano in italiano in quanto materiale di processo interno, non parte del prodotto distribuito.

---

## 5. Milestone M0 — *Hello Vulkan*

**Obiettivo (da sez. 2, doc. 02):** triangolo colorato a schermo, RHI si inizializza, swapchain funziona, gira su Pi4.

**File da creare:**
- `engine/include/engine/platform/IDisplayBackend.h` — interfaccia già definita in progettazione sez. 5.2.
- `engine/include/engine/platform/SDL2DisplayBackend.h` + `.cpp` — unica implementazione attiva in M0 (`DirectDRMDisplayBackend` arriva più avanti, non blocca il vertical slice).
- `engine/include/engine/rhi/RHIContext.h` + `.cpp` — istanza Vulkan (via volk), selezione physical device, logical device, code, swapchain. Vulkan 1.2 core baseline (progettazione sez. 3.2).
- `engine/include/engine/rhi/RHISwapchain.h` + `.cpp`.
- `samples/m0_hello_vulkan/main.cpp` — crea finestra, inizializza RHI, un pipeline grafico minimo (vertex/fragment shader inline, nessun asset esterno), clear color + triangolo, loop di presentazione.
- `shaders/m0_triangle.vert` / `.frag` — sorgenti GLSL compilate in SPIR-V a build time (target CMake custom, non ancora il Cooker).

**Criterio di uscita:** invariato da doc. 02.

---

## 6. Milestone M1 — *Hello Mesh*

**Obiettivo:** cubo/mesh statica da glTF, camera che orbita, pipeline Low-Poly Retro unlit attiva.

**File da creare:**
- `engine/include/engine/rhi/RHIBuffer.h` + `.cpp` — wrapper buffer GPU (vertex/index) via VMA.
- `engine/include/engine/rhi/RHIPipeline.h` + `.cpp` — wrapper creazione pipeline grafica Vulkan (non ancora l'RHI astratto multi-backend completo, solo il sottile layer deciso in progettazione sez. 4).
- `engine/include/engine/renderer/MeshLoader.h` + `.cpp` — carica glTF via cgltf, produce buffer vertex/index grezzi (nessuna ottimizzazione cache-vertex: quella è lavoro del Cooker, non del runtime, coerente con sez. 12 progettazione).
- `engine/include/engine/renderer/ForwardLitPipeline.h` + `.cpp` — solo variante *unlit* in M1 (l'illuminazione arriva quando serve, non blocca questa milestone).
- `engine/include/engine/core/Camera.h` — matematica camera (view/projection), orbit camera per il sample.
- `samples/m1_hello_mesh/main.cpp`.
- `assets/m1_cube.glb` — asset sorgente di test.

---

## 7. Milestone M2 — *Hello Scene*

**Obiettivo:** una manciata di oggetti, frustum culling attivo, primo uso reale del Job System.

**File da creare:**
- `engine/include/engine/ecs/Entity.h` — handle leggero (indice + generazione, per invalidare riferimenti a entity distrutte).
- `engine/include/engine/ecs/World.h` + `.cpp` — storage componenti data-oriented (array contigui per tipo, coerente con progettazione sez. 2.3/4).
- `engine/include/engine/ecs/components/TransformComponent.h`, `MeshComponent.h`.
- `engine/include/engine/jobs/JobSystem.h` + `.cpp` — task graph con work-stealing, worker thread pari a `numero core - 1` (progettazione sez. 9.3 stima iniziale, da validare su hardware reale).
- `engine/include/engine/renderer/FrustumCuller.h` + `.cpp` — primo sistema che sottomette job reali al Job System (culling parallelo su tutte le entity con `MeshComponent`).
- `engine/include/engine/core/Application.h` + `.cpp` — **nuovo modulo esplicito**: l'orchestratore del ciclo di frame (non nominato a parte nel documento di progettazione, ma implicito nelle fasi a barriere già descritte per fisica/input). Da qui in poi ogni milestone aggiunge una fase al suo loop.
- `samples/m2_hello_scene/main.cpp`.

---

## 8. Milestone M3 — *Hello Script*

**Obiettivo:** un oggetto si muove via tastiera, Script System minimo.

**File da creare:**
- `engine/include/engine/script/ScriptComponent.h` — classe base (`OnStart`, `OnUpdate`, `OnDestroy`), come da progettazione sez. 6.2.
- `engine/include/engine/script/ComponentHandle.h` — handle sicuro verso riarrangiamenti ECS (progettazione sez. 6.2).
- `engine/include/engine/script/ScriptRegistry.h` + `.cpp` — factory + macro `REGISTER_SCRIPT` (progettazione sez. 6.2).
- `engine/include/engine/script/Expose.h` — macro `EXPOSE` (progettazione sez. 6.2), solo per campi `float`/`int`/`bool`/`glm::vec3` in questa milestone (i tipi asset-reference, es. `PrefabRef`, arrivano con l'Asset Pipeline).
- `engine/include/engine/platform/InputState.h`, `InputSystem.h` — solo tastiera in M3 (gamepad via `SDL_GameController` è un'estensione successiva, non blocca).
- `samples/m3_hello_script/scripts/MoveScript.h` — primo script scritto "come uno sviluppatore lo scriverebbe".
- `samples/m3_hello_script/main.cpp` — aggiunge la Fase Script al loop di `Application` (poll input → `OnUpdate` di tutti gli script attivi).

---

## 9. Milestone M4 — *Hello Physics*

**Obiettivo:** il cubo cade per gravità e si ferma su un piano, adapter Jolt↔Job System funzionante.

**File da creare:**
- `engine/include/engine/physics/PhysicsWorld.h` + `.cpp` — wrapper `JPH::PhysicsSystem`.
- `engine/include/engine/physics/JoltJobSystemAdapter.h` + `.cpp` — implementa l'interfaccia `JPH::JobSystem` di Jolt iniettando i suoi job nel nostro `JobSystem` (progettazione sez. 9.3) — pezzo tecnicamente più delicato di questa milestone.
- `engine/include/engine/ecs/components/RigidbodyComponent.h`, `ColliderComponent.h`.
- `engine/include/engine/physics/PhysicsPhase.h` + `.cpp` — timestep fisso (accumulator pattern, progettazione sez. 9.4), orchestrazione barriere: viene agganciato come nuova fase in `Application` **tra** Script e Post-Fisica.
- `samples/m4_hello_physics/main.cpp`.

---

## 10. Milestone M5 — *Vertical Slice* (target)

**Obiettivo:** tutto insieme — muovi il cubo, salta, tocchi un oggetto e uno script reagisce.

**File da creare:**
- `engine/include/engine/physics/CollisionCallbackDispatcher.h` + `.cpp` — buffer lock-free per-thread durante il solver, poi dispatch single-thread di `OnCollisionEnter/Stay/Exit` (progettazione sez. 9.6) — implementa la Fase Callback Collisioni in `Application`.
- Estensione di `ScriptComponent.h` con `OnCollisionEnter/Stay/Exit`, `OnTriggerEnter/Exit`.
- `engine/include/engine/physics/Raycast.h` — `Physics::Raycast`/`Physics::OverlapSphere` (progettazione sez. 9.6).
- `samples/m5_vertical_slice/scripts/PlayerScript.h` (input + impulso salto + raycast a terra) e `TargetScript.h` (reagisce a `OnCollisionEnter`).
- `samples/m5_vertical_slice/main.cpp` — loop di frame completo: `Poll Input → Fase Script → barriera → Fase Fisica → barriera → Fase Callback Collisioni → Post-Fisica/Render`, esattamente come descritto in progettazione sez. 9.4.

**A questo punto** `Application`/`GameLoop` implementa l'intero schema a fasi discusso in tutta la fase di progettazione — è il modulo che "chiude il cerchio" tra tutti gli altri.

---

## 11. Testing

- **Unit test** (`tests/`, doctest): matematica (camera, trasformazioni), `World`/ECS (creazione/distruzione entity, validità `ComponentHandle` dopo riarrangiamenti), `JobSystem` (corretta esecuzione/attesa di un grafo di task semplice) — testabili su desktop x86_64, non richiedono hardware Pi.
- **Verifica su hardware reale**: ad ogni milestone, non solo a fine roadmap (coerente con doc. 02, sez. 3) — non sono unit test automatizzati in questa fase, ma un passaggio manuale di validazione (compila ed esegue su Pi4 fisico, criterio di uscita rispettato).

---

## 12. Prossimi passi

1. **Contesto per Claude Code** — documento sintetico che riassume architettura (da doc. 01), roadmap (da doc. 02) e questo documento tecnico, in un formato pensato per essere fornito come riferimento costante durante lo sviluppo assistito.
2. **Sviluppo su Claude Code** — implementazione milestone per milestone, M0 → M5, ognuna con criterio di uscita verificato prima di passare alla successiva.
