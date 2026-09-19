# FolderSnap full repository code review

Review date: 2026-09-19  
Scope: application C++, QML, tests, CMake, resources, installer, packaging script, README, and `docs/`. Generated `build/` output, vendored `third_party/`, and the ignored `FROM_OTHER_QT_APP/` reference tree were not reviewed as product source.

## Verification completed

- [x] Read the product contracts, implementation plan/checklist, README, and repository instructions.
- [x] Reviewed the scan, storage, diff, schedule, export, cleanup, application-state, Windows-integration, QML, test, build, and packaging paths.
- [x] Confirmed the current incremental build completes successfully.
- [x] Ran all 15 CTest targets. Fourteen pass; `window` fails consistently at `tests/tst_window.cpp:316` because caption double-click does not maximize. The run also emits QML `undefined`-to-typed-property warnings.
- [x] Kept this review read-only apart from this requested report.

Checkboxes below are intentionally open and are ordered by risk. Check an item only after its implementation and focused regression tests are complete.

## Critical

- [ ] **CR-01 — Cleanup can recycle a same-size file changed within the same millisecond.**
  - **Location:** `src/scanner/MetadataScanner.cpp:41-53, 116-117`; `src/cleanup/CleanupPreflight.cpp:62-75, 152-153, 393-399`.
  - **Problem:** Both snapshot capture and cleanup validation obtain a millisecond `QDateTime` and multiply it by 1,000,000 while calling the value nanoseconds. NTFS stores 100 ns timestamps. A same-size replacement whose last-write time differs only below one millisecond compares equal and is eligible for cleanup.
  - **Contract:** `docs/260915-FolderSnap_Qt_CPP_Implementation_Plan.md:128-130, 302, 606-609` and `docs/260918-FolderSnap_Cleanup_Contract.md` require native timestamp metadata and revalidation before mutation.
  - **Action:** Read Windows metadata through a non-following native handle, convert `FILETIME` with the existing checked conversion, and compare the full available precision. Add a Windows test with two same-size files whose write times differ by less than 1 ms.
- [ ] **CR-02 — Final directory cleanup does not require the directory to be empty and the per-item TOCTOU window remains open.**
  - **Location:** `src/cleanup/CleanupExecutor.cpp:275-276, 330-388`; `src/cleanup/CleanupPreflight.cpp:266-349`; `tests/tst_cleanup.cpp:310-355`.
  - **Problem:** The executor performs one bulk preflight, then later moves files without rechecking each target immediately before the Shell call. Its final directory check calls preflight with `{path}`; this expands all candidate descendants and treats a still-present selected child as allowed, rather than requiring zero children. A replaced file, a new/reappeared child, or a changed ancestor can therefore be moved after the safety decision. The existing mock test reports a child as moved without removing it and still expects the parent directory to be moved.
  - **Contract:** `docs/260915-FolderSnap_Qt_CPP_Implementation_Plan.md:617-624` explicitly requires revalidation and an empty-directory check immediately before mutation.
  - **Action:** Re-open and validate every item and all ancestors immediately before `moveItem`. Immediately before moving a directory, enumerate it and require it to contain no entries. Add race tests in which moving item A replaces item B and in which a child remains/reappears before the parent move.

## High priority

- [ ] **HI-01 — Windows Shell item failures can be reported as successful cleanup moves.**
  - **Location:** `src/cleanup/CleanupExecutor.cpp:133-207`.
  - **Problem:** `PerformOperations() == S_OK` plus `GetAnyOperationsAborted() == false` does not prove that an individual `DeleteItem` succeeded. `IFileOperation` reports item-level failures through its progress sink, so a failed item can be marked `MovedToRecycleBin`; that false result also weakens the parent-directory guard.
  - **Action:** Register an `IFileOperationProgressSink`, use the per-item result from `PostDeleteItem`, and verify the original path is gone without following reparses. Add an integration test for an item-level Shell failure with an overall successful operation call.
- [ ] **HI-02 — Reparse boundaries are incomplete at and above the watched root.**
  - **Location:** `src/AppState.cpp:1703-1717, 1509-1526`; `src/scanner/MetadataScanner.cpp:138-157, 230-260`; `src/cleanup/CleanupPreflight.cpp:200-263`.
  - **Problem:** Folder registration accepts a root that is a junction/symlink, scanning enumerates it, and cleanup starts ancestor checks at the root rather than at the volume/UNC share. A root reached through a reparse ancestor can redirect scanning or cleanup outside the intended physical tree. In addition, `requestSnapshot` catches a data-subtree validation failure and disables the mandatory protection instead of rejecting the scan.
  - **Action:** Resolve and inspect every component from the drive/UNC boundary through the target using non-following native handles; reject a reparse root or ancestor. Treat data-directory protection failures as fatal. Add root-junction, root-below-junction, moved-data-directory, and reparse-alias tests.
- [ ] **HI-03 — Long-path safety is lexical only in code that performs native I/O.**
  - **Location:** `src/scanner/MetadataScanner.cpp:85-90`; `src/cleanup/CleanupPreflight.cpp:98-107`; `src/cleanup/CleanupExecutor.cpp:154-156`.
  - **Problem:** Native calls receive ordinary drive paths without a `\\?\`/extended UNC form. `GetFileAttributesW` failure is silently converted to attributes `0` during scanning, while cleanup can reject or misclassify a path over the legacy limit. The scanner long-path test checks only that Qt finds the file, not that native attributes and cleanup work.
  - **Action:** Centralize extended native path conversion and use it for all Win32/Shell metadata and mutation calls. Assert attributes and cleanup behavior in real paths longer than 260 characters.
- [ ] **HI-04 — Configuration write failures are swallowed and then reported as success.**
  - **Location:** `src/AppState.cpp:1146-1204, 1703-1750, 2060-2084, 2303-2314`.
  - **Problem:** Callers mutate live state first. `saveConfiguration()` catches `DomainError` and returns no failure status, so callers continue to emit preference changes, enqueue work, or replace the failure toast with “Folder added”/“saved.” The session then contains state that silently disappears after restart.
  - **Action:** Stage changes in a validated copy, persist it, and swap it into live state only after success. Propagate or explicitly return write failures and never emit success on failure. Add fault-injection tests using an unwritable data directory.
- [ ] **HI-05 — Clearing history races an active scan and can resurrect cleared operational state.**
  - **Location:** `src/qml/pages/FoldersPage.qml:291-299`; `src/AppState.cpp:1891-1904`; `src/storage/HistoryStore.cpp:241-321`.
  - **Problem:** Unlike root removal, Clear history is enabled and executed while the root is scanning. An in-flight commit can add a snapshot immediately after the clear. `HistoryStore` also clears persisted `lastSnapshotUtc`/`lastScanError`, but `AppState` does not update its in-memory configuration, so a later save can write the stale values back.
  - **Action:** Disable and reject clearing an actively scanning root, then synchronize the in-memory root from the successful transaction. Add a scan/clear race test and a subsequent-preference-save regression test.
- [ ] **HI-06 — Saving folder preferences can silently turn a valid custom calendar schedule into Manual.**
  - **Location:** `src/qml/dialogs/PreviewDialog.qml:163-172, 701-703`; `src/AppState.cpp:177-202, 1758-1803`.
  - **Problem:** The schema supports arbitrary daily time, weekly weekday/time, and monthly day/time. The UI offers only 09:00/Monday/day 1. Any other persisted value maps to index zero (`Manual only`), so saving an unrelated name, retention, or exclusion change disables the schedule.
  - **Action:** Use typed schedule fields and real time/weekday/day controls initialized from the exact domain value. Add UI round-trip tests for non-default daily, weekly, and monthly schedules.
- [ ] **HI-07 — Cleanup and comparison workers have no explicit shutdown protocol.**
  - **Location:** `src/AppState.cpp:621, 936-947, 1063-1074`; `src/AppState.h:546-548`.
  - **Problem:** `AppState` has a default destructor. Unlike scan and export coordinators, it neither cancels nor waits for comparison, preflight, or cleanup execution. Destructive cleanup can continue after the UI/controller is destroyed, with results unavailable to the user.
  - **Action:** Add deterministic cancellation and `waitForFinished()` in a safe destruction order, preferably behind focused coordinator objects. Test quitting during comparison, cleanup preflight, and multi-item cleanup execution.
- [ ] **HI-08 — “Start with Windows” is exposed but does nothing, and `--background` is ignored.**
  - **Location:** `src/qml/pages/SettingsPage.qml:102-109`; `src/AppState.cpp:1158-1167`; `src/main.cpp:15-24, 69`.
  - **Problem:** The setting only writes JSON. No HKCU Run value is created/repaired, arguments are not parsed, and the window is always shown. The UI therefore promises behavior the executable cannot provide. The implementation checklist already marks both items unfinished.
  - **Action:** Hide/disable the control until the HKCU adapter, quoted command repair, and hidden `--background` startup are implemented. Cover enable/disable, moved executable repair, quoting, and no-window-flash behavior.
- [ ] **HI-09 — Scheduled failures are suppressed when “Notify before” is disabled.**
  - **Location:** `src/AppState.cpp:1527-1530`; `src/platform/windows/WindowsNotificationController.cpp:154-170, 281-299`.
  - **Problem:** Quiet scheduled scans add the root to `m_suppressedScheduledRoots`; failure handling removes it and returns without showing an error. The product contract requires scheduled failures to notify regardless of success-notification preference.
  - **Action:** Suppress only the prompt/progress/success path, never failure notifications. Add notification-policy tests for scheduled success and failure with both preference states.
- [ ] **HI-10 — Scanner enumeration is eager, memory-heavy, and cannot distinguish an empty directory from enumeration failure.**
  - **Location:** `src/scanner/MetadataScanner.cpp:259-303`.
  - **Problem:** `entryInfoList()` materializes every child before the advertised 256-entry batching and before cancellation can be checked. A directory with millions of entries can cause a large allocation and an unresponsive cancel. A permission/race failure can return no children without a warning, producing definite removals from incomplete data.
  - **Action:** Stream enumeration with a native/iterator adapter that reports errors, process bounded batches, and check cancellation while reading. Test a huge single directory and a mid-enumeration failure.
- [ ] **HI-11 — Cleanup selection expansion is quadratic.**
  - **Location:** `src/cleanup/CleanupPreflight.cpp:524-541`; `src/qml/dialogs/PreviewDialog.qml:509-512`.
  - **Problem:** For each selected path, preflight scans every candidate. “Select all” therefore performs O(n²) path checks, contrary to the cleanup contract’s large-plan responsiveness requirement.
  - **Action:** Normalize selections into a set and resolve descendant ranges using sorted paths or a trie in a single bounded pass. Add a large-plan timing/regression test.
- [ ] **HI-12 — Worst-case diff classification repeatedly recompiles regexes and rescans all warnings.**
  - **Location:** `src/diff/DiffEngine.cpp:50-84, 181-190`.
  - **Problem:** Every one-sided entry constructs a fresh `IgnoreMatcher` and linearly scans the missing snapshot’s warnings. A 100k-entry all-added/all-removed comparison becomes O(entries × (rules + warnings)) with repeated regex compilation. The current large test is mostly unchanged entries and misses this case.
  - **Action:** Compile one matcher per side and build an ancestor/prefix warning index before the linear merge. Add 100k one-sided cases with many rules and warnings.
- [ ] **HI-13 — Comparison search/filter work is quadratic on the GUI thread.**
  - **Location:** `src/AppState.cpp:1235-1287`; `src/qml/pages/ComparePage.qml:403-407, 455-463`.
  - **Problem:** Each nonmatching row scans the full match set to decide whether it is an ancestor, and this recomputes synchronously for every search keystroke. Large changed comparisons can freeze the interface.
  - **Action:** Build the visible ancestor set once in O(n × depth), debounce input, and expose a model/proxy rather than recreating a full `QVariantList`. Add a 100k changed-entry responsiveness test.

## Medium priority

- [ ] **ME-01 — Scanner destroys filesystem display casing.**
  - **Location:** `src/scanner/MetadataScanner.cpp:96-102, 271-273`; `src/paths/WindowsPaths.cpp:106-119`.
  - **Problem:** The relative path is lowercased first and then assigned to both `path` and `displayPath`. UI and exports lose the actual filesystem casing, violating the persisted-entry contract.
  - **Action:** Preserve a separator-normalized display path and derive the lowercase identity separately. Add a mixed-case scan-to-export test.
- [ ] **ME-02 — Comparison HTML reports real folder sizes as `0 B → 0 B`.**
  - **Location:** `src/export/ExportBuilder.cpp:216-225`; `resources/snapshot-export-template.html:200-212`.
  - **Problem:** The DTO includes correct recursive `folderSizes`, but rendering prefers the real directory entries’ `sizeBytes`, which are always zero. Recursive totals are only used as a fallback.
  - **Action:** For every comparison folder row, render both sides from `report.folderSizes[node.path]`. Add browser-side assertions for changed real folders and synthesized folders.
- [ ] **ME-03 — Comparison incompleteness is collapsed into a misleading “unreadable paths” number.**
  - **Location:** `src/AppState.cpp:310-316`; `src/qml/pages/ComparePage.qml:360-386`; `src/qml/dialogs/PreviewDialog.qml:327-338`.
  - **Problem:** Before/after warnings, Uncertain entries, and Scope Difference entries are added together. The UI labels the sum as unreadable paths, never exposes `ignoreRulesDiffer`, and the Review action loads warnings for one incidental snapshot instead of the active pair.
  - **Action:** Expose separate before/after warning, uncertain, scope-difference, and ignore-rule-change fields and build a comparison-specific review view.
- [ ] **ME-04 — Flat diff ordering is lexical rather than natural numeric.**
  - **Location:** `src/diff/DiffEngine.cpp:225-236`; natural comparator currently private in `src/diff/ComparisonTree.cpp:60-116`.
  - **Problem:** Same-kind results place `file10` before `file2`, contrary to the storage-level ordering contract.
  - **Action:** Share one tested natural comparator between the flat result and tree projection. Add same-kind `file2`/`file10` coverage.
- [ ] **ME-05 — Opening scan warnings can decode up to a 1 GiB snapshot on the GUI thread.**
  - **Location:** `src/AppState.cpp:1971-1995`; `src/qml/dialogs/PreviewDialog.qml:327-338`.
  - **Problem:** A QML binding calls synchronous gzip load/decode. Large payloads can freeze the window and the operation cannot be cancelled.
  - **Action:** Load warning details asynchronously or persist lightweight warning details separately. Add a large-payload responsiveness test.
- [ ] **ME-06 — CSV intended for Excel allows formula injection.**
  - **Location:** `src/export/ExportBuilder.cpp:108-127, 260-263, 287-295`.
  - **Problem:** RFC 4180 quoting does not stop spreadsheet formulas. Windows filenames may begin with `=`, `+`, `-`, or `@`, so opening an exported CSV in Excel can evaluate attacker-controlled text.
  - **Action:** Define and implement a spreadsheet-safe escaping policy for text columns while preserving raw metadata in HTML/JSON. Add hostile leading-character tests.
- [ ] **ME-07 — Cleanup audit growth causes increasing rewrite cost and eventually prevents new audit records.**
  - **Location:** `src/cleanup/CleanupExecutor.cpp:26, 220-254`; `src/storage/AtomicFile.cpp:34-52`.
  - **Problem:** Every cleanup reads and atomically rewrites the entire audit. Once it exceeds 64 MiB, the size guard throws after filesystem mutations, so the latest cleanup has no audit line and every later attempt repeats the failure.
  - **Action:** Implement a documented durable append or atomic rotation/segmentation strategy and validate audit capacity before mutation. Test the limit boundary and write failure.
- [ ] **ME-08 — Scan cancellation stops at the history-save handoff.**
  - **Location:** `src/application/ScanCoordinator.cpp:23-37`; `src/storage/HistoryStore.cpp:123-185`; `src/storage/SnapshotStore.cpp:159-165`.
  - **Problem:** Once traversal returns, JSON encoding, gzip compression, payload writing, index writing, and retention are not cancellable. A large scan can continue for a long time after the user cancels, while holding the history mutation lock during encoding/compression.
  - **Action:** Thread cancellation through serialization/compression/atomic writing and minimize time under the mutation lock. Add cancellation tests during compression and payload/index write handoff.
- [ ] **ME-09 — Worker/resource cleanup is not RAII-safe.**
  - **Location:** `src/storage/SnapshotStore.cpp:29-60, 72-120`; `src/scanner/MetadataScanner.cpp:345-353`.
  - **Problem:** Cancellation throws from inside `gzipDecompress` before `inflateEnd`, leaking zlib state. A `std::thread` creation failure leaves already-created joinable threads in a vector whose destruction terminates the process.
  - **Action:** Wrap zlib state in a scope guard/RAII type and use `std::jthread` or an RAII joining group with stop signaling. Add cancellation and partial-worker-start fault tests.
- [ ] **ME-10 — Current desktop verification is red and QML initializes typed properties from `undefined`.**
  - **Location:** `tests/tst_window.cpp:302-318`; `src/WindowsWindowController.cpp:142-216`; `src/qml/pages/FoldersPage.qml:122,137,149`; `src/qml/dialogs/PreviewDialog.qml:142,156,221`.
  - **Problem:** The window test fails consistently because native caption double-click leaves the window `Windowed`. The same run warns when an empty `currentRoot` map feeds typed `string`/`bool` properties. These warnings can hide later binding defects.
  - **Action:** Reproduce the title-bar interaction with a real double-click and correct either native handling or the test message construction. Guard empty map fields with typed defaults and configure tests to fail on unexpected QML warnings.
- [ ] **ME-11 — Future calendar schedules keep the old local clock time after a system time-zone change.**
  - **Location:** `src/schedule/ScheduleCalculator.cpp:121-159`.
  - **Problem:** If a calendar schedule already has a future UTC due value, evaluation returns it unchanged. After the machine time zone changes, the next daily/weekly/monthly run occurs at the old zone’s local time; only later runs re-anchor.
  - **Action:** Persist the zone/offset basis or detect system-zone changes and recompute future calendar occurrences. Add a test with an existing `nextDueAtUtc` and a changed time zone.
- [ ] **ME-12 — Snapshot history eagerly instantiates every row even when retention is unlimited.**
  - **Location:** `src/qml/pages/FoldersPage.qml:194-264`.
  - **Problem:** A `Repeater` inside a `ScrollView` creates every delegate. Retention `0` is unlimited, so memory and layout cost are unbounded.
  - **Action:** Use a bounded, reusable `ListView` and verify scrolling with a large unlimited history.
- [ ] **ME-13 — Root removal is both crash-inconsistent and contradicted by the product documents.**
  - **Location:** `src/storage/HistoryStore.cpp:324-403`; `src/AppState.cpp:1912-1950`; `src/qml/pages/FoldersPage.qml:281-289`; plan `docs/260915-FolderSnap_Qt_CPP_Implementation_Plan.md:265`; checklist `docs/260915-FolderSnap_Implementation_Checklist.md:168-170`.
  - **Problem:** The detailed plan forbids permanent root removal, while the later checklist requires it. If retained, the transaction tombstones payloads, saves the reduced index, then saves configuration; a crash between the two leaves the root registered and startup repair deletes its now-unindexed tombstones, permanently losing history.
  - **Action:** Resolve and document the product decision first. If removal remains, add a durable cross-file transaction marker and interruption tests at every persistence boundary; otherwise remove the workflow and use archive plus separately confirmed history clearing.
- [ ] **ME-14 — Core/application boundaries are too broad for safe continued development.**
  - **Location:** `src/AppState.h:62-139, 454-549`; `src/AppState.cpp`; `src/qml/dialogs/PreviewDialog.qml`; `src/scanner/MetadataScanner.cpp`; `src/cleanup/CleanupPreflight.cpp`; `src/cleanup/CleanupExecutor.cpp`; `CMakeLists.txt:31-57`.
  - **Problem:** `AppState` combines persistence, scheduling, scan, comparison, cleanup, export, and UI projection; one 715-line dialog switches unrelated workflows by string; generic core modules embed Win32/COM and publicly export Windows libraries. These are divergent-change hotspots and contradict the documented focused-module/platform-adapter design.
  - **Action:** Split comparison, cleanup, scheduling/preferences, and roots/history coordinators/view models; split workflow dialogs; move Win32 probing and Recycle Bin operations behind narrow adapters under `src/platform/windows/`; make OS libraries private to the Windows target.

## Low priority / documentation and tooling

- [ ] **LO-01 — README status is stale.**
  - **Location:** `README.md:17-18`.
  - **Problem:** It says scheduling, exports, cleanup, and Windows lifecycle integration remain planned, while the implementation checklist and source say most of those are implemented. This misdirects reviewers and users.
  - **Action:** Replace milestone prose with an accurate feature/status table, including genuinely unfinished startup, background, Explorer-data-folder, logging, and release verification work.
- [ ] **LO-02 — Several visible controls are placeholders rather than honest unavailable states.**
  - **Location:** `src/qml/pages/SettingsPage.qml:188-203`; checklist `docs/260915-FolderSnap_Implementation_Checklist.md:227-230`.
  - **Problem:** “Data folder” only shows a toast, “View logs” admits no viewer, and there is no rotating logger. These look actionable but do not perform the documented Explorer/diagnostic workflows.
  - **Action:** Hide or explicitly mark unfinished controls until they open Explorer/logs and the privacy-safe rotating log exists.
- [ ] **LO-03 — Packaging permits alternate build directories despite the repository’s single-build-folder rule.**
  - **Location:** `scripts/BuildInstaller.ps1:1-2, 111-120`.
  - **Problem:** `-BuildDirectory` allows products and logs outside the root `build/` directory, conflicting with `AGENTS.md` and making cleanup/reproducibility less predictable.
  - **Action:** Remove the override or constrain the resolved path to the repository’s existing `build/` directory.
- [ ] **LO-04 — Obsolete compatibility shims and string-driven UI abstractions remain.**
  - **Location:** `src/qml/preview/UiPreviewState.qml:3-5`; `src/qml/Main.qml:18`; `src/domain/JsonCodec.cpp:486-495`; `src/qml/dialogs/PreviewDialog.qml`.
  - **Problem:** The live state is still called `UiPreviewState` solely to preserve old composition, configuration decoding still accepts the deprecated `notifyScheduledSuccess` field, and unrelated dialog kinds are stringly typed. This conflicts with the repository rule to remove obsolete compatibility paths and makes invalid states easy to create.
  - **Action:** Expose/type `AppState` directly, remove the legacy JSON-field fallback (or document a required migration if released data needs one), and replace dialog-kind strings with focused components and typed workflow state.

## Suggested implementation order

1. Complete **CR-01**, **CR-02**, **HI-01**, **HI-02**, and **HI-03** before treating cleanup as release-safe.
2. Complete **HI-04** through **HI-09** to make persistence, scheduling, shutdown, and notifications honest and reliable.
3. Address **HI-10** through **HI-13** before large-tree/history testing and performance sign-off.
4. Resolve the export, UI-thread, audit, lifecycle, architecture, and documentation items, then run the full release matrix from the implementation checklist.

