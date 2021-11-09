/*
 *  Copyright 2019-2021 Diligent Graphics LLC
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

namespace Diligent
{

template <typename T>
struct IsTriviallySerializable
{
    static constexpr bool value = std::is_floating_point<T>::value || std::is_integral<T>::value || std::is_enum<T>::value;
};

template <typename T, size_t Size>
struct IsTriviallySerializable<std::array<T, Size>>
{
    static constexpr bool value = IsTriviallySerializable<T>::value;
};

template <typename T, size_t Size>
struct IsTriviallySerializable<T[Size]>
{
    static constexpr bool value = IsTriviallySerializable<T>::value;
};

#define DECL_TRIVIALLY_SERIALIZABLE(Type)   \
    template <>                             \
    struct IsTriviallySerializable<Type>    \
    {                                       \
        static constexpr bool value = true; \
    }

enum class SerializerMode
{
    Read,
    Write,
    Measure,
};


template <SerializerMode Mode>
class Serializer
{
public:
    template <typename T>
    using RawType = typename std::remove_const_t<std::remove_reference_t<T>>;

    template <typename T>
    using TEnable = typename std::enable_if_t<IsTriviallySerializable<T>::value, void>;

    template <typename T>
    using TEnableStr = typename std::enable_if_t<(std::is_same<const char* const, T>::value || std::is_same<const char*, T>::value), void>;
    using InCharPtr  = typename std::conditional_t<Mode == SerializerMode::Read, const char*&, const char*>;

    template <typename T>
    using TReadOnly = typename std::enable_if_t<Mode == SerializerMode::Read, const T*>;

    using TPointer = typename std::conditional_t<Mode == SerializerMode::Write, Uint8*, const Uint8*>;

    template <typename T>
    using TQual = typename std::conditional_t<Mode == SerializerMode::Read, T, const T>;


    Serializer() :
        m_Ptr{nullptr},
        m_End{m_Ptr + ~0u}
    {
        static_assert(Mode == SerializerMode::Measure, "only Meause mode is supported");
    }

    Serializer(TQual<void*> Ptr, size_t Size) :
        m_Ptr{static_cast<TPointer>(Ptr)},
        m_End{m_Ptr + Size}
    {
        static_assert(Mode == SerializerMode::Read || Mode == SerializerMode::Write, "only Read or Write mode are supported");
    }

    template <typename T>
    TEnable<T> Serialize(TQual<T>& Value)
    {
        VERIFY_EXPR(m_Ptr + sizeof(Value) <= m_End);
        Copy(m_Ptr, &Value, sizeof(Value));
        m_Ptr += sizeof(Value);
    }

    template <typename T>
    TEnableStr<T> Serialize(InCharPtr Str);

    template <typename T>
    TReadOnly<T> Cast()
    {
        static_assert(std::is_trivial<T>::value, "Can not cast to non triavial type");
        VERIFY(reinterpret_cast<size_t>(m_Ptr) % alignof(T) == 0, "Pointer must be properly aligned");
        VERIFY_EXPR(m_Ptr + sizeof(T) <= m_End);
        auto* Ptr = m_Ptr;
        m_Ptr += sizeof(T);
        return reinterpret_cast<const T*>(Ptr);
    }

    template <typename Arg0Type, typename... ArgTypes>
    void operator()(Arg0Type& Arg0, ArgTypes&... Args)
    {
        Serialize<RawType<Arg0Type>>(Arg0);
        operator()(Args...);
    }

    template <typename Arg0Type>
    void operator()(Arg0Type& Arg0)
    {
        Serialize<RawType<Arg0Type>>(Arg0);
    }

    size_t GetSize(const void* Ptr) const
    {
        VERIFY_EXPR(m_Ptr >= Ptr);
        return m_Ptr - static_cast<const Uint8*>(Ptr);
    }

    size_t GetRemainSize() const
    {
        return m_End - m_Ptr;
    }

    const void* GetCurrentPtr() const
    {
        return m_Ptr;
    }

    bool IsEnd() const
    {
        return m_Ptr == m_End;
    }

private:
    template <typename T1, typename T2>
    static void Copy(T1* Lhs, T2* Rhs, size_t Size);

    TPointer       m_Ptr = nullptr;
    TPointer const m_End = nullptr;
};


template <>
template <typename T1, typename T2>
void Serializer<SerializerMode::Read>::Copy(T1* Lhs, T2* Rhs, size_t Size)
{
    std::memcpy(Rhs, Lhs, Size);
}

template <>
template <typename T1, typename T2>
void Serializer<SerializerMode::Write>::Copy(T1* Lhs, T2* Rhs, size_t Size)
{
    std::memcpy(Lhs, Rhs, Size);
}

template <>
template <typename T1, typename T2>
void Serializer<SerializerMode::Measure>::Copy(T1* Lhs, T2* Rhs, size_t Size)
{
}

template <>
template <typename T>
typename Serializer<SerializerMode::Read>::TEnableStr<T> Serializer<SerializerMode::Read>::Serialize(InCharPtr Str)
{
    Uint16 Length = 0;
    VERIFY_EXPR(m_Ptr + sizeof(Length) <= m_End);
    std::memcpy(&Length, m_Ptr, sizeof(Length));
    m_Ptr += sizeof(Length);

    VERIFY_EXPR(m_Ptr + Length <= m_End);
    Str = Length > 1 ? reinterpret_cast<const char*>(m_Ptr) : "";
    m_Ptr += Length;
}

template <>
template <typename T>
typename Serializer<SerializerMode::Write>::TEnableStr<T> Serializer<SerializerMode::Write>::Serialize(InCharPtr Str)
{
    const Uint16 Length = static_cast<Uint16>((Str != nullptr && *Str != 0) ? strlen(Str) + 1 : 0);

    VERIFY_EXPR(m_Ptr + sizeof(Length) <= m_End);
    Copy(m_Ptr, &Length, sizeof(Length));
    m_Ptr += sizeof(Length);

    VERIFY_EXPR(m_Ptr + Length <= m_End);
    Copy(m_Ptr, Str, Length);
    m_Ptr += Length;
}

template <>
template <typename T>
typename Serializer<SerializerMode::Measure>::TEnableStr<T> Serializer<SerializerMode::Measure>::Serialize(InCharPtr Str)
{
    const Uint16 Length = static_cast<Uint16>((Str != nullptr && *Str != 0) ? strlen(Str) + 1 : 0);
    m_Ptr += sizeof(Length);
    m_Ptr += Length;
}

} // namespace Diligent
