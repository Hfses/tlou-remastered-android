/* Direct-memory allocator tests (sceKernelAllocateDirectMemory / ReleaseDirectMemory).
 *
 * Replays the allocation sequence from the Android log that ended in
 *   direct allocation of 805306368 bytes in [0x0, 0x13c000000) failed: no free range
 * and checks the allocator's contract: first fit, alignment, fragmentation, reuse, boundaries.
 * Build (Linux/Android, no game files needed):
 *   gcc -std=gnu11 -O1 -o test_direct_memory tests/test_direct_memory.c -lpthread && ./test_direct_memory */
#include "../src/runtime_memory.c"
#include <assert.h>

static int32_t test_errno;
int32_t *runtime_errno(void) { return &test_errno; }
uintptr_t runtime_lookup(const RuntimeExport *table, size_t count, const char *name) {
    (void)table; (void)count; (void)name; return 0;
}

#define MIB (UINT64_C(1) << 20)
#define RETAIL_POOL (UINT64_C(5056) * MIB)  /* 0x13c000000: sceKernelGetDirectMemorySize default */
#define RETAIL_REPORT (UINT64_C(4608) * MIB) /* 0x120000000: what the game expects from a retail PS4 */

/* The three requests, byte for byte, from the log. */
#define LOG_FIRST  UINT64_C(2713714688) /* type 3, offset 0 */
#define LOG_SECOND UINT64_C(2097152000) /* type 0, offset 0xa1c00000 */
#define LOG_THIRD  UINT64_C(805306368)  /* 768 MiB, the failing one */

static int32_t alloc_at(uint64_t low, uint64_t high, uint64_t size, uint64_t alignment, int type, int64_t *out) {
    return direct_allocate((int64_t)low, (int64_t)high, size, alignment, type, out);
}
static void release_all(void) {
    int64_t any;
    (void)any;
    for (int i = 0; i < LIMIT; ++i) {
        if (blocks[i].used) assert(!direct_release(blocks[i].start, blocks[i].size));
    }
    for (int i = 0; i < LIMIT; ++i) assert(!blocks[i].used);
    assert(!live_bytes);
}
static uint64_t used_total(void) {
    uint64_t total = 0;
    for (int i = 0; i < LIMIT; ++i) if (blocks[i].used) total += blocks[i].size;
    return total;
}

/* 1. The exact sequence from the log, with the pool the failing run had (5056 MiB). */
static void test_log_sequence_retail_pool(void) {
    int64_t a = -1, b = -1, c = -1;
    assert(RETAIL_POOL == UINT64_C(0x13c000000));
    assert(LOG_FIRST == 2588 * MIB && LOG_SECOND == 2000 * MIB && LOG_THIRD == 768 * MIB);
    assert(direct_size() == RETAIL_POOL); /* no GOW3_DMEM_REPORT_MB: the game is told 5056 MiB */
    assert(!alloc_at(0, RETAIL_POOL, LOG_FIRST, 0, 3, &a) && a == 0);
    assert(!alloc_at(0, RETAIL_POOL, LOG_SECOND, 0, 0, &b) && (uint64_t)b == UINT64_C(0xa1c00000));
    assert((uint64_t)b + LOG_SECOND == UINT64_C(0x11ec00000)); /* 4588 MiB, 4.48 GiB */
    /* 5056 - 4588 = 468 MiB free in one interval: genuinely smaller than 768 MiB. */
    assert(RETAIL_POOL - used_total() == 468 * MIB);
    assert(alloc_at(0, RETAIL_POOL, LOG_THIRD, 0, 0, &c) == NO_SPACE && c == -1);
    /* A failed request must not change the allocator: same blocks, same live bytes. */
    assert(used_total() == LOG_FIRST + LOG_SECOND && live_bytes == LOG_FIRST + LOG_SECOND);
    /* The 468 MiB that are left are still usable (the failure was not sticky). */
    assert(!alloc_at(0, RETAIL_POOL, 468 * MIB, 0, 0, &c) && (uint64_t)c == UINT64_C(0x11ec00000));
    assert(used_total() == RETAIL_POOL);
    release_all();
}

/* 2. What a retail console reports (4608 MiB): the game then searches up to 0x120000000 and does
 * not ask for the 768 MiB development-kit debug area, so the same two requests leave 20 MiB. */
static void test_log_sequence_retail_report(void) {
    int64_t a = -1, b = -1, c = -1;
    setenv("GOW3_DMEM_REPORT_MB", "4608", 1);
    assert(direct_size() == RETAIL_REPORT && RETAIL_REPORT == UINT64_C(0x120000000));
    assert(!alloc_at(0, RETAIL_REPORT, LOG_FIRST, 0, 3, &a) && a == 0);
    assert(!alloc_at(0, RETAIL_REPORT, LOG_SECOND, 0, 0, &b) && (uint64_t)b == UINT64_C(0xa1c00000));
    assert(RETAIL_REPORT - used_total() == 20 * MIB);
    /* Even inside the smaller window the allocator refuses what does not fit: no overrun. */
    assert(alloc_at(0, RETAIL_REPORT, 21 * MIB, 0, 0, &c) == NO_SPACE);
    assert(!alloc_at(0, RETAIL_REPORT, 20 * MIB, 0, 0, &c) && (uint64_t)c == UINT64_C(0x11ec00000));
    release_all();
    unsetenv("GOW3_DMEM_REPORT_MB");
    assert(direct_size() == RETAIL_POOL);
}

/* 3. Consecutive allocations are first-fit and contiguous. */
static void test_consecutive(void) {
    int64_t off[8];
    for (int i = 0; i < 8; ++i) assert(!alloc_at(0, RETAIL_POOL, 64 * MIB, 0, 0, &off[i]) && (uint64_t)off[i] == i * 64 * MIB);
    assert(used_total() == 512 * MIB && allocations >= 8);
    release_all();
}

/* 4. Alignments: honoured, and the skipped space stays free for smaller requests. */
static void test_alignment(void) {
    int64_t a, b, c;
    assert(!alloc_at(0, RETAIL_POOL, PAGE, 0, 0, &a) && a == 0);
    assert(!alloc_at(0, RETAIL_POOL, PAGE, 2 * MIB, 0, &b) && (uint64_t)b == 2 * MIB && b % (2 * MIB) == 0);
    assert(!alloc_at(0, RETAIL_POOL, PAGE, 0, 0, &c) && (uint64_t)c == PAGE); /* the gap is reused */
    assert(!alloc_at(1, RETAIL_POOL, PAGE, 64 * MIB, 0, &c) && (uint64_t)c == 64 * MIB); /* low is rounded up */
    /* Invalid alignments: not a power of two, below the page size, above the pool. */
    assert(alloc_at(0, RETAIL_POOL, PAGE, 3 * PAGE, 0, &c) == INVALID);
    assert(alloc_at(0, RETAIL_POOL, PAGE, PAGE / 2, 0, &c) == INVALID);
    assert(alloc_at(0, RETAIL_POOL, PAGE, RETAIL_POOL * 2, 0, &c) == INVALID);
    release_all();
}

/* 5. Fragmentation: total free space is large, but no single interval is. */
static void test_fragmentation(void) {
    int64_t slot[6], x;
    for (int i = 0; i < 6; ++i) assert(!alloc_at(0, RETAIL_POOL, 128 * MIB, 0, 0, &slot[i]));
    /* Free every other 128 MiB block: 3 holes of 128 MiB, tail of 5056-768 = 4288 MiB. */
    for (int i = 0; i < 6; i += 2) assert(!direct_release((uint64_t)slot[i], 128 * MIB));
    /* Restrict the window to the fragmented area: 384 MiB free in total, 128 MiB at most in one piece. */
    uint64_t window = 6 * 128 * MIB;
    assert(window - used_total() == 384 * MIB);
    assert(alloc_at(0, window, 256 * MIB, 0, 0, &x) == NO_SPACE);        /* enough total, no contiguous interval */
    assert(!alloc_at(0, window, 128 * MIB, 0, 0, &x) && x == slot[0]);   /* first hole, first fit */
    assert(!alloc_at(0, window, 128 * MIB, 0, 0, &x) && x == slot[2]);
    assert(!alloc_at(0, window, 128 * MIB, 0, 0, &x) && x == slot[4]);
    assert(alloc_at(0, window, PAGE, 0, 0, &x) == NO_SPACE);             /* window full */
    release_all();
}

/* 6. Release and reuse; partial release splits a block; double release is refused. */
static void test_release_reuse(void) {
    int64_t a, b, c;
    assert(!alloc_at(0, RETAIL_POOL, 256 * MIB, 0, 1, &a) && a == 0);
    assert(!alloc_at(0, RETAIL_POOL, 256 * MIB, 0, 2, &b) && (uint64_t)b == 256 * MIB);
    assert(!direct_release(0, 256 * MIB));
    assert(direct_release(0, 256 * MIB) == INVALID);        /* already released */
    assert(!alloc_at(0, RETAIL_POOL, 256 * MIB, 0, 3, &c) && c == 0 && block_type(0) == 3);
    /* Partial release of the middle of b keeps both ends allocated and their type. */
    assert(!direct_release(256 * MIB + 64 * MIB, 64 * MIB));
    assert(direct_range_allocated(256 * MIB, 64 * MIB) && !direct_range_allocated(320 * MIB, 64 * MIB));
    assert(direct_range_allocated(384 * MIB, 128 * MIB) && block_type(400 * MIB) == 2);
    assert(!alloc_at(256 * MIB, 512 * MIB, 64 * MIB, 0, 4, &c) && (uint64_t)c == 320 * MIB);
    assert(direct_release(1, PAGE) == INVALID && direct_release(0, PAGE + 1) == INVALID); /* unaligned */
    release_all();
}

/* 7. Limits: exact fit at the end of the window passes, one page more fails; no overflow. */
static void test_limits(void) {
    int64_t a;
    assert(!alloc_at(0, RETAIL_POOL, RETAIL_POOL, 0, 0, &a) && a == 0); /* whole pool, exclusive end */
    assert(alloc_at(0, RETAIL_POOL, PAGE, 0, 0, &a) == NO_SPACE);
    release_all();
    assert(alloc_at(0, RETAIL_POOL, RETAIL_POOL + PAGE, 0, 0, &a) == NO_SPACE);
    assert(alloc_at(RETAIL_POOL - PAGE, RETAIL_POOL, 2 * PAGE, 0, 0, &a) == NO_SPACE);
    assert(!alloc_at(RETAIL_POOL - PAGE, RETAIL_POOL, PAGE, 0, 0, &a) && (uint64_t)a == RETAIL_POOL - PAGE);
    release_all();
    /* High bound beyond the pool is clamped to the pool, never past it. */
    assert(!alloc_at(RETAIL_POOL - PAGE, UINT64_C(1) << 40, PAGE, 0, 0, &a) && (uint64_t)a == RETAIL_POOL - PAGE);
    assert(alloc_at(RETAIL_POOL - PAGE, UINT64_C(1) << 40, 2 * PAGE, 0, 0, &a) == NO_SPACE);
    release_all();
    /* Sizes that would overflow 64-bit arithmetic, negative ranges, bad arguments. */
    assert(alloc_at(0, RETAIL_POOL, UINT64_MAX - PAGE + 1, 0, 0, &a) == NO_SPACE);
    assert(alloc_at(RETAIL_POOL, RETAIL_POOL, PAGE, 0, 0, &a) == NO_SPACE);
    assert(direct_allocate(-1, (int64_t)RETAIL_POOL, PAGE, 0, 0, &a) == INVALID);
    assert(direct_allocate(0, -1, PAGE, 0, 0, &a) == INVALID);
    assert(alloc_at(0, RETAIL_POOL, 0, 0, 0, &a) == INVALID);
    assert(alloc_at(0, RETAIL_POOL, PAGE + 1, 0, 0, &a) == INVALID);
    assert(alloc_at(0, RETAIL_POOL, PAGE, 0, 11, &a) == INVALID);
    assert(direct_allocate(0, (int64_t)RETAIL_POOL, PAGE, 0, 0, NULL) == INVALID);
    assert(!used_total());
}

/* 8. Overlapping reservations are impossible: every live block is disjoint, and a request that
 * would cross an existing block is placed after it. */
static void test_no_overlap(void) {
    int64_t a, b, c;
    assert(!alloc_at(0, RETAIL_POOL, 100 * MIB, 0, 0, &a));
    assert(!alloc_at(50 * MIB, RETAIL_POOL, 100 * MIB, 0, 0, &b) && (uint64_t)b == 100 * MIB); /* hint inside a */
    assert(!alloc_at(120 * MIB, RETAIL_POOL, 10 * MIB, 0, 0, &c) && (uint64_t)c == 200 * MIB);
    for (int i = 0; i < LIMIT; ++i) for (int j = i + 1; j < LIMIT; ++j)
        if (blocks[i].used && blocks[j].used)
            assert(blocks[i].start + blocks[i].size <= blocks[j].start || blocks[j].start + blocks[j].size <= blocks[i].start);
    release_all();
}

/* 9. Mapping and flexible memory keep working next to the allocator (regression guard). */
static void test_map_direct(void) {
    int64_t a;
    void *at = NULL;
    assert(!alloc_at(0, RETAIL_POOL, 4 * MIB, 0, 0, &a));
    assert(map_direct_locked(&at, 4 * MIB, 0x33, 0, a, 0) == 0 && (uintptr_t)at >= USER_MIN);
    void *again = NULL; /* physical range that was never allocated */
    assert(map_direct_locked(&again, 4 * MIB, 0x33, 0, 4 * MIB + 64 * MIB, 0) == INVALID);
    assert(!unmap_locked((uintptr_t)at, 4 * MIB));
    release_all();
}

int main(void) {
    unsetenv("GOW3_DMEM_MB");
    unsetenv("GOW3_DMEM_REPORT_MB");
    setenv("GOW3_MEMORY_DEBUG", "0", 1);
    test_log_sequence_retail_pool();
    test_log_sequence_retail_report();
    test_consecutive();
    test_alignment();
    test_fragmentation();
    test_release_reuse();
    test_limits();
    test_no_overlap();
    test_map_direct();
    puts("Direct memory tests passed");
    return 0;
}
