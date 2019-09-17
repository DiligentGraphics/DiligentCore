/*     Copyright 2019 Diligent Graphics LLC
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

#include "DXGITypeConversions.h"
#include "EngineFactoryBase.h"

/// \file
/// Implementation of the Diligent::EngineFactoryD3DBase template class

namespace Diligent
{

template<typename BaseInterface, DeviceType DevType>
class EngineFactoryD3DBase : public EngineFactoryBase<BaseInterface>
{
public:
    using TEngineFactoryBase = EngineFactoryBase<BaseInterface>;

    EngineFactoryD3DBase(const INTERFACE_ID& FactoryIID) : 
        TEngineFactoryBase(FactoryIID)
    {}

    /// Enumerates hardware adapters available on this machine

    /// \param [in,out] NumAdapters - Number of adapters. If Adapters is null, this value
    ///                               will be overwritten with the number of adapters available
    ///                               on this system. If Adapters is not null, this value should
    ///                               contain maximum number of elements reserved in the array 
    ///                               pointed to by Adapters. In the latter case, this value
    ///                               is overwritten with the actual number of elements written to
    ///                               Adapters.
    /// \param [out]    Adapters - Pointer to the array conataining adapter information. If
    ///                 null is provided, the number of available adapters is written to 
    ///                 NumAdapters
    virtual void EnumerateHardwareAdapters(Uint32 &NumAdapters, 
                                           HardwareAdapterAttribs *Adapters)override final
    {
        auto DXGIAdapters = FindCompatibleAdapters();

        if (Adapters == nullptr)
            NumAdapters = static_cast<Uint32>(DXGIAdapters.size());
        else
        {
            NumAdapters = std::min(NumAdapters, static_cast<Uint32>(DXGIAdapters.size()));
            for (Uint32 adapter = 0; adapter < NumAdapters; ++adapter)
            {
                IDXGIAdapter1 *pDXIAdapter = DXGIAdapters[adapter];
                DXGI_ADAPTER_DESC1 AdapterDesc;
                pDXIAdapter->GetDesc1(&AdapterDesc);

                auto &Attribs = Adapters[adapter];
                WideCharToMultiByte(CP_ACP, 0, AdapterDesc.Description, -1, Attribs.Description, _countof(Attribs.Description), NULL, FALSE);
                Attribs.DedicatedVideoMemory = AdapterDesc.DedicatedVideoMemory;
                Attribs.DedicatedSystemMemory = AdapterDesc.DedicatedSystemMemory;
                Attribs.SharedSystemMemory = AdapterDesc.SharedSystemMemory;
                Attribs.VendorId = AdapterDesc.VendorId;
                Attribs.DeviceId = AdapterDesc.DeviceId;

                Attribs.NumOutputs = 0;
                CComPtr<IDXGIOutput> pOutput;
                while (pDXIAdapter->EnumOutputs(Attribs.NumOutputs, &pOutput) != DXGI_ERROR_NOT_FOUND)
                {
                    ++Attribs.NumOutputs;
                    pOutput.Release();
                };
            }
        }
    }

    /// Enumerates available display modes for the specified output of the specified adapter

    /// \param [in] AdapterId - Id of the adapter enumerated by EnumerateHardwareAdapters().
    /// \param [in] OutputId  - Adapter output id
    /// \param [in] Format    - Display mode format
    /// \param [in, out] NumDisplayModes - Number of display modes. If DisplayModes is null, this
    ///                                    value is overwritten with the number of display modes 
    ///                                    available for this output. If DisplayModes is not null,
    ///                                    this value should contain the maximum number of elements
    ///                                    to be written to DisplayModes array. It is overwritten with
    ///                                    the actual number of display modes written.
    virtual void EnumerateDisplayModes(Uint32 AdapterId, 
                                       Uint32 OutputId, 
                                       TEXTURE_FORMAT Format, 
                                       Uint32 &NumDisplayModes, 
                                       DisplayModeAttribs *DisplayModes)override final
    {
        auto DXGIAdapters = FindCompatibleAdapters();
        if(AdapterId >= DXGIAdapters.size())
        {
            LOG_ERROR("Incorrect adapter id ", AdapterId);
            return;
        }

        IDXGIAdapter1 *pDXIAdapter = DXGIAdapters[AdapterId];

        DXGI_FORMAT DXIGFormat = TexFormatToDXGI_Format(Format);
        CComPtr<IDXGIOutput> pOutput;
        if (pDXIAdapter->EnumOutputs(OutputId, &pOutput) == DXGI_ERROR_NOT_FOUND)
        {
            DXGI_ADAPTER_DESC1 AdapterDesc;
            pDXIAdapter->GetDesc1(&AdapterDesc);
            char DescriptionMB[_countof(AdapterDesc.Description)];
            WideCharToMultiByte(CP_ACP, 0, AdapterDesc.Description, -1, DescriptionMB, _countof(DescriptionMB), NULL, FALSE);
            LOG_ERROR_MESSAGE("Failed to enumerate output ", OutputId, " for adapter ", AdapterId, " (", DescriptionMB, ')');
            return;
        }

        UINT numModes = 0;
        // Get the number of elements
        auto hr = pOutput->GetDisplayModeList(DXIGFormat, 0, &numModes, NULL);
        if (DisplayModes != nullptr)
        {
            // Get the list
            std::vector<DXGI_MODE_DESC> DXIDisplayModes(numModes);
            hr = pOutput->GetDisplayModeList(DXIGFormat, 0, &numModes, DXIDisplayModes.data());
            for (Uint32 m = 0; m < std::min(NumDisplayModes, numModes); ++m)
            {
                const auto &SrcMode = DXIDisplayModes[m];
                auto &DstMode = DisplayModes[m];
                DstMode.Width = SrcMode.Width;
                DstMode.Height = SrcMode.Height;
                DstMode.Format = DXGI_FormatToTexFormat(SrcMode.Format);
                DstMode.RefreshRateNumerator = SrcMode.RefreshRate.Numerator;
                DstMode.RefreshRateDenominator = SrcMode.RefreshRate.Denominator;
                DstMode.Scaling = static_cast<DisplayModeAttribs::SCALING>(SrcMode.Scaling);
                DstMode.ScanlineOrder = static_cast<DisplayModeAttribs::SCANLINE_ORDER>(SrcMode.ScanlineOrdering);
            }
            NumDisplayModes = std::min(NumDisplayModes, numModes);
        }
        else
        {
            NumDisplayModes = numModes;
        }
    }


    std::vector<CComPtr<IDXGIAdapter1>> FindCompatibleAdapters()
    {
        std::vector<CComPtr<IDXGIAdapter1>> DXGIAdapters;

        CComPtr<IDXGIFactory2> pFactory;
        if (FAILED(CreateDXGIFactory1(__uuidof(IDXGIFactory2), (void**)&pFactory)))
        {
            LOG_ERROR_MESSAGE("Failed to create DXGI Factory");
            return std::move(DXGIAdapters);
        }

        CComPtr<IDXGIAdapter1> pDXIAdapter;
        UINT adapter = 0;
        for (; pFactory->EnumAdapters1(adapter, &pDXIAdapter) != DXGI_ERROR_NOT_FOUND; ++adapter, pDXIAdapter.Release())
        {
            DXGI_ADAPTER_DESC1 AdapterDesc;
            pDXIAdapter->GetDesc1(&AdapterDesc);
            if (AdapterDesc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
            {
                // Skip software devices
                continue;
            }

            bool IsCompatibleAdapter = CheckAdapterCompatibility<DevType>(pDXIAdapter);

            if (IsCompatibleAdapter)
            {
                DXGIAdapters.emplace_back(std::move(pDXIAdapter));
            }
        }

        return std::move(DXGIAdapters);
    }

private:

    template<DeviceType DevType>
    bool CheckAdapterCompatibility(IDXGIAdapter1 *pDXGIAdapter);

    template<>
    bool CheckAdapterCompatibility<DeviceType::D3D11>(IDXGIAdapter1 *pDXGIAdapter)
    {
        return true;
    }

    template<>
    bool CheckAdapterCompatibility<DeviceType::D3D12>(IDXGIAdapter1 *pDXGIAdapter)
    {
        auto hr = D3D12CreateDevice(pDXGIAdapter, D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), nullptr);
        return SUCCEEDED(hr);
    }
};

}
