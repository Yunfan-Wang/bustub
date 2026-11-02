# Project 2 Design Document

Yunfan Wang

yunfanw2

## Tombstones

Each leaf node stores physical entry indexes in a fix sized FIFO buffer. The page size counts the physical entries, which includes tombstones. Point searches and iterators exclude tombstoned entries. We may compare it to KeyAt, which returns the physical key. With no tombstone capacity, deletion shifts the arrays immediately. Otherwise, deletion appends an index. If the buffer is already full, the oldest entry is physically removed first. The shifts update every affected tombstone index. Reinserting a previously deleted key will remove its tombstone and replaces its RID without adding a second physical entry.

When those entries move between leaves, their tombstones moves alongside with them. A split preserves the relative deletion order within each resulting page. Borrowing and merging will append incoming deletions after the recipient's existing deletions, so the recipient's older deletions are processed first if their combined buffer exceeds capacity. Temporary storage holds only the moved tombstone keys. All tree nodes still remain buffer pool pages.

## B+Tree Operations (Insertion, Deletion, Point Search)

Binary search is like regular b tree search to start, chooses an internal child or the insertion position in a leaf. Internal slot zero contains a child pointer with an unused key. Separators are lower bounds for their right subtrees similar as like in bst. They need not be rewritten when an ordinary deletion removes a leaf's first key.

A leaf will split when insertion adds its physical size to its maximum. Its upper half moves to a new right sibling, and the new sibling's first key is inserted into the parent. A full internal page will split while incorporating the incoming child pointer. This will not write past its arrays. Splits propagate upward until a parent has room. Splitting the root will create a new internal root and update the header page.

Deletion will repair physical underflow. It chooses the left sibling when one exists, otherwise the right sibling. It borrows while the sibling has surplus entries. Moving a tombstone can flush an older deletion in the recipient, so a leaf may need more than one borrowed entry. If borrowing cannot restore occupancy, the right page will merge into the left page and the parent thus loses a child. Internal underflow propagates upward. 
A root with one child collapses into that child, a physically empty root leaf clears the header. Removed pages are returned to the buffer pool. A tree containing only buffered deletions might retain physical pages, but its iterator is still at the end.

## Index Iterator

The iterator holds a rguard for its current leaf and skips tombstones. At the leaf boundary it: 1. records the next page id, 2. releases the current guard 3. reads the next leaf. It repeats this across leaves with no visible entries. This keeps at most one leaf pinned during iteration. Concurrent structural changes during a scan are outside the supported iterator contract. Begin(key) is started at the first visible key that's greater than or equal to the requested key.

## Concurrency Control

Searches descend with r latch coupling by acquiring the child before releasing the parent. Updates first try an optimistic descent using rguards, then it acquires the target leaf's w guard while its parent remains r-locked. Root leaf updates use the header as that protecting ancestor. An update that cannot change the tree structure will naturally finish with one leaf wguard.

An unsafe update releases the optimistic guard and restarts with w latch crabbing. It retains ancestors only until a page can handle the possible insertion or deletion without the need to propagate a split or merge. Guards release retained ancestors from the header downward. Root safety also accounts for root collapse. Sibling changes will occur under the parent's w latch. Which prevents competing structural changes at that parent. So there is no tree-wide mutex.

## Leaderboard Optimizations

N/A

## AI Usage
Clarification:
1. Internal page size means child count.
An internal page with size 3 has three child pointers and two meaningful separator keys. Key slot 0 is unused, but child slot 0 is essential. I thought it was something like 5 in that scenario.

Design validation:
1. A leaf split copies a boundary key into the parent while keeping the data entry in the leaf. An internal split will propagate a boundary into the parent and the corresponding rightpage key slot becomes its unused slot #0.

2. Safe means the pending operation cannot require changing that parent. Optimistic updates first lock only the leaf for writing. If a split or merge might be necessary, they must release it and restart with ancestor w latches trying to acquire a parent while holding its child can cause deadlock.