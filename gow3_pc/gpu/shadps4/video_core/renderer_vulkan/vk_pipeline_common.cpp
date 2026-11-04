// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <span>

#include "video_core/renderer_vulkan/vk_descriptor_record.h"

#include <boost/container/small_vector.hpp>
#include <boost/container/static_vector.hpp>

#include "shader_recompiler/resource.h"
#include "video_core/renderer_vulkan/vk_instance.h"
#include "video_core/renderer_vulkan/vk_pipeline_cache.h"
#include "video_core/renderer_vulkan/vk_pipeline_common.h"
#include "video_core/renderer_vulkan/vk_scheduler.h"

namespace Vulkan {

Pipeline::Pipeline(const Instance& instance_, Scheduler& scheduler_, DescriptorHeap& desc_heap_,
                   const Shader::Profile& profile_, vk::PipelineCache pipeline_cache,
                   bool is_compute_ /*= false*/)
    : instance{instance_}, scheduler{scheduler_}, desc_heap{desc_heap_}, profile{profile_},
      is_compute{is_compute_} {}

Pipeline::~Pipeline() = default;


void Pipeline::BindResources(DescriptorWrites& set_writes, const Shader::PushData& push_data,
                             std::span<const vk::DescriptorBufferInfo> source_buffers,
                             std::span<const vk::DescriptorImageInfo> source_images) const {
    const auto bind_point =
        IsCompute() ? vk::PipelineBindPoint::eCompute : vk::PipelineBindPoint::eGraphics;
    const auto stage_flags = IsCompute() ? vk::ShaderStageFlagBits::eCompute : AllGraphicsStageBits;
    const vk::PipelineLayout layout = *pipeline_layout;
    scheduler.Record([layout, stage_flags, push_data](vk::CommandBuffer cmdbuf) {
        cmdbuf.pushConstants(layout, stage_flags, 0u, sizeof(push_data), &push_data);
    });

    // Bind descriptor set.
    if (set_writes.empty()) {
        return;
    }

    if (uses_push_descriptors) {
        if (!scheduler.IsRecordingDeferred()) {
            scheduler.Record([&](vk::CommandBuffer cmdbuf) {
                cmdbuf.pushDescriptorSetKHR(bind_point, layout, 0, set_writes);
            });
            return;
        }
        // The current draw owns two stable, contiguous arrays. Snapshot each one once
        // before deferring the Vulkan push-descriptor command; relocate pointers into
        // those copies. Descriptors not backed by these arrays retain the old per-write
        // snapshot path. No descriptor is reused across draws or guest/GPU writes.
        using Detail::DescriptorSourceOffset;
        using Detail::InvalidDescriptorOffset;
        const auto buffer_offset = [&](const vk::WriteDescriptorSet& w) {
            return DescriptorSourceOffset(source_buffers, w.pBufferInfo, w.descriptorCount);
        };
        const auto image_offset = [&](const vk::WriteDescriptorSet& w) {
            return DescriptorSourceOffset(source_images, w.pImageInfo, w.descriptorCount);
        };
        // Reserve enough for every copy plus alignment and the deferred command. This
        // keeps the entire snapshot in the same RecordChunk; otherwise a captured span
        // could outlive its chunk while the recording thread is using it.
        size_t bytes = set_writes.size() * sizeof(vk::WriteDescriptorSet) +
                       alignof(vk::WriteDescriptorSet) +
                       source_buffers.size_bytes() + alignof(vk::DescriptorBufferInfo) +
                       source_images.size_bytes() + alignof(vk::DescriptorImageInfo) + 64;
        for (const auto& w : set_writes) {
            if (w.pBufferInfo && buffer_offset(w) == InvalidDescriptorOffset) {
                bytes += w.descriptorCount * sizeof(vk::DescriptorBufferInfo) +
                         alignof(vk::DescriptorBufferInfo);
            }
            if (w.pImageInfo && image_offset(w) == InvalidDescriptorOffset) {
                bytes += w.descriptorCount * sizeof(vk::DescriptorImageInfo) +
                         alignof(vk::DescriptorImageInfo);
            }
            if (w.pTexelBufferView) {
                bytes += w.descriptorCount * sizeof(vk::BufferView) + alignof(vk::BufferView);
            }
        }
        scheduler.ReserveRecordData(bytes);
        const auto writes =
            scheduler.RecordData(std::span<const vk::WriteDescriptorSet>{set_writes});
        const auto saved_buffers = scheduler.RecordData(source_buffers);
        const auto saved_images = scheduler.RecordData(source_images);
        auto* patched = const_cast<vk::WriteDescriptorSet*>(writes.data());
        for (size_t i = 0; i < writes.size(); ++i) {
            auto& w = patched[i];
            if (w.pBufferInfo) {
                const size_t offset = buffer_offset(w);
                w.pBufferInfo = offset == InvalidDescriptorOffset
                                    ? scheduler.RecordData(std::span{w.pBufferInfo, w.descriptorCount}).data()
                                    : saved_buffers.data() + offset;
            }
            if (w.pImageInfo) {
                const size_t offset = image_offset(w);
                w.pImageInfo = offset == InvalidDescriptorOffset
                                   ? scheduler.RecordData(std::span{w.pImageInfo, w.descriptorCount}).data()
                                   : saved_images.data() + offset;
            }
            if (w.pTexelBufferView) {
                w.pTexelBufferView =
                    scheduler.RecordData(std::span{w.pTexelBufferView, w.descriptorCount}).data();
            }
        }
        scheduler.Record([bind_point, layout, writes](vk::CommandBuffer cmdbuf) {
            cmdbuf.pushDescriptorSetKHR(bind_point, layout, 0, writes);
        });
        return;
    }

    // Descriptor set updates are device calls: they stay on this thread.
    const auto desc_set = desc_heap.Commit(*desc_layout);
    for (auto& set_write : set_writes) {
        set_write.dstSet = desc_set;
    }
    instance.GetDevice().updateDescriptorSets(set_writes, {});
    scheduler.Record([bind_point, layout, desc_set](vk::CommandBuffer cmdbuf) {
        cmdbuf.bindDescriptorSets(bind_point, layout, 0, desc_set, {});
    });
}

std::string Pipeline::GetDebugString() const {
    std::string stage_desc;
    for (const auto& stage : stages) {
        if (stage) {
            const auto shader_name = PipelineCache::GetShaderName(stage->hw_stage, stage->pgm_hash);
            if (stage_desc.empty()) {
                stage_desc = shader_name;
            } else {
                stage_desc = fmt::format("{},{}", stage_desc, shader_name);
            }
        }
    }
    return stage_desc;
}

} // namespace Vulkan
