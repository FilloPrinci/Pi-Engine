# Analisi — Roadmap MVP e Milestone verso il Vertical Slice

**Fase:** Analisi
**Stato:** Bozza v1
**Documento di riferimento:** `01-progettazione-engine-3d-rpi.md` (tutte le decisioni architetturali citate qui sono definite e motivate lì)

---

## 1. Obiettivo di questa fase

Tradurre le decisioni architetturali della fase di progettazione in un **feature set concreto per l'MVP** e una **roadmap di milestone incrementali testabili** — il ponte tra "abbiamo deciso come deve essere fatto" e "cosa costruiamo per primo, in che ordine".

## 2. Target della primissima milestone giocabile

Non l'MVP finale, ma il primo traguardo concreto: un **vertical slice minimo** — l'intera pipeline (rendering → fisica → script → input) funzionante insieme, anche su un singolo cubo, invece di tante feature superficiali isolate. Valida l'intera architettura decisa in progettazione, non solo un pezzo alla volta.

Esempio concreto di cosa deve succedere: *muovi un cubo con la tastiera, premi Spazio per saltare (impulso fisico reale), tocchi un altro oggetto e succede qualcosa (evento di collisione gestito da uno script).*

---

## 3. Decisioni preliminari per non bloccarsi

- **Editor: fuori da questa roadmap.** Tutte le milestone sotto sono **solo codice** (modalità "libreria", sezione 6 del documento di progettazione) — l'Editor si costruisce dopo che il core è stabile e testato, non in parallelo.
- **Asset Cooker: rimandato dopo il primo vertical slice.** Per arrivare in fretta alla milestone target, le prime milestone caricano asset "grezzi" a runtime (glTF non cookato, WAV diretto). Il Cooker offline (sezione 12 del documento di progettazione) è una milestone a sé, subito dopo il vertical slice, prima di scalare a scene più grandi.
- **Piattaforma di sviluppo primaria: Linux x86_64 desktop**, per iterazione veloce (compile time, RenderDoc, validation layer Vulkan più maturi lì), con **verifica su Pi4 fisico ad ogni milestone**, non solo alla fine — per intercettare presto eventuali problemi specifici ARM/TBDR (coerente con la checklist "Vincoli di design" del documento di progettazione).

---

## 4. Roadmap di milestone verso il vertical slice

| # | Milestone | Cosa si vede/valida | Sistemi coinvolti | Criterio di uscita |
|---|---|---|---|---|
| **M0** | *Hello Vulkan* | Triangolo colorato a schermo | RHI, Platform Layer (solo `SDL2DisplayBackend`) | RHI si inizializza, swapchain funziona, primo pipeline Vulkan compila ed esegue, gira su Pi4 |
| **M1** | *Hello Mesh* | Cubo/mesh statica caricata da glTF, camera che orbita | RHI, Renderer (profilo Low-Poly base) | Loader glTF minimo funzionante, pipeline Low-Poly Retro (forward semplice, unlit) attiva, matematica camera corretta |
| **M2** | *Hello Scene* | Una manciata di oggetti nella scena, frustum culling attivo | ECS, Job System, Renderer | ECS minimo (Transform + Mesh component) funzionante, primo uso reale del Job System (culling parallelo su più oggetti) |
| **M3** | *Hello Script* | Un oggetto si muove via tastiera | Script System, Input System | `ScriptComponent`, `ComponentHandle<T>`, `REGISTER_SCRIPT` funzionanti; Input System (tastiera, gamepad può seguire dopo) |
| **M4** | *Hello Physics* | Il cubo cade per gravità e si ferma su un piano | Fisica, Job System | Adapter Jolt↔Job System funzionante, fasi a barriere (sezione 9 progettazione) rispettate, timestep fisso |
| **M5 — Target** | *Vertical Slice* | Muovi il cubo con tastiera, Spazio per saltare (impulso fisico), tocchi un oggetto e uno script reagisce (`OnCollisionEnter`/`OnTriggerEnter`) | Tutti i sistemi core insieme | Script legge input, applica impulsi fisici, riceve callback di collisione — tutto nello stesso frame, senza race condition |

Ogni milestone ha un criterio di uscita netto ("compila, gira, si vede/si sente la cosa descritta"). Se una milestone si allunga troppo o richiede di anticipare pezzi di quelle successive, è un segnale che va spezzata ulteriormente, non spinta avanti a forza.

---

## 5. Cosa NON entra nel vertical slice (e va bene così)

Sistemi già ben progettati in fase di progettazione, ma volutamente esclusi da questa prima roadmap:

- Audio (miniaudio, thread dedicato)
- Gamepad (Action Mapping completo, hot-plug)
- LOD, bloom/post-processing, profilo PBR
- Prefab
- Asset Pipeline / Cooker completo
- Editor

Vengono aggiunti **dopo** che il nucleo (rendering + fisica + scripting) è dimostrato solido su hardware reale. Introdurli prima rischierebbe di nascondere problemi nel nucleo dietro la complessità di feature periferiche.

---

## 6. Prossimi passi

1. **Milestone post-vertical-slice** (da dettagliare quando M5 è raggiunta): Asset Cooker minimo, gamepad, audio base, LOD.
2. **Analisi tecnica in preparazione a Claude Code** — per ciascuna milestone M0-M5: struttura repository, interfacce esatte dei moduli coinvolti, convenzioni di codice, dipendenze esterne da integrare (Vulkan SDK, SDL2, Jolt, glTF loader).
3. **Contesto per Claude Code** — documento di contesto sintetico da fornire come riferimento durante lo sviluppo assistito.
4. **Sviluppo su Claude Code** — implementazione incrementale seguendo esattamente M0 → M5.
5. **Test e miglioramenti** — profiling reale su hardware Pi4 fisico ad ogni milestone, non solo a fine roadmap.
