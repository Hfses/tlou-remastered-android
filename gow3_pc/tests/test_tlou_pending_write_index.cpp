// SPDX-License-Identifier: GPL-2.0-or-later
// Production index vs the old cyclic search, with identical update/commit
// semantics. All interval/filter/selection state is compared, not just hits.
#include "video_core/renderer_vulkan/tlou_pending_write_index.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <random>
#include <string_view>
#include <vector>

namespace {
using u32=std::uint32_t;
using u64=std::uint64_t;
constexpr u32 Shift=16, FilterBits=4096;
constexpr u64 Uncommitted=~u64{0};
struct Write {u64 begin,end,position;bool operator==(const Write&)const=default;};
struct Event {u64 address,size;bool pending;};
struct Model {
    std::vector<Write> writes;
    std::vector<u32> uncommitted;
    Vulkan::Detail::PendingWriteIndex<Write> index{};
    std::array<u64,FilterBits/64> filter{};
    u64 min=~u64{0},max=0,tick=0;
    u64 checks=0,marks=0,hint_hits=0,walks=0;
    u32 hint=0;

    void Mark(u64 lo,u64 hi) {
        ++marks;
        u64 first=lo>>Shift,last=(hi-1)>>Shift;
        if(last-first>=FilterBits){filter.fill(~u64{0});return;}
        for(u64 page=first;page<=last;++page) {
            u32 bit=u32(page%FilterBits);
            filter[bit/64]|=u64{1}<<(bit%64);
        }
    }
    bool FilterHit(u64 lo,u64 hi) {
        ++checks;
        u64 first=lo>>Shift,last=(hi-1)>>Shift;
        if(last-first>=FilterBits)return true;
        for(u64 page=first;page<=last;++page) {
            u32 bit=u32(page%FilterBits);
            if((filter[bit/64]>>(bit%64))&1)return true;
        }
        return false;
    }
    void Retire(u64 before) {
        const auto old_count=writes.size();
        std::erase_if(writes,[&](const Write& w){return w.position!=Uncommitted&&w.position<=before;});
        if(old_count!=writes.size())index.Rebuild(writes);
        min=~u64{0};max=0;filter.fill(0);
        for(const auto& w:writes){min=std::min(min,w.begin);max=std::max(max,w.end);Mark(w.begin,w.end);}
    }
    void Note(u64 lo,u64 size,bool pending,bool optimized) {
        const u64 hi=lo+size,pos=pending?Uncommitted:++tick;
        const auto update=[&](u32 i) {
            auto& w=writes[i];
            const bool covered=Vulkan::Detail::PendingWriteAlreadyCovered(w,lo,hi);
            hint=i;w.begin=std::min(w.begin,lo);w.end=std::max(w.end,hi);
            w.position=pos;min=std::min(min,lo);max=std::max(max,hi);
            if(optimized&&!covered)index.Set(i,w);
            if(!optimized||!covered)Mark(lo,hi);
            if(pending)uncommitted.push_back(i);
        };
        if(optimized&&Vulkan::Detail::PendingWriteHintOverlaps(writes,hint,lo,hi)) {
            ++hint_hits;update(hint);return;
        }
        if(optimized&&index.CanQuery(writes.size())) {
            if(auto found=index.FirstOverlap(writes,hint,lo,hi)) {update(*found);return;}
        } else {
            const u32 count=FilterHit(lo,hi)?u32(writes.size()):0;
            const u32 first=hint<count?hint:0;
            for(u32 step=0;step<count;++step) {
                const u32 i=first+step<count?first+step:first+step-count;
                ++walks;
                const auto& w=writes[i];
                if(lo<=w.end&&w.begin<=hi){update(i);return;}
            }
        }
        if(pending)uncommitted.push_back(u32(writes.size()));
        writes.push_back({lo,hi,pos});
        if(optimized)index.Set(writes.size()-1,writes.back());
        min=std::min(min,lo);max=std::max(max,hi);Mark(lo,hi);
    }
    bool Equals(const Model& rhs)const {
        return writes==rhs.writes&&uncommitted==rhs.uncommitted&&
            filter==rhs.filter&&min==rhs.min&&max==rhs.max&&hint==rhs.hint&&tick==rhs.tick;
    }
};
void Verify(){
    Model reference,optimized;
    std::mt19937_64 rng(0x246ed19);
    std::array<u64,256> hot{};
    u64 current=0;
    for(u32 i=0;i<180000;++i){
        if(i&&i%67==0){
            reference.uncommitted.clear();optimized.uncommitted.clear();
            for(auto& w:reference.writes)if(w.position==Uncommitted)w.position=reference.tick;
            for(auto& w:optimized.writes)if(w.position==Uncommitted)w.position=optimized.tick;
        }
        if(i&&i%709==0){
            const auto cut=reference.tick-24;
            reference.Retire(cut);optimized.Retire(cut);
        }
        if(i%7==0)current=rng()%hot.size();
        if(!hot[current])hot[current]=(rng()%8192)*65536+0x1000000;
        const u64 lo=hot[current]+((rng()%8)==0?rng()%128:0);
        const u64 size=1+(rng()%4)*4096;
        const bool pending=rng()%13==0;
        reference.Note(lo,size,pending,false);
        optimized.Note(lo,size,pending,true);
        if(i%307==0)assert(reference.Equals(optimized));
    }
    assert(reference.Equals(optimized));
    assert(optimized.hint_hits>20000);
    assert(optimized.marks<reference.marks);
    assert(optimized.checks<reference.checks);
    // Inclusive boundary: two ranges touching exactly at a 64 KiB window.
    Model a,b;
    for(u32 i=0;i<2400;++i){
        const u64 lo=0x10000000+u64(i)*0x20000;
        a.Note(lo,65536,false,false);b.Note(lo,65536,false,true);
    }
    assert(a.Equals(b)); // >2048 ranges falls back to original traversal
    a.Note(0x10000000+65536,128,false,false);
    b.Note(0x10000000+65536,128,false,true);
    assert(a.Equals(b));
    a.Retire(a.tick-1300);b.Retire(b.tick-1300);
    assert(a.Equals(b)); // after vector compaction, index must be rebuilt
    for(u32 i=0;i<1000;++i){
        const u64 lo=0x10000000+u64(i%32)*0x20000;
        a.Note(lo,32768,false,false);b.Note(lo,32768,false,true);
        if(i%29==0)assert(a.Equals(b));
    }
    assert(a.Equals(b));
    std::printf("pending index correctness: 180000 randomized writes and 2400-range overflow, inclusive boundaries, retirement/reindex passed; skipped filter checks=%llu\n",
                (unsigned long long)(reference.checks-optimized.checks));
}
std::vector<Event> Workload(int reuse){
    std::mt19937_64 rng(943u+reuse);
    std::vector<Event> result;result.reserve(240000);
    u64 current=0;
    for(int i=0;i<240000;++i){
        if(rng()%100>=u64(reuse))current=rng()%120;
        result.push_back({0x2000000+current*65536,(rng()%8==0)?512u:4096u,false});
    }
    return result;
}
static volatile u64 sink;
double Run(const std::vector<Event>& events,bool indexed){
    Model state;
    const auto start=std::chrono::steady_clock::now();
    for(const auto& e:events)state.Note(e.address,e.size,e.pending,indexed);
    sink=sink+state.walks+state.checks+state.marks;
    return std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
}
void Cost(int reuse){
    auto events=Workload(reuse);
    std::array<double,7> a{},b{};
    for(size_t i=0;i<a.size();++i){
        if(i&1){b[i]=Run(events,true);a[i]=Run(events,false);}
        else{a[i]=Run(events,false);b[i]=Run(events,true);}
    }
    std::sort(a.begin(),a.end());std::sort(b.begin(),b.end());
    std::printf("pending index %d%% repeat, 240000 writes: original=%.3fms indexed=%.3fms ratio=%.3f (host, not FPS)\n",
                reuse,a[3],b[3],b[3]/a[3]);
}
}
int main(int argc,char** argv){
    Verify();
    if(argc==1){Cost(90);Cost(70);Cost(40);return 0;}
    return argc==2&&std::string_view(argv[1])=="--correctness-only"?0:2;
}
