/*
 *  Copyright 2026 Diligent Graphics LLC
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

#pragma once

#include "ObjectBase.hpp"
#include "SuperResolution.h"

#include <vector>
#include <string>

namespace Diligent
{

class SuperResolutionBase : public ObjectBase<ISuperResolution>
{
public:
    using TBase = ObjectBase<ISuperResolution>;

    SuperResolutionBase(IReferenceCounters*        pRefCounters,
                        const SuperResolutionDesc& Desc) :
        TBase{pRefCounters},
        m_Desc{Desc}
    {
        if (Desc.Name != nullptr)
        {
            m_Name      = Desc.Name;
            m_Desc.Name = m_Name.c_str();
        }
    }

    IMPLEMENT_QUERY_INTERFACE_IN_PLACE(IID_SuperResolution, TBase)

    virtual const SuperResolutionDesc& DILIGENT_CALL_TYPE GetDesc() const override final
    {
        return m_Desc;
    }

    virtual void DILIGENT_CALL_TYPE GetJitterOffset(Uint32 Index, float* pJitterX, float* pJitterY) const override final
    {
        DEV_CHECK_ERR(pJitterX != nullptr && pJitterY != nullptr, "pJitterX and pJitterY must not be null");

        if (!m_JitterPattern.empty())
        {
            const Uint32 WrappedIndex = Index % static_cast<Uint32>(m_JitterPattern.size());
            *pJitterX                 = m_JitterPattern[WrappedIndex].X;
            *pJitterY                 = m_JitterPattern[WrappedIndex].Y;
        }
        else
        {
            *pJitterX = 0.0f;
            *pJitterY = 0.0f;
        }
    }

protected:
    struct JitterOffset
    {
        float X = 0.0f;
        float Y = 0.0f;
    };

    SuperResolutionDesc       m_Desc;
    std::string               m_Name;
    std::vector<JitterOffset> m_JitterPattern;
};

} // namespace Diligent
