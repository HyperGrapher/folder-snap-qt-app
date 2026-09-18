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
