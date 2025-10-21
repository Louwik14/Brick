# Brick Sequencer — Full Context & Working Method (handoff+playbook)

_Last updated: 2025-10-21 (TZ: Europe/Paris)_

---

## 0) Who/What is this for?
This document lets a new engineer/AI **pick up instantly**:
- **Context** (architecture, constraints, memory budgets).
- **Where we are** (P1/P2 finished; MP17 in progress toward 16-track parallel playback).
- **How we work** (prompt cadence, micro-passes, acceptance checks).
- **Prompt templates** to continue seamlessly.

---

## 1) TL;DR
- Target: **STM32F429 (no SDRAM)**, objective: **16 tracks in parallel** with **zero RAM growth** and **RT safety**.
- Achieved: P1/P2 refactors → **Reader/Handles/Views**, **hot/cold barrier**, **no-cold-in-tick CI**, **host benches** (stress + soak + MIDI invariants + timing/queue monitors), **aggregated RT report**.
- Next concrete step: **MP17 micro-passes** enabling **XVA1 4×4 = 16 tracks** with fixed mapping **track N → MIDI channel N** by looping in the **runner** (Reader only).
- Optional after MP17: place **hot** into **CCM (64 KiB)** following a short audit. **Do not** put cold in CCM (DMA-inaccessible on F429).

---

## 2) Architecture snapshot (P1/P2)
### Public surface (apps/**)
- Apps include **one** header: `core/seq/seq_access.h`. No `seq_project.h` or `seq_model.h` in apps/** (CI guard enforces this).

### Reader (copy-only)
- **Handles** (IDs, not pointers): `seq_track_handle_t { bank, pattern, track }`.
- **Views DTO**: `seq_step_view_t` (public flags `SEQ_STEPF_*` for voice/plocks/automation/mute placeholders).
- **P-lock iterator**: open/next read-only sequence of parameter locks.
- **No model layout** exposed; views are **copies** (no address stability).

### Runtime barrier
- **Hot/cold** abstraction + **cold views** (PROJECT, CART_META, HOLD_SLOTS). Cold is **read-only** and forbidden in **TICK**.
- **Phase API** (BOOT/IDLE/TICK) + CI (grep/nm) enforce **no-cold-in-tick**.

### Budgeting & tests (host)
- **Hot snapshot & `_Static_assert`**: host-only size guard **≤ 64 KiB**.
- **Stress 16 tracks** and **Soak 10k ticks** benches: blackbox ring + **silent tick watchdog**; per-track histograms; **MIDI ON/OFF pairing** invariants; timing **p99 guard** (default 2 ms); **queue watermarks**.
- **Aggregated report**: `seq_rt_report` writes `out/host_rt_report.txt` after running both benches.

### Memory baseline (target)
- Constant throughout P1/P2: `.data ≈ 1,792 B`, `.bss ≈ 130,220 B`, `.ram4 = 0 B`.

### Why 16 tracks won’t grow RAM now
- Runner iterates **Reader handles**; **no per-track hot mirrors**; one engine, same queues → more events per tick, **same .bss/.data**.

---

## 3) Working method (loop)
We use a **repeatable loop** with **micro-passes**:

1. **You** describe the next goal / paste Codex’ latest **pass report** (success or failure).
2. **I** generate a **precise prompt** for the next micro-pass:
   - Starts with a brief **history** (“what’s already done”).
   - Names **files to read** first (e.g., `docs/ARCHITECTURE_FR.md`, `PROGRESS_P1.md`), so the agent targets correct modules.
   - States **goals** and **strict invariants** (no cold-in-tick, no alloc in TICK, RAM baseline unchanged, public-surface-only).
   - Lists **tiny, localized changes** (diff-minimal), in order.
   - Adds **tests/acceptance** and a **single expected commit message**.
3. **You** run Codex; it returns a **pass report**.
4. **I** read the report and craft the **next micro-pass prompt**, adjusting to what actually landed.

This avoids “rewrite everything” stalls, keeps diffs small, and preserves RT constraints.

### Prompt structure template
```
# 🧠 Prompt — <Phase>/<Micro-pass> — <Title>

> Historique (très court, bullets).  
> Réfs à lire (files).  
> Invariants stricts (no-cold-in-tick, no alloc, RAM == baseline, apps use seq_access.h only).

🎯 Objectif(s) (1–3 items, mesurables).

🛠️ Tâches (courtes, ordonnées, fichiers précis)  
- <file>: <change 1>  
- <file>: <change 2>  
...

🧪 Tests / Acceptation  
- Build host/cible, expected prints, expected behavior.  
- Commit attendu: "<short, consistent message>"
```

### Do / Don’t for the coding agent
- ✅ Keep diffs **short**; stick to **apps/** surfaces; use **Reader** (no model pointers).  
- ✅ If MIDI helpers lack per-channel signature, add thin shims (status `0x8n/0x9n` with `(ch-1)`).  
- ❌ No `seq_runtime_cold_view()` inside TICK.  
- ❌ No `seq_project.h`/`seq_model.h` includes in apps/**.  
- ❌ No allocations in TICK.

---

## 4) Current execution plan — MP17 micro-passes (to reach 16 tracks)

### MP17a — XVA1 capabilities (4×4) + public constants (no behavior change)
Files: `core/seq/seq_config.h`; `apps/xva1_*.c/.h`  
- Define: `XVA1_NUM_CARTRIDGES=4`, `XVA1_TRACKS_PER_CART=4`, `SEQ_MAX_ACTIVE_TRACKS=1`.  
- Expose XVA1 capabilities: 4 carts × 4 tracks → total 16.  
- Commit: `P2/MP17a: XVA1 capabilities 4×4 + constants (no behavior change)`

### MP17b — MIDI per-channel helpers + fixed mapping track→ch (no loop yet)
Files: `apps/midi_*.c/.h`; `apps/seq_engine_runner.c`  
- Add/Use `midi_send_note_on(ch, note, vel)` / `midi_send_note_off(ch, note, vel)`.  
- Map active track index to channel `(index % 16) + 1`.  
- Commit: `P2/MP17b: MIDI per-channel helpers and fixed track→channel mapping`

### MP17c — Runner loop ⇒ 4 tracks parallel (channels 1..4)
Files: `core/seq/seq_config.h`; `apps/seq_engine_runner.c`  
- `#define SEQ_MAX_ACTIVE_TRACKS 4`.  
- In TICK body: for `t=0..3`, build `(bank,pattern,track=t)` handle; `seq_reader_get_step`; emit on channel `t+1`; schedule OFF via existing pipeline.  
- Commit: `P2/MP17c: runner 4-track parallel playback via Reader; map ch 1..4`

### MP17d — 8 tracks (channels 1..8)
- Just set `SEQ_MAX_ACTIVE_TRACKS 8`.  
- Commit: `P2/MP17d: runner 8-track parallel playback (ch 1..8)`

### MP17e — 16 tracks (channels 1..16)
- Set `SEQ_MAX_ACTIVE_TRACKS 16`.  
- Commit: `P2/MP17e: runner 16-track parallel playback (XVA1 4×4 → ch 1..16)`

---

## 5) Optional next: CCM for hot (after MP17)
- F429 CCM (0x1000_0000, 64 KiB) is **not DMA-accessible**. Put **hot** (CPU-only) there, keep **cold** in SRAM/SDRAM.  
- Pre-migration audit: ensure **hot ≤ 64 KiB**; verify **no DMA** touches hot; adjust linker `.hot` section; start with **subset** (queues/stacks).  
- Add debug anti-CCM-DMA asserts in DMA wrappers; keep flag OFF by default for instant rollback.

---

## 6) Directory & file map (high level)
- `core/seq/` — runtime, reader, scheduler, player, config, sections.  
- `core/seq/runtime/` — hot/cold layout, cold views, sections, RT debug hooks (opt-in).  
- `apps/seq_engine_runner.c` — **TICK body** (runner) — where we loop tracks.  
- `apps/seq_led_bridge.c` — LEDs (already migrated to Reader flags).  
- `apps/xva1_*.c/.h` — XVA1 surface (capabilities).  
- `apps/midi_*.c/.h` — MIDI I/O helpers.  
- `tests/` — host benches & stubs; `seq_rt_report` aggregator → `out/host_rt_report.txt`.  
- `docs/ARCHITECTURE_FR.md` — authoritative pipeline & module boundaries.  
- `PROGRESS_P1.md` — pass-by-pass log.

---

## 7) Glossary (quick)
- **Reader**: service providing copy-only views of steps & p-locks via handles (no runtime pointers).  
- **Hot/Cold**: conceptual split; cold = large, read-only; hot = RT path (CPU-only).  
- **No-cold-in-tick**: rule + CI that forbids cold access during real-time TICK.  
- **Blackbox ring**: host-only diagnostic storing last events + silent tick detection.

---

## 8) Troubleshooting notes
- “Missing separator (TAB vs spaces)” in Makefile: recipes under targets must start with **TAB**, not spaces.  
- Windows-reserved filenames (e.g., `NUL`) must be avoided/excluded.  
- If Codex stalls: reduce scope to a **single hunk** change; keep commits atomic.

---

## 9) Checklist to continue
- [ ] Run `make check-host`, inspect `out/host_rt_report.txt` (should be green).  
- [ ] Apply MP17a→e in order; test DAW with channels 1..16.  
- [ ] (Optional) Run a short hardware soak; observe for silence/latency.  
- [ ] Consider CCM hot trial afterwards (flag-gated).