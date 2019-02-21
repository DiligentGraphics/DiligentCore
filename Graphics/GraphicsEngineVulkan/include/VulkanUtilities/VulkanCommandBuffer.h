/*     Copyright 2015-2019 Egor Yusov
*
*  Licensed under the Apache License, Version 2.0 (the "License");
*  you may not use this file except in compliance with the License.
*  You may obtain a copy of the License at
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
*  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
*  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
*  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT OF ANY PROPRIETARY RIGHTS.
*
*  In no event and under no legal theory, whether in tort (including negligence),
*  contract, or otherwise, unless required by applicable law (such as deliberate
*  and grossly negligent acts) or agreed to in writing, shall any Contributor be
*  liable for any damages, including any direct, indirect, special, incidental,
*  or consequential damages of any character arising as a result of this License or
*  out of the use or inability to use the software (including but not limited to damages
*  for loss of goodwill, work stoppage, computer failure or malfunction, or any and
*  all other commercial damages or losses), even if such Contributor has been advised
*  of the possibility of such damages.
*/

#pragma once

#include "vulkan.h"
#include "DebugUtilities.h"

namespace VulkanUtilities
{
    class VulkanCommandBuffer
    {
    public:
        VulkanCommandBuffer(VkPipelineStageFlags EnabledGraphicsShaderStages)noexcept : 
            m_EnabledGraphicsShaderStages(EnabledGraphicsShaderStages)
        {}

        VulkanCommandBuffer             (const VulkanCommandBuffer&)  = delete;
        VulkanCommandBuffer             (      VulkanCommandBuffer&&) = delete;
        VulkanCommandBuffer& operator = (const VulkanCommandBuffer&)  = delete;
        VulkanCommandBuffer& operator = (      VulkanCommandBuffer&&) = delete;

        void ClearColorImage(VkImage Image, const VkClearColorValue& Color, const VkImageSubresourceRange& Subresource)
        {
            VERIFY_EXPR(m_VkCmdBuffer != VK_NULL_HANDLE);
            VERIFY(m_State.RenderPass == VK_NULL_HANDLE, "vkCmdClearColorImage() must be called outside of render pass (17.1)");
            VERIFY(Subresource.aspectMask == VK_IMAGE_ASPECT_COLOR_BIT, "The aspectMask of all image subresource ranges must only include VK_IMAGE_ASPECT_COLOR_BIT (17.1)");

            vkCmdClearColorImage(
                m_VkCmdBuffer,
                Image,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, // must be VK_IMAGE_LAYOUT_GENERAL or VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
                &Color,
                1,
                &Subresource
            );
        }

        void ClearDepthStencilImage(VkImage Image, const VkClearDepthStencilValue& DepthStencil, const VkImageSubresourceRange& Subresource)
        {
            VERIFY_EXPR(m_VkCmdBuffer != VK_NULL_HANDLE);
            VERIFY(m_State.RenderPass == VK_NULL_HANDLE, "vkCmdClearDepthStencilImage() must be called outside of render pass (17.1)");
            VERIFY( (Subresource.aspectMask &  (VK_IMAGE_ASPECT_DEPTH_BIT|VK_IMAGE_ASPECT_STENCIL_BIT)) != 0 && 
                    (Subresource.aspectMask & ~(VK_IMAGE_ASPECT_DEPTH_BIT|VK_IMAGE_ASPECT_STENCIL_BIT)) == 0,
                   "The aspectMask of all image subresource ranges must only include VK_IMAGE_ASPECT_DEPTH_BIT or VK_IMAGE_ASPECT_STENCIL_BIT(17.1)");

            vkCmdClearDepthStencilImage(
                m_VkCmdBuffer,
                Image,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, // must be VK_IMAGE_LAYOUT_GENERAL or VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
                &DepthStencil,
                1,
                &Subresource
            );
        }

        void ClearAttachment(const VkClearAttachment& Attachment, const VkClearRect& ClearRect)
        {
            VERIFY_EXPR(m_VkCmdBuffer != VK_NULL_HANDLE);
            VERIFY(m_State.RenderPass != VK_NULL_HANDLE, "vkCmdClearAttachments() must be called inside render pass (17.2)");
            
            vkCmdClearAttachments(
                m_VkCmdBuffer,
                1,
                &Attachment,
                1,
                &ClearRect // The rectangular region specified by each element of pRects must be 
                           // contained within the render area of the current render pass instance
            );
        }

        void Draw(uint32_t VertexCount, uint32_t InstanceCount, uint32_t FirstVertex, uint32_t FirstInstance)
        {
            VERIFY_EXPR(m_VkCmdBuffer != VK_NULL_HANDLE);
            VERIFY(m_State.RenderPass != VK_NULL_HANDLE, "vkCmdDraw() must be called inside render pass (19.3)");
            VERIFY(m_State.GraphicsPipeline != VK_NULL_HANDLE, "No graphics pipeline bound");

            vkCmdDraw(m_VkCmdBuffer, VertexCount, InstanceCount, FirstVertex, FirstInstance);
        }

        void DrawIndexed(uint32_t  IndexCount,uint32_t InstanceCount, uint32_t FirstIndex, int32_t VertexOffset, uint32_t FirstInstance)
        {
            VERIFY_EXPR(m_VkCmdBuffer != VK_NULL_HANDLE);
            VERIFY(m_State.RenderPass != VK_NULL_HANDLE, "vkCmdDrawIndexed() must be called inside render pass (19.3)");
            VERIFY(m_State.GraphicsPipeline != VK_NULL_HANDLE, "No graphics pipeline bound");
            VERIFY(m_State.IndexBuffer != VK_NULL_HANDLE, "No index buffer bound");

            vkCmdDrawIndexed(m_VkCmdBuffer, IndexCount, InstanceCount, FirstIndex, VertexOffset, FirstInstance);
        }

        void DrawIndirect(VkBuffer Buffer, VkDeviceSize Offset, uint32_t DrawCount, uint32_t Stride)
        {
            VERIFY_EXPR(m_VkCmdBuffer != VK_NULL_HANDLE);
            VERIFY(m_State.RenderPass != VK_NULL_HANDLE, "vkCmdDrawIndirect() must be called inside render pass (19.3)");
            VERIFY(m_State.GraphicsPipeline != VK_NULL_HANDLE, "No graphics pipeline bound");

            vkCmdDrawIndirect(m_VkCmdBuffer, Buffer, Offset, DrawCount, Stride);
        }

        void DrawIndexedIndirect(VkBuffer Buffer, VkDeviceSize Offset, uint32_t DrawCount, uint32_t Stride)
        {
            VERIFY_EXPR(m_VkCmdBuffer != VK_NULL_HANDLE);
            VERIFY(m_State.RenderPass != VK_NULL_HANDLE, "vkCmdDrawIndirect() must be called inside render pass (19.3)");
            VERIFY(m_State.GraphicsPipeline != VK_NULL_HANDLE, "No graphics pipeline bound");
            VERIFY(m_State.IndexBuffer != VK_NULL_HANDLE, "No index buffer bound");

            vkCmdDrawIndexedIndirect(m_VkCmdBuffer, Buffer, Offset, DrawCount, Stride);
        }

        void Dispatch(uint32_t GroupCountX, uint32_t GroupCountY, uint32_t GroupCountZ)
        {
            VERIFY_EXPR(m_VkCmdBuffer != VK_NULL_HANDLE);
            VERIFY(m_State.RenderPass == VK_NULL_HANDLE, "vkCmdDispatch() must be called outside of render pass (27)");
            VERIFY(m_State.ComputePipeline != VK_NULL_HANDLE, "No compute pipeline bound");

            vkCmdDispatch(m_VkCmdBuffer, GroupCountX, GroupCountY, GroupCountZ);
        }

        void DispatchIndirect(VkBuffer Buffer, VkDeviceSize Offset)
        {
            VERIFY_EXPR(m_VkCmdBuffer != VK_NULL_HANDLE);
            VERIFY(m_State.RenderPass == VK_NULL_HANDLE, "vkCmdDispatchIndirect() must be called outside of render pass (27)");
            VERIFY(m_State.ComputePipeline != VK_NULL_HANDLE, "No compute pipeline bound");

            vkCmdDispatchIndirect(m_VkCmdBuffer, Buffer, Offset);
        }

        void BeginRenderPass(VkRenderPass RenderPass, VkFramebuffer Framebuffer, uint32_t FramebufferWidth, uint32_t FramebufferHeight)
        {
            VERIFY_EXPR(m_VkCmdBuffer != VK_NULL_HANDLE);
            VERIFY(m_State.RenderPass == VK_NULL_HANDLE, "Current pass has not been ended");

            if (m_State.RenderPass != RenderPass || m_State.Framebuffer != Framebuffer)
            {
                VkRenderPassBeginInfo BeginInfo;
                BeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
                BeginInfo.pNext = nullptr;
                BeginInfo.renderPass = RenderPass;
                BeginInfo.framebuffer = Framebuffer;
                // The render area MUST be contained within the framebuffer dimensions (7.4)
                BeginInfo.renderArea = {{0,0}, { FramebufferWidth, FramebufferHeight }};
                BeginInfo.clearValueCount = 0;
                BeginInfo.pClearValues = nullptr; // an array of VkClearValue structures that contains clear values for 
                                                  // each attachment, if the attachment uses a loadOp value of VK_ATTACHMENT_LOAD_OP_CLEAR 
                                                  // or if the attachment has a depth/stencil format and uses a stencilLoadOp value of 
                                                  // VK_ATTACHMENT_LOAD_OP_CLEAR. The array is indexed by attachment number. Only elements 
                                                  // corresponding to cleared attachments are used. Other elements of pClearValues are 
                                                  // ignored (7.4)

                vkCmdBeginRenderPass(m_VkCmdBuffer, &BeginInfo, 
                    VK_SUBPASS_CONTENTS_INLINE // the contents of the subpass will be recorded inline in the 
                                               // primary command buffer, and secondary command buffers must not 
                                               // be executed within the subpass
                );
                m_State.RenderPass = RenderPass;
                m_State.Framebuffer = Framebuffer;
                m_State.FramebufferWidth = FramebufferWidth;
                m_State.FramebufferHeight = FramebufferHeight;
            }
        }

        void EndRenderPass()
        {
            VERIFY(m_State.RenderPass != VK_NULL_HANDLE, "Render pass has not been started");
            VERIFY_EXPR(m_VkCmdBuffer != VK_NULL_HANDLE);
            vkCmdEndRenderPass(m_VkCmdBuffer);
            m_State.RenderPass  = VK_NULL_HANDLE;
            m_State.Framebuffer = VK_NULL_HANDLE;
            m_State.FramebufferWidth  = 0;
            m_State.FramebufferHeight = 0;
        }

        void EndCommandBuffer()
        {
            VERIFY_EXPR(m_VkCmdBuffer != VK_NULL_HANDLE);
            vkEndCommandBuffer(m_VkCmdBuffer);
        }

        void Reset()
        {
            m_VkCmdBuffer = VK_NULL_HANDLE;
            m_State = StateCache{};
        }

        void BindComputePipeline(VkPipeline ComputePipeline)
        {
            // 9.8
            VERIFY_EXPR(m_VkCmdBuffer != VK_NULL_HANDLE);
            if (m_State.ComputePipeline != ComputePipeline)
            {
                vkCmdBindPipeline(m_VkCmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, ComputePipeline);
                m_State.ComputePipeline = ComputePipeline;
            }
        }

        void BindGraphicsPipeline(VkPipeline GraphicsPipeline)
        {
            // 9.8
            VERIFY_EXPR(m_VkCmdBuffer != VK_NULL_HANDLE);
            if (m_State.GraphicsPipeline != GraphicsPipeline)
            {
                vkCmdBindPipeline(m_VkCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, GraphicsPipeline);
                m_State.GraphicsPipeline = GraphicsPipeline;
            }
        }

        void SetViewports(uint32_t FirstViewport, uint32_t ViewportCount, const VkViewport* pViewports)
        {
            VERIFY_EXPR(m_VkCmdBuffer != VK_NULL_HANDLE);
            vkCmdSetViewport(m_VkCmdBuffer, FirstViewport, ViewportCount, pViewports);
        }

        void SetScissorRects(uint32_t FirstScissor, uint32_t ScissorCount, const VkRect2D* pScissors)
        {
            VERIFY_EXPR(m_VkCmdBuffer != VK_NULL_HANDLE);
            vkCmdSetScissor(m_VkCmdBuffer, FirstScissor, ScissorCount, pScissors);
        }

        void SetStencilReference(uint32_t Reference)
        {
            VERIFY_EXPR(m_VkCmdBuffer != VK_NULL_HANDLE);
            vkCmdSetStencilReference(m_VkCmdBuffer, VK_STENCIL_FRONT_AND_BACK, Reference);
        }

        void SetBlendConstants(const float BlendConstants[4])
        {
            VERIFY_EXPR(m_VkCmdBuffer != VK_NULL_HANDLE);
            vkCmdSetBlendConstants(m_VkCmdBuffer, BlendConstants);
        }

        void BindIndexBuffer(VkBuffer Buffer, VkDeviceSize Offset, VkIndexType IndexType)
        {
            VERIFY_EXPR(m_VkCmdBuffer != VK_NULL_HANDLE);
            if (m_State.IndexBuffer       != Buffer ||
                m_State.IndexBufferOffset != Offset ||
                m_State.IndexType         != IndexType)
            {
                vkCmdBindIndexBuffer(m_VkCmdBuffer, Buffer, Offset, IndexType);
                m_State.IndexBuffer = Buffer;
                m_State.IndexBufferOffset = Offset;
                m_State.IndexType = IndexType;
            }
        }

        void BindVertexBuffers(uint32_t firstBinding, uint32_t bindingCount, const VkBuffer* pBuffers, const VkDeviceSize* pOffsets)
        {
            VERIFY_EXPR(m_VkCmdBuffer != VK_NULL_HANDLE);
            vkCmdBindVertexBuffers(m_VkCmdBuffer, firstBinding, bindingCount, pBuffers, pOffsets);
        }

        static void TransitionImageLayout(VkCommandBuffer                CmdBuffer,
                                          VkImage                        Image, 
                                          VkImageLayout                  OldLayout,
                                          VkImageLayout                  NewLayout,
                                          const VkImageSubresourceRange& SubresRange,
                                          VkPipelineStageFlags           EnabledGraphicsShaderStages,
                                          VkPipelineStageFlags           SrcStages  = 0,
                                          VkPipelineStageFlags           DestStages = 0);

        void TransitionImageLayout(VkImage                        Image, 
                                   VkImageLayout                  OldLayout,
                                   VkImageLayout                  NewLayout,
                                   const VkImageSubresourceRange& SubresRange,
                                   VkPipelineStageFlags           SrcStages  = 0, 
                                   VkPipelineStageFlags           DestStages = 0)
        {
            VERIFY_EXPR(m_VkCmdBuffer != VK_NULL_HANDLE);
            if (m_State.RenderPass  != VK_NULL_HANDLE)
            {
                // Image layout transitions within a render pass execute
                // dependencies between attachments
                EndRenderPass();
            }
            TransitionImageLayout(m_VkCmdBuffer, Image, OldLayout, NewLayout, SubresRange, m_EnabledGraphicsShaderStages, SrcStages, DestStages);
        }


        static void BufferMemoryBarrier(VkCommandBuffer      CmdBuffer,
                                        VkBuffer             Buffer, 
                                        VkAccessFlags        srcAccessMask,
                                        VkAccessFlags        dstAccessMask,
                                        VkPipelineStageFlags EnabledGraphicsShaderStages,
                                        VkPipelineStageFlags SrcStages  = 0,
                                        VkPipelineStageFlags DestStages = 0);

        void BufferMemoryBarrier(VkBuffer             Buffer, 
                                 VkAccessFlags        srcAccessMask,
                                 VkAccessFlags        dstAccessMask,
                                 VkPipelineStageFlags SrcStages  = 0,
                                 VkPipelineStageFlags DestStages = 0)
        {
            VERIFY_EXPR(m_VkCmdBuffer != VK_NULL_HANDLE);
            if (m_State.RenderPass  != VK_NULL_HANDLE)
            {
                // Image layout transitions within a render pass execute
                // dependencies between attachments
                EndRenderPass();
            }
            BufferMemoryBarrier(m_VkCmdBuffer, Buffer, srcAccessMask, dstAccessMask, m_EnabledGraphicsShaderStages, SrcStages, DestStages);
        }

        void BindDescriptorSets(VkPipelineBindPoint     pipelineBindPoint,
                                VkPipelineLayout        layout,
                                uint32_t                firstSet,
                                uint32_t                descriptorSetCount,
                                const VkDescriptorSet*  pDescriptorSets,
                                uint32_t                dynamicOffsetCount = 0,
                                const uint32_t*         pDynamicOffsets    = nullptr)
        {
            VERIFY_EXPR(m_VkCmdBuffer != VK_NULL_HANDLE);
            vkCmdBindDescriptorSets(m_VkCmdBuffer, pipelineBindPoint, layout, firstSet, descriptorSetCount, pDescriptorSets, dynamicOffsetCount, pDynamicOffsets);
        }

        void CopyBuffer(VkBuffer            srcBuffer,
                        VkBuffer            dstBuffer,
                        uint32_t            regionCount,
                        const VkBufferCopy* pRegions)
        {
            VERIFY_EXPR(m_VkCmdBuffer != VK_NULL_HANDLE);
            if (m_State.RenderPass  != VK_NULL_HANDLE)
            {
                // Copy buffer operation must be performed outside of render pass.
                EndRenderPass();
            }
            vkCmdCopyBuffer(m_VkCmdBuffer, srcBuffer, dstBuffer, regionCount, pRegions);
        }
                                          
        void CopyImage(VkImage            srcImage,
                       VkImageLayout      srcImageLayout,
                       VkImage            dstImage,
                       VkImageLayout      dstImageLayout,
                       uint32_t           regionCount,
                       const VkImageCopy* pRegions)
        {
            VERIFY_EXPR(m_VkCmdBuffer != VK_NULL_HANDLE);
            if (m_State.RenderPass  != VK_NULL_HANDLE)
            {
                // Copy operations must be performed outside of render pass.
                EndRenderPass();
            }

            vkCmdCopyImage(m_VkCmdBuffer, srcImage, srcImageLayout, dstImage, dstImageLayout, regionCount, pRegions);
        }

        void CopyBufferToImage(VkBuffer                  srcBuffer,
                               VkImage                   dstImage,
                               VkImageLayout             dstImageLayout,
                               uint32_t                  regionCount,
                               const VkBufferImageCopy*  pRegions)
        {
            VERIFY_EXPR(m_VkCmdBuffer != VK_NULL_HANDLE);
            if (m_State.RenderPass  != VK_NULL_HANDLE)
            {
                // Copy operations must be performed outside of render pass.
                EndRenderPass();
            }

            vkCmdCopyBufferToImage(m_VkCmdBuffer, srcBuffer, dstImage, dstImageLayout, regionCount, pRegions);
        }

        void FlushBarriers();

        void SetVkCmdBuffer(VkCommandBuffer VkCmdBuffer)
        {
            m_VkCmdBuffer = VkCmdBuffer;
        }
        VkCommandBuffer GetVkCmdBuffer()const{return m_VkCmdBuffer;}

        struct StateCache
        {
            VkRenderPass    RenderPass          = VK_NULL_HANDLE;
            VkFramebuffer   Framebuffer         = VK_NULL_HANDLE;
            VkPipeline      GraphicsPipeline    = VK_NULL_HANDLE;
            VkPipeline      ComputePipeline     = VK_NULL_HANDLE;
            VkBuffer        IndexBuffer         = VK_NULL_HANDLE;
            VkDeviceSize    IndexBufferOffset   = 0;
            VkIndexType     IndexType           = VK_INDEX_TYPE_MAX_ENUM;
            uint32_t        FramebufferWidth    = 0;
            uint32_t        FramebufferHeight   = 0;
        };

        const StateCache& GetState()const{return m_State;}

    private:
        StateCache m_State;
        VkCommandBuffer m_VkCmdBuffer = VK_NULL_HANDLE;
        const VkPipelineStageFlags m_EnabledGraphicsShaderStages;
    };
}
