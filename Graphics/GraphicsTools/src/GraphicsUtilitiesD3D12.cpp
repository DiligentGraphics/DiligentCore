/*
 *  Copyright 2019-2025 Diligent Graphics LLC
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

#include "GraphicsUtilities.h"

#include "WinHPreface.h"
#include <d3d12.h>
#include <atlbase.h>
#include "WinHPostface.h"

#include "../../GraphicsEngineD3DBase/include/DXGITypeConversions.hpp"

#include "RenderDeviceD3D12.h"
#include "RefCntAutoPtr.hpp"

namespace Diligent
{

int64_t GetNativeTextureFormatD3D12(TEXTURE_FORMAT TexFormat)
{
    return static_cast<int64_t>(TexFormatToDXGI_Format(TexFormat));
}

TEXTURE_FORMAT GetTextureFormatFromNativeD3D12(int64_t NativeFormat)
{
    return DXGI_FormatToTexFormat(static_cast<DXGI_FORMAT>(NativeFormat));
}

IDXCompiler* GetDeviceDXCompilerD3D12(IRenderDevice* pDevice)
{
    if (RefCntAutoPtr<IRenderDeviceD3D12> pDeviceD3D12{pDevice, IID_RenderDeviceD3D12})
    {
        return pDeviceD3D12->GetDXCompiler();
    }
    else
    {
        return nullptr;
    }
}

} // namespace Diligent
