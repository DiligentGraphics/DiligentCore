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
#include "SuperResolutionFactory.h"
#include "GraphicsAccessories.hpp"

#include <vector>
#include <string>
#include <array>

namespace Diligent
{

#define LOG_SUPER_RESOLUTION_ERROR_AND_THROW(Name, ...) LOG_ERROR_AND_THROW("Super resolution upscaler '", ((Name) != nullptr ? (Name) : ""), "': ", ##__VA_ARGS__)

#define DEV_CHECK_SUPER_RESOLUTION(Name, Expr, ...) DEV_CHECK_ERR(Expr, "Super resolution upscaler '", ((Name) != nullptr ? (Name) : ""), "': ", ##__VA_ARGS__)

#define VERIFY_SUPER_RESOLUTION(Name, Expr, ...)                     \
    do                                                               \
    {                                                                \
        if (!(Expr))                                                 \
        {                                                            \
            LOG_SUPER_RESOLUTION_ERROR_AND_THROW(Name, __VA_ARGS__); \
        }                                                            \
    } while (false)

/// Validates super resolution source settings attributes using DEV checks.
void ValidateSourceSettingsAttribs(const SuperResolutionSourceSettingsAttribs& Attribs);

/// Validates super resolution description and throws an exception in case of an error.
void ValidateSuperResolutionDesc(const SuperResolutionDesc& Desc, const SuperResolutionInfo& Info) noexcept(false);

/// Validates execute super resolution attributes using DEV checks.
void ValidateExecuteSuperResolutionAttribs(const SuperResolutionDesc&           Desc,
                                           const SuperResolutionInfo&           Info,
                                           const ExecuteSuperResolutionAttribs& Attribs);

class SuperResolutionBase : public ObjectBase<ISuperResolution>
{
public:
    using TBase = ObjectBase<ISuperResolution>;

    struct JitterOffset
    {
        float X = 0.0f;
        float Y = 0.0f;
    };

    SuperResolutionBase(IReferenceCounters*        pRefCounters,
                        const SuperResolutionDesc& Desc,
                        const SuperResolutionInfo& Info) :
        TBase{pRefCounters},
        m_Desc{Desc},
        m_Info{Info}
    {
        if (Desc.Name != nullptr)
        {
            m_Name      = Desc.Name;
            m_Desc.Name = m_Name.c_str();
        }
        ValidateSuperResolutionDesc(m_Desc, m_Info);
    }

    IMPLEMENT_QUERY_INTERFACE_IN_PLACE(IID_SuperResolution, TBase)

    virtual const SuperResolutionDesc& DILIGENT_CALL_TYPE GetDesc() const override final
    {
        return m_Desc;
    }

    virtual void DILIGENT_CALL_TYPE GetJitterOffset(Uint32 Index, float& JitterX, float& JitterY) const override final
    {
        if (!m_JitterPattern.empty())
        {
            const Uint32 WrappedIndex = Index % static_cast<Uint32>(m_JitterPattern.size());
            JitterX                   = m_JitterPattern[WrappedIndex].X;
            JitterY                   = m_JitterPattern[WrappedIndex].Y;
        }
        else
        {
            JitterX = 0.0f;
            JitterY = 0.0f;
        }
    }

protected:
    void TransitionResourceStates(const ExecuteSuperResolutionAttribs& Attribs, RESOURCE_STATE OutputTextureState = RESOURCE_STATE_UNORDERED_ACCESS)
    {
        if (Attribs.StateTransitionMode != RESOURCE_STATE_TRANSITION_MODE_TRANSITION)
            return;

        std::array<StateTransitionDesc, 7> Barriers;
        Uint32                             BarrierCount = 0;

        auto AddBarrier = [&](ITextureView* pView, RESOURCE_STATE NewState) {
            if (pView == nullptr)
                return;
            VERIFY_EXPR(BarrierCount < Barriers.size());
            Barriers[BarrierCount++] = StateTransitionDesc{pView->GetTexture(), RESOURCE_STATE_UNKNOWN, NewState, STATE_TRANSITION_FLAG_UPDATE_STATE};
        };

        AddBarrier(Attribs.pColorTextureSRV, RESOURCE_STATE_SHADER_RESOURCE);
        AddBarrier(Attribs.pDepthTextureSRV, RESOURCE_STATE_SHADER_RESOURCE);
        AddBarrier(Attribs.pMotionVectorsSRV, RESOURCE_STATE_SHADER_RESOURCE);
        AddBarrier(Attribs.pOutputTextureView, OutputTextureState);
        AddBarrier(Attribs.pExposureTextureSRV, RESOURCE_STATE_SHADER_RESOURCE);
        AddBarrier(Attribs.pReactiveMaskTextureSRV, RESOURCE_STATE_SHADER_RESOURCE);
        AddBarrier(Attribs.pIgnoreHistoryMaskTextureSRV, RESOURCE_STATE_SHADER_RESOURCE);

        Attribs.pContext->TransitionResourceStates(BarrierCount, Barriers.data());
    }

protected:
    SuperResolutionDesc       m_Desc;
    SuperResolutionInfo       m_Info;
    std::string               m_Name;
    std::vector<JitterOffset> m_JitterPattern;
};

/// Populates a Halton(2,3) jitter pattern centered at origin.
void PopulateHaltonJitterPattern(std::vector<SuperResolutionBase::JitterOffset>& JitterPattern, Uint32 PatternSize);

} // namespace Diligent
