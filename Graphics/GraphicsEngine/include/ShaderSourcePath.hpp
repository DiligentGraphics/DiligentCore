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

#include <utility>

#include "BasicFileSystem.hpp"
#include "RefCntAutoPtr.hpp"
#include "Shader.h"

namespace Diligent
{

inline String NormalizeShaderSourcePath(const Char* Path, Char Slash = BasicFileSystem::UnixSlash)
{
    if (Path == nullptr || Path[0] == '\0')
        return {};

    if (Slash == 0)
        Slash = BasicFileSystem::SlashSymbol;

    // Simplify drive and UNC paths using Windows semantics to preserve their roots.
    // By default, shader source names use '/' as the canonical separator.
    const PathRootType RootType = BasicFileSystem::GetPathRootType(Path);

    const Char SimplifySlash =
        RootType == PathRootType::WindowsDrive || RootType == PathRootType::WindowsUNC ?
        BasicFileSystem::WinSlash :
        BasicFileSystem::UnixSlash;

    String NormalizedPath = BasicFileSystem::SimplifyPath(Path, SimplifySlash);
    if (SimplifySlash != Slash)
        BasicFileSystem::CorrectSlashes(NormalizedPath, Slash);
    return NormalizedPath;
}

struct ShaderIncludePathCandidates
{
    /// Path to try during local include lookup. This is either relative to the
    /// including source or a rooted local include path.
    ///
    /// Examples:
    /// - `Shaders/Main.hlsl` including `"Config.hlsl"` produces `Shaders/Config.hlsl`.
    /// - `Shaders/Main.hlsl` including `"/Config.hlsl"` produces `/Config.hlsl`.
    /// - A system include or a local include without a parent produces an empty path.
    String LocalPath;

    /// Normalized include path used for system or search-directory lookup.
    ///
    /// Examples:
    /// - `Shaders/Main.hlsl` including `"Config.hlsl"` produces `Config.hlsl` as
    ///   the fallback after `LocalPath`.
    /// - `Shaders/Main.hlsl` including `<Config.hlsl>` produces `Config.hlsl`.
    /// - `Main.hlsl` including `"Config.hlsl"` produces `Config.hlsl` because the
    ///   includer has no parent directory.
    /// - A rooted local include produces an empty path because `LocalPath` contains
    ///   the only candidate.
    String SearchPath;
};

/// Returns normalized paths that may resolve an include.
///
/// A relative local include produces a local path relative to the including
/// source and the original include path for search-directory fallback. System
/// includes and local includes without a parent only produce a search path.
/// Rooted local includes only produce a local path.
///
/// Examples:
/// - `Shaders/Main.hlsl` + `"Config.hlsl"` ->
///   `{LocalPath: "Shaders/Config.hlsl", SearchPath: "Config.hlsl"}`
/// - `Shaders/Nested/Main.hlsl` + `"../Config.hlsl"` ->
///   `{LocalPath: "Shaders/Config.hlsl", SearchPath: "../Config.hlsl"}`
/// - `Main.hlsl` + `"Config.hlsl"` ->
///   `{LocalPath: "", SearchPath: "Config.hlsl"}`
/// - `Shaders/Main.hlsl` + `<Config.hlsl>` ->
///   `{LocalPath: "", SearchPath: "Config.hlsl"}`
/// - `Shaders/Main.hlsl` + `"/Config.hlsl"` ->
///   `{LocalPath: "/Config.hlsl", SearchPath: ""}`
/// - `Shaders/Main.hlsl` + `</Config.hlsl>` ->
///   `{LocalPath: "", SearchPath: "/Config.hlsl"}`
inline ShaderIncludePathCandidates GetShaderIncludePathCandidates(const Char* IncluderPath,
                                                                  const Char* IncludeName,
                                                                  bool        IsLocalInclude)
{
    ShaderIncludePathCandidates Candidates;

    String NormalizedIncludePath = NormalizeShaderSourcePath(IncludeName);
    if (!NormalizedIncludePath.empty())
    {
        const bool IsRootedInclude = BasicFileSystem::GetPathRootType(NormalizedIncludePath.c_str()) != PathRootType::None;
        const bool IsRelativeLocalInclude =
            IsLocalInclude &&
            IncluderPath != nullptr &&
            IncluderPath[0] != '\0' &&
            !IsRootedInclude;

        if (IsRelativeLocalInclude)
        {
            const String NormalizedIncluderPath = NormalizeShaderSourcePath(IncluderPath);
            if (!NormalizedIncluderPath.empty())
            {
                String ParentDir;
                BasicFileSystem::GetPathComponents(NormalizedIncluderPath, &ParentDir, nullptr);

                // GetPathComponents() returns an empty directory for a file in the Unix root
                // (e.g. "/Main.glsl"). Restore the root so local includes remain absolute.
                if (ParentDir.empty() && NormalizedIncluderPath.front() == BasicFileSystem::UnixSlash)
                    ParentDir.push_back(BasicFileSystem::UnixSlash);

                if (!ParentDir.empty())
                {
                    const std::string ParentRelativePath = BasicFileSystem::JoinPath(ParentDir, NormalizedIncludePath, BasicFileSystem::UnixSlash);
                    Candidates.LocalPath                 = NormalizeShaderSourcePath(ParentRelativePath.c_str());
                }
            }
        }
        else if (IsLocalInclude && IsRootedInclude)
        {
            Candidates.LocalPath = NormalizedIncludePath;
        }

        if (Candidates.LocalPath != NormalizedIncludePath)
            Candidates.SearchPath = std::move(NormalizedIncludePath);
    }

    return Candidates;
}

struct OpenShaderIncludeResult
{
    RefCntAutoPtr<IFileStream> pStream;
    String                     FilePath;

    explicit operator bool() const noexcept
    {
        return pStream != nullptr;
    }
};

/// Tries to open an include using the local path first and the search path as fallback.
inline OpenShaderIncludeResult OpenShaderInclude(ShaderIncludePathCandidates      Candidates,
                                                 IShaderSourceInputStreamFactory* pStreamFactory)
{
    OpenShaderIncludeResult Result;

    if (pStreamFactory != nullptr)
    {
        if (!Candidates.LocalPath.empty())
        {
            const CREATE_SHADER_SOURCE_INPUT_STREAM_FLAGS Flags = Candidates.SearchPath.empty() ?
                CREATE_SHADER_SOURCE_INPUT_STREAM_FLAG_NONE :
                CREATE_SHADER_SOURCE_INPUT_STREAM_FLAG_SILENT;
            pStreamFactory->CreateInputStream2(Candidates.LocalPath.c_str(), Flags, &Result.pStream);
            if (Result.pStream != nullptr)
                Result.FilePath = std::move(Candidates.LocalPath);
        }

        if (Result.pStream == nullptr && !Candidates.SearchPath.empty())
        {
            pStreamFactory->CreateInputStream(Candidates.SearchPath.c_str(), &Result.pStream);
            if (Result.pStream != nullptr)
                Result.FilePath = std::move(Candidates.SearchPath);
        }
    }

    return Result;
}

} // namespace Diligent
