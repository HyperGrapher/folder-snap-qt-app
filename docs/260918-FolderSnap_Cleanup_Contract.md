# FolderSnap cleanup contract

Cleanup candidates come only from entries classified as definite `Added` changes in a completed
comparison. `Uncertain`, scope-difference, modified, removed, unchanged, and structural-only tree
rows are never selectable candidates. Structural ancestor rows may still be shown as non-selectable
context so an added descendant's hierarchy is unambiguous. Opening cleanup starts with no
selection, and this stage does not touch the watched folder.

Selecting a candidate directory selects every candidate below it. Clearing a selected directory
clears every candidate below it. Clearing a descendant also clears its selected ancestors; those
ancestors remain visibly indeterminate while another descendant is selected. Selecting the last
unselected descendant completes its candidate ancestors automatically.

Filtering changes only the visible candidate rows. It does not add or remove selections, and
matching descendants retain their candidate ancestors for context. Selected bytes count regular
file candidates once, so selecting both a directory and its descendants does not double-count
recursive folder sizes.

The modal virtualizes candidate rows, reuses delegates, caches the definite-Added projection and
selection-state sets, and debounces filter updates. Opening or selecting within a large cleanup
plan must not instantiate one QML control per candidate or rebuild the candidate model.

This selection model is only input to the later preflight stage. Selection never means that an
item is safe to move, and no cleanup mutation is available until containment, reparse, live
metadata, type, link-target, and untracked-directory-content checks have completed.

The read-only preflight implementation returns one result for every selected candidate. It
normalizes and containment-checks each path, rejects reparse-point ancestors, compares live type
and tracked metadata, verifies stored reparse targets, and recursively checks selected directory
content without following reparse directories. A selected directory inherits a blocked status from
any blocked selected descendant; `already_missing` descendants do not block their parent. The
preflight result is safe to display but does not mutate the watched folder.

The cleanup review runs preflight asynchronously after a non-empty selection. Its result is
discarded when the comparison, watched root, or selection changes, so the modal never presents a
stale safety decision. The review exposes per-item status badges and Ready, Blocked, and Already
Missing totals. The explicit cleanup action performs a second worker-side preflight immediately
before mutation, processes deepest paths first, and keeps a parent directory blocked when a child
move fails or when a final directory check finds new content.

On Windows, each validated item is submitted to `IFileOperation` with Recycle Bin and undo flags.
Shell failures and aborts are reported per item; FolderSnap never falls back to permanent
deletion. Each attempt appends one compact JSONL event under FolderSnap's root-specific local
data directory, preserving prior audit lines through an atomic rewrite. The modal reports moved,
blocked, already-missing, and failed totals and keeps the originals restorable from the Recycle
Bin.

The cleanup safety suite covers traversal rejection, Windows reparse ancestors, case-insensitive
selection, extra directory content, disappearing-path races, Shell abort/failure handling, and
audit ID validation.
