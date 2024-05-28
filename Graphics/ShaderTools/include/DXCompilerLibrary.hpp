/*
 *  Copyright 2024 Diligent Graphics LLC
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

#include "DXCompiler.hpp"

#include <mutex>
#include <atomic>

#if PLATFORM_WIN32 || PLATFORM_UNIVERSAL_WINDOWS

#    include "WinHPreface.h"
#    include <Unknwn.h>
#    include "WinHPostface.h"

#elif PLATFORM_LINUX

#else
#    error Unsupported platform
#endif

#include "dxc/dxcapi.h"

namespace Diligent
{

class DXCompilerLibrary
{
public:
    DXCompilerLibrary(const char* LibName) noexcept :
        m_LibName{LibName != nullptr ? LibName : ""}
    {
    }

    ~DXCompilerLibrary()
    {
        Unload();
    }

    DxcCreateInstanceProc GetDxcCreateInstance()
    {
        if (!m_Loaded.load())
        {
            std::lock_guard<std::mutex> Lock{m_LibraryMtx};
            if (!m_Loaded.load())
            {
                Load();
                m_Loaded.store(true);
            }
        }

        return m_DxcCreateInstance;
    }

private:
    void Load();
    void Unload();

private:
    const std::string m_LibName;

    std::mutex            m_LibraryMtx;
    void*                 m_Library = nullptr;
    std::atomic<bool>     m_Loaded{false};
    DxcCreateInstanceProc m_DxcCreateInstance = nullptr;
};

} // namespace Diligent
