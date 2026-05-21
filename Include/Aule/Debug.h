/*
 * Copyright (c) 2025 John M. Parsaie
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef AULE_DEBUG_H
#define AULE_DEBUG_H

// Thin wrappers around VK_EXT_debug_utils. Aule unconditionally enables the
// extension at instance creation, so these helpers are safe to call on any
// modern desktop driver and on MoltenVK.
//
// Why bother:
//   - On MoltenVK the name set here propagates to the underlying Metal object's
//     `.label` property. That is what Xcode Instruments displays in the
//     "Metal Resource Allocations" and "Metal System Trace" instruments, and
//     what shows up in Xcode GPU frame captures.
//   - On desktop Vulkan (validation layers, RenderDoc, Nsight) the same names
//     surface in the resource browser and capture timeline.
//
// All entry points are no-ops if vkSetDebugUtilsObjectNameEXT failed to load
// (e.g. running against a driver that silently dropped the extension).

namespace Aule::Debug
{
    // Generic typed setter. Prefer the per-object overloads below when
    // possible; they pick the correct VkObjectType for you.
    inline void SetObjectName(VkDevice device, VkObjectType type, uint64_t handle, const char* name)
    {
        if (vkSetDebugUtilsObjectNameEXT == nullptr || handle == 0 || name == nullptr)
            return;

        VkDebugUtilsObjectNameInfoEXT info = { VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT };

        info.objectType   = type;
        info.objectHandle = handle;
        info.pObjectName  = name;

        vkSetDebugUtilsObjectNameEXT(device, &info);
    }

    // clang-format off
    inline void SetName(VkDevice d, VkBuffer              h, const char* n) { SetObjectName(d, VK_OBJECT_TYPE_BUFFER,                reinterpret_cast<uint64_t>(h), n); }
    inline void SetName(VkDevice d, VkImage               h, const char* n) { SetObjectName(d, VK_OBJECT_TYPE_IMAGE,                 reinterpret_cast<uint64_t>(h), n); }
    inline void SetName(VkDevice d, VkImageView           h, const char* n) { SetObjectName(d, VK_OBJECT_TYPE_IMAGE_VIEW,            reinterpret_cast<uint64_t>(h), n); }
    inline void SetName(VkDevice d, VkSampler             h, const char* n) { SetObjectName(d, VK_OBJECT_TYPE_SAMPLER,               reinterpret_cast<uint64_t>(h), n); }
    inline void SetName(VkDevice d, VkDeviceMemory        h, const char* n) { SetObjectName(d, VK_OBJECT_TYPE_DEVICE_MEMORY,         reinterpret_cast<uint64_t>(h), n); }
    inline void SetName(VkDevice d, VkShaderModule        h, const char* n) { SetObjectName(d, VK_OBJECT_TYPE_SHADER_MODULE,         reinterpret_cast<uint64_t>(h), n); }
    inline void SetName(VkDevice d, VkPipeline            h, const char* n) { SetObjectName(d, VK_OBJECT_TYPE_PIPELINE,              reinterpret_cast<uint64_t>(h), n); }
    inline void SetName(VkDevice d, VkPipelineLayout      h, const char* n) { SetObjectName(d, VK_OBJECT_TYPE_PIPELINE_LAYOUT,       reinterpret_cast<uint64_t>(h), n); }
    inline void SetName(VkDevice d, VkDescriptorSetLayout h, const char* n) { SetObjectName(d, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, reinterpret_cast<uint64_t>(h), n); }
    inline void SetName(VkDevice d, VkDescriptorSet       h, const char* n) { SetObjectName(d, VK_OBJECT_TYPE_DESCRIPTOR_SET,        reinterpret_cast<uint64_t>(h), n); }
    inline void SetName(VkDevice d, VkDescriptorPool      h, const char* n) { SetObjectName(d, VK_OBJECT_TYPE_DESCRIPTOR_POOL,       reinterpret_cast<uint64_t>(h), n); }
    inline void SetName(VkDevice d, VkRenderPass          h, const char* n) { SetObjectName(d, VK_OBJECT_TYPE_RENDER_PASS,           reinterpret_cast<uint64_t>(h), n); }
    inline void SetName(VkDevice d, VkFramebuffer         h, const char* n) { SetObjectName(d, VK_OBJECT_TYPE_FRAMEBUFFER,           reinterpret_cast<uint64_t>(h), n); }
    inline void SetName(VkDevice d, VkCommandBuffer       h, const char* n) { SetObjectName(d, VK_OBJECT_TYPE_COMMAND_BUFFER,        reinterpret_cast<uint64_t>(h), n); }
    inline void SetName(VkDevice d, VkCommandPool         h, const char* n) { SetObjectName(d, VK_OBJECT_TYPE_COMMAND_POOL,          reinterpret_cast<uint64_t>(h), n); }
    inline void SetName(VkDevice d, VkQueue               h, const char* n) { SetObjectName(d, VK_OBJECT_TYPE_QUEUE,                 reinterpret_cast<uint64_t>(h), n); }
    inline void SetName(VkDevice d, VkSemaphore           h, const char* n) { SetObjectName(d, VK_OBJECT_TYPE_SEMAPHORE,             reinterpret_cast<uint64_t>(h), n); }
    inline void SetName(VkDevice d, VkFence               h, const char* n) { SetObjectName(d, VK_OBJECT_TYPE_FENCE,                 reinterpret_cast<uint64_t>(h), n); }
    inline void SetName(VkDevice d, VkSwapchainKHR        h, const char* n) { SetObjectName(d, VK_OBJECT_TYPE_SWAPCHAIN_KHR,         reinterpret_cast<uint64_t>(h), n); }
    // clang-format on

    // Begin a labeled region in a command buffer. Shows up as a debug group in
    // RenderDoc / Nsight and as a pushDebugGroup signpost in Metal System
    // Trace. Pair with EndLabel(), or use ScopedLabel for RAII.
    inline void BeginLabel(VkCommandBuffer cmd, const char* name, const float color[4] = nullptr)
    {
        if (vkCmdBeginDebugUtilsLabelEXT == nullptr || name == nullptr)
            return;

        VkDebugUtilsLabelEXT label = { VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT };
        label.pLabelName           = name;
        if (color)
        {
            label.color[0] = color[0];
            label.color[1] = color[1];
            label.color[2] = color[2];
            label.color[3] = color[3];
        }
        vkCmdBeginDebugUtilsLabelEXT(cmd, &label);
    }

    inline void EndLabel(VkCommandBuffer cmd)
    {
        if (vkCmdEndDebugUtilsLabelEXT == nullptr)
            return;
        vkCmdEndDebugUtilsLabelEXT(cmd);
    }

    // RAII helper: emits BeginLabel on construction and EndLabel on scope exit.
    // Intended for scoping render passes / subsystems inside a frame's command
    // buffer recording, e.g.
    //
    //     {
    //         Aule::Debug::ScopedLabel _(cmd, "Hair.Simulate");
    //         vkCmdDispatch(cmd, ...);
    //     }
    struct ScopedLabel
    {
        VkCommandBuffer cmd;

        ScopedLabel(VkCommandBuffer c, const char* name, const float color[4] = nullptr) : cmd(c)
        {
            BeginLabel(cmd, name, color);
        }

        ~ScopedLabel() { EndLabel(cmd); }

        ScopedLabel(const ScopedLabel&)            = delete;
        ScopedLabel& operator=(const ScopedLabel&) = delete;
    };
} // namespace Aule::Debug

#endif
