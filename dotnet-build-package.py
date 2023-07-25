# ----------------------------------------------------------------------------
# Copyright 2019-2023 Diligent Graphics LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# In no event and under no legal theory, whether in tort (including negligence),
# contract, or otherwise, unless required by applicable law (such as deliberate
# and grossly negligent acts) or agreed to in writing, shall any Contributor be
# liable for any damages, including any direct, indirect, special, incidental,
# or consequential damages of any character arising as a result of this License or
# out of the use or inability to use the software (including but not limited to damages
# for loss of goodwill, work stoppage, computer failure or malfunction, or any and
# all other commercial damages or losses), even if such Contributor has been advised
# of the possibility of such damages.
# ----------------------------------------------------------------------------

import subprocess
import os
import sys
import shutil
import pathlib
import xml.dom.minidom as minidom

from argparse import ArgumentParser, BooleanOptionalAction
from enum import Enum

class BuildType(Enum):
    Debug = 'Debug'
    Release = 'Release'

    def __str__(self):
        return self.value

config_windows = {
    "x86": { "platform": "Win32", "build-folder": "Win32", "dotnet-folder": "win-x86" },
    "x64": { "platform": "x64",   "build-folder": "Win64", "dotnet-folder": "win-x64" }
}

project_paths = {
    "dotnet-proj": "./Graphics/GraphicsEngine.NET",
    "dotnet-test": "./Tests/DiligentCoreTest.NET",
    "dotnet-build": "./build/.NET",
    "gpu-test-assets": "./Tests/DiligentCoreAPITest/assets"
}

gpu_tests_gapi = ["d3d11", "d3d12"]

gpu_test_list = [
    "GenerateImagesDotNetTest.GenerateCubeTexture",
    "GenerateArhiveDotNetTest.GenerateCubeArchive"
]

def cmake_build_project(config, arch):
    subprocess.run(f"cmake -S . -B ./build/{arch['build-folder']} \
            -D CMAKE_BUILD_TYPE={config} \
            -D CMAKE_INSTALL_PREFIX=./build/{arch['build-folder']}/install -A {arch['platform']} \
            -D DILIGENT_BUILD_CORE_TESTS=ON", check=True)
    subprocess.run(f"cmake --build ./build/{arch['build-folder']} --target install --config {config}", check=True)

    native_dll_path = f"{project_paths['dotnet-build']}/{project_paths['dotnet-proj']}/native/{arch['dotnet-folder']}"
    pathlib.Path(native_dll_path).mkdir(parents=True, exist_ok=True)
    for filename in os.listdir(native_dll_path):
        os.remove(os.path.join(native_dll_path, filename))

    for filename in os.listdir(f"./build/{arch['build-folder']}/install/bin/{config}"):
        shutil.copy(f"./build/{arch['build-folder']}/install/bin/{config}/{filename}", native_dll_path)
   
    for filename in os.listdir(native_dll_path):
        arguments = filename.split(".");
        dll_name = arguments[0].split("_")[0]
        src = f"{native_dll_path}/{filename}"
        dst = f"{native_dll_path}/{dll_name}.{arguments[-1]}"
        if not os.path.exists(dst):
            os.rename(src, dst)

def get_latest_tag_without_prefix():
    try:
        output = subprocess.check_output(['git', 'tag', '--list', 'v*', '--sort=-v:refname'], encoding='utf-8')
        tags = output.strip().split('\n')
        latest_tag = tags[0][1:] if tags else None
    except subprocess.CalledProcessError as exc:
        raise Exception(exc.output)
    return latest_tag

def dotnet_generate_version(path, is_local=True):
    version_str = get_latest_tag_without_prefix()
    if is_local:
        version_str += "-local"

    doc = minidom.Document()
    root = doc.createElement('Project')
    doc.appendChild(root)
    property_group = doc.createElement('PropertyGroup')
    root.appendChild(property_group)
    version = doc.createElement('PackageGitVersion')
    version_text = doc.createTextNode(version_str)
    version.appendChild(version_text)
    property_group.appendChild(version)
    xml_string = doc.toprettyxml(indent='\t')
    xml_string_without_declaration = '\n'.join(xml_string.split('\n')[1:])

    pathlib.Path(path).mkdir(parents=True, exist_ok=True)
    with open(f"{path}/Version.props", 'w') as file:
        file.write(xml_string_without_declaration)

def dotnet_pack_project(config, is_local):
    dotnet_generate_version(f"{project_paths['dotnet-build']}/{project_paths['dotnet-proj']}", is_local)
    subprocess.run(f"dotnet build -c {config} -p:Platform=x86 {project_paths['dotnet-proj']}", check=True)
    subprocess.run(f"dotnet build -c {config} -p:Platform=x64 {project_paths['dotnet-proj']}", check=True)
    subprocess.run(f"dotnet pack -c {config} {project_paths['dotnet-proj']}", check=True)

def dotnet_run_native_tests(config, arch, gapi):
    for test_name in gpu_test_list:
        subprocess.run(f"./build/{config_windows[arch]['build-folder']}/Tests/DiligentCoreAPITest/{config}/DiligentCoreAPITest.exe \
                    --mode={gapi} \
                    --gtest_filter={test_name}", cwd=project_paths['gpu-test-assets'], check=True)

def dotnet_run_managed_tests(config, arch, gapi):
    os.environ["DILIGENT_GAPI"] = gapi
    subprocess.run(f"dotnet test -c {config} -p:Platform={arch} {project_paths['dotnet-test']}", check=True)

def dotnet_copy_assets(src_dir, dst_dir):
    pathlib.Path(dst_dir).mkdir(parents=True, exist_ok=True)
    for root, dirs, files in os.walk(src_dir):
        for file in files:
            if "DotNet" in file:
                src_path = os.path.join(root, file)
                dst_path = os.path.join(dst_dir, file)
                shutil.copy2(src_path, dst_path)

def dotnet_copy_nuget_package(src_dir, dst_dir):
     pathlib.Path(dst_dir).mkdir(parents=True, exist_ok=True)
     for filename in os.listdir(src_dir):
        if filename.endswith(".nupkg"):
            shutil.copy(f"{src_dir}/{filename}", dst_dir)

def dotnet_restore_nuget_package(config, src_dir):
    shutil.rmtree(f"{src_dir}/RestoredPackages", ignore_errors=True) 
    subprocess.run(f"dotnet restore --no-cache {src_dir}", check=True)

def dotnet_build_and_run_tests(config, arch):
    dotnet_generate_version(f"{project_paths['dotnet-build']}/{project_paths['dotnet-test']}")
    dotnet_copy_nuget_package(f"{project_paths['dotnet-build']}/{project_paths['dotnet-proj']}/bin/{config}", f"{project_paths['dotnet-build']}/{project_paths['dotnet-test']}/LocalPackages")
    dotnet_restore_nuget_package(config, f"{project_paths['dotnet-test']}")

    for gapi in gpu_tests_gapi:
        dotnet_run_native_tests(config, arch, gapi)
        dotnet_copy_assets(f"{project_paths['gpu-test-assets']}", f"{project_paths['dotnet-build']}/{project_paths['dotnet-test']}/assets")
        dotnet_run_managed_tests(config, arch, gapi)

def free_disk_memory(config, arch):
    build_folder = f"./build/{arch['build-folder']}"
    exceptions = [
        os.path.join(build_folder, "Tests"),
        os.path.join(build_folder, "install")
    ]
    for item in os.listdir(build_folder):
        item_path = os.path.join(build_folder, item)
        if item_path not in exceptions:
            if os.path.isfile(item_path):
                os.remove(item_path)
            elif os.path.isdir(item_path):
                shutil.rmtree(item_path)

def main():
    parser = ArgumentParser("Build NuGet package")
    parser.add_argument("-c","--configuration",
                        type=BuildType, choices=list(BuildType),
                        required=True)

    group = parser.add_mutually_exclusive_group()

    group.add_argument('--dotnet-tests',
        action=BooleanOptionalAction,
        help="Use this flag to build and run .NET tests")

    group.add_argument('--dotnet-publish',
        action=BooleanOptionalAction,
        help="Use this flag to publish nuget package")

    parser.add_argument('--free-memory', 
        action=BooleanOptionalAction,
        help="Use this flag if there is not enough disk space to build this project")
    args = parser.parse_args()

    for arch in config_windows.keys():
        cmake_build_project(args.configuration, config_windows[arch])

    if (args.free_memory):
        for arch in config_windows.keys():
            free_disk_memory(args.configuration, config_windows[arch])

    if args.dotnet_tests:
        dotnet_pack_project(args.configuration, True)
        for arch in config_windows.keys():
            dotnet_build_and_run_tests(args.configuration, arch)
    elif args.dotnet_publish:
        dotnet_pack_project(args.configuration, False)

if __name__ == "__main__":
    main()
