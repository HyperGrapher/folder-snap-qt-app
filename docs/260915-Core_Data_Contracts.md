# Core data contracts — first backend milestone

The `folder_snap_core` library implements phase 2's domain, JSON, path and exclusion
primitives. It depends only on Qt Core. It performs no filesystem I/O and has no UI
objects. Public operations report invalid input through `foldersnap::DomainError`,
including an error code and a one-based rule line when applicable.

## Persisted values

- Configuration, index and snapshot documents use schema version 2. Snapshot headers
  repeat that version. Unsupported versions, malformed JSON, wrong field types,
  unsafe paths, duplicate identities and inconsistent snapshot totals are rejected.
- Byte counts and entry timestamps use signed 64-bit integers through Qt's integer
  JSON APIs, never a double conversion. Attributes are unsigned 32-bit values.
- UTC timestamps preserve nanoseconds and encode with nine fractional digits plus
  `Z`. Decoding also accepts explicit numeric timezone offsets. Values outside the
  signed 64-bit Unix-nanosecond range are rejected, including FILETIME conversions.
- `ignoreConfig` is `{ "rules": [...], "hash": "lowercase SHA-256 hex" }`. The hash
  covers the exact raw ordered strings joined with a newline; it is checked on load.
- Entry identities are lowercase relative paths; display paths retain their casing.
  Only files contribute bytes. Reparse entries are metadata, not traversal requests.
- Snapshot encoding sorts entries by path and warnings by path then operation.
  Decoding accepts unsorted unique entries and records a runtime `entriesSorted` hint.
- Index encoding sorts newest-first, breaking ties by snapshot ID ascending.
  `payloadAvailable` is runtime-only and defaults to false until storage checks it.
- History commits are protected by a local lock file, reject duplicate IDs, write the
  immutable gzip payload before the index, and retain only the newest configured
  records per root. Retention uses temporary `.deleting` tombstones so an index-save
  failure can restore the old payloads. Description edits only rewrite the index.
  Explicit deletion and root-history clearing use the same transaction pattern;
  startup repair restores referenced tombstones and reconstructs valid orphan payloads.
- Empty descriptions may be omitted. Missing `createdNs` means zero. Missing
  `lastScanError` means empty; absent optional timestamps mean no recorded time.
  Descriptions allow at most 500 Unicode code points, including emoji.
- Schedules support manual, interval (1/3/6/12 hours), daily, weekly and monthly.
  Irrelevant schedule JSON fields are rejected, not silently converted. A manual
  schedule cannot contain a next-due timestamp. Due-time calculation comes later.
- Configuration/index JSON uses two-space indentation; snapshot JSON is compact.
  Decode limits are 16 MiB, 32 MiB and 1 GiB respectively. Snapshot payloads use
  gzip streams, and the reader enforces the decoded limit while inflating, before
  accepting the full output.

## Windows paths and exclusions

- Drive-absolute and UNC roots normalize separators, dot segments and drive casing.
  Extended drive/UNC prefixes are accepted and normalized to display form. Long
  paths are handled lexically without a 260-character cutoff.
- Relative paths and storage IDs reject traversal, empty components, device names,
  alternate data streams, invalid characters and trailing spaces/dots. Containment
  compares complete path components, so `C:/work-other` is not inside `C:/work`.
- Containment is **lexical**, not a filesystem security guarantee. The scanner and
  cleanup platform adapter must inspect reparse-point ancestors and handle native
  long paths before any live traversal or mutation.
- Rules support `*`, `?`, `**`, root anchoring, directory suffixes, comments, and
  ordered `!` negations. Backslashes normalize to separators; whitespace is trimmed
  for matching but preserved for diagnostics/hash. Brackets are literal characters.
- A matching ancestor exclusion applies to descendants. Last matching rule wins,
  allowing explicit child reinclusion. Directory pruning is conservative: any
  negation disables user-rule pruning so an included child cannot be skipped.
- The application data subtree is a separate literal exclusion that no user rule
  can override. Watching that directory, or a directory inside it, is rejected.

## Verification and remaining work

`tst_domain`, `tst_paths`, `tst_ignore` and `tst_storage` cover canonical fixtures, all schedule kinds,
Unicode, precision beyond JavaScript's safe integer range, timestamp boundaries,
malformed values, duplicate identities, path attacks, exclusion order and protection.

Verified on 2026-09-15 with Qt 6.11.1/MinGW 13.1: the application builds and all seven
CTest suites pass (paths, ignore, domain, storage, appstate, ui, window). The existing native
window test runs at the normal desktop scale; no additional screenshot suite was added.

Configuration and history-index storage now uses Qt's same-directory `QSaveFile`
replacement. Missing documents return defaults; malformed or schema-incompatible
documents are moved to `corrupt/` before defaults are returned. Filesystem failures
are reported instead of being misclassified as corrupt. `StoragePaths::fromDataDirectory`
requires an absolute path for development and test isolation; `forCurrentUser()` uses
Qt's per-user local application-data location. Snapshot payloads are gzip-compressed,
with availability detected from the payload filename. Scanning and live UI models are
not implemented by this milestone. No watched-folder contents are modified.
