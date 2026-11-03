// SPDX-License-Identifier: GPL-2.0-or-later
#include "../gpu/shadps4/video_core/buffer_cache/dma_upload_batch.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <cstdio>
#include <random>
#include <tuple>

struct Copy { std::uint64_t srcOffset, dstOffset, size; };
using Batch = VideoCore::DmaUploadBatch<unsigned, Copy>;
using Span = std::tuple<unsigned,std::uint64_t,std::uint64_t>;

int main() {
    Batch batch;
    std::vector<Span> emitted, expected;
    unsigned batches = 0;
    auto flush = [&](unsigned arena, std::span<Copy> copies, std::uint64_t bytes) {
        assert(!copies.empty() && copies.size() <= Batch::MaxCopies);
        std::uint64_t offset = 0;
        for (auto& c : copies) {
            offset = (offset + 3) & ~std::uint64_t{3};
            assert(c.srcOffset == offset);
            offset += c.size;
            emitted.emplace_back(arena,c.dstOffset,c.size);
            // UploadCopies consumes and rebases this mutable span in production.
            c.srcOffset += 4096;
            c.dstOffset -= 128;
        }
        assert(offset == bytes);
        assert(bytes <= Batch::MaxBytes || copies.size() == 1);
        ++batches;
    };
    auto add = [&](unsigned arena,std::uint64_t address,std::uint64_t size) {
        if(size) expected.emplace_back(arena,address,size);
        batch.Add(arena,address,size,flush);
    };
    batch.Finish(flush);add(1,0,0);assert(batches==0);
    for (unsigned i=0;i<1024;++i) add(1,0x1000000000ull+i*4096,64);
    batch.Finish(flush);
    assert(batches==4 && emitted==expected);
    std::printf("dma-upload-batch: 1024 sparse uploads -> %u bounded staging/copy batches; same ranges/order\n",batches);
    add(2,0x2000000000ull,2*1024*1024);
    add(2,0x2000200000ull,2*1024*1024);
    assert(batches==5);
    add(2,0x2000400000ull,Batch::MaxBytes+4);assert(batches==6);
    add(2,0x2000900000ull,16);add(3,0x3000000000ull,16);
    add(2,0x2000a00000ull,32);batch.Finish(flush);
    assert(batches==9 && emitted==expected);

    // Byte-for-byte comparison with independent immediate uploads. Sparse holes
    // retain GPU-only bytes, including when the same arena is revisited.
    constexpr std::size_t Memory=1024*1024;
    std::array<std::vector<unsigned char>,3> guest,old_gpu,new_gpu;
    std::mt19937 rng(17);
    for(unsigned a=0;a<guest.size();++a) {
        guest[a].resize(Memory);old_gpu[a].resize(Memory);new_gpu[a].resize(Memory);
        for(std::size_t i=0;i<Memory;++i) {
            guest[a][i]=static_cast<unsigned char>(rng());
            old_gpu[a][i]=new_gpu[a][i]=static_cast<unsigned char>(rng());
        }
    }
    Batch data_batch;
    auto copy_batch = [&](unsigned a,std::span<Copy> copies,std::uint64_t bytes) {
        std::vector<unsigned char> staging(bytes);
        for(const auto& c:copies)
            std::copy_n(guest[a].data()+c.dstOffset,c.size,staging.data()+c.srcOffset);
        for(const auto& c:copies)
            std::copy_n(staging.data()+c.srcOffset,c.size,new_gpu[a].data()+c.dstOffset);
    };
    for(unsigned frame=0;frame<20;++frame) {
        for(unsigned n=0;n<1500;++n) {
            const unsigned a=rng()%3;
            const std::size_t address=rng()%(Memory-4096),size=1+rng()%4096;
            std::copy_n(guest[a].data()+address,size,old_gpu[a].data()+address);
            data_batch.Add(a,address,size,copy_batch);
        }
        data_batch.Finish(copy_batch);
        assert(new_gpu==old_gpu);
        for(auto& g:guest) for(unsigned n=0;n<100;++n) g[rng()%Memory]^=0xff;
    }
    puts("dma-upload-batch: arena boundaries, gaps, 4 MiB/256 limits, mutable offsets and 30000 byte-equivalent uploads passed");
}
