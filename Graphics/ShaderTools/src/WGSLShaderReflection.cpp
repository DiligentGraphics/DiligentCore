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

#include "WGSLShaderReflection.hpp"
#include "ParsingTools.hpp"

#include <charconv>
#include <cstring>
#include <limits>

namespace Diligent
{
namespace
{

constexpr char ReflectionMarker[] = "/*$DILIGENT_WGSL_REFLECTION=";

// Encode directly into the output so arbitrary SPIR-V names cannot terminate
// the WGSL comment, without allocating a temporary string for each name.
void WriteString(std::string& Output, const char* Str)
{
    if (Str == nullptr || *Str == '\0')
    {
        Output += "- ";
        return;
    }
    constexpr char Hex[]  = "0123456789abcdef";
    const size_t   Length = std::strlen(Str);
    const size_t   Start  = Output.size();
    Output.resize(Start + Length * 2 + 1);
    char* Dst = &Output[Start];
    for (size_t i = 0; i < Length; ++i)
    {
        const auto Ch = static_cast<unsigned char>(Str[i]);
        *Dst++        = Hex[Ch >> 4];
        *Dst++        = Hex[Ch & 15];
    }
    *Dst = ' ';
}

void WriteNumber(std::string& Output, Uint32 Value, char Separator = ' ')
{
    char       Digits[std::numeric_limits<Uint32>::digits10 + 1];
    const auto Result = std::to_chars(Digits, Digits + sizeof(Digits), Value);
    Output.append(Digits, static_cast<size_t>(Result.ptr - Digits));
    Output += Separator;
}

// Read directly from the metadata's bounded range in the WGSL source. Only
// decoded names and reflection objects need storage; tokens never allocate.
class ReflectionReader
{
public:
    ReflectionReader(const char* Begin, const char* End) :
        m_Pos{Begin},
        m_End{End}
    {}

    bool IsAtEnd()
    {
        SkipWhitespace();
        return m_Pos == m_End;
    }

    template <typename T>
    bool ReadNumber(T& Value)
    {
        SkipWhitespace();
        T           Number    = 0;
        const char* NumberEnd = Parsing::ParseInteger(m_Pos, m_End, Number);
        if (NumberEnd == m_Pos || (NumberEnd != m_End && !IsWhitespace(*NumberEnd)))
            return false;
        m_Pos = NumberEnd;
        Value = Number;
        return true;
    }

    bool ReadString(std::string& Str)
    {
        SkipWhitespace();
        const char* Begin = m_Pos;
        while (m_Pos != m_End && !IsWhitespace(*m_Pos))
            ++m_Pos;
        const size_t Length = static_cast<size_t>(m_Pos - Begin);
        if (Length == 1 && *Begin == '-')
        {
            Str.clear();
            return true;
        }
        if (Length == 0 || Length % 2 != 0)
            return false;
        auto Digit = [](char Ch) -> int {
            if (Ch >= '0' && Ch <= '9') return Ch - '0';
            if (Ch >= 'a' && Ch <= 'f') return Ch - 'a' + 10;
            return -1;
        };
        Str.resize(Length / 2);
        for (size_t i = 0; i < Str.size(); ++i)
        {
            const int Hi = Digit(Begin[i * 2]);
            const int Lo = Digit(Begin[i * 2 + 1]);
            if (Hi < 0 || Lo < 0 || (Hi == 0 && Lo == 0))
                return false;
            Str[i] = static_cast<char>((Hi << 4) | Lo);
        }
        return true;
    }

private:
    static bool IsWhitespace(char Ch)
    {
        // Preserve the classic-locale whitespace accepted by the original format.
        return Parsing::IsDelimiter(Ch) || Ch == '\v' || Ch == '\f';
    }

    void SkipWhitespace()
    {
        while (m_Pos != m_End && IsWhitespace(*m_Pos))
            ++m_Pos;
    }

    const char*       m_Pos;
    const char* const m_End;
};

void WriteVariable(std::string& Output, const ShaderCodeVariableDescX& Var)
{
    WriteString(Output, Var.Name);
    WriteString(Output, Var.TypeName);
    WriteNumber(Output, Var.Class);
    WriteNumber(Output, Var.BasicType);
    WriteNumber(Output, Var.NumRows);
    WriteNumber(Output, Var.NumColumns);
    WriteNumber(Output, Var.Offset);
    WriteNumber(Output, Var.ArraySize);
    WriteNumber(Output, Var.NumMembers, '\n');
    for (Uint32 i = 0; i < Var.NumMembers; ++i)
        WriteVariable(Output, Var.GetMember(i));
}

bool ReadVariable(ReflectionReader& Reader, ShaderCodeVariableDescX& Var, Uint32 Depth = 0)
{
    // Bound recursion for malformed externally supplied metadata.
    if (Depth >= 64)
        return false;
    std::string Name, TypeName;
    Uint8       Class = 0, BasicType = 0;
    Uint32      NumMembers = 0;
    if (!Reader.ReadString(Name) || !Reader.ReadString(TypeName) ||
        !Reader.ReadNumber(Class) || !Reader.ReadNumber(BasicType) ||
        !Reader.ReadNumber(Var.NumRows) || !Reader.ReadNumber(Var.NumColumns) ||
        !Reader.ReadNumber(Var.Offset) || !Reader.ReadNumber(Var.ArraySize) ||
        !Reader.ReadNumber(NumMembers))
        return false;
    if (Class >= SHADER_CODE_VARIABLE_CLASS_COUNT || BasicType >= SHADER_CODE_BASIC_TYPE_COUNT)
        return false;
    Var.SetName(std::move(Name));
    Var.SetTypeName(std::move(TypeName));
    Var.Class     = static_cast<SHADER_CODE_VARIABLE_CLASS>(Class);
    Var.BasicType = static_cast<SHADER_CODE_BASIC_TYPE>(BasicType);
    for (Uint32 i = 0; i < NumMembers; ++i)
    {
        ShaderCodeVariableDescX Member;
        if (!ReadVariable(Reader, Member, Depth + 1))
            return false;
        Var.AddMember(std::move(Member));
    }
    return true;
}

} // namespace

void AppendWGSLShaderReflection(std::string& WGSL, const WGSLShaderReflection& Reflection)
{
    if (Reflection.Resources.empty() && Reflection.SpecializationConstants.empty())
        return;
    WGSL += '\n';
    WGSL += ReflectionMarker;
    WGSL += "1\n";
    // Each record starts with its type: 0 for a resource, 1 for an override.
    for (const auto& Entry : Reflection.Resources)
    {
        WriteNumber(WGSL, 0);
        const auto& Resource = Entry.second;
        WriteNumber(WGSL, Entry.first.Group);
        WriteNumber(WGSL, Entry.first.Binding);
        WriteString(WGSL, Resource.Name.c_str());
        WriteNumber(WGSL, Resource.UniformBuffer ? 1 : 0, '\n');
        if (Resource.UniformBuffer)
        {
            const auto& Buffer = *Resource.UniformBuffer;
            WriteNumber(WGSL, Buffer.Size);
            WriteNumber(WGSL, Buffer.NumVariables, '\n');
            for (Uint32 i = 0; i < Buffer.NumVariables; ++i)
                WriteVariable(WGSL, Buffer.GetVariable(i));
        }
    }
    for (const auto& Entry : Reflection.SpecializationConstants)
    {
        WriteNumber(WGSL, 1);
        WriteNumber(WGSL, Entry.first);
        WriteString(WGSL, Entry.second.c_str());
        WGSL += '\n';
    }
    WGSL += "*/\n";
}

bool ParseWGSLShaderReflection(const std::string& WGSL, WGSLShaderReflection& Reflection)
{
    const size_t Start = WGSL.find(ReflectionMarker);
    if (Start == std::string::npos)
    {
        Reflection = {};
        return true;
    }
    const size_t End = WGSL.find("*/", Start);
    if (End == std::string::npos || WGSL.find(ReflectionMarker, End) != std::string::npos)
        return false;
    const size_t     DataStart = Start + sizeof(ReflectionMarker) - 1;
    ReflectionReader Reader{WGSL.data() + DataStart, WGSL.data() + End};
    Uint32           Version = 0;
    if (!Reader.ReadNumber(Version) || Version != 1)
        return false;

    WGSLShaderReflection Parsed;
    while (!Reader.IsAtEnd())
    {
        Uint32 RecordType = 0;
        if (!Reader.ReadNumber(RecordType) || RecordType > 1)
            return false;
        if (RecordType == 1)
        {
            Uint32      Id = 0;
            std::string Name;
            if (!Reader.ReadNumber(Id) || Id > 65535 || !Reader.ReadString(Name) || Name.empty() ||
                !Parsed.SpecializationConstants.emplace(Id, std::move(Name)).second)
                return false;
            continue;
        }

        WGSLShaderReflection::BindingPoint Binding;
        WGSLShaderReflection::Resource     Resource;
        Uint32                             HasBuffer = 0;
        if (!Reader.ReadNumber(Binding.Group) || !Reader.ReadNumber(Binding.Binding) ||
            !Reader.ReadString(Resource.Name) || !Reader.ReadNumber(HasBuffer) || HasBuffer > 1)
            return false;
        if (HasBuffer)
        {
            Resource.UniformBuffer.reset(new ShaderCodeBufferDescX{});
            Uint32 NumVariables = 0;
            if (!Reader.ReadNumber(Resource.UniformBuffer->Size) || !Reader.ReadNumber(NumVariables))
                return false;
            for (Uint32 i = 0; i < NumVariables; ++i)
            {
                ShaderCodeVariableDescX Var;
                if (!ReadVariable(Reader, Var))
                    return false;
                Resource.UniformBuffer->AddVariable(std::move(Var));
            }
        }
        if (!Parsed.Resources.emplace(Binding, std::move(Resource)).second)
            return false;
    }
    Reflection = std::move(Parsed);
    return true;
}

} // namespace Diligent
