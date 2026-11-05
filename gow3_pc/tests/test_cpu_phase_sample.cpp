// SPDX-License-Identifier: GPL-2.0-or-later
#include "../gpu/shadps4/video_core/renderer_vulkan/tlou_cpu_phase_sample.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
#include <cstdio>
#include <cstring>

using namespace Vulkan::Detail;
struct FakeClock {
    static inline std::uint64_t wall=100,cpu=1000,wall_calls=0,cpu_calls=0;
    static std::uint64_t WallNs(){++wall_calls;return wall;}
    static std::uint64_t CpuNs(){++cpu_calls;return cpu;}
};
void Correctness() {
    using S=CpuPhaseSampler<3,FakeClock>;
    auto report=[](const auto&,auto){};
    S exact{true,1,0};
    {
        auto p=exact.Begin(report);assert(p.Active());
        FakeClock::cpu+=100;p.Phase(1);
        FakeClock::cpu+=200;p.Phase(2);
        FakeClock::cpu+=300;
        auto moved=std::move(p);moved.Finish();moved.Finish();
    }
    assert(exact.Current().samples==1 && exact.Current().cpu_ns[0]==100 &&
           exact.Current().cpu_ns[1]==200 && exact.Current().cpu_ns[2]==300);
    assert(exact.Current().cpu_reads==4);
    S budget{true,1,0};
    for(unsigned i=0;i<100000;++i){auto p=budget.Begin(report);p.Phase(1);p.Phase(2);}
    assert(budget.Current().calls==100000 && budget.Current().samples==8);
    assert(budget.Current().cpu_reads==32);
    FakeClock::wall+=1000000000ull;
    for(unsigned i=0;i<100;++i){auto p=budget.Begin(report);p.Phase(1);p.Phase(2);}
    assert(budget.Current().samples==16 && budget.Current().cpu_reads==64);
    auto saved=budget.Current();bool reported=false;
    FakeClock::wall+=5000000000ull;
    {
        auto p=budget.Begin([&](const auto& stats,auto ns){
            assert(ns>=5000000000ull && stats.calls==saved.calls && stats.samples==16);
            reported=true;
        });
        assert(reported && budget.Current().calls==1);
    }
    assert(budget.Current().samples==1);
    S broken{true,1,0};
    {auto p=broken.Begin(report);FakeClock::cpu=0;p.Phase(1);}
    assert(broken.Current().samples==0 && broken.Current().dropped==1);
    FakeClock::cpu=2000;
    S excessive{true,1,0};
    {auto p=excessive.Begin(report);for(unsigned i=0;i<1000;++i)p.Phase(i%3);}
    assert(excessive.Current().samples==0 && excessive.Current().dropped==1);
    assert(excessive.Current().cpu_reads<=S::MaxMarks+2);
    auto old_cpu=FakeClock::cpu_calls,old_wall=FakeClock::wall_calls;
    S off{false,1,0};
    for(unsigned i=0;i<10000;++i){auto p=off.Begin(report);p.Phase(2);}
    assert(!off.Current().calls && FakeClock::cpu_calls==old_cpu && FakeClock::wall_calls==old_wall);
    std::puts("PASS: exact phase attribution, rate cap, clock-read cap, reporting/reset, "
              "failed clocks, excessive phases, move/early finish and zero clock calls when OFF");
}
struct BenchResult {double ns;std::uint64_t sink,reads,samples;};
BenchResult Measure(bool enabled) {
    CpuPhaseSampler<6> sampler{enabled};
    constexpr unsigned Draws=2000000;
    std::uint64_t work=8;
    const auto before=std::chrono::steady_clock::now();
    for(unsigned i=0;i<Draws;++i) {
        auto p=sampler.Begin([](const auto&,auto){});
        // 16 markers covers the normal multi-stage buffer/texture sequence.
        // A dependent work stream prevents the empty loop being optimized out.
        for(unsigned phase=0;phase<16;++phase){
            work=(work^i^phase)*0x9e3779b97f4a7c15ull;
            p.Phase(phase%6);
        }
    }
    const double ns=std::chrono::duration<double,std::nano>(std::chrono::steady_clock::now()-before).count()/Draws;
    const auto s=sampler.Current();
    return {ns,work,s.cpu_reads,s.samples};
}
int main(int argc,char** argv) {
    Correctness();
    if(argc==2 && !std::strcmp(argv[1],"--correctness-only"))return 0;
    std::array<double,9> off{},on{};std::uint64_t sink=0;
    for(unsigned sample=0;sample<10;++sample) {
        BenchResult a,b;
        if(sample&1){b=Measure(true);a=Measure(false);}else{a=Measure(false);b=Measure(true);}
        assert(a.sink==b.sink && !a.reads);sink+=a.sink+b.sink;
        if(sample){off[sample-1]=a.ns;on[sample-1]=b.ns;}
        assert(b.reads==b.samples*18);
    }
    std::ranges::sort(off);std::ranges::sort(on);
    std::printf("real-clock sampler median ns/draw OFF=%.2f ON=%.2f added=%.2f "
                "(16 markers; 8 samples per role per 1s budget; no GPU queries) sink=%llu\n",
                off[4],on[4],on[4]-off[4],(unsigned long long)sink);
}
