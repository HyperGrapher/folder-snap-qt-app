# FolderSnap Implementation Checklist

Status: active  
Target: Windows 10/11 x64, Qt 6.11, C++20, Qt Quick/QML  
Primary toolchain: Qt 6.11.1 MinGW 64-bit, MinGW 13.1, Ninja, and vcpkg  
Product scope: complete feature set described in `260915-FolderSnap_Qt_CPP_Implementation_Plan.md`

This is the working implementation plan. Checkboxes are updated only after the corresponding work is implemented and verified. The earlier Qt/C++ implementation plan remains the detailed behavior and persistence contract; this document defines the build order, UI direction, architecture, and approval gates.

## Product and engineering decisions

- FolderSnap is a local-only metadata snapshot and comparison application. It never stores file contents.
- The UI prototype was delivered first with deterministic fake data; the current milestone replaces it with persisted local data and real metadata scans.
- The existing frameless Windows shell, real rounded corners, resize behavior, background shader, retained-page transitions, reduced-motion option, and background-motion option will be kept and refined.
- The template's Aura branding and demo-only concepts will be removed rather than maintained through compatibility wrappers.
- Qt Quick/QML owns presentation. C++ owns domain rules, persistence, long-running work, scheduling, and Windows integration.
- Core domain and storage code will not depend on QML or visual objects.
- Worker code communicates with the UI through queued signals and immutable/value-style result objects. The GUI thread never scans folders, decodes gzip payloads, calculates large diffs, performs cleanup, or exports large reports.
- Qt facilities and the C++ standard library are preferred. zlib is the only initial third-party runtime dependency.
- Only the root `build/` directory is used for all configurations and test output.

## UI direction

The visual design will be an original FolderSnap interface rather than a reskinned template.

- A dark, soft-glass shell with the FolderSnap icon and wordmark in the sidebar.
- A restrained midnight/indigo base with cyan and violet ambient shader colors. Status colors remain accessible and are reinforced with labels/icons.
- A persistent left sidebar with four destinations: **Overview**, **Folders**, **Compare**, and **Settings**.
- A compact root selector and current-job indicator remain available in the page header where relevant. A primary **Take snapshot** action stays easy to reach.
- Overview shows watched-folder health, latest snapshot summaries, storage/history totals, scheduling state, and recent activity.
- Folders uses a master-detail layout: watched folders on the left; selected-folder snapshot timeline, warnings, actions, retention, exclusions, and schedule on the right.
- Compare provides explicit A/B snapshot selection, summary chips, search and filters, an expandable change tree, warning/scope banners, exports, and the entry point to cleanup.
- Settings contains appearance/motion, tray/startup behavior, notifications, defaults, data/log locations, and diagnostics.
- Secondary workflows use focused sheets/dialogs: add folder, edit folder settings, snapshot details, warning viewer, export progress, cleanup review, and destructive confirmation.
- Empty, loading, progress, unavailable-payload, archived, error, and cancellation states are designed in the fake-data prototype instead of being added late.
- Page changes use short opacity plus directional movement. Rapid navigation retargets transitions; invisible pages cannot receive input.
- Reduced motion disables continuous shader movement and replaces page movement with a minimal fade or immediate transition.

## Planned source structure

```text
src/
├── main.cpp
├── domain/              value types, enums, validation, serialization DTOs
├── paths/               normalization, containment, safe joins
├── ignore/              ordered gitignore-like rule compiler/matcher
├── storage/             atomic files, configuration, history, gzip
├── scanner/             metadata scanner and Windows metadata adapter
├── diff/                comparison engine and result-tree projection
├── export/              HTML/CSV DTOs and writers
├── cleanup/             candidate plan, preflight, execution, audit
├── schedule/            pure due-time calculations and scheduler
├── application/         orchestration, jobs, UI-facing models/controllers
├── platform/windows/    tray, startup, single instance, shell operations
└── qml/                 shell, navigation, pages, controls, dialogs, effects
```

Tests mirror these responsibilities under `tests/`, with fixtures under `tests/fixtures/`.

## Phase 0 — Plan and build foundation

- [x] Review the template PRD, FolderSnap behavior guide, current source tree, icons, and copied dependency setup.
- [x] Confirm the full feature scope and UI-first delivery order.
- [x] Define the FolderSnap UI direction and long-term module boundaries.
- [x] Create this tracked implementation checklist.
- [x] Rename the CMake project, executable, QML module, test targets, and remaining runtime branding from Aura to FolderSnap.
- [x] Move the project to C++20 and declare all Qt modules needed by the complete application.
- [x] Add a minimal vcpkg manifest for zlib using the existing cached baseline.
- [x] Make vcpkg manifest installation explicitly opt-in during CMake configuration so normal regenerations do not reinstall dependencies.
- [x] Add the FolderSnap PNG to Qt resources and the ICO to Windows executable resources.
- [x] Complete the user's first dependency installation and clean build in the single `build/` folder.
- [x] Run the existing template tests as a foundation regression check.

Exit: a clean FolderSnap-branded foundation configures, builds, launches, and passes the retained shell tests.

## Phase 1 — UI prototype with fake data

- [x] Replace demo state with a clearly isolated `UiPreviewState`/fake model layer that cannot touch the filesystem.
- [x] Establish the final color, type, spacing, elevation, radius, focus, and motion tokens in `Theme.qml`.
- [x] Refine the frameless title bar, resize hit testing, Windows 10 region corners, and Windows 11 native corner behavior.
- [x] Display `foldersnap-icon` in the title/sidebar brand and application/taskbar surfaces.
- [x] Build the persistent navigation shell and retained-page transition host.
- [x] Build the Overview page with realistic watched-folder, snapshot, schedule, warning, and job data.
- [x] Build the Folders master-detail page with snapshot timeline and folder settings presentation.
- [x] Build explicit A/B selection and the full Compare page with a realistic multi-level change tree.
- [x] Build the Settings page, including reduced-motion and background-animation controls.
- [x] Build fake-data versions of add-folder, snapshot-details, warnings, export, cleanup review, and confirmation sheets/dialogs.
- [x] Cover empty, active scan, compare progress, errors, missing payload, archived folder, warning, and large-list states.
- [x] Add keyboard focus states, tab navigation, accessible names, scalable text, and color-independent statuses.
- [x] Verify the normal and minimum layouts at 100% Windows scaling, maximization, rapid navigation, and motion pausing while hidden/minimized.
- [x] Inspect representative screens and key overlays; retain only the overview verification screenshot.
- [x] Obtain user approval for the visual direction before connecting real functionality.

Exit: every planned workflow can be reviewed visually using fake data, with no real file or system operations.

## Phase 2 — Domain contracts and safety primitives

- [x] Implement schema-v2 domain types, enums, typed errors, UUID handling, and UTC timestamp conversion.
- [x] Implement JSON encoding/decoding for config, index, snapshot headers, entries, warnings, schedules, and ignore configuration.
- [x] Add canonical fixtures covering every field, Unicode, zero values, fractional timestamps, warnings, and all schedule kinds.
- [x] Implement normalized watched-root and relative identity paths with Windows case-insensitive semantics.
- [x] Implement storage-ID validation, lexical containment-checked joins, long-path normalization, and data-directory self-protection. Native long-path access and reparse checks belong to scanner/cleanup integration.
- [x] Implement the ordered ignore matcher, negation handling, validation diagnostics, and test-path result.
- [x] Implement atomic same-directory replacement and corrupt-file backup helpers.
- [x] Add unit tests for schema validation, precision/boundary cases, path attacks, containment, and ignore behavior.

First backend milestone: the Qt-Core-only `folder_snap_core` library and three new
test suites are implemented. See [core contracts](260915-Core_Data_Contracts.md).
The prototype UI is now backed by persisted local configuration and history. The
scanner and live AppState wiring are intentionally incremental; scheduling, full
diff classification, exports, cleanup, and Windows lifecycle work remain open.

Exit: persistence values round-trip without semantic loss, and unsafe paths/identifiers are rejected before any filesystem mutation.

## Phase 3 — Configuration and history storage

- [x] Implement `%LOCALAPPDATA%\FolderSnap` paths plus a mandatory development/test override.
- [x] Implement config defaults, load/save, size limits, schema checks, and malformed-config preservation.
- [x] Implement gzip snapshot encoding/decoding with zlib and the 1 GiB decoded-size guard.
- [x] Implement the global lightweight history index and payload-availability detection.
- [x] Serialize history mutations and prove concurrent saves cannot lose records.
- [x] Implement description edits without mutating immutable snapshot payloads.
- [x] Implement per-root retention after successful saves.
- [x] Implement transactional snapshot deletion, root-history clearing, and tombstone rollback.
- [x] Implement startup repair for tombstones, orphan payloads, corrupt index reconstruction, and missing payload records.
- [x] Add interruption, corruption, concurrency, retention-isolation, and recovery tests.

Exit: history remains consistent across failures and restart-repair scenarios.

Storage milestone: configuration and the history index now use `QSaveFile` atomic
replacement, preserve malformed documents under `corrupt/`, and have an explicit
absolute-directory constructor for development and tests. History commits use a
cross-instance lock, write payloads before the index, preserve immutable payload
descriptions, and apply per-root retention with rollback tombstones. Deletion,
root-history clearing, and startup repair now recover tombstones and valid orphan
payloads; broader interruption simulation remains part of later integration work.

## Phase 4 — Metadata scanner

- [ ] Implement cancellable traversal with a default of four directory workers, a configurable ceiling of 32, and batches of 256.
- [ ] Enforce two concurrent root scans globally and one active scan per root.
- [x] Capture file, directory, reparse, and other metadata without reading file contents.
- [x] Never follow reparse/symlink directories; capture link targets when possible.
- [x] Apply ignore rules and mandatory data-directory protection correctly, including negation-safe traversal.
- [ ] Distinguish fatal root failures from recoverable descendant warnings.
- [x] Produce deterministic sorted entries, sorted warnings, counts, total bytes, and bounded progress updates.
- [ ] Honor cancellation throughout traversal, collection, sorting, and save handoff.
- [ ] Test determinism across worker counts, partial failures, cancellation, exclusions, reparse points, Unicode, and long paths.

Exit: equivalent scans produce deterministic snapshots without blocking the GUI or reading content.

## Phase 5 — Diff engine and comparison projection

- [x] Implement chronological validation and a linear merge over path-sorted snapshot entries.
- [x] Implement Added, Removed, Modified, Unchanged, Uncertain, and Scope Difference classifications exactly as documented.
- [x] Calculate counts and byte summaries without retaining unchanged entries.
- [ ] Implement cancellation and stale-result protection for the single active comparison.
- [ ] Build the expandable multi-level result tree with synthesized parents.
- [ ] Calculate recursive Before/After folder sizes from complete snapshots.
- [ ] Implement filter/search, folder-first presentation, effective-size ordering, and deterministic natural-name tie-breaking.
- [ ] Add large mostly-unchanged fixture tests and all classification/scope/warning cases.

Exit: large comparisons remain responsive and return the documented classifications, summaries, and tree ordering.

## Phase 6 — Application service and scheduler

- [ ] Implement watched-folder add, duplicate rejection, rename, archive/unarchive, settings, and Explorer commands.
- [ ] Offer an immediate first snapshot after folder registration.
- [ ] Implement scan request orchestration, the two-scan limit, per-root coalescing, and Manual-before-Scheduled ordering.
- [ ] Expose bounded UI-facing progress, completion, warning, failure, and configuration signals.
- [ ] Implement Manual, 1/3/6/12-hour, daily, weekly, and monthly schedules.
- [ ] Implement local-time calendar calculation, UTC persistence, DST behavior, monthly clamping, missed-run collapse, and anchor preservation.
- [ ] Implement clean cancellation and service shutdown.
- [ ] Add orchestration and schedule tests, including sleep/resume and archived roots.

Exit: manual and scheduled snapshot work is reliable, cancellable, persisted, and correctly serialized.

## Phase 7 — Connect the approved UI to real services

- [x] Replace fake overview values with UI-facing models backed by the live `AppState` models.
- [x] Connect folder registration, folder configuration, archive, Explorer, snapshot, and history actions.
- [x] Connect scan progress, cancellation, warnings, and failure recovery.
- [x] Implement the exact explicit A/B click, rollover, deselection, refresh, and root-change rules.
- [x] Connect comparison calculation, filters, search, expansion, and summaries.
- [x] Preserve usable empty/loading/error/missing-payload states when switching from fake to real data.
- [ ] Add QML/UI integration tests for all primary workflows and accessibility behavior.

Exit: all non-export and non-cleanup core workflows operate on real local data without GUI-thread stalls.

## Phase 8 — Snapshot and comparison export

- [ ] Create the versioned standalone `resources/snapshot-export-template.html` supporting snapshot and comparison DTOs.
- [ ] Use safe JSON injection and DOM text APIs so stored paths cannot inject markup or script.
- [ ] Implement offline multi-level tree rendering, search, filters, ancestor visibility, and sibling sorting.
- [ ] Implement accessible Added/Deleted/Modified/warning row treatments for comparison reports.
- [ ] Implement snapshot and comparison DTO builders with decimal-string sizes and ISO dates.
- [ ] Implement UTF-8 CSV writers with RFC 4180 quoting and a documented Excel-compatible BOM choice.
- [ ] Run decode and export work off the GUI thread with cancellation and atomic final output.
- [ ] Connect per-snapshot and completed-comparison export actions, progress, and errors.
- [ ] Test hostile strings, Unicode, quoting, missing payloads, offline operation, large reports, and immutability.

Exit: saved snapshots and completed comparisons export complete offline HTML and CSV reports without touching live files or history.

## Phase 9 — Safe cleanup to Recycle Bin

- [ ] Build cleanup candidates only from definite Added entries and begin with nothing selected.
- [ ] Implement hierarchical selection, descendant propagation, indeterminate parents, persistent selection through filtering, and byte totals.
- [ ] Implement containment, reparse-ancestor, live-type, metadata, link-target, and untracked-directory-content preflight checks.
- [ ] Propagate blocked descendants to selected parent directories.
- [ ] Present Ready, Blocked, and Already Missing results before explicit confirmation.
- [ ] Re-run preflight immediately before mutation to close the time-of-check/time-of-use window.
- [ ] Execute deepest-first through Windows `IFileOperation` with Recycle Bin/undo semantics.
- [ ] Never fall back to permanent deletion.
- [ ] Write an append-preserving JSONL audit and present per-item results.
- [ ] Add safety tests for traversal, reparse, casing, races, extra content, Shell abort/failure, and audit ID validation.

Exit: only unchanged, contained Added items can be moved to the Recycle Bin, with a durable audit trail.

## Phase 10 — Windows lifecycle integration

- [ ] Implement named-mutex ownership plus `QLocalServer`/`QLocalSocket` activation for a race-safe single instance.
- [ ] Implement the persistent FolderSnap tray icon, left-click restore, and Open/Snapshot/Settings/Quit actions.
- [ ] Implement close-to-tray and explicit Quit semantics.
- [ ] Implement scheduled failure and optional success notifications.
- [ ] Implement `--background` startup without briefly showing the window.
- [ ] Implement HKCU startup registration and executable-path repair.
- [ ] Implement Explorer actions for watched roots and the FolderSnap data directory.
- [ ] Implement rotating local operational logs with UTC timestamps and privacy-safe content.
- [ ] Verify per-monitor-v2 DPI behavior, taskbar/tray branding, and Windows 10/11 corner behavior.

Exit: lifecycle, activation, tray, startup, Explorer, logging, and branding work correctly on Windows 10/11.

## Phase 11 — Release hardening and delivery

- [ ] Run all unit, integration, QML, native-window, and Windows integration tests from a clean build.
- [ ] Test inaccessible and network roots, large trees/histories, long paths, Unicode, sleep/resume, restart, and overdue schedules.
- [ ] Measure idle, animated, reduced-motion, minimized, scanning, and comparison resource usage.
- [ ] Verify no decorative animation runs while hidden or minimized.
- [ ] Verify every destructive action has specific confirmation and no permanent-delete fallback exists.
- [ ] Run `clang-format` and `qmlformat`, then inspect the final diff for obsolete template code and branding.
- [ ] Update README build, test, deployment, data location, privacy, and operational instructions.
- [ ] Produce a deployable folder with `windeployqt` and all required runtime assets.
- [ ] Perform clean-machine Windows 10/11 smoke tests and record any platform limitations.

Exit: the complete product meets the guide's definition of done and is ready for normal use.

## Approval gates

- [x] Gate A — User confirms the first dependency install and foundation build succeed.
- [x] Gate B — User approves the fake-data UI direction.
- [ ] Gate C — Domain/storage contracts and safety primitives pass review and tests.
- [ ] Gate D — Live snapshot and comparison workflows pass end-to-end testing.
- [ ] Gate E — Export, cleanup, and Windows integration pass safety/release review.
