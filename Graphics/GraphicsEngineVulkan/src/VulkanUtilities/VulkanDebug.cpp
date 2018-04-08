/*
* Vulkan examples debug wrapper
* 
* Appendix for VK_EXT_Debug_Report can be found at https://github.com/KhronosGroup/Vulkan-Docs/blob/1.0-VK_EXT_debug_report/doc/specs/vulkan/appendices/debug_report.txt
*
* Copyright (C) 2016 by Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

#include <sstream>

#include "VulkanUtilities/VulkanDebug.h"
#include "Errors.h"
#include "DebugUtilities.h"

namespace VulkanUtilities
{
    static PFN_vkDestroyDebugReportCallbackEXT DestroyDebugReportCallback = VK_NULL_HANDLE;
    static VkDebugReportCallbackEXT msgCallback = VK_NULL_HANDLE;

    VKAPI_ATTR VkBool32 VKAPI_CALL MessageCallback(
        VkDebugReportFlagsEXT flags,
        VkDebugReportObjectTypeEXT objType,
        uint64_t srcObject,
        size_t location,
        int32_t msgCode,
        const char* pLayerPrefix,
        const char* pMsg,
        void* pUserData)
    {
        std::stringstream debugMessage;

        debugMessage << "Vulkan debug message";

        // Select prefix depending on flags passed to the callback
        // Note that multiple flags may be set for a single validation message

        // Error that may result in undefined behaviour
        if (flags & VK_DEBUG_REPORT_ERROR_BIT_EXT)
            debugMessage << " (ERROR)";

        // Warnings may hint at unexpected / non-spec API usage
        if (flags & VK_DEBUG_REPORT_WARNING_BIT_EXT)
            debugMessage << " (WARNING)";

        // May indicate sub-optimal usage of the API
        if (flags & VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT)
            debugMessage << " (Performance)";

        // Informal messages that may become handy during debugging
        if (flags & VK_DEBUG_REPORT_INFORMATION_BIT_EXT)
            debugMessage << " (Info)";

        // Diagnostic info from the Vulkan loader and layers
        // Usually not helpful in terms of API usage, but may help to debug layer and loader problems 
        if (flags & VK_DEBUG_REPORT_DEBUG_BIT_EXT)
            debugMessage << " (Debug)";

        debugMessage << " [" << pLayerPrefix << "] Code " << msgCode << "\n" << pMsg << std::endl;

        Diligent::DebugMessageSeverity MsgSeverity = Diligent::DebugMessageSeverity::Info;
        if (flags & VK_DEBUG_REPORT_ERROR_BIT_EXT)
            MsgSeverity = Diligent::DebugMessageSeverity::Error;
        else if (flags & VK_DEBUG_REPORT_WARNING_BIT_EXT)
            MsgSeverity = Diligent::DebugMessageSeverity::Warning;
        else
            MsgSeverity = Diligent::DebugMessageSeverity::Info;
        LOG_DEBUG_MESSAGE(MsgSeverity, debugMessage.str().c_str());

        // The return value of this callback controls wether the Vulkan call that caused
        // the validation message will be aborted or not
        // We return VK_FALSE as we DON'T want Vulkan calls that cause a validation message 
        // (and return a VkResult) to abort
        // If you instead want to have calls abort, pass in VK_TRUE and the function will 
        // return VK_ERROR_VALIDATION_FAILED_EXT 
        return VK_FALSE;
    }

    void SetupDebugging(VkInstance instance, VkDebugReportFlagsEXT flags, VkDebugReportCallbackEXT callBack)
    {
        auto CreateDebugReportCallback = reinterpret_cast<PFN_vkCreateDebugReportCallbackEXT>(vkGetInstanceProcAddr(instance, "vkCreateDebugReportCallbackEXT"));
        VERIFY_EXPR(CreateDebugReportCallback != VK_NULL_HANDLE);
        //auto dbgBreakCallback = reinterpret_cast<PFN_vkDebugReportMessageEXT>(vkGetInstanceProcAddr(instance, "vkDebugReportMessageEXT"));
        
        DestroyDebugReportCallback = reinterpret_cast<PFN_vkDestroyDebugReportCallbackEXT>(vkGetInstanceProcAddr(instance, "vkDestroyDebugReportCallbackEXT"));
        VERIFY_EXPR(DestroyDebugReportCallback != VK_NULL_HANDLE);

        VkDebugReportCallbackCreateInfoEXT dbgCreateInfo = {};
        dbgCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CREATE_INFO_EXT;
        dbgCreateInfo.pfnCallback = (PFN_vkDebugReportCallbackEXT)MessageCallback;
        dbgCreateInfo.flags = flags;

        VkResult err = CreateDebugReportCallback(
            instance,
            &dbgCreateInfo,
            nullptr,
            (callBack != VK_NULL_HANDLE) ? &callBack : &msgCallback);
        VERIFY_EXPR(err == VK_SUCCESS);
    }

    void FreeDebugCallback(VkInstance instance)
    {
        if (msgCallback != VK_NULL_HANDLE)
        {
            DestroyDebugReportCallback(instance, msgCallback, nullptr);
        }
    }

    static PFN_vkDebugMarkerSetObjectTagEXT pfnDebugMarkerSetObjectTag = VK_NULL_HANDLE;
    static PFN_vkDebugMarkerSetObjectNameEXT pfnDebugMarkerSetObjectName = VK_NULL_HANDLE;
    static PFN_vkCmdDebugMarkerBeginEXT pfnCmdDebugMarkerBegin = VK_NULL_HANDLE;
    static PFN_vkCmdDebugMarkerEndEXT pfnCmdDebugMarkerEnd = VK_NULL_HANDLE;
    static PFN_vkCmdDebugMarkerInsertEXT pfnCmdDebugMarkerInsert = VK_NULL_HANDLE;

    void SetupDebugMarkers(VkDevice device)
    {
        pfnDebugMarkerSetObjectTag = reinterpret_cast<PFN_vkDebugMarkerSetObjectTagEXT>(vkGetDeviceProcAddr(device, "vkDebugMarkerSetObjectTagEXT"));
        pfnDebugMarkerSetObjectName = reinterpret_cast<PFN_vkDebugMarkerSetObjectNameEXT>(vkGetDeviceProcAddr(device, "vkDebugMarkerSetObjectNameEXT"));
        pfnCmdDebugMarkerBegin = reinterpret_cast<PFN_vkCmdDebugMarkerBeginEXT>(vkGetDeviceProcAddr(device, "vkCmdDebugMarkerBeginEXT"));
        pfnCmdDebugMarkerEnd = reinterpret_cast<PFN_vkCmdDebugMarkerEndEXT>(vkGetDeviceProcAddr(device, "vkCmdDebugMarkerEndEXT"));
        pfnCmdDebugMarkerInsert = reinterpret_cast<PFN_vkCmdDebugMarkerInsertEXT>(vkGetDeviceProcAddr(device, "vkCmdDebugMarkerInsertEXT"));
    }

    void SetObjectName(VkDevice device, uint64_t object, VkDebugReportObjectTypeEXT objectType, const char *name)
    {
        // Check for valid function pointer (may not be present if not running in a debugging application)
        if (pfnDebugMarkerSetObjectName != VK_NULL_HANDLE)
        {
            VkDebugMarkerObjectNameInfoEXT nameInfo = {};
            nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_OBJECT_NAME_INFO_EXT;
            nameInfo.objectType = objectType;
            nameInfo.object = object;
            nameInfo.pObjectName = name;
            pfnDebugMarkerSetObjectName(device, &nameInfo);
        }
    }

    void SetObjectTag(VkDevice device, uint64_t object, VkDebugReportObjectTypeEXT objectType, uint64_t name, size_t tagSize, const void* tag)
    {
        // Check for valid function pointer (may not be present if not running in a debugging application)
        if (pfnDebugMarkerSetObjectTag)
        {
            VkDebugMarkerObjectTagInfoEXT tagInfo = {};
            tagInfo.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_OBJECT_TAG_INFO_EXT;
            tagInfo.objectType = objectType;
            tagInfo.object = object;
            tagInfo.tagName = name;
            tagInfo.tagSize = tagSize;
            tagInfo.pTag = tag;
            pfnDebugMarkerSetObjectTag(device, &tagInfo);
        }
    }

    //void beginRegion(VkCommandBuffer cmdbuffer, const char* pMarkerName, glm::vec4 color)
    //{
    //    // Check for valid function pointer (may not be present if not running in a debugging application)
    //    if (pfnCmdDebugMarkerBegin)
    //    {
    //        VkDebugMarkerMarkerInfoEXT markerInfo = {};
    //        markerInfo.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_MARKER_INFO_EXT;
    //        memcpy(markerInfo.color, &color[0], sizeof(float) * 4);
    //        markerInfo.pMarkerName = pMarkerName;
    //        pfnCmdDebugMarkerBegin(cmdbuffer, &markerInfo);
    //    }
    //}

    //void insert(VkCommandBuffer cmdbuffer, std::string markerName, glm::vec4 color)
    //{
    //    // Check for valid function pointer (may not be present if not running in a debugging application)
    //    if (pfnCmdDebugMarkerInsert)
    //    {
    //        VkDebugMarkerMarkerInfoEXT markerInfo = {};
    //        markerInfo.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_MARKER_INFO_EXT;
    //        memcpy(markerInfo.color, &color[0], sizeof(float) * 4);
    //        markerInfo.pMarkerName = markerName.c_str();
    //        pfnCmdDebugMarkerInsert(cmdbuffer, &markerInfo);
    //    }
    //}

    //void endRegion(VkCommandBuffer cmdBuffer)
    //{
    //    // Check for valid function (may not be present if not runnin in a debugging application)
    //    if (pfnCmdDebugMarkerEnd)
    //    {
    //        pfnCmdDebugMarkerEnd(cmdBuffer);
    //    }
    //}

    void SetCommandPoolName(VkDevice device, VkCommandPool cmdPool, const char * name)
    {
        SetObjectName(device, (uint64_t)cmdPool, VK_DEBUG_REPORT_OBJECT_TYPE_COMMAND_POOL_EXT, name);
    }

    void SetCommandBufferName(VkDevice device, VkCommandBuffer cmdBuffer, const char * name)
    {
        SetObjectName(device, (uint64_t)cmdBuffer, VK_DEBUG_REPORT_OBJECT_TYPE_COMMAND_BUFFER_EXT, name);
    }

    void SetQueueName(VkDevice device, VkQueue queue, const char * name)
    {
        SetObjectName(device, (uint64_t)queue, VK_DEBUG_REPORT_OBJECT_TYPE_QUEUE_EXT, name);
    }

    void SetImageName(VkDevice device, VkImage image, const char * name)
    {
        SetObjectName(device, (uint64_t)image, VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT, name);
    }

    void SetImageViewName(VkDevice device, VkImageView imageView, const char * name)
    {
        SetObjectName(device, (uint64_t)imageView, VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT, name);
    }

    void SetSamplerName(VkDevice device, VkSampler sampler, const char * name)
    {
        SetObjectName(device, (uint64_t)sampler, VK_DEBUG_REPORT_OBJECT_TYPE_SAMPLER_EXT, name);
    }

    void SetBufferName(VkDevice device, VkBuffer buffer, const char * name)
    {
        SetObjectName(device, (uint64_t)buffer, VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT, name);
    }

    void SetBufferViewName(VkDevice device, VkBufferView bufferView, const char * name)
    {
        SetObjectName(device, (uint64_t)bufferView, VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_VIEW_EXT, name);
    }

    void SetDeviceMemoryName(VkDevice device, VkDeviceMemory memory, const char * name)
    {
        SetObjectName(device, (uint64_t)memory, VK_DEBUG_REPORT_OBJECT_TYPE_DEVICE_MEMORY_EXT, name);
    }

    void SetShaderModuleName(VkDevice device, VkShaderModule shaderModule, const char * name)
    {
        SetObjectName(device, (uint64_t)shaderModule, VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT, name);
    }

    void SetPipelineName(VkDevice device, VkPipeline pipeline, const char * name)
    {
        SetObjectName(device, (uint64_t)pipeline, VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_EXT, name);
    }

    void SetPipelineLayoutName(VkDevice device, VkPipelineLayout pipelineLayout, const char * name)
    {
        SetObjectName(device, (uint64_t)pipelineLayout, VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_LAYOUT_EXT, name);
    }

    void SetRenderPassName(VkDevice device, VkRenderPass renderPass, const char * name)
    {
        SetObjectName(device, (uint64_t)renderPass, VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT, name);
    }

    void SetFramebufferName(VkDevice device, VkFramebuffer framebuffer, const char * name)
    {
        SetObjectName(device, (uint64_t)framebuffer, VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT, name);
    }

    void SetDescriptorSetLayoutName(VkDevice device, VkDescriptorSetLayout descriptorSetLayout, const char * name)
    {
        SetObjectName(device, (uint64_t)descriptorSetLayout, VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT_EXT, name);
    }

    void SetDescriptorSetName(VkDevice device, VkDescriptorSet descriptorSet, const char * name)
    {
        SetObjectName(device, (uint64_t)descriptorSet, VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_SET_EXT, name);
    }

    void SetSemaphoreName(VkDevice device, VkSemaphore semaphore, const char * name)
    {
        SetObjectName(device, (uint64_t)semaphore, VK_DEBUG_REPORT_OBJECT_TYPE_SEMAPHORE_EXT, name);
    }

    void SetFenceName(VkDevice device, VkFence fence, const char * name)
    {
        SetObjectName(device, (uint64_t)fence, VK_DEBUG_REPORT_OBJECT_TYPE_FENCE_EXT, name);
    }

    void SetEventName(VkDevice device, VkEvent _event, const char * name)
    {
        SetObjectName(device, (uint64_t)_event, VK_DEBUG_REPORT_OBJECT_TYPE_EVENT_EXT, name);
    }

    const char* VkResultToString(VkResult errorCode)
    {
        switch (errorCode)
        {
#define STR(r) case VK_ ##r: return #r
            STR(NOT_READY);
            STR(TIMEOUT);
            STR(EVENT_SET);
            STR(EVENT_RESET);
            STR(INCOMPLETE);
            STR(ERROR_OUT_OF_HOST_MEMORY);
            STR(ERROR_OUT_OF_DEVICE_MEMORY);
            STR(ERROR_INITIALIZATION_FAILED);
            STR(ERROR_DEVICE_LOST);
            STR(ERROR_MEMORY_MAP_FAILED);
            STR(ERROR_LAYER_NOT_PRESENT);
            STR(ERROR_EXTENSION_NOT_PRESENT);
            STR(ERROR_FEATURE_NOT_PRESENT);
            STR(ERROR_INCOMPATIBLE_DRIVER);
            STR(ERROR_TOO_MANY_OBJECTS);
            STR(ERROR_FORMAT_NOT_SUPPORTED);
            STR(ERROR_SURFACE_LOST_KHR);
            STR(ERROR_NATIVE_WINDOW_IN_USE_KHR);
            STR(SUBOPTIMAL_KHR);
            STR(ERROR_OUT_OF_DATE_KHR);
            STR(ERROR_INCOMPATIBLE_DISPLAY_KHR);
            STR(ERROR_VALIDATION_FAILED_EXT);
            STR(ERROR_INVALID_SHADER_NV);
#undef STR
        default:
            return "UNKNOWN_ERROR";
        }
    }
}
