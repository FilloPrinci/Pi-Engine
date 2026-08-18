# Contesto Progetto — Engine 3D Open Source per Raspberry Pi 4/5

> Questo documento condensa `docs/01-progettazione-engine-3d-rpi.md`, `docs/02-analisi-mvp-roadmap.md` e `docs/03-analisi-tecnica-claude-code.md`. È tenuto come riferimento costante durante lo sviluppo. Per il "perché" dietro ogni scelta, i tre documenti completi in `docs/` restano la fonte di verità.

---

## 1. Cos'è il progetto

Engine 3D **open source**, **accessibile** (documentazione chiara, curva di apprendimento ragionevole per sviluppatori indie), **ottimizzato per Raspberry Pi 4** con compatibilità forward Pi5. Target primario: giochi 3D low-poly stile retro. Target secondario opzionale: 3D più realistico (profilo PBR).

## 2. Stack tecnologico

| | |
|---|---|
| Linguaggio | **C++20**, nessuna estensione compilatore non portabile |
| API grafica | **Vulkan 1.2 core** baseline (driver V3DV), feature detection per estensioni 1.3 |
| Build | CMake + preset (`pi4`, `pi5`, `windows`, `linux-pc`) |
| Dipendenze | vcpkg manifest mode (`vcpkg.json`) |
| Piattaforma di sviluppo primaria | Linux x86_64 desktop, verifica su Pi4 fisico ad ogni milestone |

## 3. Hardware target — vincoli da rispettare sempre

- **Pi4** (target primario): Cortex-A72 quad-core no SMT, 1MB L2 condivisa, GPU VideoCore VI TBDR ~4.4 GFLOPS, banda memoria ~13GB/s condivisa CPU/GPU, nessuna VRAM dedicata.
- **Pi5** (compatibilità forward): Cortex-A76, GPU VideoCore VII TBDR, ~2-4× più potente.
- **La GPU è il collo di bottiglia, non la CPU** — spostare lavoro su CPU (culling, batching, LOD) quando possibile.
- **GPU TBDR** (tile-based deferred, come mobile) — evitare readback framebuffer, minimizzare cambi render target, sfruttare l'Hidden Surface Removal nativo.
- **Banda memoria è la seconda risorsa più scarsa** — texture compresse obbligatorie, overdraw da ridurre attivamente.

**Checklist da applicare a ogni nuova feature:**
- [ ] Riduce o aumenta i draw call?
- [ ] Riduce o aumenta l'overdraw?
- [ ] Compatibile con TBDR (no readback non necessari)?
- [ ] Rispetta il budget di banda del profilo hardware attivo?
- [ ] Parallelizzabile sul Job System, o forzatamente single-thread?
- [ ] Scala tra profilo Pi4/Pi5/Desktop senza retuning manuale?

## 4. Architettura — moduli e responsabilità

```
Application/GameLoop (orchestratore fasi a barriere)
├── Platform Layer — IDisplayBackend (SDL2DisplayBackend | DirectDRMDisplayBackend)
├── RHI — wrapper sottile Vulkan (volk + VMA)
├── ECS — data-oriented, array contigui per componente
├── Job System — work-stealing, condiviso da culling/fisica/animazione
├── Renderer — culling, batching, LOD, 2 pipeline (ForwardLitPipeline | ForwardPlusPBRPipeline)
├── Script System — ScriptComponent, ComponentHandle<T>, REGISTER_SCRIPT/EXPOSE
├── Physics — Jolt Physics via adapter nel Job System
├── Audio — miniaudio, thread dedicato FUORI dal Job System
└── Resource Manager — budget memoria espliciti per profilo hardware
```

**Regola architetturale centrale — fasi a barriere nel frame** (mai violarla):
```
Poll Input → Fase Script (OnUpdate) → barriera → Fase Fisica (parallela) → barriera
→ Fase Callback Collisioni (single-thread) → Post-Fisica/Render
```
Nessuno script legge/scrive dati fisici mentre il solver ci lavora — garantito dalla barriera, mai da disciplina del programmatore.

**`ComponentHandle<T>`**: mai puntatori raw permanenti verso componenti ECS — i dati possono spostarsi in memoria tra frame. Sempre l'handle che si risolve a ogni accesso.

## 5. Convenzioni di codice

- **Lingua: SEMPRE inglese** — codice, commenti, log, messaggi di errore, testo Editor. Nessuna eccezione. (Solo i documenti di progettazione/analisi in `docs/` restano in italiano.)
- Classi/tipi: `PascalCase`. Metodi pubblici: `PascalCase` (`OnUpdate`, `GetComponent`). Membri privati: `m_camelCase`. File: `snake_case`.
- Namespace: `engine::<modulo>` (`engine::rhi`, `engine::ecs`, `engine::jobs`, `engine::renderer`, `engine::script`, `engine::physics`, `engine::platform`).
- **Niente eccezioni C++ nel codice hot-path** (renderer, fisica, job system) — return code/bool + out param, macro `ENGINE_ASSERT`. Eccezioni ammesse solo nel Cooker/tooling offline.
- `std::` standard per l'MVP M0-M5 — niente allocator custom prematuri.
- `.clang-format` unico al root (LLVM base, 4 spazi, colonna 100).

## 6. Struttura repository

```
Pi-Engine/
├── CMakeLists.txt / CMakePresets.json / vcpkg.json
├── .clang-format / .clang-tidy / .gitignore
├── cmake/
│   ├── toolchains/aarch64-linux-gnu.cmake
│   └── CompilerWarnings.cmake
├── engine/
│   ├── include/engine/{core,platform,rhi,ecs,jobs,renderer,script,physics}/
│   └── src/                    # stessa struttura di include/
├── tests/                      # doctest — ECS, math, Job System
├── samples/
│   ├── m0_hello_vulkan/  m1_hello_mesh/  m2_hello_scene/
│   ├── m3_hello_script/  m4_hello_physics/  m5_vertical_slice/
├── assets/                     # grezzi, no Cooker in M0-M5
├── shaders/                    # GLSL → SPIR-V a build time
└── docs/                       # documenti di progettazione/analisi
```

## 7. Dipendenze pinnate (vcpkg manifest)

| Libreria | Ruolo | Integrazione |
|---|---|---|
| volk | Meta-loader Vulkan | vcpkg |
| VMA | Allocatore memoria GPU | vcpkg (header-only) |
| SDL2 | Platform Layer + gamepad | vcpkg |
| GLM | Matematica | vcpkg (header-only) |
| cgltf | Loader glTF | vendored (single header) |
| Jolt Physics | Fisica | vcpkg |
| miniaudio | Audio | vendored (single header) |
| Dear ImGui | Debug overlay | vcpkg |
| doctest | Unit test | vcpkg |

## 8. Roadmap milestone (stato attuale del progetto)

Obiettivo primo traguardo: **vertical slice minimo** — muovi un cubo con tastiera, salta (impulso fisico), tocchi un oggetto e uno script reagisce a una collisione. Tutta la pipeline (rendering→fisica→script→input) insieme, non feature isolate.

| # | Milestone | Criterio di uscita | Stato |
|---|---|---|---|
| M0 | Hello Vulkan — triangolo a schermo | RHI init, swapchain, pipeline compila, gira su Pi4 | ⬜ Scaffold pronto, implementazione da iniziare |
| M1 | Hello Mesh — cubo da glTF, camera orbit | Loader glTF minimo, pipeline unlit attiva | ⬜ |
| M2 | Hello Scene — culling attivo | ECS minimo (Transform+Mesh), Job System in uso reale | ⬜ |
| M3 | Hello Script — oggetto si muove via tastiera | ScriptComponent/ComponentHandle/REGISTER_SCRIPT funzionanti | ⬜ |
| M4 | Hello Physics — cubo cade e si ferma | Adapter Jolt↔JobSystem, barriere rispettate, timestep fisso | ⬜ |
| M5 | **Vertical Slice** — salto + collisione via script | Tutto insieme, stesso frame, nessuna race condition | ⬜ |

**Decisioni preliminari per non bloccarsi:**
- Editor: fuori da questa roadmap, si costruisce dopo che il core è stabile.
- Asset Cooker: rimandato a dopo M5 — asset "grezzi" caricati a runtime nelle milestone M0-M5.
- Solo `SDL2DisplayBackend` nelle milestone M0-M5 — `DirectDRMDisplayBackend` arriva più avanti, non blocca il vertical slice.

**Esplicitamente fuori scope per M0-M5** (già progettati, ma dopo): Audio, gamepad, LOD, bloom/post-processing, profilo PBR, Prefab, Asset Pipeline/Cooker completo, Editor, Networking.

Per l'elenco esatto dei file/classi da creare in ciascuna milestone: `docs/03-analisi-tecnica-claude-code.md` (sezioni 5-10).

## 9. Regole che non vanno mai violate

1. Nessuno script accede a dati fisici fuori dalle fasi previste (sez. 4) — la barriera è strutturale, non convenzionale.
2. Nessun secondo thread pool indipendente dal Job System (Jolt va iniettato via adapter, non gira per conto suo).
3. L'audio non condivide mai il Job System — thread dedicato, sempre, per evitare buffer underrun.
4. Mai puntatori raw permanenti a componenti ECS — sempre `ComponentHandle<T>`.
5. Mai eccezioni nel codice hot-path engine (renderer/fisica/job system).
6. Mai testo non-inglese in codice, commenti, log, errori.
7. Ogni pipeline di rendering è una classe concreta separata (`ForwardLitPipeline`/`ForwardPlusPBRPipeline`) — mai un uber-shader con branching.
8. `DirectDRMDisplayBackend` è Linux-only, esclusa a compile-time su build Windows.

## 10. Per approfondire

- **Perché** ogni scelta architetturale (hardware, rendering TBDR, fisica, audio, input, Editor/Script System, Asset Pipeline, Prefab): `docs/01-progettazione-engine-3d-rpi.md`
- **Roadmap completa** e cosa è escluso dal vertical slice: `docs/02-analisi-mvp-roadmap.md`
- **File/classi esatti** da creare per ogni milestone, motivazione dipendenze: `docs/03-analisi-tecnica-claude-code.md`
