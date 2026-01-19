# Returns platform-specific Vulkan definitions
function(get_vulkan_platform_definitions OutVarName)
    if(PLATFORM_WIN32)
        set(Defs
            VK_USE_PLATFORM_WIN32_KHR=1
        )
    elseif(PLATFORM_LINUX)
        set(Defs
            VK_USE_PLATFORM_XCB_KHR=1
            VK_USE_PLATFORM_XLIB_KHR=1
            VK_USE_PLATFORM_WAYLAND_KHR=1
        )
    elseif(PLATFORM_APPLE)
        set(Defs
            VK_USE_PLATFORM_METAL_EXT=1
        )
    elseif(PLATFORM_ANDROID)
        set(Defs
            VK_USE_PLATFORM_ANDROID_KHR=1
        )
    else()
        message(FATAL_ERROR "Unsupported platform")
    endif()

    set(${OutVarName} ${Defs} PARENT_SCOPE)
endfunction()
