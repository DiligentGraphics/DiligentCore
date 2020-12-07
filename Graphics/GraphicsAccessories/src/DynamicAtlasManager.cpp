/*
 *  Copyright 2019-2020 Diligent Graphics LLC
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

#include "DynamicAtlasManager.hpp"

#include <climits>

namespace Diligent
{

static const DynamicAtlasManager::Region InvalidRegion{UINT_MAX, UINT_MAX, 0, 0};
static const DynamicAtlasManager::Region AllocatedRegion{UINT_MAX, UINT_MAX, UINT_MAX, UINT_MAX};

DynamicAtlasManager::DynamicAtlasManager(Uint32 Width, Uint32 Height) :
    m_Width{Width},
    m_Height{Height},
    m_RegionMap{new Region[Width * Height]}
{
    const Region R{0, 0, m_Width, m_Height};
#ifdef DILIGENT_DEBUG
    InitRegion(R, InvalidRegion);
#endif
    AddFreeRegion(R);
}

DynamicAtlasManager::~DynamicAtlasManager()
{
    if (m_RegionMap)
    {
#if DILIGENT_DEBUG
        DbgVerifyConsistency();
#endif

        VERIFY_EXPR(m_FreeRegionsByWidth.size() == m_FreeRegionsByHeight.size());
        //VERIFY(m_dbgRegions.size() == 1 && m_dbgRegions.begin()->R == Region(0, 0, m_Width, m_Height) && !m_dbgRegions.begin()->IsAllocated,
        //       "Not all allocations have been freed");
        //DEV_CHECK_ERR(m_FreeRegionsByWidth.size() == 1, "There expected to be a single free region");
    }
    else
    {
        VERIFY_EXPR(m_FreeRegionsByWidth.empty());
        VERIFY_EXPR(m_FreeRegionsByHeight.empty());
        VERIFY_EXPR(m_dbgRegions.empty());
    }
}

DynamicAtlasManager::Region DynamicAtlasManager::Allocate(Uint32 Width, Uint32 Height)
{
    auto it_w = m_FreeRegionsByWidth.lower_bound(Region{0, 0, Width, 0});
    while (it_w != m_FreeRegionsByWidth.end() && it_w->height < Height)
        ++it_w;
    VERIFY_EXPR(it_w == m_FreeRegionsByWidth.end() || (it_w->width >= Width && it_w->height >= Height));

    auto it_h = m_FreeRegionsByHeight.lower_bound(Region{0, 0, 0, Height});
    while (it_h != m_FreeRegionsByHeight.end() && it_h->width < Width)
        ++it_h;
    VERIFY_EXPR(it_h == m_FreeRegionsByHeight.end() || (it_h->width >= Width && it_h->height >= Height));

    const auto AreaW = it_w != m_FreeRegionsByWidth.end() ? it_w->width * it_w->height : 0;
    const auto AreaH = it_h != m_FreeRegionsByHeight.end() ? it_h->width * it_h->height : 0;
    VERIFY_EXPR(AreaW == 0 || AreaW >= Width * Height);
    VERIFY_EXPR(AreaH == 0 || AreaH >= Width * Height);

    Region R;
    // Use the smaller area source region
    if (AreaW > 0 && AreaH > 0)
    {
        R = AreaW < AreaH ? *it_w : *it_h;
    }
    else if (AreaW > 0)
    {
        R = *it_w;
    }
    else if (AreaH > 0)
    {
        R = *it_h;
    }
    else
    {
        return Region{};
    }

    RemoveFreeRegion(R);

    if (R.width > Width && R.height > Height)
    {
        if (R.width > R.height)
        {
            //    _____________________
            //   |       |             |
            //   |   B   |             |
            //   |_______|      A      |
            //   |       |             |
            //   |   R   |             |
            //   |_______|_____________|
            //
            AddFreeRegion(Region{R.x + Width, R.y, R.width - Width, R.height}); // A
            AddFreeRegion(Region{R.x, R.y + Height, Width, R.height - Height}); // B
        }
        else
        {
            //   _____________
            //  |             |
            //  |             |
            //  |      A      |
            //  |             |
            //  |_____ _______|
            //  |     |       |
            //  |  R  |   B   |
            //  |_____|_______|
            AddFreeRegion(Region{R.x, R.y + Height, R.width, R.height - Height}); // A
            AddFreeRegion(Region{R.x + Width, R.y, R.width - Width, Height});     // B
        }
    }
    else if (R.width > Width)
    {
        //   _______ __________
        //  |       |          |
        //  |   R   |    A     |
        //  |_______|__________|
        AddFreeRegion(Region{R.x + Width, R.y, R.width - Width, R.height}); // A
    }
    else if (R.height > Height)
    {
        //    _______
        //   |       |
        //   |   A   |
        //   |_______|
        //   |       |
        //   |   R   |
        //   |_______|
        AddFreeRegion(Region{R.x, R.y + Height, R.width, R.height - Height}); // A
    }

    R.width  = Width;
    R.height = Height;

    InitRegion(R, AllocatedRegion);

#if DILIGENT_DEBUG
    {
        auto inserted = m_dbgRegions.emplace(R, true).second;
        VERIFY_EXPR(inserted);
    }
    DbgVerifyConsistency();
#endif

    return R;
}

void DynamicAtlasManager::Free(Region&& R)
{
#if DILIGENT_DEBUG
    DbgVerifyRegion(R);
    {
        auto it = m_dbgRegions.find({R, true});
        VERIFY(it != m_dbgRegions.end(),
               "Unable to find region [", R.x, ", ", R.x + R.width, ") x [", R.y, ", ", R.y + R.height, ") among allocated regions");
        m_dbgRegions.erase(it);
    }
#endif

    AddFreeRegion(R);

#if DILIGENT_DEBUG
    DbgVerifyConsistency();
#endif

    R = InvalidRegion;
}

void DynamicAtlasManager::AddFreeRegion(Region R)
{
#ifdef DILIGENT_DEBUG
    {
        const auto& R0 = GetRegion(R.x, R.y);
        VERIFY_EXPR(R0 == AllocatedRegion || R0 == InvalidRegion);
        for (Uint32 y = R.y; y < R.y + R.height; ++y)
        {
            for (Uint32 x = R.x; x < R.x + R.width; ++x)
            {
                VERIFY_EXPR(GetRegion(x, y) == R0);
            }
        }
    }
#endif

    bool Merged = false;
    do
    {
        auto TryMergeHorz = [&]() //
        {
            if (R.x > 0)
            {
                const auto& lftR = GetRegion(R.x - 1, R.y);
                if (lftR != AllocatedRegion && lftR != InvalidRegion)
                {
                    VERIFY_EXPR(lftR.x + lftR.width == R.x);
                    if (lftR.y == R.y && lftR.height == R.height)
                    {
                        //   __________ __________
                        //  |          |          |
                        //  |   lftR   |    R     |
                        //  |__________|__________|
                        R.x = lftR.x;
                        R.width += lftR.width;
                        RemoveFreeRegion(lftR);
                        VERIFY_EXPR(lftR == InvalidRegion);
                        return true;
                    }
                }
            }

            if (R.x + R.width < m_Width)
            {
                const auto& rgtR = GetRegion(R.x + R.width, R.y);
                if (rgtR != AllocatedRegion && rgtR != InvalidRegion)
                {
                    VERIFY_EXPR(R.x + R.width == rgtR.x);
                    if (rgtR.y == R.y && rgtR.height == R.height)
                    {
                        //   _________ ____________
                        //  |         |            |
                        //  |    R    |    rgtR    |
                        //  |_________|____________|
                        R.width += rgtR.width;
                        RemoveFreeRegion(rgtR);
                        VERIFY_EXPR(rgtR == InvalidRegion);
                        return true;
                    }
                }
            }

            return false;
        };

        auto TryMergeVert = [&]() //
        {
            if (R.y > 0)
            {
                const auto& btmR = GetRegion(R.x, R.y - 1);
                if (btmR != AllocatedRegion && btmR != InvalidRegion)
                {
                    VERIFY_EXPR(btmR.y + btmR.height == R.y);
                    if (btmR.x == R.x && btmR.width == R.width)
                    {
                        //    ________
                        //   |        |
                        //   |   R    |
                        //   |________|
                        //   |        |
                        //   |  btmR  |
                        //   |________|
                        R.y = btmR.y;
                        R.height += btmR.height;
                        RemoveFreeRegion(btmR);
                        VERIFY_EXPR(btmR == InvalidRegion);
                        return true;
                    }
                }
            }

            if (R.y + R.height < m_Height)
            {
                const auto& tpR = GetRegion(R.x, R.y + R.height);
                if (tpR != AllocatedRegion && tpR != InvalidRegion)
                {
                    VERIFY_EXPR(R.y + R.height == tpR.y);
                    if (tpR.x == R.x && tpR.width == R.width)
                    {
                        //    _______
                        //   |       |
                        //   |  tpR  |
                        //   |_______|
                        //   |       |
                        //   |   R   |
                        //   |_______|
                        R.height += tpR.height;
                        RemoveFreeRegion(tpR);
                        VERIFY_EXPR(tpR == InvalidRegion);
                        return true;
                    }
                }
            }

            return false;
        };

        // Try to merge along the longest edge first
        Merged = (R.width > R.height) ? TryMergeVert() : TryMergeHorz();

        // If not merged, try another edge
        if (!Merged)
        {
            Merged = (R.width > R.height) ? TryMergeHorz() : TryMergeVert();
        }
    } while (Merged);

    InitRegion(R, R);

    {
        auto inserted = m_FreeRegionsByWidth.emplace(R).second;
        VERIFY_EXPR(inserted);
    }
    {
        auto inserted = m_FreeRegionsByHeight.emplace(R).second;
        VERIFY_EXPR(inserted);
    }

#if DILIGENT_DEBUG
    {
        auto inserted = m_dbgRegions.emplace(R, false).second;
        VERIFY_EXPR(inserted);
    }
#endif
}

// Do NOT use refernce as parameter as we may mess it up while writing InvalidRegion
void DynamicAtlasManager::RemoveFreeRegion(const Region R)
{
#if DILIGENT_DEBUG
    DbgVerifyRegion(R);
    {
        auto it = m_dbgRegions.find({R, false});
        VERIFY(it != m_dbgRegions.end(),
               "Unable to find region [", R.x, ", ", R.x + R.width, ") x [", R.y, ", ", R.y + R.height, ") among free regions");
        m_dbgRegions.erase(it);
    }
#endif

    VERIFY_EXPR(m_FreeRegionsByWidth.find(R) != m_FreeRegionsByWidth.end());
    VERIFY_EXPR(m_FreeRegionsByHeight.find(R) != m_FreeRegionsByHeight.end());
    m_FreeRegionsByWidth.erase(R);
    m_FreeRegionsByHeight.erase(R);

    // Use InvalidRegion to indicate that the region is
    // neither allocated nor free.
    InitRegion(R, InvalidRegion);
}

void DynamicAtlasManager::InitRegion(const Region R, const Region Val)
{
    VERIFY_EXPR(Val == R || Val == InvalidRegion || Val == AllocatedRegion);

#if DILIGENT_DEBUG
    DbgVerifyRegion(R);
#endif

    for (Uint32 y = R.y; y < R.y + R.height; ++y)
    {
        for (Uint32 x = R.x; x < R.x + R.width; ++x)
        {
            GetRegion(x, y) = Val;
        }
    }
}


#if DILIGENT_DEBUG

void DynamicAtlasManager::DbgVerifyRegion(const Region& R) const
{
    VERIFY_EXPR(R != InvalidRegion && R != AllocatedRegion);
    VERIFY_EXPR(R.width > 0 && R.height > 0);

    VERIFY(R.x < m_Width, "Region x (", R.x, ") exceeds atlas width (", m_Width, ").");
    VERIFY(R.y < m_Height, "Region y (", R.y, ") exceeds atlas height (", m_Height, ").");
    VERIFY(R.x + R.width <= m_Width, "Region right boundary (", R.x + R.width, ") exceeds atlas width (", m_Width, ").");
    VERIFY(R.y + R.height <= m_Height, "Region top boundart (", R.y + R.height, ") exceeds atlas height (", m_Height, ").");
}

void DynamicAtlasManager::DbgVerifyConsistency() const
{
    VERIFY_EXPR(m_FreeRegionsByWidth.size() == m_FreeRegionsByHeight.size());

    Uint32 Area = 0;
    for (const auto& RI : m_dbgRegions)
    {
        const auto& R = RI.R;
        DbgVerifyRegion(R);

        {
            for (Uint32 y = R.y; y < R.y + R.height; ++y)
            {
                for (Uint32 x = R.x; x < R.x + R.width; ++x)
                {
                    const auto& R1 = GetRegion(x, y);
                    if (RI.IsAllocated)
                    {
                        VERIFY(R1 == AllocatedRegion,
                               "Region at position (", x, ", ", y, ") is not labeled as allocated");
                    }
                    else
                    {
                        VERIFY(R == R1,
                               "Region [", R1.x, ", ", R1.x + R1.width, ") x [", R1.y, ", ", R1.y + R1.height,
                               ") is incosistent wiht its base region [",
                               R.x, ", ", R.x + R.width, ") x [", R.y, ", ", R.y + R.height, ")");
                    }
                }
            }
        }

        Area += R.width * R.height;
        if (RI.IsAllocated)
        {
            VERIFY(m_FreeRegionsByWidth.find(R) == m_FreeRegionsByWidth.end(),
                   "Allocated region [", R.x, ", ", R.x + R.width, ") x [", R.y, ", ", R.y + R.height,
                   ") was found in free regions-by-width map");

            VERIFY(m_FreeRegionsByHeight.find(R) == m_FreeRegionsByHeight.end(),
                   "Allocated region [", R.x, ", ", R.x + R.width, ") x [", R.y, ", ", R.y + R.height,
                   ") was found in free regions-by-width map");
        }
        else
        {
            VERIFY(m_FreeRegionsByWidth.find(R) != m_FreeRegionsByWidth.end(),
                   "region [", R.x, ", ", R.x + R.width, ") x [", R.y, ", ", R.y + R.height,
                   ") is not found in free regions-by-width map");


            VERIFY(m_FreeRegionsByHeight.find(R) != m_FreeRegionsByHeight.end(),
                   "region [", R.x, ", ", R.x + R.width, ") x [", R.y, ", ", R.y + R.height,
                   ") is not found in free regions-by-height map");

            if (R.x + R.width < m_Width)
            {
                // Right neighbor
                const auto& rgtR = GetRegion(R.x + R.width, R.y);
                VERIFY_EXPR(rgtR != InvalidRegion);
                if (rgtR != AllocatedRegion)
                {
                    VERIFY(!(rgtR.y == R.y && rgtR.height == R.height), "region [", R.x, ", ", R.x + R.width, ") x [", R.y, ", ", R.y + R.height,
                           ") can be merged with its right neighbor [", rgtR.x, ", ", rgtR.x + rgtR.width, ") x [", rgtR.y, ", ", rgtR.y + rgtR.height, ")");
                }
            }

            if (R.y + R.height < m_Height)
            {
                // Top neighbor
                const auto& tpR = GetRegion(R.x, R.y + R.height);
                VERIFY_EXPR(tpR != InvalidRegion);
                if (tpR != AllocatedRegion)
                {
                    VERIFY(!(tpR.x == R.x && tpR.width == R.width), "region [", R.x, ", ", R.x + R.width, ") x [", R.y, ", ", R.y + R.height,
                           ") can be merged with its top neighbor [", tpR.x, ", ", tpR.x + tpR.width, ") x [", tpR.y, ", ", tpR.y + tpR.height, ")");
                }
            }
        }
    }
    VERIFY(Area == m_Width * m_Height, "Not entire atlas area has been covered");
}
#endif

} // namespace Diligent
