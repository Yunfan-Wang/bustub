# Project 1 Design Document

Damien Wang
yunfanw2

## Replacer

My ARC replacer is built around four lists: two for pages that are currently resident, MRU and MFU, and two ghost lists that remember pages evicted recently. I identify resident entries by frame id and ghost entries by page id. Since a frame from an evicted page can later be reused. Each entry holds its own list iterator, so when a page is accessed I can locate and relocate it without walking the whole list. Hash lookups are expected O(1), and splicing within the list means a resident hit does not need a fresh node allocation.

The first time a page is accessed, it is placed in MRU. If the page is already resident or if it is found in a ghost list, it goes to MFU. A ghost hit in MRU raises the MRU target size and a ghost hit in MFU lowers that target. I adjust the target using the ratio between the ghost list sizes, with a minimum change of one, and I clamp it between zero and the frame capacity. The idea is to give more room to the side that has recently lost pages which turned out to still be useful.

During eviction, I start at the LRU end of the preferred list, skip any pinned frames, and fall back to the other list if needed. I track the evictable count separately from the list sizes because a frame that is in the replacer may still be pinned. All replacer state is protected by a mutex. Eviction can become O(n) if many frames are pinned. I chose that simple approach instead of maintaining another set of eviction lists.

## Buffer Pool Manager

One invariant I maintain is that every resident page maps to exactly one frame, and a frame that is pinned cannot be reused. The buffer pool latch guards the page table, the freeframe list, and the updates that keep pin counts and eviction eligibility consistent. ARC has a separate latch. I acquire ARC's latch inside buffer pool operations, and ARC never calls back into the buffer pool.

When there is a hit, the manager pins the frame that already holds the page and records the access. When there is a miss, the manager either takes a free frame or asks ARC for a victim. If the victim is dirty, I write it back before resetting the frame and filling it with the requested page. While waiting for disk requests, the manager holds the buffer pool latch. That serializes misses, but it also makes duplicate loads and frame reuse easier to prevent. The disk worker only does I/O and completes promises， it does not take the buffer pool latch or any page latch.

Each frame also has a reader writer latch. The manager pins the frame before releasing the buffer pool latch and then the guard acquires the page latch. A guard releases its page latch before taking the buffer pool latch (unpin). This can avoid an internal cycle where one thread holds the buffer pool latch while waiting for another thread’s page latch. (However, callers still need sensible ordering if they hold guards on multiple pages)

Guards release resources automatically and moving a guard invalidates the old owner to prevent double unpinning. Mutable access marks the page dirty. Safe flushing pins and latches the page. An unsafe flushing expects the caller to prevent concurrent modification. The dirty flag is atomic because several read guards might flush the same page.

## Memory Footprint

With N frames, page data occupies n times “BUSTUB_PAGE_SIZE” bytes, or 8192n bytes in this version. Additional storage is O(n): frame headers, shared pointer control blocks, the page table, free list nodes, and ARC metadata. ARC retains at most 2n entries across its four lists under normal buffer pool use. Ghost entries store the metadata insteadof page contents. Exact overhead depends on STL node sizes, padding etc.

I do not keep a permanent metadata entry for every allocated database page, new page ids come from one atomic counter. FlushAllPages temporarily copies up to m page ids. Scheduler requests also need queue and promise storage, and that depends on how many requests are outstanding. The disk manager's storage is separate from the buffer pool's frame allocation.

## Leaderboard Optimizations

I ran out of time to implement dedicated leaderboard optimizations such as prefetching or parallel I/O. I have been seeking HPC jobs, so some designs from my recent practices apply here. Small implementation choices include storing list iterators, reusing list nodes, and using std::move to transfer guards, shared pointers are implemented. Moves avoid unnecessary shared pointer updates where ownership is transferred, and are required for non copyable guards and promises.

## AI Usage

I had confusion of understanding the difference between pinning a page and locking its contents. I originally thought they serve similar purpose and really have some hardship formulate a concreate vision in my mind so I used claude to tell me what the purpose each of them is. The general result is that a thread can have the page pinned, but still be waiting for permission to access its contents. They protect different interference. Pinning is keep this page in this frame while I need it. Locking contents is control who can read or odify this page while I access it. Both are freezing actions in different directions.
