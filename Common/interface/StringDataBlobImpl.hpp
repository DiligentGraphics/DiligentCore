/*
 *  Copyright 2019-2024 Diligent Graphics LLC
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

#pragma once

/// \file
/// Implementation for the IDataBlob interface

#include <vector>

#include "../../Primitives/interface/BasicTypes.h"
#include "../../Primitives/interface/DataBlob.h"
#include "ObjectBase.hpp"
#include "RefCntAutoPtr.hpp"

namespace Diligent
{

/// String data blob implementation.
class StringDataBlobImpl : public ObjectBase<IDataBlob>
{
public:
    typedef ObjectBase<IDataBlob> TBase;

    template <typename... ArgsType>
    StringDataBlobImpl(IReferenceCounters* pRefCounters, ArgsType&&... Args) :
        TBase{pRefCounters},
        m_String{std::forward<ArgsType>(Args)...}
    {}

    template <typename... ArgsType>
    static RefCntAutoPtr<StringDataBlobImpl> Create(ArgsType&&... Args)
    {
        return RefCntAutoPtr<StringDataBlobImpl>{MakeNewRCObj<StringDataBlobImpl>()(std::forward<ArgsType>(Args)...)};
    }

    IMPLEMENT_QUERY_INTERFACE_IN_PLACE(IID_DataBlob, TBase)

    /// Sets the size of the internal data buffer
    virtual void DILIGENT_CALL_TYPE Resize(size_t NewSize) override
    {
        m_String.resize(NewSize);
    }

    /// Returns the size of the internal data buffer
    virtual size_t DILIGENT_CALL_TYPE GetSize() const override
    {
        return m_String.length();
    }

    /// Returns the pointer to the internal data buffer
    virtual void* DILIGENT_CALL_TYPE GetDataPtr(size_t Offset = 0) override
    {
        return &m_String[Offset];
    }

    /// Returns the pointer to the internal data buffer
    virtual const void* DILIGENT_CALL_TYPE GetConstDataPtr(size_t Offset = 0) const override
    {
        return &m_String[Offset];
    }

private:
    String m_String;
};

} // namespace Diligent
