# FolderSnap export contract

FolderSnap export DTOs use schema version 1 and an explicit `reportType` of `snapshot` or
`comparison`. File sizes and recursive folder sizes are decimal strings so JavaScript cannot lose
64-bit precision. Known timestamps are UTC ISO strings with nanoseconds; unavailable entry times
are JSON `null` and empty CSV fields.

Snapshot CSV columns are `path`, `displayPath`, `type`, `sizeBytes`, `createdAtUtc`,
`modifiedAtUtc`, `attributes`, and `linkTarget`. Comparison CSV includes both sides' type, size,
creation, and modification metadata plus change classification. CSV files use UTF-8 with a BOM for
Windows Excel compatibility, CRLF rows, and RFC 4180 quoting.

DTO and CSV generation consumes immutable in-memory snapshots and comparison results. It does not
read watched folders or modify snapshot payloads, history, configuration, or comparison selection.
Background file writing and UI wiring are implemented separately.

The packaged standalone HTML template contains exactly one `/* FOLDERSNAP_REPORT_DATA */` marker.
Injection escapes `<`, `>`, `&`, U+2028, and U+2029 before replacing that marker. Report rendering
uses DOM text nodes for stored values, makes no network requests, and supports snapshot and
comparison trees with debounced search, comparison filters, ancestor visibility, expansion, and
deterministic sibling sorting.
