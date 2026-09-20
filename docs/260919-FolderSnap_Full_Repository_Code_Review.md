# FolderSnap full repository code review

Review date: 2026-09-19  
Scope: application C++, QML, tests, CMake, resources, installer, packaging script, README, and `docs/`. Generated `build/` output, vendored `third_party/`, and the ignored `FROM_OTHER_QT_APP/` reference tree were not reviewed as product source.

## Verification completed

- [x] Read the product contracts, implementation plan/checklist, README, and repository instructions.
- [x] Reviewed the scan, storage, diff, schedule, export, cleanup, application-state, Windows-integration, QML, test, build, and packaging paths.
- [x] Confirmed the current incremental build completes successfully.
- [x] Ran all 15 CTest targets after the remediation batches; all fifteen pass, including the native-window test without QML typed-property warnings.
- [x] Re-verified the affected targets after each remediation batch and kept generated build products under `build/`.

Checkboxes below are intentionally open and are ordered by risk. Check an item only after its implementation and focused regression tests are complete.

## Critical

- [x] **CR-01 — Cleanup can recycle a same-size file changed within the same millisecond.**
  - **Location:** `src/scanner/MetadataScanner.cpp:41-53, 116-117`; `src/cleanup/CleanupPreflight.cpp:62-75, 152-153, 393-399`.
  - **Problem:** Both snapshot capture and cleanup validation obtain a millisecond `QDateTime` and multiply it by 1,000,000 while calling the value nanoseconds. NTFS stores 100 ns timestamps. A same-size replacement whose last-write time differs only below one millisecond compares equal and is eligible for cleanup.
  - **Contract:** `docs/260915-FolderSnap_Qt_CPP_Implementation_Plan.md:128-130, 302, 606-609` and `docs/260918-FolderSnap_Cleanup_Contract.md` require native timestamp metadata and revalidation before mutation.
  - **Resolution:** Added a non-following native metadata adapter using `FILE_BASIC_INFO`, extended paths, and checked `FILETIME` conversion. Scanner and cleanup now compare the full native timestamp precision; `capturesSubMillisecondNativeTimestamps()` covers the Windows path.
- [x] **CR-02 — Final directory cleanup does not require the directory to be empty and the per-item TOCTOU window remains open.**
  - **Location:** `src/cleanup/CleanupExecutor.cpp:275-276, 330-388`; `src/cleanup/CleanupPreflight.cpp:266-349`; `tests/tst_cleanup.cpp:310-355`.
  - **Problem:** The executor performs one bulk preflight, then later moves files without rechecking each target immediately before the Shell call. Its final directory check calls preflight with `{path}`; this expands all candidate descendants and treats a still-present selected child as allowed, rather than requiring zero children. A replaced file, a new/reappeared child, or a changed ancestor can therefore be moved after the safety decision. The existing mock test reports a child as moved without removing it and still expects the parent directory to be moved.
  - **Contract:** `docs/260915-FolderSnap_Qt_CPP_Implementation_Plan.md:617-624` explicitly requires revalidation and an empty-directory check immediately before mutation.
  - **Resolution:** Every ready item is re-preflighted immediately before its move, directory moves additionally require a successful live empty-directory check, and failed descendants keep their parents in place. `executionRevalidatesAndMovesDeepestFirst()`, `executionRevalidationRejectsAFileReplacedByAnEarlierMove()`, and `finalDirectoryRevalidationKeepsNonEmptyParent()` cover disappearing, replacement, ordering, and non-empty-directory races.

## High priority

- [ ] **HI-01 — Windows Shell item failures can be reported as successful cleanup moves.**
  - **Location:** `src/cleanup/CleanupExecutor.cpp:133-207`.
  - **Problem:** `PerformOperations() == S_OK` plus `GetAnyOperationsAborted() == false` does not prove that an individual `DeleteItem` succeeded. `IFileOperation` reports item-level failures through its progress sink, so a failed item can be marked `MovedToRecycleBin`; that false result also weakens the parent-directory guard.
  - **Resolution so far:** Recycle Bin moves now pass an `IFileOperationProgressSink` to `DeleteItem`, require a reported successful `PostDeleteItem` result, and retain the original-path postcondition check. An integration fixture that forces an item-level Shell failure while the overall operation returns success remains open.
- [ ] **HI-02 — Reparse boundaries are incomplete at and above the watched root.**
  - **Location:** `src/AppState.cpp:1703-1717, 1509-1526`; `src/scanner/MetadataScanner.cpp:138-157, 230-260`; `src/cleanup/CleanupPreflight.cpp:200-263`.
  - **Problem:** Folder registration accepts a root that is a junction/symlink, scanning enumerates it, and cleanup starts ancestor checks at the root rather than at the volume/UNC share. A root reached through a reparse ancestor can redirect scanning or cleanup outside the intended physical tree. In addition, `requestSnapshot` catches a data-subtree validation failure and disables the mandatory protection instead of rejecting the scan.
  - **Resolution:** Added non-following component-by-component reparse inspection for registration, scanning, and cleanup; reparse roots/ancestors are rejected, and protected-data validation failures now fail the scan instead of disabling protection. Existing junction/preflight coverage and the full suite pass; expanded moved-data-directory race coverage remains a release-hardening follow-up.
- [ ] **HI-03 — Long-path safety is lexical only in code that performs native I/O.**
  - **Location:** `src/scanner/MetadataScanner.cpp:85-90`; `src/cleanup/CleanupPreflight.cpp:98-107`; `src/cleanup/CleanupExecutor.cpp:154-156`.
  - **Problem:** Native calls receive ordinary drive paths without a `\\?\`/extended UNC form. `GetFileAttributesW` failure is silently converted to attributes `0` during scanning, while cleanup can reject or misclassify a path over the legacy limit. The scanner long-path test checks only that Qt finds the file, not that native attributes and cleanup work.
  - **Resolution so far:** Centralized extended drive/UNC conversion and applied it to native metadata, scanner attributes, cleanup probes, and Shell path lookup. Scanner and cleanup now cover a >260-character Unicode file with native metadata and a real cleanup callback; a Recycle Bin integration test remains open.
- [x] **HI-04 — Configuration write failures are swallowed and then reported as success.**
  - **Location:** `src/AppState.cpp:1146-1204, 1703-1750, 2060-2084, 2303-2314`.
  - **Problem:** Callers mutate live state first. `saveConfiguration()` catches `DomainError` and returns no failure status, so callers continue to emit preference changes, enqueue work, or replace the failure toast with “Folder added”/“saved.” The session then contains state that silently disappears after restart.
  - **Resolution:** Configuration mutations now stage a validated copy, persist it, and publish the in-memory change only after success; callers return without success toasts/signals when persistence fails. `configurationWriteFailureDoesNotPublishLiveState()` uses a file-backed data-path fault to verify add-folder and preference writes remain unchanged.
- [x] **HI-05 — Clearing history races an active scan and can resurrect cleared operational state.**
  - **Location:** `src/qml/pages/FoldersPage.qml:291-299`; `src/AppState.cpp:1891-1904`; `src/storage/HistoryStore.cpp:241-321`.
  - **Problem:** Unlike root removal, Clear history is enabled and executed while the root is scanning. An in-flight commit can add a snapshot immediately after the clear. `HistoryStore` also clears persisted `lastSnapshotUtc`/`lastScanError`, but `AppState` does not update its in-memory configuration, so a later save can write the stale values back.
  - **Resolution:** Clear-history is disabled/rejected while a root is active, and successful clearing resets the in-memory root status. `HistoryStore::clearRootHistory()` also persists status resets when the root has no records, avoiding the previous early-return gap. `clearHistoryRejectsAnActiveScan()` invokes the action from the scan-start boundary and verifies the snapshot is not cleared mid-commit.
- [x] **HI-06 — Saving folder preferences can silently turn a valid custom calendar schedule into Manual.**
  - **Location:** `src/qml/dialogs/PreviewDialog.qml:163-172, 701-703`; `src/AppState.cpp:177-202, 1758-1803`.
  - **Problem:** The schema supports arbitrary daily time, weekly weekday/time, and monthly day/time. The UI offers only 09:00/Monday/day 1. Any other persisted value maps to index zero (`Manual only`), so saving an unrelated name, retention, or exclusion change disables the schedule.
  - **Resolution:** Schedule parsing now round-trips exact daily, weekly, and monthly text values, and the preferences model retains a persisted custom value instead of falling back to index zero. `preservesNonDefaultCalendarScheduleWhenSavingPreferences()` covers a non-default daily schedule; dedicated per-field calendar controls remain a future usability improvement.
- [ ] **HI-07 — Cleanup and comparison workers have no explicit shutdown protocol.**
  - **Location:** `src/AppState.cpp:621, 936-947, 1063-1074`; `src/AppState.h:546-548`.
  - **Problem:** `AppState` has a default destructor. Unlike scan and export coordinators, it neither cancels nor waits for comparison, preflight, or cleanup execution. Destructive cleanup can continue after the UI/controller is destroyed, with results unavailable to the user.
  - **Resolution:** `AppState` now stops scheduling and cancels/waits for cleanup execution, cleanup preflight, and comparison futures during destruction. Quit-during-worker stress coverage remains a release-hardening follow-up.
- [ ] **HI-08 — “Start with Windows” is exposed but does nothing, and `--background` is ignored.**
  - **Location:** `src/qml/pages/SettingsPage.qml:102-109`; `src/AppState.cpp:1158-1167`; `src/main.cpp:15-24, 69`.
  - **Problem:** The setting only writes JSON. No HKCU Run value is created/repaired, arguments are not parsed, and the window is always shown. The UI therefore promises behavior the executable cannot provide. The implementation checklist already marks both items unfinished.
  - **Resolution:** Added a Windows startup adapter that writes/removes a quoted HKCU Run command and repairs the executable path when enabled. `main()` honors `--background` without showing the window when a tray is available; registry/lifecycle integration tests remain a release-hardening follow-up.
- [ ] **HI-09 — Scheduled failures are suppressed when “Notify before” is disabled.**
  - **Location:** `src/AppState.cpp:1527-1530`; `src/platform/windows/WindowsNotificationController.cpp:154-170, 281-299`.
  - **Problem:** Quiet scheduled scans add the root to `m_suppressedScheduledRoots`; failure handling removes it and returns without showing an error. The product contract requires scheduled failures to notify regardless of success-notification preference.
  - **Resolution:** Failure notifications no longer return early when the scheduled-success suppression flag is present; only the success/prompt suppression state is consumed.
- [x] **HI-10 — Scanner enumeration is eager, memory-heavy, and cannot distinguish an empty directory from enumeration failure.**
  - **Location:** `src/scanner/MetadataScanner.cpp:259-303`.
  - **Problem:** `entryInfoList()` materializes every child before the advertised 256-entry batching and before cancellation can be checked. A directory with millions of entries can cause a large allocation and an unresponsive cancel. A permission/race failure can return no children without a warning, producing definite removals from incomplete data.
  - **Resolution:** Replaced the eager `entryInfoList()` materialization with a lazy native `FindFirstFileExW`/`FindNextFileW` adapter (with a Qt fallback), bounded progress batches, per-entry cancellation checks, and explicit empty-versus-error reporting. Scanner tests cover empty-directory success, missing-directory failure mapping, descendant disappearance warnings, and Unicode/long paths.
- [ ] **HI-11 — Cleanup selection expansion is quadratic.**
  - **Location:** `src/cleanup/CleanupPreflight.cpp:524-541`; `src/qml/dialogs/PreviewDialog.qml:509-512`.
  - **Problem:** For each selected path, preflight scans every candidate. “Select all” therefore performs O(n²) path checks, contrary to the cleanup contract’s large-plan responsiveness requirement.
  - **Resolution:** Selection roots are normalized into a set and candidates walk their bounded ancestor chains once, removing the selected-root × candidate nested scan. A large-plan timing test remains useful for release sign-off.
- [ ] **HI-12 — Worst-case diff classification repeatedly recompiles regexes and rescans all warnings.**
  - **Location:** `src/diff/DiffEngine.cpp:50-84, 181-190`.
  - **Problem:** Every one-sided entry constructs a fresh `IgnoreMatcher` and linearly scans the missing snapshot’s warnings. A 100k-entry all-added/all-removed comparison becomes O(entries × (rules + warnings)) with repeated regex compilation. The current large test is mostly unchanged entries and misses this case.
  - **Resolution:** Diff comparison now compiles one ignore matcher per side and builds warning prefix indexes before the merge. The existing diff suite passes; a 100k one-sided benchmark remains a performance follow-up.
- [ ] **HI-13 — Comparison search/filter work is quadratic on the GUI thread.**
  - **Location:** `src/AppState.cpp:1235-1287`; `src/qml/pages/ComparePage.qml:403-407, 455-463`.
  - **Problem:** Each nonmatching row scans the full match set to decide whether it is an ancestor, and this recomputes synchronously for every search keystroke. Large changed comparisons can freeze the interface.
  - **Resolution so far:** `displayedChanges()` now builds the match-ancestor set once per calculation and QML search input is debounced before updating the model. A 100k GUI responsiveness test and a fully projected model remain open.

## Medium priority

- [x] **ME-01 — Scanner destroys filesystem display casing.**
  - **Location:** `src/scanner/MetadataScanner.cpp:96-102, 271-273`; `src/paths/WindowsPaths.cpp:106-119`.
  - **Problem:** The relative path is lowercased first and then assigned to both `path` and `displayPath`. UI and exports lose the actual filesystem casing, violating the persisted-entry contract.
  - **Resolution:** Scanner now stores separator-normalized filesystem casing separately from lowercase identity; `preservesFilesystemDisplayCasing()` covers a mixed-case Unicode path.
- [x] **ME-02 — Comparison HTML reports real folder sizes as `0 B → 0 B`.**
  - **Location:** `src/export/ExportBuilder.cpp:216-225`; `resources/snapshot-export-template.html:200-212`.
  - **Problem:** The DTO includes correct recursive `folderSizes`, but rendering prefers the real directory entries’ `sizeBytes`, which are always zero. Recursive totals are only used as a fallback.
  - **Resolution:** Comparison HTML folder rows now use the recursive `report.folderSizes` values for both real and synthesized folders. `comparisonDtoAndCsvContainBothSides()` verifies recursive before/after folder totals survive into the embedded HTML report.
- [x] **ME-03 — Comparison incompleteness is collapsed into a misleading “unreadable paths” number.**
  - **Location:** `src/AppState.cpp:310-316`; `src/qml/pages/ComparePage.qml:360-386`; `src/qml/dialogs/PreviewDialog.qml:327-338`.
  - **Problem:** Before/after warnings, Uncertain entries, and Scope Difference entries are added together. The UI labels the sum as unreadable paths, never exposes `ignoreRulesDiffer`, and the Review action loads warnings for one incidental snapshot instead of the active pair.
  - **Resolution:** AppState now exposes separate before/after warning, uncertain, scope-difference, and ignore-rule-change fields; the comparison banner labels those categories, and the Review action loads warnings from the active before/after pair. `scansAndComparesARealFolder()` injects before/after warning payloads and verifies the projected counts and source labels.
- [x] **ME-04 — Flat diff ordering is lexical rather than natural numeric.**
  - **Location:** `src/diff/DiffEngine.cpp:225-236`; natural comparator currently private in `src/diff/ComparisonTree.cpp:60-116`.
  - **Problem:** Same-kind results place `file10` before `file2`, contrary to the storage-level ordering contract.
  - **Resolution:** Exposed and reused the tree’s natural path comparator for flat diff ordering; `ordersSameKindEntriesNaturally()` covers `file2`/`file10`.
- [ ] **ME-05 — Opening scan warnings can decode up to a 1 GiB snapshot on the GUI thread.**
  - **Location:** `src/AppState.cpp:1971-1995`; `src/qml/dialogs/PreviewDialog.qml:327-338`.
  - **Problem:** A QML binding calls synchronous gzip load/decode. Large payloads can freeze the window and the operation cannot be cancelled.
  - **Action:** Load warning details asynchronously or persist lightweight warning details separately. Add a large-payload responsiveness test.
- [x] **ME-06 — CSV intended for Excel allows formula injection.**
  - **Location:** `src/export/ExportBuilder.cpp:108-127, 260-263, 287-295`.
  - **Problem:** RFC 4180 quoting does not stop spreadsheet formulas. Windows filenames may begin with `=`, `+`, `-`, or `@`, so opening an exported CSV in Excel can evaluate attacker-controlled text.
  - **Resolution:** CSV text fields beginning with `=`, `+`, `-`, or `@` are prefixed with an apostrophe before RFC 4180 quoting; JSON/HTML metadata remains raw. The export test covers a leading `=` formula.
- [x] **ME-07 — Cleanup audit growth causes increasing rewrite cost and eventually prevents new audit records.**
  - **Location:** `src/cleanup/CleanupExecutor.cpp:26, 220-254`; `src/storage/AtomicFile.cpp:34-52`.
  - **Problem:** Every cleanup reads and atomically rewrites the entire audit. Once it exceeds 64 MiB, the size guard throws after filesystem mutations, so the latest cleanup has no audit line and every later attempt repeats the failure.
  - **Resolution:** Cleanup audit events append to the active JSONL file, rotate to uniquely named segments at the 64 MiB boundary, and validate that the audit directory/file is writable before any selected item is moved. `rotatesFullAuditBeforeRecordingTheNextCleanup()` covers the limit boundary.
- [ ] **ME-08 — Scan cancellation stops at the history-save handoff.**
  - **Location:** `src/application/ScanCoordinator.cpp:23-37`; `src/storage/HistoryStore.cpp:123-185`; `src/storage/SnapshotStore.cpp:159-165`.
  - **Problem:** Once traversal returns, JSON encoding, gzip compression, payload writing, index writing, and retention are not cancellable. A large scan can continue for a long time after the user cancels, while holding the history mutation lock during encoding/compression.
  - **Resolution so far:** Cancellation now reaches snapshot gzip compression, atomic payload writing, and history retention/index checkpoints; cancelled payload writes are removed before the history index can reference them. `snapshotSaveCancellationStopsCompressionAndLeavesNoPayload()` covers the compression path. JSON encoding and reducing the mutation-lock duration remain open.
- [ ] **ME-09 — Worker/resource cleanup is not RAII-safe.**
  - **Location:** `src/storage/SnapshotStore.cpp:29-60, 72-120`; `src/scanner/MetadataScanner.cpp:345-353`.
  - **Problem:** Cancellation throws from inside `gzipDecompress` before `inflateEnd`, leaking zlib state. A `std::thread` creation failure leaves already-created joinable threads in a vector whose destruction terminates the process.
  - **Resolution:** zlib streams now use scope guards, and scanner workers use `std::jthread` with partial-start cleanup signaling. Existing cancellation/worker determinism tests pass; fault-injection coverage remains open.
- [x] **ME-10 — Current desktop verification is red and QML initializes typed properties from `undefined`.**
  - **Location:** `tests/tst_window.cpp:302-318`; `src/WindowsWindowController.cpp:142-216`; `src/qml/pages/FoldersPage.qml:122,137,149`; `src/qml/dialogs/PreviewDialog.qml:142,156,221`.
  - **Problem:** The window test fails consistently because native caption double-click leaves the window `Windowed`. The same run warns when an empty `currentRoot` map feeds typed `string`/`bool` properties. These warnings can hide later binding defects.
  - **Resolution:** Guarded empty QML map fields with typed defaults, made custom schedules visible in the selector, and made the Windows test close the shell-hosted “Choose a folder to watch” dialog before reactivating the app window for caption checks. All 15 CTest targets pass without the prior warnings.
- [x] **ME-11 — Future calendar schedules keep the old local clock time after a system time-zone change.**
  - **Location:** `src/schedule/ScheduleCalculator.cpp:121-159`.
  - **Problem:** If a calendar schedule already has a future UTC due value, evaluation returns it unchanged. After the machine time zone changes, the next daily/weekly/monthly run occurs at the old zone’s local time; only later runs re-anchor.
  - **Resolution:** Calendar evaluation now recalculates the next wall-clock occurrence whenever it runs, while interval schedules retain their persisted due time. `reanchorsFutureCalendarDueTimeAfterTimeZoneChange()` covers a persisted UTC due value after switching to Europe/Berlin.
- [x] **ME-12 — Snapshot history eagerly instantiates every row even when retention is unlimited.**
  - **Location:** `src/qml/pages/FoldersPage.qml:194-264`.
  - **Problem:** A `Repeater` inside a `ScrollView` creates every delegate. Retention `0` is unlimited, so memory and layout cost are unbounded.
  - **Resolution:** Folder history now uses a bounded, reusable `ListView` with a vertical scrollbar instead of a `Repeater`; the QML workflow test verifies delegate reuse and model population.
- [ ] **ME-13 — Root removal is both crash-inconsistent and contradicted by the product documents.**
  - **Location:** `src/storage/HistoryStore.cpp:324-403`; `src/AppState.cpp:1912-1950`; `src/qml/pages/FoldersPage.qml:281-289`; plan `docs/260915-FolderSnap_Qt_CPP_Implementation_Plan.md:265`; checklist `docs/260915-FolderSnap_Implementation_Checklist.md:168-170`.
  - **Problem:** The detailed plan forbids permanent root removal, while the later checklist requires it. If retained, the transaction tombstones payloads, saves the reduced index, then saves configuration; a crash between the two leaves the root registered and startup repair deletes its now-unindexed tombstones, permanently losing history.
  - **Action:** Resolve and document the product decision first. If removal remains, add a durable cross-file transaction marker and interruption tests at every persistence boundary; otherwise remove the workflow and use archive plus separately confirmed history clearing.
- [ ] **ME-14 — Core/application boundaries are too broad for safe continued development.**
  - **Location:** `src/AppState.h:62-139, 454-549`; `src/AppState.cpp`; `src/qml/dialogs/PreviewDialog.qml`; `src/scanner/MetadataScanner.cpp`; `src/cleanup/CleanupPreflight.cpp`; `src/cleanup/CleanupExecutor.cpp`; `CMakeLists.txt:31-57`.
  - **Problem:** `AppState` combines persistence, scheduling, scan, comparison, cleanup, export, and UI projection; one 715-line dialog switches unrelated workflows by string; generic core modules embed Win32/COM and publicly export Windows libraries. These are divergent-change hotspots and contradict the documented focused-module/platform-adapter design.
  - **Action:** Split comparison, cleanup, scheduling/preferences, and roots/history coordinators/view models; split workflow dialogs; move Win32 probing and Recycle Bin operations behind narrow adapters under `src/platform/windows/`; make OS libraries private to the Windows target.

## Low priority / documentation and tooling

- [x] **LO-01 — README status is stale.**
  - **Location:** `README.md:17-18`.
  - **Problem:** It says scheduling, exports, cleanup, and Windows lifecycle integration remain planned, while the implementation checklist and source say most of those are implemented. This misdirects reviewers and users.
  - **Resolution:** Updated the README to describe the implemented scan, scheduling, export, cleanup, notification, and Windows lifecycle features and to call out remaining release-hardening and unfinished settings/logging work.
- [x] **LO-02 — Several visible controls are placeholders rather than honest unavailable states.**
  - **Location:** `src/qml/pages/SettingsPage.qml:188-203`; checklist `docs/260915-FolderSnap_Implementation_Checklist.md:227-230`.
  - **Problem:** “Data folder” only shows a toast, “View logs” admits no viewer, and there is no rotating logger. These look actionable but do not perform the documented Explorer/diagnostic workflows.
  - **Resolution:** The Data folder action opens the configured data directory in Explorer. The unavailable log viewer is visibly disabled and labeled “coming soon” until rotating diagnostics are implemented.
- [x] **LO-03 — Packaging permits alternate build directories despite the repository’s single-build-folder rule.**
  - **Location:** `scripts/BuildInstaller.ps1:1-2, 111-120`.
  - **Problem:** `-BuildDirectory` allows products and logs outside the root `build/` directory, conflicting with `AGENTS.md` and making cleanup/reproducibility less predictable.
  - **Resolution:** Removed `-BuildDirectory`; the installer script always resolves products and logs under the repository’s `build/` directory.
- [ ] **LO-04 — Obsolete compatibility shims and string-driven UI abstractions remain.**
  - **Location:** `src/qml/Main.qml:18`; `src/domain/JsonCodec.cpp:486-490`; `src/qml/dialogs/PreviewDialog.qml`.
  - **Problem:** The live state is still called `UiPreviewState` solely to preserve old composition, configuration decoding still accepts the deprecated `notifyScheduledSuccess` field, and unrelated dialog kinds are stringly typed. This conflicts with the repository rule to remove obsolete compatibility paths and makes invalid states easy to create.
  - **Resolution so far:** QML now exposes `AppState` directly and configuration decoding no longer accepts the obsolete `notifyScheduledSuccess` field. The string-driven multi-workflow dialog remains open for a later component split.

## Suggested implementation order

1. Complete the remaining **HI-01** and **HI-03** integration coverage before treating cleanup as release-safe.
2. Add lifecycle/integration tests for the partially covered **HI-01**, **HI-03**, **HI-07**, **HI-08**, and **HI-09** behavior.
3. Address the remaining **HI-13** GUI projection work and the open **ME-05/ME-08** UI-thread/cancellation paths before large-tree/history performance sign-off.
4. Resolve the export, lifecycle, architecture, and documentation items, then run the full release matrix from the implementation checklist.
