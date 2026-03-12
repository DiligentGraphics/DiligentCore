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

#include "SuperResolution.h"
#include "SuperResolutionFactory.h"

int TestObjectCInterface(struct IObject* pObject);

int TestSuperResolutionCInterface(struct ISuperResolution* pUpscaler)
{
    IObject*                  pUnknown = NULL;
    ReferenceCounterValueType RefCnt1 = 0, RefCnt2 = 0;

    const SuperResolutionDesc* pUpscalerDesc = NULL;
    float                      JitterX       = 0.0f;
    float                      JitterY       = 0.0f;

    int num_errors =
        TestObjectCInterface((struct IObject*)pUpscaler);

    IObject_QueryInterface(pUpscaler, &IID_Unknown, &pUnknown);
    if (pUnknown != NULL)
        IObject_Release(pUnknown);
    else
        ++num_errors;

    RefCnt1 = IObject_AddRef(pUpscaler);
    if (RefCnt1 <= 1)
        ++num_errors;
    RefCnt2 = IObject_Release(pUpscaler);
    if (RefCnt2 <= 0)
        ++num_errors;
    if (RefCnt2 != RefCnt1 - 1)
        ++num_errors;

    pUpscalerDesc = ISuperResolution_GetDesc(pUpscaler);
    if (pUpscalerDesc == NULL)
        ++num_errors;
    if (pUpscalerDesc->Name == NULL)
        ++num_errors;
    if (pUpscalerDesc->InputWidth == 0)
        ++num_errors;
    if (pUpscalerDesc->InputHeight == 0)
        ++num_errors;

    ISuperResolution_GetJitterOffset(pUpscaler, 0, &JitterX, &JitterY);
    (void)JitterX;
    (void)JitterY;

    return num_errors;
}

int TestSuperResolutionFactoryCInterface(struct ISuperResolutionFactory* pFactory)
{
    int    num_errors  = 0;
    Uint32 NumVariants = 0;

    ISuperResolutionFactory_EnumerateVariants(pFactory, &NumVariants, NULL);
    (void)NumVariants;

    return num_errors;
}
