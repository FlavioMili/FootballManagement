# Football Management: sequential implementation and acceptance plan

Prepared: 2026-09-10. Status: planning only; no implementation tasks are complete.

## 1. Intended result

Build a professional-looking, believable, enjoyable single-player football management game in which the player can build a squad, establish a tactical identity, develop players, negotiate transfers, watch their decisions affect matches, and sustain a career across many seasons. Make the next useful action obvious and make routine work delegable.

The central loop is: understand the club's situation → make a meaningful decision → prepare the team → play/watch the match → understand the consequences → improve the squad. Each new system must strengthen this loop and explain its consequences in the interface.

Football Manager is a reference for management depth, interconnected systems, and football presentation. Use original layouts, text, branding, assets, and game data. The target is a coherent first release, with a defined scope, rather than indefinite feature accumulation.

“Professional graphics” covers both the management interface and match presentation. The release target includes a realistic 3D broadcast view with animated human players, believable ball contact, pitch materials, lighting, stadiums, camera direction, and sound. A polished 2D tactical view remains available. Improving circles and pitch stripes alone does not complete the graphics requirement. Photorealistic close-up faces and AAA broadcast production are outside this release's acceptance criteria.

### Default scope and decisions

- Keep C++23, SQLite, SDL3, and Dear ImGui initially. Improve the existing product incrementally.
- Linux desktop is the primary implementation and performance platform. Produce and smoke-test Windows packaging before final release readiness; document macOS as unsupported unless it is separately validated.
- Offline single-player; no account, paid service, online LLM, or network connection required to play.
- Start with one fictional country, two linked divisions, one domestic cup, and approximately 40 clubs. Add a second country and a continental competition after the first career works. Preserve compatible existing saves and data packs.
- Default to original fictional players, clubs, kits, and crests with credible attributes and identities. Real-world data packs are a separate import capability with provenance and versioning.
- English and Italian are supported. Default management UI: restrained dark neutral surfaces, clear typography, compact tables, limited club-color accents; retain a usable light theme.
- Design for 1280×720 through 3840×2160, including 1366×768, 1920×1080, ultrawide, and 100–200% UI scaling. Effective layout space, not physical pixel count alone, determines layout changes.
- Keep the current renderer boundary. Evaluate SDL GPU for the 3D renderer in T34; that is a proposed technical route, not an instruction to rewrite the entire game around a new engine.
- No purchases, external publishing, asset subscriptions, or remote messages are implied by this document.

### Deliberately outside this release

Multiplayer, mobile/console ports, a complete worldwide database, full international management, every competition format, live-service features, spoken AI commentary, an unrestricted scripting marketplace, and close-up facial motion capture are not required. Record later ideas in a separate backlog; do not silently add them to the completion condition.

## 2. Verified starting point and risks

This inventory comes from source inspection, not assumptions based on the README. Recheck it in T00 because the repository can change.

| Area | Present in the inspected source | Consequence for this plan |
| --- | --- | --- |
| Build | C++23; SDL3/SDL_ttf, ImGui, SQLite, GTest, Google Benchmark | Reuse existing tools. Several dependencies follow moving branches; pin compatible revisions. |
| Structure | `GameController`, `Game`, `GameData`, repositories, GUI scenes | Introduce bounded services where needed, preserving the existing seams. `core_lib` currently also compiles GUI code. |
| Career clock | `Game::advanceDay()`, `GameDateValue`, date-indexed fixtures | A daily calendar already exists. Extend event processing and scheduling; do not implement a second clock because the README calls it missing. |
| Transfers | Persistent listings/bids, AI acceptance, atomic transfer operations, basic contract demand/acceptance, expiry test | Extend negotiations and contract dates. Do not rebuild completed functionality or describe it as absent. |
| Match simulation | Fixed steps, seeded engine, team phases/intents, pass/shot scenarios, goalkeeper states, interpolation | Preserve and extend this engine; establish statistical realism and law coverage. |
| Background matches | `Match::simulate()` already uses `MatchEngine` | Preserve consistency with watched matches; unify seed and match-input construction. |
| Presentation | Custom light theme, scene overlays, roster/lineup views, ImDrawList-based 2D stadium/pitch/players | A theme already exists, but visual hierarchy, workflows, art, and animation need a coherent overhaul. |
| Rendering seam | `IMatchRenderer` and read-only `MatchRenderSnapshot` | Reuse for 2D, 3D, replay, and renderer-equivalence checks; audit snapshot lifetimes before threading. |
| Finance | Balance and wage calculations; main-menu finance button has an empty handler | First make displayed actions functional, then add a transaction ledger and economic model. |
| Tests | 52 tests discovered with `ctest --test-dir build-release -N` | Discovery is not execution. No pass/fail baseline or benchmark timings were established while writing this plan. |
| GUI tests | `GUIFlowLifecycle` changes scenes programmatically and saves BMPs | Extend to actual input-driven journeys and semantic assertions. A screenshot file existing proves neither usability nor visual quality. |
| Test safety | Some fixtures remove `DATABASE_PATH`, configured as source-root `FootballManagement.db`; shared save slots and `/tmp` paths | Fix isolation before running the full existing suite. Setting `XDG_DATA_HOME` alone does not redirect every path. |
| Test integration | ImGui Test Engine is fetched; current target source list does not integrate its automation engine | Add and verify real integration, compatible version, test-only build option, and applicable license. |
| Persistence | Ad hoc `ALTER TABLE` calls in `GameData`; schema includes free-agent team with league `-1` | Introduce versioned migrations and repair sentinels before enabling/enforcing referential integrity. |
| Randomness | Match seeds exist; generator, transfers, and player progression also use `random_device` | Persist deterministic world RNG streams; a match-only environment seed is insufficient. |
| Portability | Paths generated with source-root locations; Release uses `-march=native`; warning flags assume compiler families | Separate runtime paths and portable release builds from local profiling builds. |
| Documentation | README links missing `DEVELOPMENT_HANDOFF.md`; some completion claims are stale | Reconcile docs against verified behavior, restore a compact handoff, and make this the ordered roadmap. |
| Existing build directories | Both inspected `build` and `build-release` caches say Release | Never infer sanitizers from a directory name. Use separate explicit Debug builds. |

Important starting files: `CMakeLists.txt`, `src/CMakeLists.txt`, `test/CMakeLists.txt`, `benchmarks/`, `src/global/paths.h.in`, `src/model/`, `src/controller/game_controller.*`, `src/database/`, `src/gui/scenes/`, `src/gui/render/`, `assets/db/schema.sql`, and `.github/workflows/ci.yml`.

## 3. Operating contract for the implementing LLM

### Authority and review

This document specifies future work; creating it does not start that work. Read `AGENT.md`, `AI_GUIDELINES.md`, and `CONTRIBUTING.md` before implementation. They currently require confirmation of the implementation plan before substantial code, human responsibility for review, and prohibit agent commits and pushes. An explicit subsequent instruction to implement this plan establishes plan acceptance; record it once and do not repeatedly ask for routine steps already covered by it. Follow any newer explicit user instructions.

Keep local work progressing through accepted tasks without asking the user to approve every file, test, or design token. Architectural departures outside the accepted plan require a concrete proposal with tradeoffs and evidence. Do not change repository review rules merely to make the loop claim completion. This plan minimizes the work needed to review; it cannot remove the repository's human review obligations. No agent commits or pushes under the current policy.

For major implementation, attempt the read-only `gh issue list` required by `AGENT.md`; record relevant existing issue IDs. If unavailable, use the stable task IDs below and record that traceability limitation. Do not create issues or send review requests externally without authorization. Keep temporary builds, saves, logs, captures, and benchmark data under a unique `/tmp` directory. Intentional durable source, test fixtures, specifications, and concise evidence summaries belong in the repository.

### Sequential loop

The only authoritative completion checkboxes are the 50 lines `- [ ]`/`- [x]` T00 through T49 below. Every task depends on all lower-numbered tasks being complete, including milestone gates. Work in order. Within a task, use its numbered steps in order; split a large step into small recorded substeps without moving its acceptance criteria elsewhere.

```text
read instructions, TODO.md, latest handoff, and worktree status
verify the task state against actual files and evidence
if implementation authorization is absent: present the prepared plan once
otherwise:
    while at least one T00..T49 is unchecked:
        choose the lowest-numbered unchecked task
        verify prior gates and inspect the task's affected code
        record an achievable substep, risks, and required checks
        implement that substep and run its focused checks
        inspect failures, output, and visual evidence; repair and rerun affected checks
        if all task acceptance criteria and applicable global gates pass:
            write the evidence entry and check the task complete
            update the handoff and continue immediately
        else if useful authorized work remains inside the task:
            continue inside the task
        else:
            record BLOCKED, exact cause, attempted alternatives, next action
            ask only for the missing authority/resource/decision and stop dependent work
    run final completion audit; report completion only if it passes
```

A document does not run an agent by itself. Use this contract with an available continuation mechanism, or resume it with the prompt at the end. On context limits or session interruption, save the handoff and leave the task unchecked. Never mark work complete because time or tokens ran out.

### Progress state and evidence

Create `docs/implementation/STATUS.md` in T00. Its initial fields are:

```yaml
plan_version: 1
implementation_authorization: not_recorded
current_task: T00
current_substep: inventory
status: NOT_STARTED  # NOT_STARTED, IN_PROGRESS, VERIFYING, BLOCKED, COMPLETE
last_completed_task: null
baseline_revision: pending
working_diff_fingerprint: pending
artifact_root: pending
next_action: verify source inventory and document unsafe test paths
blocker: null
```

Record resumable details there; keep this document's task checkboxes synchronized. A status file cannot overrule missing acceptance evidence. A future validator should fail if task order has gaps, a completed task has no evidence, or T49 is checked with incomplete tasks. New discoveries become repair substeps under the current task; keep the parent unchecked until repaired. If a later change invalidates an earlier guarantee, record and fix it before advancing, and rerun the relevant earlier gate.

Create one concise `docs/implementation/evidence/Txx.md` per completed task containing:

```text
Task / acceptance criteria satisfied / linked issue if available
Behavior before and after, affected source files, architectural decision
Source revision + tracked diff hash + hashes of relevant untracked source/assets
Commands, toolchain, seeds, dataset/config hashes, exit codes, test counts
Benchmark baseline/candidate comparison and hardware/driver metadata when relevant
Screenshot/video/contact-sheet locations plus visual findings when relevant
Save compatibility result, known limitations, remaining risks
Agent self-review findings and fixes; next task
```

Keep raw evidence in the run's `/tmp` directory; copy durable concise results into the evidence entry. Preserve small intentional golden fixtures and reference renders in dedicated test assets. If a needed raw artifact expires, regenerate it; do not claim an unavailable screenshot or benchmark report was reviewed. Do not accumulate giant binary videos, profiling dumps, or generated saves in the source tree.

### Every task's definition of done

- The player-visible behavior works through the actual UI where applicable, including cancel, error, empty, and reload states.
- Focused behavior tests pass; relevant previous acceptance journeys remain green. Tests must be able to fail for the defect they cover.
- New persisted state has a migration, round-trip test, and failure-path test. New random behavior is seeded and replayable.
- Performance-affecting changes have local Release benchmark evidence. GUI changes have actual rendered evidence; 3D changes have hardware evidence.
- No unrelated edits, new compiler/linter warnings, ignored failures, silently skipped required checks, fake data on production screens, or dead buttons.
- Update localization, help text, user-facing explanations, docs, and task evidence as appropriate.
- Self-review the final diff for ownership/lifetime problems, stale caches, overflow, nondeterminism, save compatibility, and scope creep.
- A task is complete only when its own exit criteria and applicable global gates pass. Milestone tasks cannot be skipped.

Do not weaken a threshold, delete a regression test, broaden a visual mask, or regenerate a golden image merely to make checks pass. Fix the cause. A justified spec change must retain the old result, explain the tradeoff, and identify which acceptance promise changed.

## 4. Validation system to build and reuse

### Test levels and execution policy

| Level | Purpose and implementation | When |
| --- | --- | --- |
| Unit | GTest for pure rules, boundaries, formulas, sorting/filtering, state transitions | Every affected substep |
| Scenario | Small handcrafted match/negotiation/calendar fixtures; assert outcomes and reasons | Changes to the affected subsystem |
| Property/invariant | Fixed generated cases and seed corpus; save minimized failing inputs | Rules, scheduler, transactions, RNG, match physics |
| Integration | Temporary SQLite saves, migrations, replay, workflows through controllers/services | Every persisted behavior change |
| UI journey | Real mouse/keyboard input, stable widget IDs, actual state assertions | Every changed workflow; fast subset in CI |
| Visual | Fixed-state captures, geometry/text checks, regional image diffs, agent image inspection | Every visible change |
| Statistical | Cohorts of matches/seasons with prespecified tolerances and holdout seeds | Match/economy/development tuning and milestones |
| Performance | Google Benchmark plus end-to-end timing, frame traces, memory and IO telemetry | Relevant local changes; never noisy shared-CI timing gates |
| Soak/release | Long careers, migration history, packaged binary, recovery, hardware 3D | Milestones and release candidate |

Introduce CTest labels `unit`, `integration`, `scenario`, `ui`, `statistical`, `soak`, and `hardware`. A fast default CI run should finish within 10 minutes on its documented runner after dependency caching. Slower correctness jobs can be scheduled or explicitly invoked. Benchmarks remain local per repository policy. Hardware jobs must report “not run: no device” distinctly and cannot satisfy a graphics gate.

Do not add trivial tests for static padding or duplicated getters. Use tests for behavior and regressions; use snapshots and layout assertions for styling. Prefer stable widget IDs over translated labels and screen coordinates. UI setup may seed a fixture through services, but actions under test must use the public UI.

### Commands: existing tools versus planned tools

At plan-writing time only CMake, CTest, `unit_tests`, `db_benchmarks`, `match_benchmarks`, and the existing application `--profile-match` path are present. All runner scripts, labels, report schemas, and additional command-line options specified below are deliverables, not working commands yet.

Before T01, use inspection and test discovery only. Do not run the full legacy suite until every destructive fixture path is isolated. Do not run `--profile-match` against a personal save: it currently picks the first existing slot. T03 must add an explicit generated fixture path and bounded execution.

After T01–T04 establish isolation and reproducibility, the underlying commands are:

```sh
# Execute from the source root. Keep these variables in the same shell session.
fm_run_root=$(mktemp -d /tmp/fm-validation.XXXXXX)
cmake -S . -B "$fm_run_root/debug" -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DBUILD_BENCHMARKS=OFF
cmake --build "$fm_run_root/debug" --parallel 2
ctest --test-dir "$fm_run_root/debug" --output-on-failure --parallel 2

cmake -S . -B "$fm_run_root/release" -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON -DBUILD_BENCHMARKS=ON
cmake --build "$fm_run_root/release" --parallel 2
ctest --test-dir "$fm_run_root/release" --output-on-failure --parallel 2

"$fm_run_root/release/benchmarks/db_benchmarks" --benchmark_repetitions=7 --benchmark_min_time=1s --benchmark_out="$fm_run_root/db.json" --benchmark_out_format=json
"$fm_run_root/release/benchmarks/match_benchmarks" --benchmark_repetitions=7 --benchmark_min_time=1s --benchmark_out="$fm_run_root/match.json" --benchmark_out_format=json
```

T02 must make the toolchain/flags portable and pin dependencies; check the pinned benchmark binary's `--help` before standardizing the command interface. T01 must make individual fixtures unique even when CTest starts multiple processes. Do not assume the shell's temporary root automatically overrides paths compiled into the application.

T03 will provide `scripts/verify.py` with a documented interface for task ID, build directory, run directory, seed, and test tier. It should orchestrate real commands, preserve their exit codes, list required checks, and emit JSON plus a short Markdown summary. T04 will add a benchmark comparison command. Do not create a second build/test framework in these scripts.

### Determinism and statistical correctness

- Separate world generation, transfers, progression, injuries, match simulation, and cosmetic RNG streams. Persist their state or stable counter-based inputs, RNG algorithm version, match input/config hashes, and world seed.
- Exact reproducibility is required on the same supported build and platform. Do not promise floating-point bit identity across all compilers/CPUs without demonstrating it. Cross-platform rules/invariants and distribution tolerances remain required.
- Compare uninterrupted versus save/reload runs, watched versus headless matches, different render rates, pauses, highlight speeds, and UI navigation. Cosmetic choices must not change world outcomes.
- Start with a fixed small seed suite for CI. For tuning use at least 1,000 paired matches across relevant cohorts; use at least 10,000 for milestone realism validation. Increase sample size for rare events or inconclusive results.
- Define metrics, cohorts, minimum practically meaningful effects, and acceptance intervals before tuning. Freeze separate calibration and holdout seeds. Use confidence intervals and effect sizes, not one match or a favorable average.
- Preserve distributions by team strength, home/away, tactics, score state, and competition. A plausible league mean cannot excuse every match producing the same score.
- Use real observations only with a documented definition, provenance, competition, season, sample size, and applicable data terms. When empirical data is unavailable, label ranges as design assumptions rather than real-world facts.
- A red card reducing expected performance, fatigue increasing workload cost, or a stronger team having an advantage is a distributional claim. Do not force any single match's outcome.

### Performance budgets and methodology

These are proposed engineering targets, not measured current results. T04 records the actual baseline, reference CPU/GPU, RAM, OS, drivers, compiler, power mode, resolution, world size, and quality preset. If baseline exceeds a target, record a performance debt with a repair task; do not relabel the target as passed. Frame budgets apply to warmed-up Release builds on the reference hardware.

| Workload | Initial release target | Evidence |
| --- | --- | --- |
| Management screens at 1080p | p95 frame ≤16.7 ms, p99 ≤33.3 ms; input feedback ≤100 ms | Frame trace plus scripted interactions |
| Full 3D match, medium preset at 1080p | p95 frame ≤16.7 ms, p99 ≤33.3 ms on reference GPU | Ten-minute capture including set pieces/replays |
| Low 3D preset at 720p | p95 frame ≤33.3 ms on declared minimum GPU | Same sequence and player count |
| Match simulation tick | p95 ≤2 ms for 22 players on reference CPU | Separate simulation timing from render and waits |
| Roster/search on 50,000 players | p95 response ≤100 ms after cached data is ready | Filter/sort/search benchmark, no per-row SQL |
| Save/load, standard ~10,000-player world | Save p95 ≤2 s; load p95 ≤3 s | Transaction/cold-load and warm-load measured separately |
| Save/load, large 50,000-player world | Save p95 ≤5 s; load p95 ≤8 s | Same schema/features as production |
| Continue on standard non-match day | p95 ≤250 ms | End-to-end command timing |
| Continue on standard match day | p95 ≤2 s; progress feedback within 100 ms if slower | All required fixtures processed, no reduced correctness |
| One standard unattended season | ≤120 s in Release | Explicit AI delegation, complete fixtures and accounting |
| RAM, standard world plus medium 3D | ≤2 GiB resident after warm-up; no sustained unexplained growth | RSS samples and allocation/resource counts |
| VRAM, medium 1080p | ≤2 GiB planned asset residency | Backend instrumentation/profiler and resource audit |

For each benchmark use a clean fixture, fixed seed, unchanging workload, at least seven repetitions, and record raw results. For p95/p99 latency collect enough individual operations/frames; do not call the p95 of seven aggregate benchmark means a tail-latency measurement. Separate setup/loading from steady-state timing. Compare on the same hardware/build flags and interleave baseline/candidate runs when drift matters.

Investigate repeatable regressions above 10% in median or tail latency and above 15% in memory. For very small timings also apply a documented absolute noise floor. Use confidence intervals or repeated confirmation to separate noise from regression. More simulated detail can justify a measured cost, but it must remain within the release budget or have an explicit accepted change. Never compare sanitizer timings to Release timings.

### Visual acceptance and minimum manual review

For every major screen capture normal, empty, error, long-name, crowded-table, and selected-player states in both languages. Test the resolution/scaling matrix through pairwise coverage, with critical workflows explicitly covered at the minimum effective viewport. Fix the clock, seed, font, theme, and backend for deterministic UI captures.

Use text/geometry assertions for overlap, clipping, invisible controls, focus order, and minimum hit areas; regional pixel/perceptual comparisons for rendering regressions. Calibrate tolerances from repeated identical captures on each backend. Keep masks small and documented. Screenshots on different GPUs need separate tolerances or references.

The implementing agent must open and inspect contact sheets and clips. Check typography, alignment, density, hierarchy, contrast, kit separation, anatomical scale, feet sliding, ball contact, camera motion, and lighting. Automated image similarity does not prove professional quality or fun.

Use three prepared review packets: playable management slice (T09), complete career loop (T31), and graphics/release candidate (T40/T49). Each packet should contain a one-page explanation, six annotated screenshots, a short scripted clip, test summary, performance changes, and known limitations. These are review opportunities, not mandatory new approval interruptions under an already accepted plan. If user feedback arrives, incorporate it. If no human playtest occurred, report that explicitly; agent evaluation must not be described as human validation.

## 5. Ordered implementation tasks

Each task lists concrete work, verification, and the condition for moving on. “Global checks” means the applicable requirements in sections 3–4. All new file locations below are proposed deliverables unless listed in the starting inventory.

### T00 — Establish the actual baseline

- [x] T00 Complete the source inventory and create the resumable handoff.

1. Read project instructions, record implementation authorization if present, inspect the worktree, compiler/dependency versions, configured targets, and existing issues when accessible.
2. Verify the inventory in section 2, map every visible menu action to its implementation, and list current supported workflows and missing/error states.
3. Inspect all fixture setup/teardown, database paths, logger/settings paths, and profiling commands before executing them. Record unsafe paths for T01.
4. Create `docs/implementation/STATUS.md`, `docs/implementation/evidence/T00.md`, and `DEVELOPMENT_HANDOFF.md`; repair stale README roadmap statements and link this plan.

**Verify:** Test discovery, source-reference checks, and documentation links. Record the revision and existing dirty files. Do not call discovery a passing suite.

**Exit:** The next agent can identify the first task, risks, exact build commands, and current source capabilities without reconstructing the investigation.

### T01 — Make tests and development runs safe and isolated

- [x] T01 Isolate all generated data and cleanup operations.

1. Introduce injectable runtime paths for assets, saves, settings, cache, logs, and captures. Production user data uses the user-data location; fixtures receive a unique temporary root.
2. Remove test dependence on source-root `DATABASE_PATH`, global slot 0/99, shared screenshot filenames, and the fixed DB benchmark path. Each process/fixture owns its directory through RAII.
3. Ensure cleanup can remove only paths created by that fixture; close connections before cleanup and handle SQLite WAL/SHM sidecars.
4. Add regression tests that place sentinel files in source/user-data locations, run a sandboxed fixture, and verify sentinel contents are untouched. Avoid using real personal data as sentinels.

**Verify:** Run focused isolation tests, then the existing suite serially and concurrently twice. Confirm no new files outside the injected roots and no cross-test interference.

**Exit:** Full-suite, UI, benchmark, and profiling runs cannot erase or mutate a personal save or repository database. Only now establish execution baselines.

### T02 — Reproducible builds and useful diagnostics

- [x] T02 Pin the toolchain/dependencies and make build modes trustworthy.

1. Pin compatible SDL3, SDL_ttf, ImGui, test-engine, and CI dependency revisions; document update procedure. Retain dependency caches without silently using floating heads.
2. Add CMake presets for Debug sanitizers, portable Release, local profiling, and tests. Make compiler flags conditional; remove `-march=native` from distributable builds.
3. Separate test/benchmark/UI automation options; avoid fetching test tooling for production-only builds. Enforce a truthful minimum CMake version for used test-launcher features.
4. Wire clang-tidy so it actually applies to intended targets; lint changed project code without analyzing vendored dependencies. Avoid a full-tree formatter rewriting unrelated files.
5. Split pure model/persistence code from GUI linkage where needed for headless tests, in a behavior-preserving change. Add a core leak-check lane and narrow documented SDL suppressions instead of suppressing all project leaks.

**Verify:** Clean Debug/Release configuration and builds, current test suite, compile-command inspection, explicit sanitizer smoke detection in an isolated test harness, and a production build with testing disabled.

**Exit:** A clean machine can reproduce the build; diagnostics execute as advertised; dependencies and build modes are documented.

### T03 — Automated user journeys and evidence runner

- [ ] T03 Make the application controllable and verifiable without manual clicking.

1. Integrate the pinned ImGui Test Engine under a test-only option after reading its actual license; use SDL input injection for interactions it cannot exercise.
2. Add stable widget IDs, fixture injection, deterministic clock/input scheduling, screenshot output, bounded frame/time execution, and an explicit profile fixture/save option.
3. Automate new game → club selection → roster → lineup → tactic → match → result → save → reload using actual UI actions. Assert domain outcomes, not only scene IDs.
4. Create `scripts/verify.py` and CTest labels. Emit machine-readable results with failures, seeds, captures, timeouts, missing hardware, and reproduction commands.
5. Add runner tests for failed subprocesses, timeouts, missing required artifacts, unsafe output paths, and skipped checks. Make the runner return failure when a required check did not execute.

**Verify:** Deliberately break a button binding in a temporary patch and confirm its journey fails; restore it. Test keyboard-only navigation and both languages for the initial workflow.

**Exit:** One documented invocation reproduces a journey, captures evidence, and reliably reports success or failure without touching personal saves.

### T04 — Freeze baseline workloads and performance reports

- [ ] T04 Establish reproducible benchmarks and performance budgets.

1. Record reference hardware and freeze small, standard (~10k players), and large (50k players) deterministic fixtures with hashes and entity counts.
2. Run existing DB and match benchmarks locally in Release. Preserve actual workload sizes; the current DB ranges start at 8,192 because generation already exceeds 7,000 players.
3. Add startup, save/load, daily continuation, complete season, roster filtering, scene entry, frame time, snapshot creation, and allocation/resource benchmarks where supported; mark later-system cases pending until implemented.
4. Implement comparison/report tooling, noise handling, percentile sampling, and resource traces. Track baseline debt separately from regressions.

**Verify:** Repeated unchanged runs produce stable comparisons. A controlled slowdown is detected by the comparison tool. No benchmark reuses a player's database or requires random manual setup.

**Exit:** Baseline measurements are real, reproducible, and tied to hardware/source/fixtures; later tasks have an unambiguous comparison procedure.

### T05 — Define and implement the management design system

- [ ] T05 Replace ad hoc styling with a coherent professional design system.

1. Create `docs/design/visual-spec.md` describing hierarchy, navigation, color tokens, typography, spacing, icons, tables, forms, charts, and match overlays, including annotated target layouts.
2. Implement shared theme tokens and font loading with licensed fonts, tabular numeric alignment, glyph fallbacks, and DPI-aware sizing. Default body text should remain comfortably readable at intended scale.
3. Build reusable page headers, section panels, status badges, searchable tables, validation messages, tooltips, confirmation dialogs, loading/empty/error states, and accessible focus states.
4. Use color plus text/icon meaning. Target readable contrast (design target: 4.5:1 normal text, 3:1 large text/essential controls), restrained borders, and consistent density. Avoid decorative gradients and charts without a purpose.

**Verify:** Render a component gallery in both themes/languages across the viewport matrix; automatically check contrast tokens and inspect contact sheets. Benchmark gallery frame cost.

**Exit:** Components are reusable, readable, and visually consistent; the design specification is concrete enough for subsequent agents to apply without redesigning each page.

### T06 — Build a persistent management shell

- [ ] T06 Make navigation and the next action clear on every management screen.

1. Add a persistent club/date header, navigation sidebar, content area, global search, back/forward history, and context-aware Continue/Play Match action.
2. Replace routine scene-overlay stacking with stable navigation; reserve overlays for short tasks and detail dialogs. Preserve table filters/scroll/selection when returning.
3. Create a dashboard with next fixture, recent form, league position, squad availability, transfer/wage budget, and actionable alerts using current real state.
4. Wire the currently empty finance action to a truthful current-balance/wage view. Expose future screens only when meaningful content exists; every visible enabled action must work.

**Verify:** Input-driven navigation/back/forward tests, resize/scale tests, stale-data refresh after transfers/results, and dashboard screenshots with long names and no fixtures.

**Exit:** A user always knows which club/date/screen they are in and how to proceed. No navigation traps, dead actions, or loss of work on returning.

### T07 — Make the squad and player pages useful

- [ ] T07 Deliver a readable squad workspace and linked player profiles.

1. Add sortable/filterable roster columns for role, age, ability, wage, contract, current availability, and recorded form; save column preferences and use clipping for large lists.
2. Build player profiles with role-relevant attributes, contract, current career record, comparison, and explicit actions. Show unknown/unavailable information honestly.
3. Use stable player IDs for selection, comparison, and navigation. Handle a player being sold/retired while their profile is open.
4. Add a squad-depth view and multi-selection actions where existing systems support them. Prepare sections for fitness/scouting/history to populate in their later tasks.

**Verify:** Sorting/filtering correctness including ties, missing values, accents, and duplicate names; sell/reload/return journey; 50k-player table benchmark and screenshot matrix.

**Exit:** A manager can identify a weak position, compare candidates, and reach the relevant action in no more than three navigation steps from the squad screen.

### T08 — Make current tactics and lineup management playable

- [ ] T08 Deliver a usable lineup/tactics editor using existing engine behavior.

1. Unify squad selection and pitch positions with drag/drop plus keyboard alternatives, role labels, bench, captain, set-piece takers where supported, and saved lineup/tactic presets.
2. Validate duplicates, missing goalkeeper, unavailable players, and invalid positions; provide an assistant auto-pick with visible reasons and reversible changes.
3. Replace unexplained strategy sliders with named presets and concise explanations mapped to existing engine parameters. Do not advertise tactical behaviors that are not implemented.
4. Add pre-match opponent context, a readiness checklist, and clear save/apply/cancel semantics. Preserve work across page changes and reloads.

**Verify:** Input-driven selection/substitution/preset tests, invalid-lineup recovery, persistence, and same-seed checks that the current tactic reaches the engine.

**Exit:** A new player can select a legal team and understandable tactic without knowing internal role weights or editing data files.

### T09 — Gate A: the first polished playable slice

- [ ] T09 Prove the existing short gameplay loop works end to end.

1. Run the journey from a fresh fictional save through club selection, squad inspection, tactics, first match, result, and reload.
2. Fix all blockers, misleading labels, layout defects, and unnecessary navigation discovered along that route before adding deeper simulation.
3. Create the first review packet with before/after screenshots and a scripted walkthrough. Record a provisional friction baseline: navigation count, required decisions, idle waiting, and error recoveries.

**Verify:** All T00–T08 gates, Debug/Release suites, both-language UI journeys, visual inspection, and local UI/DB/match benchmark comparison.

**Exit:** The route is automated and coherent; no core action is missing. Present it for low-effort feedback without requiring a new routine approval to continue an accepted plan.

### T10 — Version saves and stabilize domain data

- [ ] T10 Introduce reliable schema migration and save recovery.

1. Define schema/version metadata, ordered transactional migrations, fixture saves from every supported old version, and compatibility policy. Replace error-ignoring migrations during reads.
2. Audit ID widths, seasons, money, dates, sentinel values, and ownership. Use explicit free-agent/no-club semantics; repair invalid foreign keys before enforcing them. Keep currency arithmetic in checked integer units.
3. Add save metadata for schema, simulation/rule/config versions, world seed, content packs, and timestamp. Reject unsupported future versions without modifying them.
4. Implement SQLite-consistent backups, autosave rotation, transaction rollback, disk-full/permission failures, corrupt-save detection, and recoverable load errors. Never copy only the main DB while ignoring a live WAL.
5. Document which in-memory state is dirty and where transactions commit; prevent UI caches and repositories from disagreeing after failure.

**Verify:** Upgrade every fixture, repeated-load idempotence, rollback fault injection, backup restore, `integrity_check`/`foreign_key_check`, and save/load benchmarks.

**Exit:** Old supported saves upgrade safely, failed saves preserve a usable prior state, and future features have a consistent migration path.

### T11 — Deterministic world simulation and traceability

- [ ] T11 Make careers reproducible across save/reload and presentation choices.

1. Introduce versioned RNG services/streams for generation, transfer AI, development, injuries, and matches; remove uncontrolled `random_device` use after initial world seeding.
2. Build stable fixture identities and a common match-input factory for watched and background matches. Sort unordered collections before order-sensitive operations.
3. Persist RNG/counter state and configuration hashes. Add command/event tracing and deterministic state digests excluding wall-clock/presentation data.
4. Audit `MatchRenderSnapshot` pointer lifetimes and define immutable owned data or guaranteed lifetimes before adding asynchronous simulation/rendering.

**Verify:** Same world generated twice; uninterrupted versus reload at multiple dates; watched/headless and 30/60/144 FPS results; no outcome changes when opening screens or changing camera/theme.

**Exit:** A bug report containing save/seed/commands/build reproduces the same failure on the same supported platform; RNG version changes are explicit.

### T12 — Extend the daily calendar into a safe event scheduler

- [ ] T12 Make Continue process time, decisions, and consequences exactly once.

1. Extend the existing date model with deterministic daily phases: scheduled obligations, squad recovery/training, deadlines/AI activity, fixtures, accounting, notifications. Define exact ordering.
2. Add persisted event IDs, processed markers, resumable phase boundaries, and a central Continue service. Do not advance past an unplayed managed fixture or unresolved mandatory decision.
3. Implement advance-to-next-event/match with cancellation, visible progress, and configurable interruption categories. Match simulation must not block the UI event loop.
4. Cover leap years, year/season transitions, no-event days, postponed fixtures, and deadlines on the same date. Later systems register handlers here rather than adding independent clocks.

**Verify:** Save/reload at each phase, duplicate event delivery, cancellation/resume, back-to-back Continue clicks, July transition, and all-fixtures-accounted-for invariants.

**Exit:** Time progresses predictably, no event runs twice, and users can skip quiet periods without missing a meaningful decision.

### T13 — Establish trustworthy fixtures and competition rules

- [ ] T13 Make the initial league and cup calendars/rules complete.

1. Define versioned competition rules for points, tie-breakers, squad/bench sizes, substitution allowances/windows, eligibility, suspensions, and season dates. Keep football laws separate from competition regulations.
2. Verify round-robin home/away coverage, odd-team byes, friendly eligibility, conflict-free scheduling, minimum rest preferences, and bounded rescheduling with explicit failure explanations.
3. Add a domestic cup with draws, progression, extra time/penalties, and trophy/history results. Persist competition and season identity on fixtures/results.
4. Ensure league points come only from eligible league matches. Replace standings that rely on too little persisted data with complete played/won/drawn/lost/goals/points records.

**Verify:** Property tests for 2–40 teams including odd counts, pair coverage, no duplicate fixture application, cup ties, equal-point tie-breakers, and zero participation by the free-agent sentinel team.

**Exit:** A complete season can be scheduled and resolved with no missing fixtures, contradictory standings, or unexplained eligibility decisions.

### T14 — Build a believable club economy

- [ ] T14 Connect finances to recurring football activity and decisions.

1. Add a dated ledger for wages, transfer fees/installments, bonuses, gate receipts, sponsorship, prize money, facilities, and staff costs. Each entry records its originating event and category.
2. Separate available cash, transfer budget, wage budget, committed future obligations, and board-controlled limits. Define payroll cadence and prorating precisely.
3. Implement budgets, projections, debt/interest, and visible board interventions for insolvency using configurable fictional rules. Do not silently give clubs infinite money.
4. Build finance summaries/charts with reconciled totals, runway estimates, and explanations of what the manager can influence.

**Verify:** Ledger reconciliation, payroll exactly once, leap/season boundaries, transfer fee conservation excluding explicit fees, integer overflow, debt cases, and 100-club daily accounting benchmarks.

**Exit:** A season's ending cash reconciles to opening cash plus ledger entries; spending decisions have understandable present and future costs.

### T15 — Extend contracts and transfers into negotiation gameplay

- [ ] T15 Deliver complete, persistent transfer/contract workflows.

1. Extend existing demand/acceptance logic into explicit offer, counteroffer, rejected, withdrawn, expired, accepted, and registered states; keep club agreement and player agreement separate.
2. Add exact contract start/end dates, renewal, free agency, signing/agent fees, bonuses, promises, installments, release clauses, and wage-budget checks in bounded increments.
3. Add loans with wages, duration, recall conditions, optional/mandatory future fees, parent-club ownership, and return processing. Add transfer windows and future agreed moves.
4. Make player decisions depend on role, playing time, reputation, location/preferences, wage, and realistic alternatives, with an explanation of key factors and incomplete knowledge where appropriate.
5. Implement a clear offer comparison/confirmation UI showing total commitment and cancellation consequences. Make AI clubs use the same validation and transaction services.

**Verify:** Duplicate acceptance, competing offers, insufficient funds after agreement, deadline expiry, failed second-party terms, mid-loan save/reload, retirement/expiry, and transaction rollback at each mutation point.

**Exit:** A manager can buy, sell, loan, renew, or release through the UI; money, ownership, contracts, registrations, and messages remain consistent across reloads.

### T16 — Make scouting a decision under uncertainty

- [ ] T16 Add scouting knowledge, recruitment priorities, and useful recommendations.

1. Separate true player attributes/potential from the manager's knowledge, including observation date, uncertainty, scout confidence, and report expiry.
2. Add scouts, assignments, regions, travel/time/cost, shortlists, alerts, and recruitment filters for role, affordability, availability, age, and squad need.
3. Build comparisons showing suitability and total expected cost without leaking hidden attributes through sorting, tooltips, ratings, or assistant recommendations.
4. Explain why a scout recommends a player, conflicting reports, expected information gain, and alternatives when the preferred target is unavailable.

**Verify:** Observation updates, bounded confidence, stale reports, different scout ability, hidden-value leakage tests, shortlist persistence, and 50k-player search/recommendation benchmarks.

**Exit:** Scouting changes what the manager knows and whom they recruit; uncertainty is visible and decisions remain actionable.

### T17 — Add staff and delegation that reduce busywork

- [ ] T17 Let the manager delegate recurring responsibilities with clear limits.

1. Add assistant, coaching, scouting, medical, and recruitment staff roles with ability, wages, contracts, and responsibilities.
2. Define delegation policies for selection, training, scouting, renewals, and offers: limits, protected players, required confirmations, and reversible preference changes.
3. Route delegated actions through the same domain commands as user actions; add a concise audit/inbox summary and explanations for failures.
4. Provide quick-start and detailed-management presets. Until a later system exists, its responsibility must remain unavailable with truthful text.

**Verify:** Spending caps, protected-player handling, policy persistence, no double action when user intervenes, and one-month unattended progression with zero unexplained blocking decisions.

**Exit:** Routine work can be delegated safely, and the manager can always see what staff did and retake control.

### T18 — Give rival clubs coherent management AI

- [ ] T18 Make opponents build and manage squads with the same rules.

1. Model club objectives, finances, positional depth, age balance, preferred tactics, staff competence, and recruitment horizons.
2. Schedule bounded planning cycles for lineups, rotation, renewals, scouting, sales, purchases, and emergency cover. Avoid globally rescanning every player for every club every day.
3. Use limited knowledge and lawful budgets; persist plans and avoid cycling the same player through repeated buy/sell decisions.
4. Add observable explanations for debug reports while keeping hidden knowledge out of the player UI.

**Verify:** Scripted weak-squad/debt/injury scenarios, no invalid registrations or self-bids, keeper coverage, transaction invariants, repeated-seed stability, and large-world AI planning benchmarks.

**Exit:** Rival clubs pursue understandable needs without hidden unlimited money or routine squad collapse; expensive planning meets the Continue budget.

### T19 — Fitness, injuries, suspensions, and availability

- [ ] T19 Make workload and player availability matter coherently.

1. Add condition, sharpness, accumulated load, recovery, injury susceptibility, injury type/severity, rehabilitation, and medical estimates.
2. Connect match minutes, pressing/work rate, travel/schedule congestion, training load, age, and staff to these states through documented bounded models.
3. Persist injury/recovery events and suspension eligibility by competition. Add rotation warnings, medical advice, and safe auto-selection fallbacks.
4. Distinguish risk from certainty; avoid deterministic injury punishments or exact recovery knowledge when it should be uncertain.

**Verify:** No negative/NaN condition, recovery bounds, duplicate minutes, unavailable-player exclusion, suspension scope, reloading before injury resolution, and statistical load/risk sensitivity.

**Exit:** Rotation is a meaningful tradeoff; availability is consistent in roster, selection, match, calendar, and AI decisions.

### T20 — Training and development with visible tradeoffs

- [ ] T20 Replace generic progression with a useful training system.

1. Define weekly schedules, session types, individual focus, positional familiarity, coaching quality, facilities, recovery, and match preparation.
2. Extend current progression so age, potential, training quality, playing time, morale, and workload influence bounded development. Avoid double progression from match-completion and daily handlers.
3. Provide templates, delegated scheduling, workload warnings, development history, and explanations of stalled improvement.
4. Link tactical familiarity to choices gradually and transparently; preserve the ability to change tactics without making experimentation useless.

**Verify:** Attribute limits, young/peak/older cohorts, no instant-max training exploit, rest/development tradeoffs, save/reload equivalence, and season-level distribution reports.

**Exit:** Training choices produce believable long-term changes and short-term preparation costs visible to the manager.

### T21 — Youth intake and sustainable player populations

- [ ] T21 Maintain a believable football world across generations.

1. Add dated youth intakes with club investment, facilities, regional pools, varied potential, positions, and personalities; use fictional identities with stable IDs.
2. Add youth/reserve squads, promotion, development minutes, loans, mentorship, retirement, and career-history retention. Keep youth scheduling affordable.
3. Prevent forced one-for-one clones of retiring stars and ensure the world retains enough goalkeepers and role coverage.
4. Widen or validate IDs and age/season fields as necessary; never reuse an ID still referenced by history, contracts, or reports.

**Verify:** Twenty-year population/age/role/potential curves across fixed seeds, ID uniqueness, no dangling retired-player references, and stable memory/database growth.

**Exit:** Careers do not run out of players or inflate everyone into elite ability; youth development creates identifiable long-term stories.

### T22 — Morale, relationships, and contextual stories

- [ ] T22 Make players and clubs feel consequential without repetitive interruptions.

1. Add bounded morale, form, playing-time expectations, leadership/cohesion, promises, and relationship changes driven by actual events.
2. Build a prioritized inbox with actionable messages, deadlines, links, archive/search, notification preferences, and summaries for routine outcomes.
3. Add concise authored/template-based conversations and event chains: debut, breakout prospect, captain dispute, poor run, transfer saga, comeback, rivalry, and milestone.
4. Ensure choices have documented conditions, plausible reactions, cooldowns, and visible consequences. No online generative service is required.

**Verify:** Promise fulfillment/breach, cooldown/deduplication, reload during conversation, no contradictory messages after a transfer, localization, and interruption-count measurements.

**Exit:** Stories emerge from the save's events and decisions; routine inbox traffic can be delegated or summarized without losing essential information.

### T23 — Board expectations and a complete managerial career

- [ ] T23 Add goals, job security, club development, and career continuity.

1. Model season objectives, financial expectations, playing style/youth preferences, board patience, and evaluation based on context and resources.
2. Add board requests for facilities, staff, youth, stadium improvements, and budgets with delivery dates and financial effects.
3. Add manager identity/history, reputation, contract, job offers, resignation, dismissal, unemployment progression, and new-club appointments.
4. Present reasons and recovery options clearly; avoid opaque firing from a single unlucky match or terminal saves after dismissal.

**Verify:** Objective boundaries, promotion/relegation expectation changes, dismissed-manager continuation, midseason club switches, no control of the former club, and complete career reload.

**Exit:** The manager has medium/long-term goals and can continue a career after changing or losing a job.

### T24 — Promotion, relegation, and a connected competition world

- [ ] T24 Expand the proven domestic loop to the defined release world.

1. Connect two domestic divisions through configured promotion/relegation; add the second fictional country and a continental qualification/competition format.
2. Add prize distributions, qualification ownership, draw seeding, travel/rest constraints, transfer/registration dates, and postponed fixture handling.
3. Preserve season archives, records, winners, player totals, and club movements independently from mutable current standings.
4. State supported formats explicitly and validate data-pack rules. Represent unsupported formats as validation errors, not approximated hidden behavior.

**Verify:** Full-cycle title/relegation/qualification cases, simultaneous competitions, tied qualification positions, fixture congestion, and season transition repeated across ten seeds.

**Exit:** Every released competition finishes, pays out, qualifies teams, and starts its next season consistently; histories remain inspectable.

### T25 — Structured match events and a common result pipeline

- [ ] T25 Make every match outcome explainable and reusable.

1. Extend current text events into structured timestamped events with stable IDs, participants, team, pitch position, outcome, and relevant quality/context values.
2. Define one match setup and result-application path for managed, background, replayed, and instant matches. Include minutes, discipline, injuries, stats, and accounting consequences.
3. Persist compact match reports and event summaries; define replay checkpoints/command logs and retention before storing full trajectories.
4. Extend renderer snapshots with semantic action/contact information while preserving read-only presentation and bounded memory.

**Verify:** Applying results twice, restart after partial application, score/event/stat reconciliation, watched/background equivalence, report round trip, and event/snapshot overhead benchmarks.

**Exit:** The same match produces the same authoritative result and consequences through every presentation route, with enough evidence to explain and debug it.

### T26 — Football law and officiating coverage

- [ ] T26 Implement and verify the supported football rules explicitly.

1. Create a law-coverage matrix tied to a specific IFAB edition and competition rules; avoid changing old saves automatically when real rules change.
2. Cover kickoff/restarts, throw-ins, corners, goal kicks, free kicks, penalties, fouls/advantage, handball abstraction, offside position versus involvement, cards, and goalkeeper restrictions.
3. Cover substitution count/windows, injuries, player dismissals, minimum-player rules, added time, halftime, extra time, penalty shootouts, abandonment, and resumed/awarded results as supported by the chosen rules.
4. Provide credible referee variation within rules and event explanations; if VAR is represented, define its reviewable events and decisions precisely instead of adding decorative delays.

**Verify:** Table-driven boundary scenarios for each supported law, offside at the relevant touch, restart exceptions, second yellow, keeper dismissal, multiple substitutions in one stoppage, shootout order/ties, and no illegal player counts.

**Exit:** The law matrix has passing tests for all supported rules and explicit limitations for approximations. Consult primary IFAB documents for the saved rules edition.

### T27 — Tactical depth with observable effects

- [ ] T27 Connect roles, team phases, and instructions to football behavior.

1. Extend the existing phases/intents into distinct in-possession/out-of-possession shapes, role behavior, pressing triggers, defensive height/width, buildup, transitions, and rest defense.
2. Add player instructions, opponent instructions, marking priorities, set-piece routines, substitutions, and AI tactical adjustments with documented bounds/conflicts.
3. Add a visualizer showing intended team shape across phases and concise explanations of physical/positional demands. Connect suitability to scouting, training, and squad planning.
4. Introduce choices in small groups with counterplay; avoid a universally dominant preset or parameters that only alter labels.

**Verify:** Scenario tests for each instruction, shape/spacing constraints, side symmetry, counters, fatigue cost, and paired-seed cohorts measuring possession, territory, chances, pressing, and exposure.

**Exit:** Managers can recognize their tactical choices on the pitch and in reports; all advertised controls have measured effects and plausible tradeoffs.

### T28 — Believable movement, ball behavior, and decisions

- [ ] T28 Improve football motion and decision quality before polishing animations.

1. Audit pitch dimensions and coordinate conversions; use explicit physical units at the boundary for speed, acceleration, ball flight, and timing.
2. Improve acceleration/deceleration, turning, spacing, ball carrying, first touch, interception, tackling, aerial contests, keeper positioning/claims, shots, rebounds, and goal-line detection.
3. Preserve existing scenario intent logic; add decision commitment/hysteresis and bounded perception so players neither jitter between actions nor react omnisciently.
4. Make passes/shots resolve through documented contact/trajectory/outcome rules; reconcile any presampled outcomes with visible ball events. Avoid impossible saves, teleports, or goals without a crossing.
5. Export scenario replays and diagnostic overlays for decisions, targets, control/contact, velocity, and event boundaries.

**Verify:** No NaNs/out-of-bounds states, bounded speed/acceleration, ball ownership/contact consistency, repeated-seed scenarios, convergence across supported frame chunking, and local match benchmarks.

**Exit:** Short clips show recognizable football sequences and every reported outcome agrees with the visible ball/player state.

### T29 — Calibrate match realism with independent evidence

- [ ] T29 Establish and satisfy a statistical football-realism specification.

1. Define metrics: score distribution, draws, home advantage, shots/on-target/xG, conversion, possession, pass completion/length, corners, fouls/cards, saves, set-piece share, offside, and ball-in-play time.
2. Build cohorts by relative strength, tactical matchup, game state, home/away, and player availability; compare to a documented real-data reference where suitable.
3. Calibrate on fixed training seeds, then evaluate untouched holdout seeds. Report intervals, outliers, and known approximations; inspect sample replays from both typical and extreme matches.
4. Run tactic tournaments and adversarial cases for kickoff loops, possession farming, infinite pressing, all-striker lineups, keeper exploits, and repeatable set-piece farming.
5. Store a versioned tuning profile and evidence. Change one coherent mechanism at a time; do not clamp final scores to manufacture a believable distribution.

**Verify:** At least 10,000 release-gate matches plus targeted rare-event scenarios; prespecified distribution/effect thresholds, current scenario suite, and benchmark comparison.

**Exit:** Football statistics, tactical sensitivity, and visible sequences agree well enough with the written realism spec; no known dominant trivial exploit remains.

### T30 — A compelling matchday experience

- [ ] T30 Add meaningful match control, highlights, and explanations.

1. Build pre-match preparation, team news, opposition advice, tunnel/lineup presentation, halftime analysis/decisions, and a clear post-match summary.
2. Support full match, extended/key highlights, commentary-only, and instant simulation with explicit pause/speed controls and tactical/substitution access.
3. Select highlights from actual structured events with buildup and aftermath; simulate intervening football fully. Replays must not consume world RNG or reapply results.
4. Add scoreboard, clock/added time, event timeline, live statistics, shot map, player condition, substitution confirmation, and relevant tactical feedback.
5. Save at safe match boundaries, and implement an explicit resumable in-match checkpoint or a clearly stated recovery boundary; no silent loss of the last hour's decisions.

**Verify:** Mode changes during play/stoppage, seeking replays, pause/substitution, halftime/fulltime, quit/reload, identical match results across highlight modes, and highlight duration/camera coverage.

**Exit:** Watching helps the manager understand and influence matches, while a delegated highlights session remains quick and enjoyable.

### T31 — Gate B: a complete management career loop

- [ ] T31 Prove the game supports a coherent full season and continuation.

1. Run scripted careers for a title contender, mid-table club, and relegation candidate: recruit, negotiate, train, rotate, resolve an injury/promise, play fixtures, review finances, finish the season, and start the next.
2. Include one loan return, youth intake, contract expiry, promotion/relegation, continental qualification, and manager dismissal/new job across the scenario set.
3. Fix integration gaps and busywork. Produce the second review packet and compare friction against T09.

**Verify:** All prior gates, full persistence/migration suite, season invariants, deterministic replay, world/economy statistical report, and standard/large-world local benchmarks.

**Exit:** The game is a complete management experience in 2D, with meaningful choices and durable consequences. A graphics project cannot substitute for this gate.

### T32 — Analytics that teach the manager what happened

- [ ] T32 Turn event/history data into actionable analysis.

1. Add match/season dashboards for shot quality, chance origins, passing networks, territory/heatmaps, workload, player development, wage efficiency, and scouting comparisons where reliable data exists.
2. Provide clear definitions, denominators, sample sizes, competition filters, comparison baselines, and uncertainty. Mark any modeled xG metric as an estimate and validate calibration.
3. Link tactical advice to actual observed events with examples and alternative explanations. Avoid causal claims based only on correlation or a single match.
4. Cache aggregates and invalidate them on authoritative events; keep graphs readable and drill-downs consistent with raw reports.

**Verify:** Aggregates reconcile to event records, zero/missing-data states, filters, season archives, reload, calibration bins, and large-history query benchmarks.

**Exit:** A manager can explain a recent result, diagnose a squad weakness, and identify a reasonable next action from the displayed evidence.

### T33 — Polish the 2D tactical presentation

- [ ] T33 Make the fallback/tactical match view professional and readable.

1. Apply the shared visual system to match panels and overlays; improve pitch proportions, markings, grass detail, goals, shadows, and stadium framing.
2. Use clearly distinguishable kit-based player markers/sprites, readable numbers/names, stable interpolation, visible ball height/shadow, and restrained possession/selection cues.
3. Add pan/zoom and tactical overlays with readable legends; avoid overcrowding at minimum resolutions and provide reduced-motion options.
4. Cache static pitch/crowd geometry and batch drawing. Move diagnostic AI labels behind developer controls.

**Verify:** Renderer/state equivalence, screenshot matrix, color-vision/kit-clash cases, resize, extreme aspect ratios, and warmed-up/first-frame benchmarks.

**Exit:** The 2D view is a finished tactical mode and a dependable fallback during the 3D work. It does not satisfy the final realistic-graphics requirement by itself.

### T34 — Prove the 3D rendering route with a bounded prototype

- [ ] T34 Select a maintainable 3D backend through a measured vertical slice.

1. Write an ADR comparing SDL GPU in the current application with one credible alternative only if a concrete requirement is unmet. Include development complexity, UI composition, portability, animation/assets, profiling, and dependency terms.
2. Prototype a pitch, one animated skinned actor, ball, shadow, camera, and ImGui overlay driven by a recorded existing match snapshot. Use a bounded feature branch/change set, not a wholesale rewrite.
3. Prove presentation ownership: the current SDL_Renderer path and a GPU device must not compete for the same window/swapchain. Choose one coherent composition path, including UI textures and capture support.
4. Measure startup, frame time, resource lifetime, resize/minimize, and minimum-hardware behavior. Validate required shader/tool support on Linux and the planned Windows backend.
5. Record the selected path and remaining work. A failure of the prototype leaves the 3D requirement open; present a concrete alternative if it requires authority outside the accepted plan.

**Verify:** Hardware render capture, deterministic snapshot playback, resize stress, clean teardown, UI input/capture, and benchmark against T04's budgets.

**Exit:** The selected backend demonstrably draws the required assets and UI together within a plausible budget; no unresolved integration assumption is delegated to later tasks.

### T35 — Build the production graphics and asset pipeline

- [ ] T35 Make realistic assets reproducible, validated, and distributable.

1. Define asset conventions for units, axes, skeletons, material channels, animation names, kit masks, texture color space, LODs, compression, and naming/versioning.
2. Add a reproducible importer/build step for the selected mesh/animation/material format, shader compilation, dependency manifests, cache invalidation, and packaged runtime loading.
3. Create an asset manifest with source, author, license/permission, modifications, and redistribution conditions. Use original or appropriately licensed content; keep unresolved assets out of release packages.
4. Establish a small coherent art set before scaling: player/keeper, ball, pitch, goals, seats, crowd, kit/crest variations, fonts/icons, and sound placeholders with clear replacement tracking.
5. Support load failure, missing textures, invalid skeletons, device resource recreation, bounded asynchronous upload, and consistent fallback materials.

**Verify:** Clean asset build, deterministic output hashes where supported, corrupt/missing input fixtures, install-time loading without source tree, resource leak checks, and load/VRAM budget reports.

**Exit:** Another agent can add an asset through a documented command without hand-fixing scale/materials or introducing unknown redistribution requirements.

### T36 — Render believable footballers and identities

- [ ] T36 Replace debug actors with production-quality players, keepers, and officials.

1. Add proportionate human meshes, a common validated rig, plausible height/body variation, skin/hair variation, correct kit materials, socks/boots, numbers, and keeper gloves.
2. Keep identities stable across matches, profiles, transfers, and seasons. Generate original portrait representations consistently; never imply fictional portraits depict real people.
3. Add home/away/keeper kit selection and clash detection including accessibility cases. Ensure shirt numbers and nameplates agree with the squad.
4. Add appropriate LODs, skinning budgets, culling, and instancing/batching where supported. Tune camera distance to the actual asset fidelity.

**Verify:** Reference poses, deformations across actor sizes, no missing joints/materials, kit clashes, ID/number consistency, 22-player/official scene capture, and skinning/VRAM benchmarks.

**Exit:** Players read as proportionate footballers at the intended broadcast distance with consistent identities and no production-visible debug geometry.

### T37 — Animation, locomotion, and ball-contact fidelity

- [ ] T37 Make rendered movement agree with the authoritative simulation.

1. Implement idle/walk/jog/sprint/turn/stop blends, receive/dribble/pass/cross/shoot/header/tackle/stumble/fall/get-up animations, keeper set/dive/claim/distribute, and restrained celebrations.
2. Map semantic simulation actions to animation states with timed contact markers. Define who owns motion: the simulation remains authoritative; animation/root-motion adaptation cannot silently move the ball or actor to change an outcome.
3. Add foot placement/contact correction and stride matching to reduce sliding; align kicks, headers, tackles, and keeper hands with ball events within a documented tolerance.
4. Handle interruption, wrong-foot actions, rapid turns, collisions, substitutions, red cards, injuries, replay seeking, and transitions across different actor sizes.

**Verify:** Fixed clips for every action and interruption, contact-position/time error measurements, actor/ball divergence checks, frame-rate independence, and visual inspection at normal speed and slow motion.

**Exit:** No obvious teleporting, persistent foot sliding, ball-through-foot kicks, keeper catches at a distance, or animation-driven changes to the match result in the acceptance clip set.

### T38 — Pitch, stadium, lighting, and environmental realism

- [ ] T38 Deliver coherent football environments at multiple quality levels.

1. Add physically proportioned pitches/markings/goals/nets, believable grass materials, contact shadows, ball shadows, and readable penalty/goal areas.
2. Build small/medium/large stadium variants from reusable stands, seating, tunnels, dugouts, barriers, and original advertising; scale attendance to club/match context.
3. Add calibrated daylight/floodlights, tone mapping, anti-aliasing, shadow quality, and modest weather/wetness effects. Separate cosmetic weather from any explicitly modeled gameplay effect.
4. Use crowd LOD/instancing and bounded ambience animation. Prevent z-fighting, flicker, excessive bloom, oversaturation, and visual clutter around the ball.

**Verify:** Day/night/weather fixtures, shadow stability, goal/ball visibility, hardware screenshots at all presets, large-crowd performance, and resource-budget comparison.

**Exit:** The environment resembles a coherent football venue, retains ball/player readability, and scales to the declared minimum hardware.

### T39 — Broadcast cameras, match sound, and presentation rhythm

- [ ] T39 Make match presentation clear, lively, and comfortable to watch.

1. Add broadcast, tactical-wide, behind-goal, and replay cameras with pitch-aware framing, smooth tracking, predictable cuts, and user override.
2. Direct highlights from recorded events, including buildup and aftermath; prevent cuts that hide decisive actions. Preserve score/time context and indicate replay clearly.
3. Add licensed/original crowd beds and reactions, whistle, ball contact, net, UI feedback, and volume buses. Tie cues to event IDs so replay/seek cannot duplicate world effects.
4. Add mute, subtitles/text equivalents for essential cues, reduced camera motion, and independent commentary/crowd/effects settings. Avoid repetitive loud crowd loops and gratuitous camera shake.

**Verify:** Goal/corner/penalty/counterattack clips, camera bounds/occlusion, replay seek, pause/fast-forward audio handling, settings persistence, and CPU/audio/frame budget traces.

**Exit:** Every important event is visible and understandable; sound supports context; a full highlights match is comfortable to watch.

### T40 — Gate C: realistic graphics acceptance

- [ ] T40 Prove the complete 3D presentation meets the written visual target.

1. Capture the same scripted match in 2D and 3D with day/night, small/large venue, home/away/keeper kits, set pieces, tackles, goals, substitutions, and replays.
2. Inspect annotated stills and clips for anatomy, animation contacts, feet sliding, ball trajectory, pitch proportions, materials, lighting, camera framing, UI legibility, and atmosphere.
3. Measure ten-minute frame traces and resource residency at 1080p medium and minimum-hardware 720p low. Verify renderer changes leave match outcomes unchanged.
4. Replace remaining production-visible prototype assets and fix visible defects before advancing. Prepare the graphics review packet with explicit observed limitations.

**Verify:** T34–T39 acceptance suites, hardware captures, backend-equivalence tests, resize/device recovery, first-frame and steady-state performance, and agent visual self-review.

**Exit:** Realistic 3D players, animation, stadium/lighting, cameras, sound, and management UI are present and coherent. Missing GPU access or missing art cannot be recorded as a passed graphics gate.

### T41 — Scale simulation without changing its meaning

- [ ] T41 Meet world-size and responsiveness budgets with measured optimization.

1. Profile the standard and 50k-player worlds to locate actual hotspots: full-cache saves, transfers, AI scans, fixture simulation, analytics, and asset upload.
2. Add indexes, batched SQL, dirty tracking, cached aggregates, bounded planning schedules, and allocation reductions only where measurements justify them.
3. Introduce worker execution where needed, with deterministic job inputs/results, controlled SQLite ownership, immutable presentation snapshots, cancellation, and explicit main-thread UI updates.
4. Keep simulation fidelity selectable only when its consequences are documented and validated; default watched/background matches retain the agreed equivalence contract.

**Verify:** Before/after Release benchmarks, identical deterministic outputs, concurrency stress/race tooling where supported, cancellation/save/exit during work, and no UI stalls beyond budget.

**Exit:** Performance targets are met on the recorded hardware without silently dropping fixtures, reducing rules, or changing game outcomes through scheduling order.

### T42 — Multi-season stability and economic balance

- [ ] T42 Prove the world survives long careers without drift or collapse.

1. Run at least ten standard-world seeds for 20 seasons; include lower-budget clubs, multiple manager changes, promotion/relegation, and continental participation.
2. Track population, age/role/ability distributions, wages, transfer fees, debt, insolvency, club competitiveness, player minutes, injuries, histories, and simulation/runtime/database growth.
3. Compare seeded replay with periodic reloads and recovery interruptions. Add a 100-season reduced-world stress case for counters, IDs, date arithmetic, and archive retention.
4. Fix economy/development feedback loops and data leaks, then rerun affected cohorts using holdout seeds. Keep believable variance and failure rather than forcing every club to prosper.

**Verify:** Zero corrupt saves, orphan references, duplicate IDs/fixtures/payroll, permanently invalid squads, or unexplained monotonic memory growth. Prespecified economy/population envelopes pass.

**Exit:** Long-term careers remain playable, varied, and financially/population-wise coherent; performance growth follows the documented retention policy.

### T43 — Onboarding and deliberate pacing

- [ ] T43 Help a new manager reach meaningful play quickly.

1. Add a short skippable onboarding path: choose club → understand objective → inspect squad → pick tactic → delegate routine work → play first match.
2. Add context-specific help, readable football terms, recommended actions with reasons, and a persistent way to revisit guidance.
3. Measure default-flow time/clicks/interruptions: target first meaningful tactical choice within five minutes and first match within ten minutes for a scripted novice-speed journey, excluding download/build time.
4. Target a normal delegated highlights session completing one in-game week in about 5–10 minutes, with no mandatory acknowledgement of routine informational messages. Keep detailed control available.
5. Add difficulty/assistance presets that change information/help/delegation transparently; avoid hidden match-result manipulation to make the player win.

**Verify:** Novice-speed input journey, keyboard route, skip/revisit tutorial, no tutorial state leaking across saves, and friction report across contender/mid-table/relegation starts.

**Exit:** A user can start playing from the application itself and understand the key choices without reading developer documentation.

### T44 — Accessibility, localization, and settings completeness

- [ ] T44 Make every released workflow usable in supported configurations.

1. Finish English/Italian key coverage, plurals, dates, currency/number formatting, long strings, diacritics, and font fallback. Eliminate hard-coded production labels.
2. Complete keyboard navigation, visible focus, configurable text scale, color-independent statuses, reduced motion, audio controls, and clear hit areas. Document any platform assistive-technology limitations honestly.
3. Add graphics presets, window/fullscreen modes, resolution/UI scale, vsync/frame limit, input bindings where supported, and reset-to-safe-default recovery.
4. Verify settings are user-local, migrate across versions, and apply safely during management and matches without changing simulation outcomes.

**Verify:** Complete workflow matrix in both languages, effective minimum viewport, 4K/200%, ultrawide, keyboard-only route, missing translation fixture, and broken settings recovery.

**Exit:** Supported languages/settings have complete usable workflows; accessibility claims match tested capabilities.

### T45 — Content packs and safe customization

- [ ] T45 Make data-driven content practical without destabilizing saves.

1. Define versioned pack schemas for clubs, leagues, players, rules, kits/crests, and tuning with validation, namespaces, dependency order, and provenance.
2. Add a local pack selection/import UI with readable errors, preview counts, duplicate/conflict detection, and explicit new-save versus existing-save compatibility rules.
3. Validate malformed JSON, invalid references, impossible dates/values, path traversal, oversized resources, missing assets, and incompatible rule versions before applying any change.
4. Ship the complete fictional release world and a tiny documented example pack. Persist pack versions/hashes in saves; never silently reinterpret an existing career after pack edits.

**Verify:** Valid/invalid import fixtures, atomic failure, missing-pack save behavior, deterministic generation, asset resolution, and release-world completeness checks.

**Exit:** Another contributor can add a valid content pack using documented examples, and invalid/custom data cannot silently damage a save.

### T46 — Packaging, installs, and update recovery

- [ ] T46 Build reproducible distributable applications with no source-tree dependency.

1. Add install/package rules for binaries, assets, shaders, runtime libraries, licenses/credits, version metadata, and default packs. Resolve assets relative to installation/resources, not compiled source paths.
2. Produce a Linux package and a Windows package using portable Release settings and pinned toolchains. Test Windows path/case/DPI behavior on an actual Windows environment or qualified runner.
3. Test first launch, writable user-data creation, offline play, read-only install locations, paths with spaces/non-ASCII, missing dependencies, update over previous version, and save recovery.
4. Add clear crash/error reporting and locally saved diagnostic bundles with opt-in export. No automatic telemetry/upload is needed.

**Verify:** Launch the installed package away from the checkout, run a complete smoke journey, save/reload, and upgrade an old fixture save. Inspect package contents and provenance manifest.

**Exit:** Supported-platform packages run independently and preserve user data. An untested platform is recorded as blocked/unsupported, never as validated.

### T47 — Adversarial gameplay and final regression repair

- [ ] T47 Close exploits, destructive edge cases, and integration regressions.

1. Attack the full game through repeated bids, double-click Continue, reload-before-random-event, contract/loan arbitrage, wage underflow, zero-player squads, abandoned matches, and tactical exploit presets.
2. Exercise UI churn while background work runs: open/sell player, change club, save, cancel, resize, minimize, change renderer/settings, and exit during a safe boundary.
3. Re-run every milestone journey against packaged builds; audit all visible actions, tooltips, help, and documentation for promises the game does not meet.
4. Fix and minimize each discovered failure, add a meaningful regression, and update the failure corpus. Do not add broad unrelated features during hardening.

**Verify:** Sanitizer/lint suites, failure/recovery corpus, migration history, statistical holdouts affected by fixes, UI/visual baselines, and performance reruns only where fixes affect them.

**Exit:** No known reproducible crash, save loss, progression blocker, severe visual defect, or trivial dominant exploit remains in the supported release workflows.

### T48 — Evaluate fun and reduce the final review burden

- [ ] T48 Assess decision quality and assemble concise acceptance evidence.

1. Run three 60-minute-equivalent scripted playstyles: tactics-focused, recruitment-focused, and development-focused, each with a distinct club constraint.
2. For each, record meaningful decisions, visible consequences, interesting reversals/stories, periods of waiting, unnecessary clicks, confusing advice, and recovery after failure.
3. Use a written rubric: clarity, agency, feedback, variety, attachment, pace, and fairness. Require a concrete observed example and remediation for every weak area; do not use an unexplained self-awarded score as proof of fun.
4. If human feedback is available, fold it into this report and fix confirmed friction. Otherwise label enjoyment conclusions as agent-assessed and unvalidated by players; provide an optional ten-minute review route.
5. Produce a one-page release overview, annotated contact sheet, short gameplay clip, three ready-to-play scenario saves generated by tooling, and a compact risk list.

**Verify:** Every claimed consequence is backed by event/state evidence; no fake player feedback, fabricated retention metrics, or hidden skipped gates. Re-run journeys affected by usability fixes.

**Exit:** The game offers understandable, consequential choices and distinct playstyles; the user can review the result from a short packet without repeating months of manual verification.

### T49 — Final completion audit and handoff

- [ ] T49 Confirm all promised work is complete and hand over the release candidate.

1. Validate all T00–T48 checkboxes against evidence and the final source/assets. Reconcile README, architecture, controls/help, compatibility policy, known limitations, and this document.
2. Run the final supported-platform clean build/package checks, fast/full correctness suites, save migrations/recovery, representative statistical holdout, required long-career evidence, and hardware visual/performance acceptance on the final candidate.
3. Verify that later changes did not invalidate earlier evidence; rerun affected checks rather than blindly repeating unrelated expensive experiments. Record final source/diff/asset hashes.
4. Deliver the review packet, package locations, reproduction commands, benchmark comparison, migration notes, and concise walkthrough required by `AI_GUIDELINES.md`.
5. Leave commits/pushes/publication to the user under the current repository policy. Distinguish “implementation and validation complete” from “human reviewed,” “merged,” and “published.”

**Verify:** No incomplete tasks, required checks marked skipped, pending production art, unmeasured graphics requirement, unverified package platform, or unsupported claims. Review the final diff for unrelated changes and generated debris.

**Exit:** All defined gameplay, realism, professional UI, realistic 3D, stability, performance, and packaging criteria have evidence. Set status to COMPLETE and stop the implementation loop; optional backlog items do not reopen it.

## 6. Failure handling and keeping the sequence honest

| Situation | Required response |
| --- | --- |
| A focused test fails | Reproduce with saved input/seed, fix the underlying issue, rerun focused and affected integration checks. |
| The existing baseline already fails | Preserve evidence; fix within the relevant foundation task or record a scoped repair before a dependent gate. Do not normalize failure as success. |
| Failure seems flaky | Reproduce with deterministic seeds and isolated paths; investigate timing/ordering. Do not make the runner pass by retrying until green. |
| Benchmark fluctuates | Check fixture/hardware/power/load, repeat a controlled comparison, retain all samples; do not choose the fastest run. |
| UI screenshot differs | Inspect the actual image and semantic layout; distinguish intended change from regression. Update reference only with documented evidence. |
| Realism metrics pass but clips look wrong | Fix the physical/decision/presentation inconsistency; numerical averages alone do not satisfy T28/T40. |
| Hardware or a required platform is unavailable | Complete safe independent work inside the task, record the exact missing run, and leave the gate blocked. Do not substitute a dummy SDL capture for 3D validation. |
| External source/data is unavailable | Use an already licensed pinned fixture or clearly labeled synthetic assumption where acceptance allows; retain empirical-validation work if required. |
| Art is missing | Use explicitly tracked placeholders during implementation; a placeholder cannot pass a production-art gate. |
| Work exceeds one session | Record current substep, modified files, commands/results, artifact paths, and the next action; keep task unchecked and resume. |
| A design choice exceeds accepted scope | Prepare the smallest reviewable proposal with evidence, cost/benefit, and rollback path; ask once for the missing decision. |
| A milestone is blocked | Repair inside it; do not mark later tasks done to manufacture progress or quietly downgrade the release target. |

## 7. Primary references and how to use them

These references informed the plan; they are not dependency pins or automatic specifications. Recheck technical APIs and applicable terms at implementation time, then record exact versions. Short descriptions below summarize only the part used.

- [Football Manager's in/out-of-possession tactics](https://www.footballmanager.com/fm26/features/possession-out-possession-fm26s-new-tactical-evolution): reference for separate phase shapes and linking tactical roles to recruitment/training. Our design choices and scope are specified above.
- [Football Manager's recruitment systems](https://www.footballmanager.com/fm26/features/powered-transferroom-fm26s-recruitment-revamp): reference for needs-based recruitment, role-aware scouting, and reducing repetitive management work. Do not copy product-specific branding or assets.
- [SDL3 GPU API](https://wiki.libsdl.org/SDL3/CategoryGPU): primary documentation for SDL's cross-platform 3D/compute API; T34 must validate integration in this application's window/UI architecture.
- [Dear ImGui Test Engine](https://github.com/ocornut/imgui_test_engine): input-driven UI automation and capture facilities. Its engine has its own license; inspect the exact pinned version rather than assuming all ImGui-related code has the same terms.
- [StatsBomb/Hudl open data](https://github.com/hudl/open-data): candidate event datasets for offline calibration. Inspect coverage, metric definitions, and current applicable terms before use; do not assume complete world coverage or unrestricted redistribution.
- [IFAB laws documents by edition](https://www.theifab.com/laws-of-the-game-documents/), [Law 3: players](https://theifab.com/laws/latest/the-players/), and [Law 11: offside](https://theifab.com/laws/latest/offside/): primary references for the law-coverage task. Pin the actual edition used by a save; latest web pages can change.

## 8. Resume prompt

Use after the user has accepted implementation of this plan:

```text
Implement the accepted TODO.md plan. Read repository instructions and the latest
handoff, inspect the current worktree, and continue with the lowest-numbered
unchecked T00–T49 task. Complete its substeps and applicable validation gates,
repair failures, write truthful evidence, update progress, then continue with
the next task. Preserve unrelated changes and personal saves. Do not ask me
to approve routine choices already covered by this plan. Keep working until
all tasks pass or a concrete missing resource/authority blocks the current
gate. Do not fake passing checks, relax acceptance criteria, skip realistic
3D graphics, or stop at a plan for a task. Follow the current no-commit/no-push
policy. Before any interruption, save a precise handoff for the next loop.
```
