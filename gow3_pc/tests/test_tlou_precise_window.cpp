// SPDX-License-Identifier: GPL-2.0-or-later
#include "../gpu/shadps4/video_core/buffer_cache/tlou_precise_window.h"
#include <cassert>
#include <cstdint>
#include <cstdio>
int main() {
    using namespace TlouPreciseWindow;
    static_assert(Width(false,false)==OriginalBytes);
    static_assert(Width(true,false)==OriginalBytes);
    static_assert(Width(false,true)==OriginalBytes);
    static_assert(Width(true,true)==NarrowBytes);
    const std::uint64_t base=0x10000000ULL, size=4*1024*1024;
    for(std::uint64_t pos=0;pos<size;pos+=127)for(auto len:{1u,4u,4096u,8192u,65536u,100000u}) {
        if(pos+len>size)continue;
        const auto wide=Window(base+pos,len,base,size,OriginalBytes);
        const auto narrow=Window(base+pos,len,base,size,NarrowBytes);
        assert(wide.first<=base+pos && wide.second>=base+pos+len);
        assert(narrow.first<=base+pos && narrow.second>=base+pos+len);
        assert(narrow.first>=wide.first && narrow.second<=wide.second);
    }
    constexpr auto misaligned=0x20012345ULL;
    for(auto offset:{0u,4090u,65000u,1000000u}) {
        const auto win=Window(misaligned+offset,4096,misaligned,2*1024*1024,NarrowBytes);
        assert(win.first>=misaligned && win.second<=misaligned+2*1024*1024);
        assert(win.first<=misaligned+offset && win.second>=misaligned+offset+4096);
    }
    std::puts("TLOU precise-window: PASS");
}
