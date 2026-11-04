// SPDX-License-Identifier: GPL-2.0-or-later
#include "../gpu/shadps4/video_core/buffer_cache/tlou_write_tick.h"
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <unordered_map>
int main() {
using namespace VideoCore::TlouWriteTick;
assert(FineIndexable(65537) && FineIndexable(1048576));
assert(!FineIndexable(65536) && !FineIndexable(1048577));
std::unordered_map<std::uint64_t,std::uint64_t> pages,fine,coarse,huge;
const auto write=[&](std::uint64_t addr,std::uint64_t size,std::uint64_t tick) {
    if(size>FineBytes) {
        ForEachBlock(addr,size,WindowBytes,[&](auto n){coarse[n]=std::max(coarse[n],tick);});
        if(FineIndexable(size)) ForEachBlock(addr,size,FineBytes,[&](auto n){fine[n]=std::max(fine[n],tick);});
        else ForEachBlock(addr,size,WindowBytes,[&](auto n){huge[n]=std::max(huge[n],tick);});
    } else ForEachBlock(addr,size,4096,[&](auto n){pages[n]=std::max(pages[n],tick);});
};
constexpr std::uint64_t b=0x10000000;
write(b+0x10000,65537,3);
write(b+0xE0000,65537,12);
const auto read=[&](auto a,bool on) {
    return ReadTick(pages[a/4096],coarse[a/WindowBytes],fine[a/FineBytes],huge[a/WindowBytes],on);
};
assert(read(b+0x10020,false)==12);
assert(read(b+0x10020,true)==3);
assert(read(b+0xE0010,true)==12);
assert(read(b,true)==0);
write(b+0x10020,8,19);
assert(read(b+0x10020,true)==19);
assert(read(b+0x22000,true)==3);
write(b+0x200000,2*WindowBytes+1,25);
assert(read(b+0x200010,true)==25);
assert(read(b+0x400000,true)==25);
assert(read(b+0x200010,false)==25);
std::puts("TLOU fine producer tick: PASS");
}
