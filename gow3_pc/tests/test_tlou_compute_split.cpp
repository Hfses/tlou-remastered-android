// SPDX-License-Identifier: GPL-2.0-or-later
#include "../gpu/shadps4/shader_recompiler/backend/spirv/tlou_split_compute.h"
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <unordered_set>
int main() {
    using namespace TlouComputeSplit;
    assert(IsTarget(HashA) && IsTarget(HashB));
    assert(!IsTarget(NativeFallbackHash(HashA)));
    assert(!IsTarget(NativeFallbackHash(HashB)));
    // The original 1024-thread meta and experimental 64-thread meta must
    // never share the same cache identity across A/B sessions.
    for (std::uint64_t base : {0ULL, 1ULL, 0xa15b4c93de120001ULL}) {
        assert(VariantPermutationKey(HashA, base, false) == base);
        assert(VariantPermutationKey(HashB, base, false) == base);
        assert(VariantPermutationKey(HashA, base, true) != base);
        assert(VariantPermutationKey(HashB, base, true) != base);
        assert(VariantPermutationKey(0xdeadbeef, base, true) == base);
        assert(VariantPermutationKey(NativeFallbackHash(HashA), base, true) == base);
        assert(VariantPermutationKey(HashA, base, true) ==
               VariantPermutationKey(HashA, base, true));
        // A/B mode toggles must not cause old warmup entries to be
        // misdiagnosed as corrupted SPIR-V and deleted.
        for (const auto hash : {HashA, HashB}) {
            const auto native = VariantPermutationKey(hash, base, false);
            const auto split = VariantPermutationKey(hash, base, true);
            assert(AcceptCachedMeta(hash, base, native, false));
            assert(AcceptCachedMeta(hash, base, split, true));
            assert(!AcceptCachedMeta(hash, base, native, true));
            assert(!AcceptCachedMeta(hash, base, split, false));
        }
        assert(AcceptCachedMeta(0xdeadbeefULL, base, base, true));
        assert(AcceptCachedMeta(NativeFallbackHash(HashA), base, base, true));
        // Fused-FP optimization must stay cache-isolated from original,
        // split64, both-toggles and opposite-mode warmup metadata.
        for (const auto target : {HashA, HashB}) {
            const auto native = VariantPermutationKey(target, base, false, false);
            const auto split = VariantPermutationKey(target, base, true, false);
            const auto relaxed = VariantPermutationKey(target, base, false, true);
            const auto both = VariantPermutationKey(target, base, true, true);
            assert(native == base && split != base && relaxed != base);
            assert(both != native && both != split && both != relaxed);
            const std::uint64_t hashes[4] = {native, split, relaxed, both};
            for (unsigned split_on = 0; split_on != 2; ++split_on) {
                for (unsigned relaxed_on = 0; relaxed_on != 2; ++relaxed_on) {
                    const unsigned selected = split_on + relaxed_on * 2;
                    for (unsigned candidate = 0; candidate != 4; ++candidate)
                        assert(AcceptCachedMeta(target, base, hashes[candidate],
                            split_on, relaxed_on) == (candidate == selected));
                }
            }
        }
        assert(VariantPermutationKey(0xdeadbeef, base, true, true) == base);
        assert(VariantPermutationKey(NativeFallbackHash(HashA), base, true, true) == base);
        assert(RelaxedFpFor(HashA, true) && RelaxedFpFor(HashB, true));
        assert(!RelaxedFpFor(HashA, false) && !RelaxedFpFor(0xdeadbeef, true));
    }
    assert(EligibleGeometry(HashA, {1024,1,1}, 0, false));
    assert(EligibleGeometry(HashB, {1024,1,1}, 0, false));
    assert(!EligibleGeometry(HashA, {64,1,1}, 0, false));
    assert(!EligibleGeometry(HashA, {1024,2,1}, 0, false));
    assert(!EligibleGeometry(HashA, {1024,1,1}, 1024, false));
    assert(!EligibleGeometry(HashA, {1024,1,1}, 0, true));
    assert(!EligibleGeometry(0xdeadbeef, {1024,1,1}, 0, false));
    assert(CanExpandX(4095,65535));
    assert(!CanExpandX(4096,65535));
    assert(!CanExpandX(0,65535));
    for (std::uint32_t groups : {1u, 2u, 7u, 53u}) {
        std::unordered_set<std::uint32_t> visited;
        visited.reserve(groups*OriginalSize);
        for (std::uint32_t physical_group=0; physical_group<PhysicalGroupsX(groups); ++physical_group) {
            const auto original_group=VirtualGroupX(physical_group);
            for (std::uint32_t physical_local=0; physical_local<PhysicalSize; ++physical_local) {
                const auto original_local=VirtualLocalX(physical_group,physical_local);
                assert(original_group<groups && original_local<OriginalSize);
                const auto original_index=original_group*OriginalSize+original_local;
                assert(original_index == physical_group*PhysicalSize+physical_local);
                assert(visited.insert(original_index).second);
            }
        }
        assert(visited.size() == groups*OriginalSize);
    }
    std::puts("TLOU 1024->64 index equivalence: PASS");
}
