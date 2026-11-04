// SPDX-License-Identifier: GPL-2.0-or-later
#include "../gpu/shadps4/video_core/renderer_vulkan/srt_readback_batch.h"
#include <array>
#include <cassert>
#include <cstring>
#include <iostream>
#include <random>
#include <unordered_map>

using namespace Vulkan;
struct Node { int parent; unsigned offset, extent; bool dynamic{}; };
using Address = std::uintptr_t;
using Bytes = std::vector<std::uint8_t>;

struct WalkResult { Bytes flat; unsigned downloads{}; };
WalkResult RunTree(const std::vector<Node>& nodes, const std::vector<Address>& target,
                   bool batch, unsigned seed) {
    std::unordered_map<Address, Bytes> cpu, gpu;
    std::unordered_map<Address, bool> dirty;
    std::mt19937 rng(seed);
    for (std::size_t i = 0; i < nodes.size(); ++i) {
        cpu[target[i]] = Bytes(nodes[i].extent, 0xcc);
        auto& data = gpu[target[i]];
        data.resize(nodes[i].extent);
        for (auto& b : data) b = rng();
        dirty[target[i]] = true;
    }
    // All pointers inside tables initially stale: reading children early is a bug.
    for (std::size_t i = 0; i < nodes.size(); ++i) if (nodes[i].parent >= 0) {
        auto& data = gpu[target[nodes[i].parent]];
        assert(nodes[i].offset + 8 <= data.size());
        std::memcpy(data.data() + nodes[i].offset, &target[i], 8);
    }
    std::vector<Address> address(nodes.size());
    WalkResult result;
    for (std::size_t i = 0; i < nodes.size(); ++i) {
        const auto resolve = [&](std::size_t n) -> Address {
            if (nodes[n].parent < 0) return target[n];
            const auto source = address[nodes[n].parent];
            assert(source && !dirty[source]); // no pointer read before its parent completes
            Address value{};
            std::memcpy(&value, cpu.at(source).data() + nodes[n].offset, 8);
            assert(value == target[n]);
            return value;
        };
        address[i] = resolve(i);
        if (dirty[address[i]]) {
            const auto ranges = PlanSrtSiblingBatch(std::span{nodes}, i, address[i],
                nodes[i].parent < 0 ? Address{0} : address[nodes[i].parent], batch,
                resolve, [&](Address p, auto) { return dirty[p]; });
            ++result.downloads;
            for (const auto& [p, size] : ranges) {
                assert(size == gpu.at(p).size());
                cpu[p] = gpu.at(p);
                dirty[p] = false;
            }
        }
        result.flat.insert(result.flat.end(), cpu.at(address[i]).begin(), cpu.at(address[i]).end());
    }
    return result;
}

int main() {
    std::array<Node, 8> n{{{-1,0,64}, {0,0,64}, {1,0,64}, {0,8,64},
                           {-1,8,64}, {0,16,64,true}, {0,24,64}, {0,32,64}}};
    std::array<Address,8> a{0x10000,0x20000,0x30000,0x40000,0x50000,0x60000,0x70000,0x80000};
    auto resolve = [&](std::size_t i) { return a[i]; };
    auto dirty = [](Address, auto) { return true; };
    auto roots = PlanSrtSiblingBatch(std::span{n},0,a[0],0,true,resolve,dirty);
    assert(roots.size()==2 && roots[1].first==a[4]);
    auto nested = PlanSrtSiblingBatch(std::span{n},1,a[1],a[0],true,resolve,dirty);
    assert(nested.size()==1); // do not resolve siblings beyond an intervening descendant
    n[2].parent=0;
    auto siblings = PlanSrtSiblingBatch(std::span{n},1,a[1],a[0],true,resolve,dirty);
    assert(siblings.size()==3 && siblings[1].first==a[2]); // stops at next branch
    n[4].parent=0;
    siblings = PlanSrtSiblingBatch(std::span{n},1,a[1],a[0],true,resolve,dirty);
    assert(siblings.size()==6); // dynamic node excluded
    assert(PlanSrtSiblingBatch(std::span{n},1,a[1],a[0],false,resolve,dirty).size()==1);
    assert(PlanSrtSiblingBatch(std::span{n},1,a[1],a[0],true,resolve,
        [](Address,auto){return false;}).size()==1);
    // A sibling aliases the pointer source: skip it, so resolving later pointers stays valid.
    a[3]=a[0]+8;
    assert(PlanSrtSiblingBatch(std::span{n},1,a[1],a[0],true,resolve,dirty).size()==5);
    // The first table aliases that source: retain sequential behavior altogether.
    assert(PlanSrtSiblingBatch(std::span{n},1,a[0],a[0],true,resolve,dirty).size()==1);
    a[3]=a[1]; a[6]=a[1]+32; // duplicate/overlap: never introduce a second copy
    auto overlap=PlanSrtSiblingBatch(std::span{n},1,a[1],a[0],true,resolve,dirty);
    assert(overlap.size()==4 && overlap.back().first==a[7]);
    a[2]=a[4]=a[1];
    a[7]=0; assert(PlanSrtSiblingBatch(std::span{n},1,a[1],a[0],true,resolve,dirty).size()==1);
    a[7]=UINTPTR_MAX-2;
    assert(PlanSrtSiblingBatch(std::span{n},1,a[1],a[0],true,resolve,dirty).size()==1);
    std::vector<Node> big(40,Node{-1,0,4096});
    const auto limits=PlanSrtSiblingBatch(std::span{big},0,Address{0x100000},0,true,
        [](std::size_t i)->Address{return 0x100000+i*0x20000;},dirty);
    assert(limits.size()==16);
    std::uint64_t bytes=0; for(auto r:limits)bytes+=r.second; assert(bytes==SrtBatchBytes);
    big[0].extent=65537;
    assert(PlanSrtSiblingBatch(std::span{big},0,Address{0x100000},0,true,resolve,dirty).size()==1);

    // Parent-before-child, nested siblings, and independent roots with stale guest pointers.
    std::vector<Node> tree{{-1,0,32},{0,0,32},{1,0,16},{0,8,32},{3,0,16},
                           {-1,8,32},{5,0,32},{5,8,16}};
    std::vector<Address> target{0x10000,0x20000,0x30000,0x40000,0x50000,0x60000,0x70000,0x80000};
    auto sequential=RunTree(tree,target,false,7), batched=RunTree(tree,target,true,7);
    assert(sequential.flat==batched.flat && batched.downloads<sequential.downloads);
    std::mt19937 rng(015);
    unsigned sequential_count=0,batch_count=0;
    for(unsigned test=0;test<4000;++test) {
        const unsigned count=2+rng()%60;
        std::vector<Node> nodes;
        std::vector<Address> addresses;
        std::vector<unsigned> child_count(count);
        for(unsigned i=0;i<count;++i) {
            const int parent=i && rng()%4 ? int(rng()%i) : -1;
            const unsigned offset=parent<0 ? 0 : child_count[parent]++*8;
            nodes.push_back({parent,offset,512});
            addresses.push_back(0x100000+i*0x20000);
        }
        const auto old=RunTree(nodes,addresses,false,test), opt=RunTree(nodes,addresses,true,test);
        assert(old.flat==opt.flat && opt.downloads<=old.downloads);
        sequential_count+=old.downloads; batch_count+=opt.downloads;
    }
    std::cout << "srt-batch: dependency order, aliases, gaps, dynamic nodes, queued-copy fallback, limits and 4000 byte-equivalent trees passed; synthetic calls "
              << sequential_count << " -> " << batch_count << " (not device FPS)\n";
}
