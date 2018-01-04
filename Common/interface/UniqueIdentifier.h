/*     Copyright 2015-2018 Egor Yusov
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

#include "Atomics.h"

namespace Diligent
{
    typedef Atomics::Long UniqueIdentifier;

    // Template switch is used to have distinct counters 
    // for unrelated groups of objects
    template<typename ObjectsClass> 
    class UniqueIdHelper
    {
    public:
        UniqueIdHelper() :
            m_ID(0),
            m_bIsInitialized( false )
        {}

        UniqueIdHelper( UniqueIdHelper&& RHS) : 
            m_ID( RHS.m_ID ),
            m_bIsInitialized( RHS.m_bIsInitialized )
        {
            RHS.m_ID = 0;
            RHS.m_bIsInitialized = false;
        }

        const UniqueIdHelper& operator = (UniqueIdHelper&& RHS)
        {
            m_ID = RHS.m_ID;
            m_bIsInitialized = RHS.m_bIsInitialized;
            RHS.m_ID = 0;
            RHS.m_bIsInitialized = false;
            
            return *this;
        }

        UniqueIdentifier GetID()const
        { 
            if( !m_bIsInitialized )
            {
                static Atomics::AtomicLong sm_GlobalCounter;
                m_ID = Atomics::AtomicIncrement(sm_GlobalCounter);
                m_bIsInitialized = true;
            }
            return m_ID; 
        }

    private:
        UniqueIdHelper( const UniqueIdHelper& );
        const UniqueIdHelper& operator = ( const UniqueIdHelper& );

        mutable UniqueIdentifier m_ID;
        mutable bool m_bIsInitialized;
    };
}
