/*
 *  Copyright 2019-2026 Diligent Graphics LLC
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

#include <deque>

#include "ShaderToolsCommon.hpp"
#include "ShaderSourcePath.hpp"
#include "DefaultShaderSourceStreamFactory.h"
#include "ObjectBase.hpp"
#include "RenderDevice.h"
#include "ShaderSourceFactoryUtils.hpp"
#include "TestingEnvironment.hpp"

#include "gtest/gtest.h"

using namespace Diligent;
using namespace Diligent::Testing;

namespace
{

class ProbeOnlyShaderSourceFactory final : public ObjectBase<IShaderSourceInputStreamFactory>
{
public:
    using TBase = ObjectBase<IShaderSourceInputStreamFactory>;

    static RefCntAutoPtr<IShaderSourceInputStreamFactory> Create(IShaderSourceInputStreamFactory* pFactory,
                                                                 const Char*                      ProbeOnlyPath)
    {
        return RefCntAutoPtr<IShaderSourceInputStreamFactory>{MakeNewRCObj<ProbeOnlyShaderSourceFactory>()(pFactory, ProbeOnlyPath)};
    }

    ProbeOnlyShaderSourceFactory(IReferenceCounters*              pRefCounters,
                                 IShaderSourceInputStreamFactory* pFactory,
                                 const Char*                      ProbeOnlyPath) :
        TBase{pRefCounters},
        m_pFactory{pFactory},
        m_ProbeOnlyPath{NormalizeShaderSourcePath(ProbeOnlyPath)}
    {
        VERIFY_EXPR(m_pFactory != nullptr);
    }

    Bool DILIGENT_CALL_TYPE CreateInputStream(const Char* Name, IFileStream** ppStream) override final
    {
        return CreateInputStream2(Name, CREATE_SHADER_SOURCE_INPUT_STREAM_FLAG_NONE, ppStream);
    }

    Bool DILIGENT_CALL_TYPE CreateInputStream2(const Char*                             Name,
                                               CREATE_SHADER_SOURCE_INPUT_STREAM_FLAGS Flags,
                                               IFileStream**                           ppStream) override final
    {
        const bool IsProbeOnlyPath = NormalizeShaderSourcePath(Name) == m_ProbeOnlyPath;
        Bool       SourceFound     = True;
        if (IsProbeOnlyPath)
        {
            if (ppStream != nullptr)
            {
                DEV_CHECK_ERR(*ppStream == nullptr, "Output stream pointer must be null");
                *ppStream   = nullptr;
                SourceFound = False;
            }
        }
        else
        {
            SourceFound = m_pFactory->CreateInputStream2(Name, Flags, ppStream);
        }
        return SourceFound;
    }

    IMPLEMENT_QUERY_INTERFACE_IN_PLACE(IID_IShaderSourceInputStreamFactory, TBase)

private:
    RefCntAutoPtr<IShaderSourceInputStreamFactory> m_pFactory;
    const String                                   m_ProbeOnlyPath;
};

TEST(ShaderPreprocessTest, NormalizeShaderSourcePath)
{
    const struct
    {
        const Char* Path;
        const Char* ExpectedPath;
    } TestCases[] = {
        {nullptr, ""},
        {"", ""},
        {"Shader.hlsl", "Shader.hlsl"},
        {"./Shader.hlsl", "Shader.hlsl"},
        {"Directory//./Subdir/../Shader.hlsl", "Directory/Shader.hlsl"},
        {"Directory\\Subdir\\Shader.hlsl", "Directory/Subdir/Shader.hlsl"},
        {"/Directory\\Subdir/../Shader.hlsl", "/Directory/Shader.hlsl"},
        {"C:\\Directory\\Subdir\\..\\Shader.hlsl", "C:/Directory/Shader.hlsl"},
        {"C:/../Shader.hlsl", "C:/Shader.hlsl"},
        {"\\\\Server\\Share\\Directory\\..\\Shader.hlsl", "//Server/Share/Shader.hlsl"},
        {"//Server\\Share/Directory/./Shader.hlsl", "//Server/Share/Directory/Shader.hlsl"},
    };

    for (const auto& TestCase : TestCases)
    {
        EXPECT_EQ(NormalizeShaderSourcePath(TestCase.Path), TestCase.ExpectedPath)
            << "Path: " << (TestCase.Path != nullptr ? TestCase.Path : "<null>");
    }

    EXPECT_EQ(NormalizeShaderSourcePath("Directory/Subdir/../Shader.hlsl", BasicFileSystem::WinSlash), "Directory\\Shader.hlsl");
    EXPECT_EQ(NormalizeShaderSourcePath("/Directory/Subdir/../Shader.hlsl", BasicFileSystem::WinSlash), "\\Directory\\Shader.hlsl");
    EXPECT_EQ(NormalizeShaderSourcePath("C:/Directory/Subdir/../Shader.hlsl", BasicFileSystem::WinSlash), "C:\\Directory\\Shader.hlsl");
    EXPECT_EQ(NormalizeShaderSourcePath("//Server/Share/Directory/../Shader.hlsl", BasicFileSystem::WinSlash), "\\\\Server\\Share\\Shader.hlsl");

    String PlatformPath = "Directory";
    PlatformPath += BasicFileSystem::SlashSymbol;
    PlatformPath += "Shader.hlsl";
    EXPECT_EQ(NormalizeShaderSourcePath("Directory/Subdir/../Shader.hlsl", 0), PlatformPath);
}

TEST(ShaderPreprocessTest, GetShaderIncludePathCandidates)
{
    struct TestCase
    {
        const Char* IncluderPath;
        const Char* IncludeName;
        bool        IsLocalInclude;
        const Char* ExpectedLocalPath;
        const Char* ExpectedSearchPath;
    };

    const TestCase TestCases[] = {
        // Empty include names do not produce candidates.
        {nullptr, nullptr, true, "", ""},
        {"Shaders/Main.hlsl", "", true, "", ""},
        {"Shaders/Main.hlsl", ".", true, "", ""},

        // System includes and local includes without a parent use the normalized
        // include name directly.
        {"Shaders/Main.hlsl", "Headers\\./Nested\\../Types.hlsl", false, "", "Headers/Types.hlsl"},
        {nullptr, "Headers\\Types.hlsl", true, "", "Headers/Types.hlsl"},
        {"", "Headers/Types.hlsl", true, "", "Headers/Types.hlsl"},
        {"Main.hlsl", "Headers/Types.hlsl", true, "", "Headers/Types.hlsl"},

        // Relative local includes use the parent directory first and preserve the
        // normalized include name as the search-directory fallback.
        {"Shaders/Main.hlsl", "Config.hlsl", true, "Shaders/Config.hlsl", "Config.hlsl"},
        {"Shaders/Nested/Main.hlsl", "../Common.hlsl", true, "Shaders/Common.hlsl", "../Common.hlsl"},
        {"Shaders\\Nested/Main.hlsl", "..\\Common\\Types.hlsl", true, "Shaders/Common/Types.hlsl", "../Common/Types.hlsl"},
        {"Shaders/./Nested/../Main.hlsl", "Config.hlsl", true, "Shaders/Config.hlsl", "Config.hlsl"},
        {"Package/Main.hlsl", "../../Config.hlsl", true, "../Config.hlsl", "../../Config.hlsl"},
        {"Package/Main.hlsl", "C:Config.hlsl", true, "Package/C:Config.hlsl", "C:Config.hlsl"},

        // Rooted includers keep their root when forming the parent-relative candidate.
        {"/Main.hlsl", "Config.hlsl", true, "/Config.hlsl", "Config.hlsl"},
        {"\\Main.hlsl", "Config.hlsl", true, "/Config.hlsl", "Config.hlsl"},
        {"C:\\Main.hlsl", "Config.hlsl", true, "C:/Config.hlsl", "Config.hlsl"},
        {"C:/Shaders/Main.hlsl", "../Config.hlsl", true, "C:/Config.hlsl", "../Config.hlsl"},
        {"\\\\Server\\Share\\Main.hlsl", "Config.hlsl", true, "//Server/Share/Config.hlsl", "Config.hlsl"},

        // A rooted include is independent of its delimiter and includer.
        {"Shaders/Main.hlsl", "/Common/Config.hlsl", true, "/Common/Config.hlsl", ""},
        {"Shaders/Main.hlsl", "D:\\Common\\Config.hlsl", true, "D:/Common/Config.hlsl", ""},
        {"Shaders/Main.hlsl", "\\\\Server\\Share\\Config.hlsl", true, "//Server/Share/Config.hlsl", ""},
        {"Shaders/Main.hlsl", "/Common/Config.hlsl", false, "", "/Common/Config.hlsl"},
    };

    for (const TestCase& Test : TestCases)
    {
        const ShaderIncludePathCandidates Candidates = GetShaderIncludePathCandidates(Test.IncluderPath, Test.IncludeName, Test.IsLocalInclude);
        EXPECT_EQ(Candidates.LocalPath, Test.ExpectedLocalPath)
            << "Includer: " << (Test.IncluderPath != nullptr ? Test.IncluderPath : "<null>")
            << ", include: " << (Test.IncludeName != nullptr ? Test.IncludeName : "<null>")
            << ", local: " << Test.IsLocalInclude;
        EXPECT_EQ(Candidates.SearchPath, Test.ExpectedSearchPath)
            << "Includer: " << (Test.IncluderPath != nullptr ? Test.IncluderPath : "<null>")
            << ", include: " << (Test.IncludeName != nullptr ? Test.IncludeName : "<null>")
            << ", local: " << Test.IsLocalInclude;
    }
}

TEST(ShaderPreprocessTest, Include)
{
    RefCntAutoPtr<IShaderSourceInputStreamFactory> pShaderSourceFactory;
    CreateDefaultShaderSourceStreamFactory("shaders/ShaderPreprocessor", &pShaderSourceFactory);
    ASSERT_NE(pShaderSourceFactory, nullptr);

    {
        std::deque<const char*> Includes{
            "IncludeCommon0.hlsl",
            "IncludeCommon1.hlsl",
            "IncludeBasicTest.hlsl"};

        ShaderCreateInfo ShaderCI{};
        ShaderCI.Desc.Name                  = "TestShader";
        ShaderCI.FilePath                   = "IncludeBasicTest.hlsl";
        ShaderCI.pShaderSourceStreamFactory = pShaderSourceFactory;

        const auto Result = ProcessShaderIncludes(ShaderCI, [&](const ShaderIncludePreprocessInfo& ProcessInfo) {
            EXPECT_EQ(ProcessInfo.FilePath, Includes.front());
            Includes.pop_front();
        });
        EXPECT_EQ(Result, true);
    }

    {
        std::deque<const char*> Includes{
            "IncludeCommon0.hlsl",
            "IncludeWhiteSpaceTest.hlsl"};

        ShaderCreateInfo ShaderCI{};
        ShaderCI.Desc.Name                  = "TestShader";
        ShaderCI.FilePath                   = "IncludeWhiteSpaceTest.hlsl";
        ShaderCI.pShaderSourceStreamFactory = pShaderSourceFactory;

        const auto Result = ProcessShaderIncludes(ShaderCI, [&](const ShaderIncludePreprocessInfo& ProcessInfo) {
            EXPECT_EQ(ProcessInfo.FilePath, Includes.front());
            Includes.pop_front();
        });

        EXPECT_EQ(Result, true);
    }

    {
        std::deque<const char*> Includes{
            "IncludeCommon0.hlsl",
            "IncludeCommentsSingleLineTest.hlsl"};

        ShaderCreateInfo ShaderCI{};
        ShaderCI.Desc.Name                  = "TestShader";
        ShaderCI.FilePath                   = "IncludeCommentsSingleLineTest.hlsl";
        ShaderCI.pShaderSourceStreamFactory = pShaderSourceFactory;

        const auto Result = ProcessShaderIncludes(ShaderCI, [&](const ShaderIncludePreprocessInfo& ProcessInfo) {
            EXPECT_EQ(ProcessInfo.FilePath, Includes.front());
            Includes.pop_front();
        });

        EXPECT_EQ(Result, true);
    }

    {
        std::deque<const char*> Includes{
            "IncludeCommon0.hlsl",
            "IncludeCommentsMultiLineTest.hlsl"};

        ShaderCreateInfo ShaderCI{};
        ShaderCI.Desc.Name                  = "TestShader";
        ShaderCI.FilePath                   = "IncludeCommentsMultiLineTest.hlsl";
        ShaderCI.pShaderSourceStreamFactory = pShaderSourceFactory;

        const auto Result = ProcessShaderIncludes(ShaderCI, [&](const ShaderIncludePreprocessInfo& ProcessInfo) {
            EXPECT_EQ(ProcessInfo.FilePath, Includes.front());
            Includes.pop_front();
        });

        EXPECT_EQ(Result, true);
    }

    {
        std::deque<const char*> Includes{
            "IncludeCommentsTrickyCasesTest.hlsl"};

        ShaderCreateInfo ShaderCI{};
        ShaderCI.Desc.Name                  = "TestShader";
        ShaderCI.FilePath                   = "IncludeCommentsTrickyCasesTest.hlsl";
        ShaderCI.pShaderSourceStreamFactory = pShaderSourceFactory;

        const auto Result = ProcessShaderIncludes(ShaderCI, [&](const ShaderIncludePreprocessInfo& ProcessInfo) {
            EXPECT_EQ(ProcessInfo.FilePath, Includes.front());
            Includes.pop_front();
        });

        EXPECT_EQ(Result, true);
    }
}

TEST(ShaderPreprocessTest, ShaderSourceFactoryProbe)
{
    RefCntAutoPtr<IShaderSourceInputStreamFactory> pDefaultFactory;
    CreateDefaultShaderSourceStreamFactory("shaders/ShaderPreprocessor", &pDefaultFactory);
    ASSERT_NE(pDefaultFactory, nullptr);

    EXPECT_TRUE(pDefaultFactory->CreateInputStream("IncludeBasicTest.hlsl", nullptr));
    EXPECT_FALSE(pDefaultFactory->CreateInputStream("Missing.hlsl", nullptr));
    EXPECT_TRUE(pDefaultFactory->CreateInputStream("Nested\\Config.hlsl", nullptr));
    EXPECT_TRUE(pDefaultFactory->CreateInputStream2("IncludeBasicTest.hlsl", CREATE_SHADER_SOURCE_INPUT_STREAM_FLAG_NONE, nullptr));
    EXPECT_FALSE(pDefaultFactory->CreateInputStream2("Missing.hlsl", CREATE_SHADER_SOURCE_INPUT_STREAM_FLAG_NONE, nullptr));

    auto pMemoryFactory0 = CreateMemoryShaderSourceFactory(
        {
            {"Memory0.hlsl", "// Memory0.hlsl\n"},
            {"Directory/Memory0.hlsl", "// Directory/Memory0.hlsl\n"},
            {"C:\\Directory\\DriveMemory.hlsl", "// DriveMemory.hlsl\n"},
            {"\\\\server\\share\\NetworkMemory.hlsl", "// NetworkMemory.hlsl\n"},
        },
        false);
    auto pMemoryFactory1 = CreateMemoryShaderSourceFactory(
        {
            {"Memory1.hlsl", "// Memory1.hlsl\n"},
            {"Directory\\Memory1.hlsl", "// Directory/Memory1.hlsl\n"},
        },
        false);
    ASSERT_NE(pMemoryFactory0, nullptr);
    ASSERT_NE(pMemoryFactory1, nullptr);

    EXPECT_TRUE(pMemoryFactory0->CreateInputStream("Memory0.hlsl", nullptr));
    EXPECT_TRUE(pMemoryFactory0->CreateInputStream("Directory\\Memory0.hlsl", nullptr));
    EXPECT_TRUE(pMemoryFactory1->CreateInputStream("Directory/Memory1.hlsl", nullptr));
    EXPECT_TRUE(pMemoryFactory0->CreateInputStream("C:/Directory/DriveMemory.hlsl", nullptr));
    EXPECT_TRUE(pMemoryFactory0->CreateInputStream("//server/share/NetworkMemory.hlsl", nullptr));
    EXPECT_FALSE(pMemoryFactory0->CreateInputStream("Missing.hlsl", nullptr));
    EXPECT_TRUE(pMemoryFactory0->CreateInputStream2("Memory0.hlsl", CREATE_SHADER_SOURCE_INPUT_STREAM_FLAG_NONE, nullptr));
    EXPECT_FALSE(pMemoryFactory0->CreateInputStream2("Missing.hlsl", CREATE_SHADER_SOURCE_INPUT_STREAM_FLAG_NONE, nullptr));

    auto pCompoundFactory = CreateCompoundShaderSourceFactory({pMemoryFactory0.RawPtr(), pMemoryFactory1.RawPtr()});
    ASSERT_NE(pCompoundFactory, nullptr);

    EXPECT_TRUE(pCompoundFactory->CreateInputStream("Memory1.hlsl", nullptr));
    EXPECT_FALSE(pCompoundFactory->CreateInputStream("Missing.hlsl", nullptr));
    EXPECT_TRUE(pCompoundFactory->CreateInputStream2("Memory0.hlsl", CREATE_SHADER_SOURCE_INPUT_STREAM_FLAG_NONE, nullptr));
    EXPECT_TRUE(pCompoundFactory->CreateInputStream2("Memory1.hlsl", CREATE_SHADER_SOURCE_INPUT_STREAM_FLAG_NONE, nullptr));
    EXPECT_FALSE(pCompoundFactory->CreateInputStream2("Missing.hlsl", CREATE_SHADER_SOURCE_INPUT_STREAM_FLAG_NONE, nullptr));

    auto pSubstituteFactory = CreateCompoundShaderSourceFactory(
        {pMemoryFactory0.RawPtr(), pMemoryFactory1.RawPtr()},
        {{"Alias\\Memory.hlsl", "Directory/Memory1.hlsl"}});
    ASSERT_NE(pSubstituteFactory, nullptr);
    EXPECT_TRUE(pSubstituteFactory->CreateInputStream("Alias/Memory.hlsl", nullptr));

    RefCntAutoPtr<IFileStream> pStream;
    EXPECT_TRUE(pCompoundFactory->CreateInputStream2("Memory1.hlsl", CREATE_SHADER_SOURCE_INPUT_STREAM_FLAG_NONE, &pStream));
    EXPECT_NE(pStream, nullptr);
}

TEST(ShaderPreprocessTest, IncludeNestedParentRelative)
{
    RefCntAutoPtr<IShaderSourceInputStreamFactory> pShaderSourceFactory;
    CreateDefaultShaderSourceStreamFactory("shaders/ShaderPreprocessor", &pShaderSourceFactory);
    ASSERT_NE(pShaderSourceFactory, nullptr);

    std::deque<std::string> Includes{
        "Nested/Config.hlsl",
        "Nested/Types.hlsl",
        "IncludeNestedParentRelativeTest.hlsl"};

    ShaderCreateInfo ShaderCI{};
    ShaderCI.Desc.Name                  = "TestShader";
    ShaderCI.FilePath                   = "IncludeNestedParentRelativeTest.hlsl";
    ShaderCI.pShaderSourceStreamFactory = pShaderSourceFactory;

    const auto Result = ProcessShaderIncludes(ShaderCI, [&](const ShaderIncludePreprocessInfo& ProcessInfo) {
        ASSERT_FALSE(Includes.empty());
        EXPECT_EQ(ProcessInfo.FilePath, Includes.front());
        Includes.pop_front();
    });

    EXPECT_EQ(Result, true);
    EXPECT_TRUE(Includes.empty());
}

TEST(ShaderPreprocessTest, IncludeNestedLocalAndSystemFromMemory)
{
    auto pShaderSourceFactory = CreateMemoryShaderSourceFactory(
        {
            {"Main.hlsl",
             "// Start Main.hlsl\n"
             "#include \"Nested/LocalTypes.hlsl\"\n"
             "#include \"Nested/SystemTypes.hlsl\"\n"
             "// End Main.hlsl\n"},
            {"Nested/LocalTypes.hlsl",
             "// Start Nested/LocalTypes.hlsl\n"
             "#include \"Config.hlsl\"\n"
             "// End Nested/LocalTypes.hlsl\n"},
            {"Nested/SystemTypes.hlsl",
             "// Start Nested/SystemTypes.hlsl\n"
             "#include <Config.hlsl>\n"
             "// End Nested/SystemTypes.hlsl\n"},
            {"Nested/Config.hlsl",
             "// Start Nested/Config.hlsl\n"
             "#define CONFIG_SOURCE 2\n"
             "// End Nested/Config.hlsl\n"},
            {"Config.hlsl",
             "// Start Config.hlsl\n"
             "#define CONFIG_SOURCE 1\n"
             "// End Config.hlsl\n"},
        },
        false);
    ASSERT_NE(pShaderSourceFactory, nullptr);

    std::deque<const char*> Includes{
        "Nested/Config.hlsl",
        "Nested/LocalTypes.hlsl",
        "Config.hlsl",
        "Nested/SystemTypes.hlsl",
        "Main.hlsl"};

    ShaderCreateInfo ShaderCI{};
    ShaderCI.Desc.Name                  = "TestShader";
    ShaderCI.FilePath                   = "Main.hlsl";
    ShaderCI.pShaderSourceStreamFactory = pShaderSourceFactory;

    const auto Result = ProcessShaderIncludes(ShaderCI, [&](const ShaderIncludePreprocessInfo& ProcessInfo) {
        ASSERT_FALSE(Includes.empty());
        EXPECT_EQ(ProcessInfo.FilePath, Includes.front());
        Includes.pop_front();
    });

    EXPECT_EQ(Result, true);
    EXPECT_TRUE(Includes.empty());

    constexpr char RefString[] =
        "// Start Main.hlsl\n"
        "// Start Nested/LocalTypes.hlsl\n"
        "// Start Nested/Config.hlsl\n"
        "#define CONFIG_SOURCE 2\n"
        "// End Nested/Config.hlsl\n"
        "\n"
        "// End Nested/LocalTypes.hlsl\n"
        "\n"
        "// Start Nested/SystemTypes.hlsl\n"
        "// Start Config.hlsl\n"
        "#define CONFIG_SOURCE 1\n"
        "// End Config.hlsl\n"
        "\n"
        "// End Nested/SystemTypes.hlsl\n"
        "\n"
        "// End Main.hlsl\n";

    auto UnrolledStr = UnrollShaderIncludes(ShaderCI);
    ASSERT_EQ(RefString, UnrolledStr);
}

TEST(ShaderPreprocessTest, IncludeExcludesInvalidCandidates)
{
    auto pShaderSourceFactory = CreateMemoryShaderSourceFactory(
        {
            {"Main.hlsl", "#include \"Nested/Types.hlsl\"\n"},
            {"Nested/Types.hlsl",
             "#include \"Local.hlsl\"\n"
             "#include <System.hlsl>\n"
             "#include \"Fallback.hlsl\"\n"},
            {"Nested/Local.hlsl", "// Selected local source\n"},
            {"Local.hlsl", "#error The search path must not be used when the local source exists\n"},
            {"Nested/System.hlsl", "#error A system include must not be resolved relative to its includer\n"},
            {"System.hlsl", "// Selected system source\n"},
            {"Fallback.hlsl", "// Selected search-path fallback\n"},
        },
        false);
    ASSERT_NE(pShaderSourceFactory, nullptr);

    ShaderCreateInfo ShaderCI{};
    ShaderCI.Desc.Name                  = "TestShader";
    ShaderCI.FilePath                   = "Main.hlsl";
    ShaderCI.pShaderSourceStreamFactory = pShaderSourceFactory;

    std::deque<const char*> Includes{
        "Nested/Local.hlsl",
        "System.hlsl",
        "Fallback.hlsl",
        "Nested/Types.hlsl",
        "Main.hlsl"};

    const bool Result = ProcessShaderIncludes(ShaderCI, [&](const ShaderIncludePreprocessInfo& ProcessInfo) {
        ASSERT_FALSE(Includes.empty());
        EXPECT_EQ(ProcessInfo.FilePath, Includes.front());
        Includes.pop_front();
    });

    EXPECT_TRUE(Result);
    EXPECT_TRUE(Includes.empty());

    const std::string UnrolledSource = UnrollShaderIncludes(ShaderCI);
    EXPECT_EQ(UnrolledSource.find("#error"), std::string::npos);
    EXPECT_NE(UnrolledSource.find("Selected local source"), std::string::npos);
    EXPECT_NE(UnrolledSource.find("Selected system source"), std::string::npos);
    EXPECT_NE(UnrolledSource.find("Selected search-path fallback"), std::string::npos);
}

TEST(ShaderPreprocessTest, IncludeFallsBackWhenLocalSourceCannotBeOpened)
{
    auto pMemoryFactory = CreateMemoryShaderSourceFactory(
        {
            {"Main.hlsl",
             "// Start Main.hlsl\n"
             "#include \"Nested/Local.hlsl\"\n"
             "// End Main.hlsl\n"},
            {"Nested/Local.hlsl",
             "// Start Local.hlsl\n"
             "#include \"Config.hlsl\"\n"
             "// End Local.hlsl\n"},
            {"Nested/Config.hlsl", "#error This source exists, but cannot be opened\n"},
            {"Config.hlsl",
             "// Search path Config.hlsl\n"
             "#define CONFIG_SOURCE 1\n"},
        },
        false);
    ASSERT_NE(pMemoryFactory, nullptr);

    auto pShaderSourceFactory = ProbeOnlyShaderSourceFactory::Create(pMemoryFactory, "Nested/Config.hlsl");
    ASSERT_NE(pShaderSourceFactory, nullptr);

    ShaderCreateInfo ShaderCI{};
    ShaderCI.Desc.Name                  = "TestShader";
    ShaderCI.FilePath                   = "Main.hlsl";
    ShaderCI.pShaderSourceStreamFactory = pShaderSourceFactory;

    std::deque<const char*> Includes{
        "Config.hlsl",
        "Nested/Local.hlsl",
        "Main.hlsl"};

    const bool Result = ProcessShaderIncludes(ShaderCI, [&](const ShaderIncludePreprocessInfo& ProcessInfo) {
        ASSERT_FALSE(Includes.empty());
        EXPECT_EQ(ProcessInfo.FilePath, Includes.front());
        Includes.pop_front();
    });

    EXPECT_TRUE(Result);
    EXPECT_TRUE(Includes.empty());

    constexpr Char RefString[] =
        "// Start Main.hlsl\n"
        "// Start Local.hlsl\n"
        "// Search path Config.hlsl\n"
        "#define CONFIG_SOURCE 1\n"
        "\n"
        "// End Local.hlsl\n"
        "\n"
        "// End Main.hlsl\n";
    EXPECT_EQ(UnrollShaderIncludes(ShaderCI), RefString);
}

TEST(ShaderPreprocessTest, IncludeCanonicalPathsFromMemory)
{
    auto pShaderSourceFactory = CreateMemoryShaderSourceFactory(
        {
            {"/pkg/Main.hlsl",
             "// Start Main.hlsl\n"
             "#include \"Nested\\Types.hlsl\"\n"
             "// End Main.hlsl\n"},
            {"/pkg\\Nested/Types.hlsl",
             "// Start Types.hlsl\n"
             "#include \"Config.hlsl\"\n"
             "// End Types.hlsl\n"},
            {"/pkg/Nested\\Config.hlsl",
             "// Start Correct Config.hlsl\n"
             "#define CONFIG_SOURCE 2\n"
             "// End Correct Config.hlsl\n"},
            {"pkg/Nested/Config.hlsl",
             "// Start Decoy Config.hlsl\n"
             "#define CONFIG_SOURCE 1\n"
             "// End Decoy Config.hlsl\n"},
            {"/Main.hlsl",
             "// Start Root Main.hlsl\n"
             "#include \"Config.hlsl\"\n"
             "// End Root Main.hlsl\n"},
            {"/Config.hlsl",
             "// Start Root Config.hlsl\n"
             "#define ROOT_CONFIG_SOURCE 2\n"
             "// End Root Config.hlsl\n"},
            {"Config.hlsl",
             "// Start Decoy Root Config.hlsl\n"
             "#define ROOT_CONFIG_SOURCE 1\n"
             "// End Decoy Root Config.hlsl\n"},
            {"C:\\pkg\\Main.hlsl",
             "// Start Drive Main.hlsl\n"
             "#include \"../Config.hlsl\"\n"
             "// End Drive Main.hlsl\n"},
            {"C:/Config.hlsl",
             "// Start Drive Config.hlsl\n"
             "#define DRIVE_CONFIG_SOURCE 2\n"
             "// End Drive Config.hlsl\n"},
        },
        false);
    ASSERT_NE(pShaderSourceFactory, nullptr);

    {
        std::deque<const char*> Includes{
            "/pkg/Nested/Config.hlsl",
            "/pkg/Nested/Types.hlsl",
            "/pkg/Main.hlsl"};

        ShaderCreateInfo ShaderCI{};
        ShaderCI.Desc.Name                  = "TestShader";
        ShaderCI.FilePath                   = "/pkg/Main.hlsl";
        ShaderCI.pShaderSourceStreamFactory = pShaderSourceFactory;

        const auto Result = ProcessShaderIncludes(ShaderCI, [&](const ShaderIncludePreprocessInfo& ProcessInfo) {
            ASSERT_FALSE(Includes.empty());
            EXPECT_EQ(ProcessInfo.FilePath, Includes.front());
            Includes.pop_front();
        });

        EXPECT_EQ(Result, true);
        EXPECT_TRUE(Includes.empty());

        constexpr char RefString[] =
            "// Start Main.hlsl\n"
            "// Start Types.hlsl\n"
            "// Start Correct Config.hlsl\n"
            "#define CONFIG_SOURCE 2\n"
            "// End Correct Config.hlsl\n"
            "\n"
            "// End Types.hlsl\n"
            "\n"
            "// End Main.hlsl\n";

        EXPECT_EQ(UnrollShaderIncludes(ShaderCI), RefString);
    }

    {
        std::deque<const char*> Includes{
            "/Config.hlsl",
            "/Main.hlsl"};

        ShaderCreateInfo ShaderCI{};
        ShaderCI.Desc.Name                  = "TestShader";
        ShaderCI.FilePath                   = "/Main.hlsl";
        ShaderCI.pShaderSourceStreamFactory = pShaderSourceFactory;

        const auto Result = ProcessShaderIncludes(ShaderCI, [&](const ShaderIncludePreprocessInfo& ProcessInfo) {
            ASSERT_FALSE(Includes.empty());
            EXPECT_EQ(ProcessInfo.FilePath, Includes.front());
            Includes.pop_front();
        });

        EXPECT_EQ(Result, true);
        EXPECT_TRUE(Includes.empty());

        constexpr char RefString[] =
            "// Start Root Main.hlsl\n"
            "// Start Root Config.hlsl\n"
            "#define ROOT_CONFIG_SOURCE 2\n"
            "// End Root Config.hlsl\n"
            "\n"
            "// End Root Main.hlsl\n";

        EXPECT_EQ(UnrollShaderIncludes(ShaderCI), RefString);
    }

    {
        std::deque<const char*> Includes{
            "C:/Config.hlsl",
            "C:/pkg/Main.hlsl"};

        ShaderCreateInfo ShaderCI{};
        ShaderCI.Desc.Name                  = "TestShader";
        ShaderCI.FilePath                   = "C:\\pkg\\Main.hlsl";
        ShaderCI.pShaderSourceStreamFactory = pShaderSourceFactory;

        const auto Result = ProcessShaderIncludes(ShaderCI, [&](const ShaderIncludePreprocessInfo& ProcessInfo) {
            ASSERT_FALSE(Includes.empty());
            EXPECT_EQ(ProcessInfo.FilePath, Includes.front());
            Includes.pop_front();
        });

        EXPECT_EQ(Result, true);
        EXPECT_TRUE(Includes.empty());

        constexpr char RefString[] =
            "// Start Drive Main.hlsl\n"
            "// Start Drive Config.hlsl\n"
            "#define DRIVE_CONFIG_SOURCE 2\n"
            "// End Drive Config.hlsl\n"
            "\n"
            "// End Drive Main.hlsl\n";

        EXPECT_EQ(UnrollShaderIncludes(ShaderCI), RefString);
    }
}

TEST(ShaderPreprocessTest, InvalidInclude)
{
    RefCntAutoPtr<IShaderSourceInputStreamFactory> pShaderSourceFactory;
    CreateDefaultShaderSourceStreamFactory("shaders/ShaderPreprocessor", &pShaderSourceFactory);
    ASSERT_NE(pShaderSourceFactory, nullptr);

    constexpr size_t TestCount = 12;
    for (size_t TestId = 0; TestId < TestCount; ++TestId)
    {
        String FilePath = "IncludeInvalidCase" + std::to_string(TestId) + ".hlsl";

        ShaderCreateInfo ShaderCI{};
        ShaderCI.Desc.Name                  = "TestShader";
        ShaderCI.FilePath                   = FilePath.c_str();
        ShaderCI.pShaderSourceStreamFactory = pShaderSourceFactory;

        TestingEnvironment::ErrorScope ExpectedErrors{String{"Failed to process includes in file '"} + FilePath + "'"};

        const auto Result = ProcessShaderIncludes(ShaderCI, {});
        EXPECT_EQ(Result, false);
    }
}

TEST(ShaderPreprocessTest, UnrollIncludes)
{
    RefCntAutoPtr<IShaderSourceInputStreamFactory> pShaderSourceFactory;
    CreateDefaultShaderSourceStreamFactory("shaders/ShaderPreprocessor", &pShaderSourceFactory);
    ASSERT_NE(pShaderSourceFactory, nullptr);

    {
        ShaderCreateInfo ShaderCI{};
        ShaderCI.Desc.Name                  = "TestShader";
        ShaderCI.FilePath                   = "InlineIncludeShaderTest.hlsl";
        ShaderCI.pShaderSourceStreamFactory = pShaderSourceFactory;

        constexpr char RefString[] =
            "// Start InlineIncludeShaderTest.hlsl\n"
            "// Start InlineIncludeShaderCommon1.hlsl\n"
            "// #include \"InlineIncludeShaderCommon0.hlsl\"\n"
            "\n"
            "#define MACRO\n"
            "// End InlineIncludeShaderCommon1.hlsl\n"
            "\n"
            "// Start InlineIncludeShaderCommon2.hlsl\n"
            "\n"
            "\n"
            "\n"
            "// End InlineIncludeShaderCommon2.hlsl\n"
            "\n"
            "\n"
            "\n"
            "\n"
            "// End InlineIncludeShaderTest.hlsl\n";

        auto UnrolledStr = UnrollShaderIncludes(ShaderCI);
        ASSERT_EQ(RefString, UnrolledStr);
    }

    {
        ShaderCreateInfo ShaderCI{};
        ShaderCI.Desc.Name                  = "TestShader";
        ShaderCI.FilePath                   = "IncludeNestedParentRelativeTest.hlsl";
        ShaderCI.pShaderSourceStreamFactory = pShaderSourceFactory;

        constexpr char RefString[] =
            "// Start IncludeNestedParentRelativeTest.hlsl\n"
            "// Start Nested/Types.hlsl\n"
            "// Start Nested/Config.hlsl\n"
            "#define NESTED_CONFIG_VALUE 1\n"
            "// End Nested/Config.hlsl\n"
            "\n"
            "// End Nested/Types.hlsl\n"
            "\n"
            "// End IncludeNestedParentRelativeTest.hlsl\n";

        auto UnrolledStr = UnrollShaderIncludes(ShaderCI);
        ASSERT_EQ(RefString, UnrolledStr);
    }
}

TEST(ShaderPreprocessTest, ShaderSourceLanguageDefiniton)
{
    EXPECT_EQ(ParseShaderSourceLanguageDefinition(""), SHADER_SOURCE_LANGUAGE_DEFAULT);
    EXPECT_EQ(ParseShaderSourceLanguageDefinition("abc"), SHADER_SOURCE_LANGUAGE_DEFAULT);
    EXPECT_EQ(ParseShaderSourceLanguageDefinition("/"), SHADER_SOURCE_LANGUAGE_DEFAULT);
    EXPECT_EQ(ParseShaderSourceLanguageDefinition("*/"), SHADER_SOURCE_LANGUAGE_DEFAULT);
    EXPECT_EQ(ParseShaderSourceLanguageDefinition("**/"), SHADER_SOURCE_LANGUAGE_DEFAULT);
    EXPECT_EQ(ParseShaderSourceLanguageDefinition("/*/"), SHADER_SOURCE_LANGUAGE_DEFAULT);
    EXPECT_EQ(ParseShaderSourceLanguageDefinition("abc*/"), SHADER_SOURCE_LANGUAGE_DEFAULT);
    EXPECT_EQ(ParseShaderSourceLanguageDefinition("*abc*/"), SHADER_SOURCE_LANGUAGE_DEFAULT);
    EXPECT_EQ(ParseShaderSourceLanguageDefinition("/*abc*/"), SHADER_SOURCE_LANGUAGE_DEFAULT);
    EXPECT_EQ(ParseShaderSourceLanguageDefinition("/**/"), SHADER_SOURCE_LANGUAGE_DEFAULT);
    EXPECT_EQ(ParseShaderSourceLanguageDefinition("/****/"), SHADER_SOURCE_LANGUAGE_DEFAULT);

    EXPECT_EQ(ParseShaderSourceLanguageDefinition("/*$*/"), SHADER_SOURCE_LANGUAGE_DEFAULT);
    EXPECT_EQ(ParseShaderSourceLanguageDefinition("/*$SHADER_SOURCE_LANG*/"), SHADER_SOURCE_LANGUAGE_DEFAULT);
    EXPECT_EQ(ParseShaderSourceLanguageDefinition("/*$SHADER_SOURCE_LANG=1*/"), SHADER_SOURCE_LANGUAGE_DEFAULT);
    EXPECT_EQ(ParseShaderSourceLanguageDefinition("/*$SHADER_SOURCE_LANGUAGE*/"), SHADER_SOURCE_LANGUAGE_DEFAULT);
    EXPECT_EQ(ParseShaderSourceLanguageDefinition("/*$SHADER_SOURCE_LANGUAGE   */"), SHADER_SOURCE_LANGUAGE_DEFAULT);
    EXPECT_EQ(ParseShaderSourceLanguageDefinition("/*$SHADER_SOURCE_LANGUAGEx*/"), SHADER_SOURCE_LANGUAGE_DEFAULT);
    EXPECT_EQ(ParseShaderSourceLanguageDefinition("/*$SHADER_SOURCE_LANGUAGE   x*/"), SHADER_SOURCE_LANGUAGE_DEFAULT);
    EXPECT_EQ(ParseShaderSourceLanguageDefinition("/*$SHADER_SOURCE_LANGUAGE=*/"), SHADER_SOURCE_LANGUAGE_DEFAULT);
    EXPECT_EQ(ParseShaderSourceLanguageDefinition("/*$SHADER_SOURCE_LANGUAGE=   */"), SHADER_SOURCE_LANGUAGE_DEFAULT);
    EXPECT_EQ(ParseShaderSourceLanguageDefinition("/*$SHADER_SOURCE_LANGUAGE   =*/"), SHADER_SOURCE_LANGUAGE_DEFAULT);
    EXPECT_EQ(ParseShaderSourceLanguageDefinition("/*$SHADER_SOURCE_LANGUAGE=X*/"), SHADER_SOURCE_LANGUAGE_DEFAULT);
    EXPECT_EQ(ParseShaderSourceLanguageDefinition("/*$SHADER_SOURCE_LANGUAGE = X*/"), SHADER_SOURCE_LANGUAGE_DEFAULT);
    EXPECT_EQ(ParseShaderSourceLanguageDefinition("/*$SHADER_SOURCE_LANGUAGE  =   2*/"), SHADER_SOURCE_LANGUAGE_GLSL);
    EXPECT_EQ(ParseShaderSourceLanguageDefinition("/*  $SHADER_SOURCE_LANGUAGE  =   2  */"), SHADER_SOURCE_LANGUAGE_GLSL);
    EXPECT_EQ(ParseShaderSourceLanguageDefinition("/**$SHADER_SOURCE_LANGUAGE  =   3**/"), SHADER_SOURCE_LANGUAGE_GLSL_VERBATIM);
    EXPECT_EQ(ParseShaderSourceLanguageDefinition("/***$SHADER_SOURCE_LANGUAGE  =   3***/"), SHADER_SOURCE_LANGUAGE_GLSL_VERBATIM);
    EXPECT_EQ(ParseShaderSourceLanguageDefinition("/*$SHADER_SOURCE_LANGUAGE=9*/"), SHADER_SOURCE_LANGUAGE_DEFAULT);
    EXPECT_EQ(ParseShaderSourceLanguageDefinition("/*$SHADER_SOURCE_LANGUAGE=11*/"), SHADER_SOURCE_LANGUAGE_DEFAULT);

    for (Uint32 lang = SHADER_SOURCE_LANGUAGE_DEFAULT; lang < SHADER_SOURCE_LANGUAGE_COUNT; ++lang)
    {
        const auto Lang = static_cast<SHADER_SOURCE_LANGUAGE>(lang);
        {
            std::string Source;
            AppendShaderSourceLanguageDefinition(Source, Lang);
            EXPECT_EQ(ParseShaderSourceLanguageDefinition(Source), Lang);
        }
    }
}

} // namespace
