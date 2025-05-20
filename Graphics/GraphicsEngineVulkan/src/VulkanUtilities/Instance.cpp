/*
 *  Copyright 2019-2025 Diligent Graphics LLC
 *  Copyright 2015-2019 Egor Yusov
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
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

#include "VulkanUtilities/Instance.hpp"
#include "APIInfo.h"

#include <vector>
#include <cstring>
#include <algorithm>
#include <array>

#if DILIGENT_USE_VOLK
#    include "volk.h"
#endif

#include "VulkanErrors.hpp"
#include "VulkanUtilities/Debug.hpp"

#if !DILIGENT_NO_GLSLANG
#    include "GLSLangUtils.hpp"
#endif

#if DILIGENT_USE_OPENXR
#    define XR_USE_GRAPHICS_API_VULKAN
#    include <openxr/openxr_platform.h>
#endif

#include "PlatformDefinitions.h"

namespace VulkanUtilities
{

std::string PrintExtensionsList(const std::vector<VkExtensionProperties>& Extensions, size_t NumColumns)
{
    VERIFY_EXPR(NumColumns > 0);

    std::vector<std::string> ExtStrings;
    ExtStrings.reserve(Extensions.size());
    for (const VkExtensionProperties& Ext : Extensions)
    {
        std::stringstream ss;
        ss << Ext.extensionName << ' '
           << VK_API_VERSION_MAJOR(Ext.specVersion) << '.'
           << VK_API_VERSION_MINOR(Ext.specVersion) << '.'
           << VK_API_VERSION_PATCH(Ext.specVersion);
        ExtStrings.emplace_back(ss.str());
    }

    std::vector<size_t> ColWidth(NumColumns);
    for (size_t i = 0; i < Extensions.size();)
    {
        for (size_t col = 0; col < NumColumns && i < Extensions.size(); ++col, ++i)
        {
            ColWidth[col] = std::max(ColWidth[col], ExtStrings[i].length());
        }
    }

    std::stringstream ss;
    for (size_t i = 0; i < Extensions.size();)
    {
        for (size_t col = 0; col < NumColumns && i < Extensions.size(); ++col, ++i)
        {
            ss << (col == 0 ? "\n    " : "    ");
            if (col + 1 < NumColumns && i + 1 < Extensions.size())
                ss << std::setw(static_cast<int>(ColWidth[col])) << std::left;
            ss << ExtStrings[i];
        }
    }

    return ss.str();
}

bool Instance::IsLayerAvailable(const char* LayerName, uint32_t& Version) const
{
    for (const VkLayerProperties& Layer : m_Layers)
    {
        if (strcmp(Layer.layerName, LayerName) == 0)
        {
            Version = Layer.specVersion;
            return true;
        }
    }
    return false;
}

namespace
{

// clang-format off
static constexpr std::array<const char*, 1> ValidationLayerNames =
{
    // Unified validation layer used on Desktop and Mobile platforms
    "VK_LAYER_KHRONOS_validation"
};
// clang-format on


bool EnumerateInstanceExtensions(const char* LayerName, std::vector<VkExtensionProperties>& Extensions)
{
    uint32_t ExtensionCount = 0;

    if (vkEnumerateInstanceExtensionProperties(LayerName, &ExtensionCount, nullptr) != VK_SUCCESS)
        return false;

    Extensions.resize(ExtensionCount);
    if (vkEnumerateInstanceExtensionProperties(LayerName, &ExtensionCount, Extensions.data()) != VK_SUCCESS)
    {
        Extensions.clear();
        return false;
    }
    VERIFY(ExtensionCount == Extensions.size(),
           "The number of extensions written by vkEnumerateInstanceExtensionProperties is not consistent "
           "with the count returned in the first call. This is a Vulkan loader bug.");

    return true;
}

bool IsExtensionAvailable(const std::vector<VkExtensionProperties>& Extensions, const char* ExtensionName)
{
    VERIFY_EXPR(ExtensionName != nullptr);

    for (const VkExtensionProperties& Extension : Extensions)
    {
        if (strcmp(Extension.extensionName, ExtensionName) == 0)
            return true;
    }

    return false;
}

} // namespace

bool Instance::IsExtensionAvailable(const char* ExtensionName) const
{
    return VulkanUtilities::IsExtensionAvailable(m_Extensions, ExtensionName);
}

bool Instance::IsExtensionEnabled(const char* ExtensionName) const
{
    for (const char* Extension : m_EnabledExtensions)
    {
        if (strcmp(Extension, ExtensionName) == 0)
            return true;
    }

    return false;
}

#if DILIGENT_USE_OPENXR
static uint32_t GetRequiredOpenXRVulkanVersion(uint32_t                  VulkanVersion,
                                               XrInstance                xrInstance,
                                               XrSystemId                xrSystemId,
                                               PFN_xrGetInstanceProcAddr xrGetInstanceProcAddr) noexcept(false)
{
    PFN_xrGetVulkanGraphicsRequirements2KHR xrGetVulkanGraphicsRequirements2KHR = nullptr;
    if (XR_FAILED(xrGetInstanceProcAddr(xrInstance, "xrGetVulkanGraphicsRequirements2KHR", reinterpret_cast<PFN_xrVoidFunction*>(&xrGetVulkanGraphicsRequirements2KHR))))
    {
        LOG_ERROR_AND_THROW("Failed to get xrGetVulkanGraphicsRequirements2KHR function. Make sure that XR_KHR_vulkan_enable2 extension is enabled.");
    }

    VERIFY_EXPR(xrGetVulkanGraphicsRequirements2KHR != nullptr);

    XrGraphicsRequirementsVulkan2KHR xrVulkan2Req{XR_TYPE_GRAPHICS_REQUIREMENTS_VULKAN2_KHR};
    if (XR_SUCCEEDED(xrGetVulkanGraphicsRequirements2KHR(xrInstance, xrSystemId, &xrVulkan2Req)))
    {
        auto XrVersionToVkVersion = [](XrVersion XrVersion) {
            return VK_MAKE_API_VERSION(0, XR_VERSION_MAJOR(XrVersion), XR_VERSION_MINOR(XrVersion), 0);
        };

        uint32_t MinVkVersion = XrVersionToVkVersion(xrVulkan2Req.minApiVersionSupported);
        if (VulkanVersion < MinVkVersion)
        {
            LOG_ERROR_AND_THROW("OpenXR requires Vulkan version ", VK_API_VERSION_MAJOR(MinVkVersion), '.', VK_API_VERSION_MINOR(MinVkVersion),
                                ", but this device only supports Vulkan ", VK_API_VERSION_MAJOR(VulkanVersion), '.', VK_API_VERSION_MINOR(VulkanVersion));
        }
        uint32_t MaxVkVersion = VK_MAKE_API_VERSION(0, XR_VERSION_MAJOR(xrVulkan2Req.maxApiVersionSupported), XR_VERSION_MINOR(xrVulkan2Req.maxApiVersionSupported), 0);
        if (MaxVkVersion < VulkanVersion)
        {
            LOG_INFO_MESSAGE("This device supports Vulkan ", VK_API_VERSION_MAJOR(VulkanVersion), '.', VK_API_VERSION_MINOR(VulkanVersion),
                             ", but OpenXR was only tested with Vulkan up to ", VK_API_VERSION_MAJOR(MaxVkVersion), '.', VK_API_VERSION_MINOR(MaxVkVersion),
                             ". Proceeding with Vulkan ", VK_API_VERSION_MAJOR(MaxVkVersion), '.', VK_API_VERSION_MINOR(MaxVkVersion), '.');
            VulkanVersion = MaxVkVersion;
        }
    }
    else
    {
        LOG_WARNING_MESSAGE("Failed to get Vulkan requirements from OpenXR. Proceeding without checking Vulkan instance version requirements.");
    }

    return VulkanVersion;
}

static VkResult CreateVkInstanceForOpenXR(XrInstance                   xrInstance,
                                          XrSystemId                   xrSystemId,
                                          PFN_xrGetInstanceProcAddr    xrGetInstanceProcAddr,
                                          const VkInstanceCreateInfo*  pCreateInfo,
                                          const VkAllocationCallbacks* vkAllocator,
                                          VkInstance*                  pInstance)
{
    PFN_xrCreateVulkanInstanceKHR xrCreateVulkanInstanceKHR = nullptr;
    if (XR_FAILED(xrGetInstanceProcAddr(xrInstance, "xrCreateVulkanInstanceKHR", reinterpret_cast<PFN_xrVoidFunction*>(&xrCreateVulkanInstanceKHR))))
    {
        LOG_ERROR_AND_THROW("Failed to get xrCreateVulkanInstanceKHR function");
    }
    VERIFY_EXPR(xrCreateVulkanInstanceKHR != nullptr);

    XrVulkanInstanceCreateInfoKHR xrVkInstanceCreateInfo{XR_TYPE_VULKAN_INSTANCE_CREATE_INFO_KHR};
    xrVkInstanceCreateInfo.systemId               = xrSystemId;
    xrVkInstanceCreateInfo.createFlags            = 0;
    xrVkInstanceCreateInfo.pfnGetInstanceProcAddr = vkGetInstanceProcAddr;
    xrVkInstanceCreateInfo.vulkanCreateInfo       = pCreateInfo;
    xrVkInstanceCreateInfo.vulkanAllocator        = vkAllocator;

    VkResult vkRes = VK_ERROR_UNKNOWN;
    if (XR_FAILED(xrCreateVulkanInstanceKHR(xrInstance, &xrVkInstanceCreateInfo, pInstance, &vkRes)))
    {
        LOG_ERROR_AND_THROW("Failed to create Vulkan instance using OpenXR");
    }

    return vkRes;
}
#endif

std::shared_ptr<Instance> Instance::Create(const CreateInfo& CI)
{
    return std::shared_ptr<Instance>{new Instance{CI}};
}

#if DILIGENT_USE_VOLK
static bool LoadVulkan()
{
    static bool VulkanLoaded = false;
    if (VulkanLoaded)
        return true;

    VulkanLoaded = (volkInitialize() == VK_SUCCESS);
    return VulkanLoaded;
}
#endif

uint32_t Instance::GetApiVersion()
{
#if DILIGENT_USE_VOLK
    if (!LoadVulkan())
    {
        return 0;
    }
#endif

    uint32_t ApiVersion = VK_API_VERSION_1_0;
#if DILIGENT_USE_VOLK
    if (vkEnumerateInstanceVersion != nullptr)
    {
        vkEnumerateInstanceVersion(&ApiVersion);
    }
#endif

    VkApplicationInfo AppInfo{};
    AppInfo.sType      = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    AppInfo.apiVersion = ApiVersion;

    VkInstanceCreateInfo InstCI{};
    InstCI.sType            = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    InstCI.pApplicationInfo = &AppInfo;

    VkInstance vkInstance = VK_NULL_HANDLE;
    VkResult   Result     = vkCreateInstance(&InstCI, nullptr, &vkInstance);
    if (Result != VK_SUCCESS)
    {
        return 0;
    }

    uint32_t DeviceCount = 0;
    if (PFN_vkEnumeratePhysicalDevices EnumeratePhysicalDevices = reinterpret_cast<PFN_vkEnumeratePhysicalDevices>(vkGetInstanceProcAddr(vkInstance, "vkEnumeratePhysicalDevices")))
    {
        Result = EnumeratePhysicalDevices(vkInstance, &DeviceCount, nullptr);
    }
    else
    {
        UNEXPECTED("Unable to load vkEnumeratePhysicalDevices function.");
    }

    if (PFN_vkDestroyInstance DestroyInstance = reinterpret_cast<PFN_vkDestroyInstance>(vkGetInstanceProcAddr(vkInstance, "vkDestroyInstance")))
    {
        DestroyInstance(vkInstance, nullptr);
    }
    else
    {
        UNEXPECTED("Unable to load vkDestroyInstance function.");
    }

    return DeviceCount > 0 ? ApiVersion : 0;
}

Instance::Instance(const CreateInfo& CI) :
    m_pvkAllocator{CI.pVkAllocator}
{
#if DILIGENT_USE_VOLK
    if (!LoadVulkan())
    {
        LOG_ERROR_AND_THROW("Failed to load Vulkan.");
    }
#endif

    {
        // Enumerate available layers
        uint32_t LayerCount = 0;

        VkResult res = vkEnumerateInstanceLayerProperties(&LayerCount, nullptr);
        CHECK_VK_ERROR_AND_THROW(res, "Failed to query layer count");
        m_Layers.resize(LayerCount);
        vkEnumerateInstanceLayerProperties(&LayerCount, m_Layers.data());
        CHECK_VK_ERROR_AND_THROW(res, "Failed to enumerate extensions");
        VERIFY_EXPR(LayerCount == m_Layers.size());
    }

    if (CI.LogExtensions)
    {
        if (!m_Layers.empty())
        {
            std::stringstream ss;
            for (const VkLayerProperties& Layer : m_Layers)
            {
                ss << "\n    "
                   << Layer.layerName << ' '
                   << VK_API_VERSION_MAJOR(Layer.specVersion) << '.'
                   << VK_API_VERSION_MINOR(Layer.specVersion) << '.'
                   << VK_API_VERSION_PATCH(Layer.specVersion);
            }
            LOG_INFO_MESSAGE("Available Vulkan instance layers: ", ss.str());
        }
        else
        {
            LOG_INFO_MESSAGE("No Vulkan instance layers found");
        }
    }

    // Enumerate available instance extensions
    if (!EnumerateInstanceExtensions(nullptr, m_Extensions))
        LOG_ERROR_AND_THROW("Failed to enumerate instance extensions");

    if (CI.LogExtensions)
    {
        if (!m_Extensions.empty())
            LOG_INFO_MESSAGE("Supported Vulkan instance extensions: ", PrintExtensionsList(m_Extensions, 1));
        else
            LOG_INFO_MESSAGE("No Vulkan instance extensions found");
    }

    std::vector<const char*> InstanceExtensions;
    if (IsExtensionAvailable(VK_KHR_SURFACE_EXTENSION_NAME))
    {
        InstanceExtensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);

        // Enable surface extensions depending on OS
#if defined(VK_USE_PLATFORM_WIN32_KHR)
        InstanceExtensions.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
#endif
#if defined(VK_USE_PLATFORM_ANDROID_KHR)
        InstanceExtensions.push_back(VK_KHR_ANDROID_SURFACE_EXTENSION_NAME);
#endif
#if defined(VK_USE_PLATFORM_WAYLAND_KHR)
        InstanceExtensions.push_back(VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME);
#endif
#if defined(VK_USE_PLATFORM_XLIB_KHR)
        InstanceExtensions.push_back(VK_KHR_XLIB_SURFACE_EXTENSION_NAME);
#endif
#if defined(VK_USE_PLATFORM_XCB_KHR)
        InstanceExtensions.push_back(VK_KHR_XCB_SURFACE_EXTENSION_NAME);
#endif
#if defined(VK_USE_PLATFORM_METAL_EXT)
        InstanceExtensions.push_back(VK_EXT_METAL_SURFACE_EXTENSION_NAME);
#endif
    };

#if PLATFORM_MACOS
    // Beginning with the 1.3.216 Vulkan SDK, the Vulkan Loader is strictly
    // enforcing the new VK_KHR_PORTABILITY_subset extension.
    constexpr bool UsePortabilityEnumeartion = true;
#else
    constexpr bool UsePortabilityEnumeartion = false;
#endif

    if (UsePortabilityEnumeartion)
        InstanceExtensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);

    // This extension added to core in 1.1, but current version is 1.0
    if (IsExtensionAvailable(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME))
    {
        InstanceExtensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
    }

    for (const char* ExtName : InstanceExtensions)
    {
        if (!IsExtensionAvailable(ExtName))
            LOG_ERROR_AND_THROW("Required extension ", ExtName, " is not available");
    }

    if (CI.ppExtensionNames != nullptr)
    {
        for (uint32_t ext = 0; ext < CI.ExtensionCount; ++ext)
        {
            const char* ExtName = CI.ppExtensionNames[ext];
            if (ExtName == nullptr)
                continue;
            if (IsExtensionAvailable(ExtName))
                InstanceExtensions.push_back(ExtName);
            else
                LOG_WARNING_MESSAGE("Extension ", ExtName, " is not available");
        }
    }
    else
    {
        DEV_CHECK_ERR(CI.ExtensionCount == 0,
                      "Global extensions pointer is null while extensions count is ", CI.ExtensionCount,
                      ". Please initialize 'ppInstanceExtensionNames' member of EngineVkCreateInfo struct.");
    }

    uint32_t ApiVersion = CI.ApiVersion;
#if DILIGENT_USE_VOLK
    if (vkEnumerateInstanceVersion != nullptr && ApiVersion > VK_API_VERSION_1_0)
    {
        uint32_t MaxApiVersion = 0;
        vkEnumerateInstanceVersion(&MaxApiVersion);
        ApiVersion = std::min(ApiVersion, MaxApiVersion);
    }
    else
    {
        // Only Vulkan 1.0 is supported.
        ApiVersion = VK_API_VERSION_1_0;
    }
#else
    // Without Volk we can only use Vulkan 1.0
    ApiVersion = VK_API_VERSION_1_0;
#endif

#if DILIGENT_USE_OPENXR
    XrInstance                xrInstance            = XR_NULL_HANDLE;
    XrSystemId                xrSystemId            = XR_NULL_SYSTEM_ID;
    PFN_xrGetInstanceProcAddr xrGetInstanceProcAddr = nullptr;
    if (CI.XR.Instance != 0)
    {
        if (CI.XR.GetInstanceProcAddr == nullptr)
            LOG_ERROR_AND_THROW("xrGetInstanceProcAddr must not be null");

        static_assert(sizeof(XrInstance) == sizeof(CI.XR.Instance), "XrInstance size mismatch");
        memcpy(&xrInstance, &CI.XR.Instance, sizeof(XrInstance));
        static_assert(sizeof(XrSystemId) == sizeof(CI.XR.SystemId), "XrSystemId size mismatch");
        memcpy(&xrSystemId, &CI.XR.SystemId, sizeof(XrSystemId));
        xrGetInstanceProcAddr = reinterpret_cast<PFN_xrGetInstanceProcAddr>(CI.XR.GetInstanceProcAddr);
    }
    if (xrInstance != 0)
    {
        ApiVersion = GetRequiredOpenXRVulkanVersion(ApiVersion, xrInstance, xrSystemId, xrGetInstanceProcAddr);
    }
#endif

    std::vector<const char*> InstanceLayers;

    // Use VK_DEVSIM_FILENAME environment variable to define the simulation layer
    if (CI.EnableDeviceSimulation)
    {
        static const char* DeviceSimulationLayer = "VK_LAYER_LUNARG_device_simulation";

        uint32_t LayerVer = 0;
        if (IsLayerAvailable(DeviceSimulationLayer, LayerVer))
        {
            InstanceLayers.push_back(DeviceSimulationLayer);
        }
    }

    if (CI.EnableValidation)
    {
        if (IsExtensionAvailable(VK_EXT_DEBUG_UTILS_EXTENSION_NAME))
        {
            // Prefer VK_EXT_debug_utils
            m_DebugMode = DebugMode::Utils;
        }
        else if (IsExtensionAvailable(VK_EXT_DEBUG_REPORT_EXTENSION_NAME))
        {
            // If debug utils are unavailable (e.g. on Android), use VK_EXT_debug_report
            m_DebugMode = DebugMode::Report;
        }

        for (const char* LayerName : ValidationLayerNames)
        {
            uint32_t LayerVer = ~0u;
            if (!IsLayerAvailable(LayerName, LayerVer))
            {
                LOG_WARNING_MESSAGE("Validation layer ", LayerName, " is not available.");
                continue;
            }

            // Beta extensions may vary and result in a crash.
            // New enums are not supported and may cause validation error.
            if (LayerVer < VK_HEADER_VERSION_COMPLETE)
            {
                LOG_WARNING_MESSAGE("Layer '", LayerName, "' version (", VK_API_VERSION_MAJOR(LayerVer), '.', VK_API_VERSION_MINOR(LayerVer), '.', VK_API_VERSION_PATCH(LayerVer),
                                    ") is less than the header version (",
                                    VK_API_VERSION_MAJOR(VK_HEADER_VERSION_COMPLETE), '.', VK_API_VERSION_MINOR(VK_HEADER_VERSION_COMPLETE), '.', VK_API_VERSION_PATCH(VK_HEADER_VERSION_COMPLETE),
                                    ").");
            }

            InstanceLayers.push_back(LayerName);

            if (m_DebugMode != DebugMode::Utils)
            {
                // On Android, VK_EXT_debug_utils extension may not be supported by the loader,
                // but supported by the layer.
                std::vector<VkExtensionProperties> LayerExtensions;
                if (EnumerateInstanceExtensions(LayerName, LayerExtensions))
                {
                    if (VulkanUtilities::IsExtensionAvailable(LayerExtensions, VK_EXT_DEBUG_UTILS_EXTENSION_NAME))
                        m_DebugMode = DebugMode::Utils;

                    if (m_DebugMode == DebugMode::Disabled && VulkanUtilities::IsExtensionAvailable(LayerExtensions, VK_EXT_DEBUG_REPORT_EXTENSION_NAME))
                        m_DebugMode = DebugMode::Report; // Resort to debug report
                }
                else
                {
                    LOG_ERROR_MESSAGE("Failed to enumerate extensions for ", LayerName, " layer");
                }
            }
        }


        if (m_DebugMode == DebugMode::Utils)
            InstanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        else if (m_DebugMode == DebugMode::Report)
            InstanceExtensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
        else
            LOG_WARNING_MESSAGE("Neither ", VK_EXT_DEBUG_UTILS_EXTENSION_NAME, " nor ", VK_EXT_DEBUG_REPORT_EXTENSION_NAME, " extension is available. Debug tools (validation layer message logging, performance markers, etc.) will be disabled.");
    }

    if (CI.ppEnabledLayerNames != nullptr)
    {
        for (size_t i = 0; i < CI.EnabledLayerCount; ++i)
        {
            const char* LayerName = CI.ppEnabledLayerNames[i];
            if (LayerName == nullptr)
                return;
            uint32_t LayerVer = 0;
            if (IsLayerAvailable(LayerName, LayerVer))
                InstanceLayers.push_back(LayerName);
            else
                LOG_WARNING_MESSAGE("Instance layer ", LayerName, " is not available");
        }
    }

    VkApplicationInfo appInfo{};
    appInfo.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pNext              = nullptr; // Pointer to an extension-specific structure.
    appInfo.pApplicationName   = nullptr;
    appInfo.applicationVersion = 0; // Developer-supplied version number of the application
    appInfo.pEngineName        = "Diligent Engine";
    appInfo.engineVersion      = DILIGENT_API_VERSION; // Developer-supplied version number of the engine used to create the application.
    appInfo.apiVersion         = ApiVersion;

    VkInstanceCreateInfo InstanceCreateInfo{};
    InstanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    InstanceCreateInfo.pNext = nullptr; // Pointer to an extension-specific structure.
    InstanceCreateInfo.flags = 0;
    if (UsePortabilityEnumeartion)
    {
        // The instance will enumerate available Vulkan Portability-compliant physical
        // devices and groups in addition to the Vulkan physical devices and groups that
        // are enumerated by default.
        InstanceCreateInfo.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
    }
    InstanceCreateInfo.pApplicationInfo        = &appInfo;
    InstanceCreateInfo.enabledExtensionCount   = static_cast<uint32_t>(InstanceExtensions.size());
    InstanceCreateInfo.ppEnabledExtensionNames = InstanceExtensions.empty() ? nullptr : InstanceExtensions.data();
    InstanceCreateInfo.enabledLayerCount       = static_cast<uint32_t>(InstanceLayers.size());
    InstanceCreateInfo.ppEnabledLayerNames     = InstanceLayers.empty() ? nullptr : InstanceLayers.data();

    VkResult res = VK_ERROR_UNKNOWN;
#if DILIGENT_USE_OPENXR
    if (xrInstance != XR_NULL_HANDLE)
    {
        res = CreateVkInstanceForOpenXR(xrInstance, xrSystemId, xrGetInstanceProcAddr, &InstanceCreateInfo, m_pvkAllocator, &m_vkInstance);
    }
    else
#endif
    {
        res = vkCreateInstance(&InstanceCreateInfo, m_pvkAllocator, &m_vkInstance);
    }
    CHECK_VK_ERROR_AND_THROW(res, "Failed to create Vulkan instance");

#if DILIGENT_USE_VOLK
    volkLoadInstance(m_vkInstance);
#endif

    m_EnabledExtensions = std::move(InstanceExtensions);
    m_vkVersion         = ApiVersion;

    // If requested, we enable the default validation layers for debugging
    if (m_DebugMode == DebugMode::Utils)
    {
        constexpr VkDebugUtilsMessageSeverityFlagsEXT messageSeverity =
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        constexpr VkDebugUtilsMessageTypeFlagsEXT messageType =
            VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        if (!VulkanUtilities::SetupDebugUtils(m_vkInstance, messageSeverity, messageType, CI.IgnoreDebugMessageCount, CI.ppIgnoreDebugMessageNames, nullptr))
            LOG_ERROR_MESSAGE("Failed to initialize debug utils. Validation layer message logging, performance markers, etc. will be disabled.");
    }
    else if (m_DebugMode == DebugMode::Report)
    {
        constexpr VkDebugReportFlagBitsEXT flags = static_cast<VkDebugReportFlagBitsEXT>(
            VK_DEBUG_REPORT_WARNING_BIT_EXT |
            VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT |
            VK_DEBUG_REPORT_ERROR_BIT_EXT);
        if (!VulkanUtilities::SetupDebugReport(m_vkInstance, flags, nullptr))
            LOG_ERROR_MESSAGE("Failed to initialize debug report. Validation layer message logging will be disabled.");
    }

    // Enumerate physical devices
    {
        // Physical device
        uint32_t PhysicalDeviceCount = 0;
        // Get the number of available physical devices
        VkResult err = vkEnumeratePhysicalDevices(m_vkInstance, &PhysicalDeviceCount, nullptr);
        CHECK_VK_ERROR(err, "Failed to get physical device count");
        if (PhysicalDeviceCount == 0)
            LOG_ERROR_AND_THROW("No physical devices found on the system");

        // Enumerate devices
        m_PhysicalDevices.resize(PhysicalDeviceCount);
        err = vkEnumeratePhysicalDevices(m_vkInstance, &PhysicalDeviceCount, m_PhysicalDevices.data());
        CHECK_VK_ERROR(err, "Failed to enumerate physical devices");
        VERIFY_EXPR(m_PhysicalDevices.size() == PhysicalDeviceCount);
    }
#if !DILIGENT_NO_GLSLANG
    Diligent::GLSLangUtils::InitializeGlslang();
#endif
}

Instance::~Instance()
{
    if (m_DebugMode != DebugMode::Disabled)
    {
        VulkanUtilities::FreeDebug(m_vkInstance);
    }
    vkDestroyInstance(m_vkInstance, m_pvkAllocator);

#if !DILIGENT_NO_GLSLANG
    Diligent::GLSLangUtils::FinalizeGlslang();
#endif
}

VkPhysicalDevice Instance::SelectPhysicalDevice(uint32_t AdapterId) const noexcept(false)
{
    const auto IsGraphicsAndComputeQueueSupported = [](VkPhysicalDevice Device) {
        uint32_t QueueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(Device, &QueueFamilyCount, nullptr);
        VERIFY_EXPR(QueueFamilyCount > 0);
        std::vector<VkQueueFamilyProperties> QueueFamilyProperties(QueueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(Device, &QueueFamilyCount, QueueFamilyProperties.data());
        VERIFY_EXPR(QueueFamilyCount == QueueFamilyProperties.size());

        // If an implementation exposes any queue family that supports graphics operations,
        // at least one queue family of at least one physical device exposed by the implementation
        // must support both graphics and compute operations.
        for (const VkQueueFamilyProperties& QueueFamilyProps : QueueFamilyProperties)
        {
            if ((QueueFamilyProps.queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0 &&
                (QueueFamilyProps.queueFlags & VK_QUEUE_COMPUTE_BIT) != 0)
            {
                return true;
            }
        }
        return false;
    };

    VkPhysicalDevice SelectedPhysicalDevice = VK_NULL_HANDLE;

    if (AdapterId < m_PhysicalDevices.size() &&
        IsGraphicsAndComputeQueueSupported(m_PhysicalDevices[AdapterId]))
    {
        SelectedPhysicalDevice = m_PhysicalDevices[AdapterId];
    }

    // Select a device that exposes a queue family that supports both compute and graphics operations.
    // Prefer discrete GPU.
    if (SelectedPhysicalDevice == VK_NULL_HANDLE)
    {
        for (VkPhysicalDevice Device : m_PhysicalDevices)
        {
            VkPhysicalDeviceProperties DeviceProps;
            vkGetPhysicalDeviceProperties(Device, &DeviceProps);

            if (IsGraphicsAndComputeQueueSupported(Device))
            {
                SelectedPhysicalDevice = Device;
                if (DeviceProps.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
                    break;
            }
        }
    }

    if (SelectedPhysicalDevice != VK_NULL_HANDLE)
    {
        VkPhysicalDeviceProperties SelectedDeviceProps;
        vkGetPhysicalDeviceProperties(SelectedPhysicalDevice, &SelectedDeviceProps);
        LOG_INFO_MESSAGE("Using physical device '", SelectedDeviceProps.deviceName,
                         "', API version ",
                         VK_API_VERSION_MAJOR(SelectedDeviceProps.apiVersion), '.',
                         VK_API_VERSION_MINOR(SelectedDeviceProps.apiVersion), '.',
                         VK_API_VERSION_PATCH(SelectedDeviceProps.apiVersion),
                         ", Driver version ",
                         VK_API_VERSION_MAJOR(SelectedDeviceProps.driverVersion), '.',
                         VK_API_VERSION_MINOR(SelectedDeviceProps.driverVersion), '.',
                         VK_API_VERSION_PATCH(SelectedDeviceProps.driverVersion), '.');
    }
    else
    {
        LOG_ERROR_AND_THROW("Failed to find suitable physical device");
    }

    return SelectedPhysicalDevice;
}

#ifdef _MSC_VER
#    pragma warning(push)
#    pragma warning(disable : 4702) // unreachable code
#endif

VkPhysicalDevice Instance::SelectPhysicalDeviceForOpenXR(const CreateInfo::OpenXRInfo& XRInfo) const noexcept(false)
{
#if DILIGENT_USE_OPENXR
    XrInstance xrInstance = XR_NULL_HANDLE;
    static_assert(sizeof(XrInstance) == sizeof(XRInfo.Instance), "XrInstance size mismatch");
    memcpy(&xrInstance, &XRInfo.Instance, sizeof(XrInstance));

    XrVulkanGraphicsDeviceGetInfoKHR vkGraphicsDeviceGetInfo{XR_TYPE_VULKAN_GRAPHICS_DEVICE_GET_INFO_KHR};
    static_assert(sizeof(vkGraphicsDeviceGetInfo.systemId) == sizeof(XRInfo.SystemId), "SystemId Size mismatch");
    memcpy(&vkGraphicsDeviceGetInfo.systemId, &XRInfo.SystemId, sizeof(vkGraphicsDeviceGetInfo.systemId));
    vkGraphicsDeviceGetInfo.vulkanInstance = m_vkInstance;

    PFN_xrGetInstanceProcAddr         xrGetInstanceProcAddr         = reinterpret_cast<PFN_xrGetInstanceProcAddr>(XRInfo.GetInstanceProcAddr);
    PFN_xrGetVulkanGraphicsDevice2KHR xrGetVulkanGraphicsDevice2KHR = nullptr;
    if (XR_FAILED(xrGetInstanceProcAddr(xrInstance, "xrGetVulkanGraphicsDevice2KHR", reinterpret_cast<PFN_xrVoidFunction*>(&xrGetVulkanGraphicsDevice2KHR))))
    {
        LOG_ERROR_AND_THROW("Failed to get xrGetVulkanGraphicsDevice2KHR function");
    }

    VkPhysicalDevice vkDevice = VK_NULL_HANDLE;
    if (XR_FAILED(xrGetVulkanGraphicsDevice2KHR(xrInstance, &vkGraphicsDeviceGetInfo, &vkDevice)))
    {
        LOG_ERROR_AND_THROW("Failed to get Vulkan physical device for OpenXR");
    }

    return vkDevice;
#else
    LOG_ERROR_AND_THROW("OpenXR is not supported. Use DILIGENT_USE_OPENXR CMake option to enable it.");
    return VK_NULL_HANDLE; // Triggers unreachable code warning on MSVC
#endif
}

#ifdef _MSC_VER
#    pragma warning(pop)
#endif

} // namespace VulkanUtilities
