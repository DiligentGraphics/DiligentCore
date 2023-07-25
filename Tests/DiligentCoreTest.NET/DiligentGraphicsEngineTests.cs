/*
 *  Copyright 2019-2023 Diligent Graphics LLC
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

using Diligent;
using System;
using System.Linq;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Drawing;
using System.Drawing.Imaging;
using System.Windows.Forms;
using System.IO;
using System.Numerics;
using System.Runtime.CompilerServices;
using Xunit;
using Xunit.Abstractions;

using IDeviceContext = Diligent.IDeviceContext;

namespace DiligentCoreTest.NET;

public class DiligentGraphicsEngineTests : IDisposable
{
    struct Vertex
    {
        public Vector3 Position;
        public Vector4 Color;

        public Vertex(Vector3 position, Vector4 color)
        {
            Position = position;
            Color = color;
        }
    }

    private const string PathToAssets = "./assets";
    private const int ContentVersion = 1234;

    private readonly ITestOutputHelper m_Output;
    private readonly Form m_Window;
    private readonly IEngineFactory m_EngineFactory;
    private readonly IRenderDevice m_RenderDevice;
    private readonly IDeviceContext m_DeviceContext;
    private readonly ISwapChain m_SwapChain;
    private readonly SwapChainDesc m_SwapChainDesc = new()
    {
        ColorBufferFormat = TextureFormat.RGBA8_UNorm,
        DepthBufferFormat = TextureFormat.D32_Float,
        Width = 512,
        Height = 512,
    };

    public DiligentGraphicsEngineTests(ITestOutputHelper output)
    {
        m_Output = output;
        m_Window = new Form();
        Assert.NotNull(m_Window);

        var graphicsApiEnvVariable = Environment.GetEnvironmentVariable("DILIGENT_GAPI");
        m_EngineFactory = graphicsApiEnvVariable switch
        {
            "d3d11" => Native.CreateEngineFactory<IEngineFactoryD3D11>(),
            "d3d12" => Native.CreateEngineFactory<IEngineFactoryD3D12>(),
            "vk" => Native.CreateEngineFactory<IEngineFactoryVk>(),
            "gl" => Native.CreateEngineFactory<IEngineFactoryOpenGL>(),
            _ => throw new ArgumentOutOfRangeException($"Not expected graphics API: {graphicsApiEnvVariable}")
        };
        Assert.NotNull(m_EngineFactory);

        m_EngineFactory.SetMessageCallback((severity, message, function, file, line) =>
        {
            switch (severity)
            {
                case DebugMessageSeverity.Warning:
                case DebugMessageSeverity.Error:
                case DebugMessageSeverity.FatalError:
                    m_Output.WriteLine($"Diligent Engine: {severity} in {function}() ({file}, {line}): {message}");
                    break;
                case DebugMessageSeverity.Info:
                    m_Output.WriteLine($"Diligent Engine: {severity} {message}");
                    break;
                default:
                    throw new ArgumentOutOfRangeException(nameof(severity), severity, null);
            }
        });

        ISwapChain swapChainOut;
        IRenderDevice renderDeviceOut;
        IDeviceContext[] contextsOut;
        switch (m_EngineFactory)
        {
            case IEngineFactoryD3D11 engineFactoryD3D11:
                engineFactoryD3D11.CreateDeviceAndContextsD3D11(new()
                {
                    EnableValidation = true
                }, out renderDeviceOut, out contextsOut);
                swapChainOut = engineFactoryD3D11.CreateSwapChainD3D11(renderDeviceOut, contextsOut[0], m_SwapChainDesc, new(), new() { Wnd = m_Window.Handle });
                break;
            case IEngineFactoryD3D12 engineFactoryD3D12:
                engineFactoryD3D12.CreateDeviceAndContextsD3D12(new()
                {
                    EnableValidation = true
                }, out renderDeviceOut, out contextsOut);
                swapChainOut = engineFactoryD3D12.CreateSwapChainD3D12(renderDeviceOut, contextsOut[0], m_SwapChainDesc, new(), new() { Wnd = m_Window.Handle });
                break;
            case IEngineFactoryVk engineFactoryVk:
                engineFactoryVk.CreateDeviceAndContextsVk(new()
                {
                    EnableValidation = true
                }, out renderDeviceOut, out contextsOut);
                swapChainOut = engineFactoryVk.CreateSwapChainVk(renderDeviceOut, contextsOut[0], m_SwapChainDesc, new() { Wnd = m_Window.Handle });
                break;
            case IEngineFactoryOpenGL engineFactoryOpenGL:
                engineFactoryOpenGL.CreateDeviceAndSwapChainGL(new()
                {
                    EnableValidation = true,
                    Window = new() { Wnd = m_Window.Handle }
                }, out renderDeviceOut, out var glContext, m_SwapChainDesc, out swapChainOut);
                contextsOut = glContext != null ? new[] { glContext } : null;
                break;
            default:
                throw new Exception("Unexpected exception");
        }

        Assert.NotNull(renderDeviceOut);
        Assert.NotNull(contextsOut);
        Assert.NotNull(swapChainOut);

        m_RenderDevice = renderDeviceOut;
        m_DeviceContext = contextsOut[0];
        m_SwapChain = swapChainOut;
    }

    private static Type FindType(string qualifiedTypeName)
    {
        var type = Type.GetType(qualifiedTypeName);

        if (type != null)
            return type;

        foreach (var assembly in AppDomain.CurrentDomain.GetAssemblies())
        {
            type = assembly.GetType(qualifiedTypeName);
            if (type != null)
                return type;
        }
        return null;
    }

    private static void CheckStructSizes(in APIInfo info)
    {
        nuint GetManagedSize(Type type)
        {
            var native = Array.Find(type.GetNestedTypes(BindingFlags.NonPublic), (e) => e.Name == "__Native");
            return (nuint)Marshal.SizeOf(native ?? type);
        }

        var apiInfoType = typeof(APIInfo);
        var fields = apiInfoType.GetFields();

        foreach (var field in fields.Skip(2))
        {
            var type = FindType($"Diligent.{field.Name.Replace("Size", "")}");
            var nativeSize = (nuint)field.GetValue(info)!;
            var managedSize = GetManagedSize(type);
            Assert.True(nativeSize == managedSize, $"{type.Name}: {nativeSize} -> {managedSize}");
        }
    }
    private static (byte[], int) LoadImage(string fileName)
    {
        var bitmap = new Bitmap(fileName);
        var bitmapData = bitmap.LockBits(new Rectangle(0, 0, bitmap.Width, bitmap.Height),
            ImageLockMode.ReadOnly,
            PixelFormat.Format24bppRgb);

        var totalBytes = bitmapData.Stride * bitmapData.Height;
        var pixelData = new byte[totalBytes];
        Marshal.Copy(bitmapData.Scan0, pixelData, 0, totalBytes);
        bitmap.UnlockBits(bitmapData);
        return (pixelData, bitmapData.Stride);
    }

    private static void SaveImage(string fileName, byte[] data, int width, int height)
    {
        var bitmap = new Bitmap(width, height, PixelFormat.Format24bppRgb);
        var bitmapData = bitmap.LockBits(new Rectangle(0, 0, bitmap.Width, bitmap.Height),
            ImageLockMode.WriteOnly,
            PixelFormat.Format24bppRgb);
        Marshal.Copy(data, 0, bitmapData.Scan0, bitmapData.Stride * height);
        bitmap.UnlockBits(bitmapData);
        bitmap.Save(fileName, ImageFormat.Png);
    }

    private void CompareImages(MappedTextureSubresource mappedResource, string fileName, bool isOpenGL)
    {
        var (imageData, refStride) = LoadImage(fileName);
        var renderTargetWidth = (int)m_SwapChainDesc.Width;
        var renderTargetHeight = (int)m_SwapChainDesc.Height;
        unsafe
        {
            var pData = (byte*)mappedResource.Data.ToPointer();
            var isEqual = true;
            var pixelCount = 0;

            for (var y = 0; y < renderTargetHeight; y++)
            {
                for (var x = 0; x < renderTargetWidth; x++)
                {
                    var flipCoord = isOpenGL ? (renderTargetHeight - 1 - y) : y;

                    if (pData[(int)mappedResource.Stride * flipCoord + 4 * x + 2] != imageData[refStride * y + 3 * x + 0] ||
                        pData[(int)mappedResource.Stride * flipCoord + 4 * x + 1] != imageData[refStride * y + 3 * x + 1] ||
                        pData[(int)mappedResource.Stride * flipCoord + 4 * x + 0] != imageData[refStride * y + 3 * x + 2])
                    {
                        isEqual = false;
                        pixelCount++;
                    }
                }
            }

            if (!isEqual)
            {
                var dumpStride = 2 * 3 * renderTargetWidth;
                var combinedImage = new byte[renderTargetHeight * dumpStride];
                for (var y = 0; y < renderTargetHeight; y++)
                {
                    for (var x = 0; x < renderTargetWidth; x++)
                    {
                        var flipCoord = isOpenGL ? (renderTargetHeight - 1 - y) : y;

                        combinedImage[dumpStride * y + 3 * x + 0] = imageData[refStride * y + 3 * x + 0];
                        combinedImage[dumpStride * y + 3 * x + 1] = imageData[refStride * y + 3 * x + 1];
                        combinedImage[dumpStride * y + 3 * x + 2] = imageData[refStride * y + 3 * x + 2];

                        combinedImage[dumpStride * y + 3 * (x + renderTargetWidth - 1) + 0] = pData[(int)mappedResource.Stride * flipCoord + 4 * x + 2];
                        combinedImage[dumpStride * y + 3 * (x + renderTargetWidth - 1) + 1] = pData[(int)mappedResource.Stride * flipCoord + 4 * x + 1];
                        combinedImage[dumpStride * y + 3 * (x + renderTargetWidth - 1) + 2] = pData[(int)mappedResource.Stride * flipCoord + 4 * x + 0];
                    }
                }

                var dumpFileName = Path.GetFileNameWithoutExtension(fileName) + "_Dump" + Path.GetExtension(fileName);
                SaveImage(dumpFileName, combinedImage, 2 * renderTargetWidth, renderTargetHeight);
                Assert.True(false, $"The rendered image will not match with {fileName}; PixelCount: {pixelCount}");
            }
        }
    }

    private static Matrix4x4 CreatePerspectiveFieldOfView(float fieldOfView, float aspectRatio, float nearPlaneDistance, float farPlaneDistance, bool isOpenGL)
    {
        if (fieldOfView <= 0.0f || fieldOfView >= MathF.PI)
            throw new ArgumentOutOfRangeException(nameof(fieldOfView));

        if (nearPlaneDistance <= 0.0f)
            throw new ArgumentOutOfRangeException(nameof(nearPlaneDistance));

        if (farPlaneDistance <= 0.0f)
            throw new ArgumentOutOfRangeException(nameof(farPlaneDistance));

        if (nearPlaneDistance >= farPlaneDistance)
            throw new ArgumentOutOfRangeException(nameof(nearPlaneDistance));

        float yScale = 1.0f / MathF.Tan(fieldOfView * 0.5f);
        float xScale = yScale / aspectRatio;

        Matrix4x4 result = new()
        {
            M11 = xScale,
            M22 = yScale
        };

        if (isOpenGL)
        {
            result.M33 = (farPlaneDistance + nearPlaneDistance) / (farPlaneDistance - nearPlaneDistance);
            result.M43 = -2 * nearPlaneDistance * farPlaneDistance / (farPlaneDistance - nearPlaneDistance);
            result.M34 = 1.0f;
        }
        else
        {
            result.M33 = farPlaneDistance / (farPlaneDistance - nearPlaneDistance);
            result.M43 = -nearPlaneDistance * farPlaneDistance / (farPlaneDistance - nearPlaneDistance);
            result.M34 = 1.0f;
        }

        return result;
    }

    [Fact]
    public void ValidateApiVersion()
    {
        var apiVersion = m_EngineFactory.GetAPIInfo();
        Assert.Equal(Native.ApiVersion, apiVersion.APIVersion);
    }

    [Fact]
    public void CheckStructureSizes()
    {
        CheckStructSizes(m_EngineFactory.GetAPIInfo());
    }

    [Fact]
    public void CompareRenderedCubeImage()
    {
        using var renderTexture = m_RenderDevice.CreateTexture(new()
        {
            Name = "Color buffer",
            Type = ResourceDimension.Tex2d,
            Width = m_SwapChainDesc.Width,
            Height = m_SwapChainDesc.Height,
            SampleCount = 1,
            Usage = Usage.Default,
            Format = m_SwapChainDesc.ColorBufferFormat,
            BindFlags = BindFlags.RenderTarget
        });
        Assert.NotNull(renderTexture);

        using var stagingTexture = m_RenderDevice.CreateTexture(new()
        {
            Name = "Staging buffer",
            Type = ResourceDimension.Tex2d,
            Width = m_SwapChainDesc.Width,
            Height = m_SwapChainDesc.Height,
            SampleCount = 1,
            Usage = Usage.Staging,
            Format = m_SwapChainDesc.ColorBufferFormat,
            BindFlags = BindFlags.None,
            CPUAccessFlags = CpuAccessFlags.Read
        });
        Assert.NotNull(stagingTexture);

        using var depthTexture = m_RenderDevice.CreateTexture(new()
        {
            Name = "Depth buffer",
            Type = ResourceDimension.Tex2d,
            Width = m_SwapChainDesc.Width,
            Height = m_SwapChainDesc.Height,
            SampleCount = 1,
            Usage = Usage.Default,
            Format = m_SwapChainDesc.DepthBufferFormat,
            BindFlags = BindFlags.DepthStencil
        });
        Assert.NotNull(depthTexture);

        var cubeVertices = new Vertex[] {
            new (new(-1, -1, -1), new(1, 0, 0, 1)),
            new (new(-1, +1, -1), new(0, 1, 0, 1)),
            new (new(+1, +1, -1), new(0, 0, 1, 1)),
            new (new(+1, -1, -1), new(1, 1, 1, 1)),

            new (new(-1, -1, +1), new(1, 1, 0, 1)),
            new (new(-1, +1, +1), new(0, 1, 1, 1)),
            new (new(+1, +1, +1), new(1, 0, 1, 1)),
            new (new(+1, -1, +1), new(0.2f, 0.2f, 0.2f, 1))
        };

        var cubeIndices = new uint[]
        {
            2,0,1, 2,3,0,
            4,6,5, 4,7,6,
            0,7,4, 0,3,7,
            1,0,4, 1,4,5,
            1,5,2, 5,6,2,
            3,6,7, 3,2,6
        };

        using var vertexBuffer = m_RenderDevice.CreateBuffer(new()
        {
            Name = "Cube vertex buffer",
            Usage = Usage.Immutable,
            BindFlags = BindFlags.VertexBuffer,
            Size = (ulong)(Unsafe.SizeOf<Vertex>() * cubeVertices.Length)
        }, cubeVertices);
        Assert.NotNull(vertexBuffer);

        using var indexBuffer = m_RenderDevice.CreateBuffer(new()
        {
            Name = "Cube index buffer",
            Usage = Usage.Immutable,
            BindFlags = BindFlags.IndexBuffer,
            Size = (ulong)(Unsafe.SizeOf<uint>() * cubeIndices.Length)
        }, cubeIndices);
        Assert.NotNull(indexBuffer);

        using var uniformBuffer = m_RenderDevice.CreateBuffer(new()
        {
            Name = "Uniform buffer",
            Size = (ulong)Unsafe.SizeOf<Matrix4x4>(),
            Usage = Usage.Dynamic,
            BindFlags = BindFlags.UniformBuffer,
            CPUAccessFlags = CpuAccessFlags.Write
        });
        Assert.NotNull(uniformBuffer);

        using var shaderSourceFactory = m_EngineFactory.CreateDefaultShaderSourceStreamFactory(PathToAssets);
        Assert.NotNull(shaderSourceFactory);

        using var vs = m_RenderDevice.CreateShader(new()
        {
            FilePath = "DotNetCube.vsh",
            ShaderSourceStreamFactory = shaderSourceFactory,
            Desc = new()
            {
                Name = "Cube vertex shader",
                ShaderType = ShaderType.Vertex,
                UseCombinedTextureSamplers = true
            },
            SourceLanguage = ShaderSourceLanguage.Hlsl
        }, out _);
        Assert.NotNull(vs);

        using var ps = m_RenderDevice.CreateShader(new()
        {
            FilePath = "DotNetCube.psh",
            ShaderSourceStreamFactory = shaderSourceFactory,
            Desc = new()
            {
                Name = "Cube pixel shader",
                ShaderType = ShaderType.Pixel,
                UseCombinedTextureSamplers = true
            },
            SourceLanguage = ShaderSourceLanguage.Hlsl
        }, out _);
        Assert.NotNull(ps);

        using var pipeline = m_RenderDevice.CreateGraphicsPipelineState(new()
        {
            PSODesc = new() { Name = "Cube Graphics PSO" },
            Vs = vs,
            Ps = ps,
            GraphicsPipeline = new()
            {
                InputLayout = new()
                {
                    LayoutElements = new[]
                    {
                        new LayoutElement
                        {
                            InputIndex = 0,
                            NumComponents = 3,
                            ValueType = Diligent.ValueType.Float32,
                            IsNormalized = false,
                        },
                        new LayoutElement
                        {
                            InputIndex = 1,
                            NumComponents = 4,
                            ValueType = Diligent.ValueType.Float32,
                            IsNormalized = false,
                        }
                    }
                },
                PrimitiveTopology = PrimitiveTopology.TriangleList,
                RasterizerDesc = new() { CullMode = CullMode.Back },
                DepthStencilDesc = new() { DepthEnable = true },
                NumRenderTargets = 1,
                RTVFormats = new[] { m_SwapChainDesc.ColorBufferFormat },
                DSVFormat = m_SwapChainDesc.DepthBufferFormat
            }
        });
        Assert.NotNull(pipeline);

        var variable = pipeline.GetStaticVariableByName(ShaderType.Vertex, "Constants");
        Assert.NotNull(variable);

        variable.Set(uniformBuffer, SetShaderResourceFlags.None);

        using var shaderResourceBinding = pipeline.CreateShaderResourceBinding(true);
        Assert.NotNull(shaderResourceBinding);

        var isOpenGL = m_RenderDevice.GetDeviceInfo().Type == RenderDeviceType.Gl || m_RenderDevice.GetDeviceInfo().Type == RenderDeviceType.Gles;
        var worldMatrix = Matrix4x4.CreateRotationY(MathF.PI / 4.0f) * Matrix4x4.CreateRotationX(-MathF.PI * 0.1f);
        var viewMatrix = Matrix4x4.CreateTranslation(0.0f, 0.0f, 5.0f);
        var projMatrix = CreatePerspectiveFieldOfView(MathF.PI / 4.0f, m_SwapChainDesc.Width / (float)m_SwapChainDesc.Height, 0.01f, 100.0f,isOpenGL);
        var wvpMatrix = Matrix4x4.Transpose(worldMatrix * viewMatrix * projMatrix);

        var mapUniformBuffer = m_DeviceContext.MapBuffer<Matrix4x4>(uniformBuffer, MapType.Write, MapFlags.Discard);
        mapUniformBuffer[0] = wvpMatrix;
        m_DeviceContext.UnmapBuffer(uniformBuffer, MapType.Write);

        var rtv = renderTexture.GetDefaultView(TextureViewType.RenderTarget);
        var dsv = depthTexture.GetDefaultView(TextureViewType.DepthStencil);
        Assert.NotNull(rtv);
        Assert.NotNull(dsv);

        m_DeviceContext.SetRenderTargets(new[] { rtv }, dsv, ResourceStateTransitionMode.Transition);
        m_DeviceContext.ClearRenderTarget(rtv, new(0.35f, 0.35f, 0.35f, 1.0f), ResourceStateTransitionMode.Transition);
        m_DeviceContext.ClearDepthStencil(dsv, ClearDepthStencilFlags.Depth, 1.0f, 0, ResourceStateTransitionMode.Transition);
        m_DeviceContext.SetPipelineState(pipeline);
        m_DeviceContext.SetVertexBuffers(0, new[] { vertexBuffer }, new[] { 0ul }, ResourceStateTransitionMode.Transition);
        m_DeviceContext.SetIndexBuffer(indexBuffer, 0, ResourceStateTransitionMode.Transition);
        m_DeviceContext.CommitShaderResources(shaderResourceBinding, ResourceStateTransitionMode.Transition);
        m_DeviceContext.DrawIndexed(new()
        {
            IndexType = Diligent.ValueType.UInt32,
            NumIndices = 36,
            Flags = DrawFlags.VerifyAll
        });
        m_DeviceContext.SetRenderTargets(null, null, ResourceStateTransitionMode.None);
        m_DeviceContext.CopyTexture(new()
        {
            SrcTexture = renderTexture,
            SrcTextureTransitionMode = ResourceStateTransitionMode.Transition,
            DstTexture = stagingTexture,
            DstTextureTransitionMode = ResourceStateTransitionMode.Transition
        });

        m_DeviceContext.WaitForIdle();
        var mapStagingTexture = m_DeviceContext.MapTextureSubresource(stagingTexture, 0, 0, MapType.Read, MapFlags.DoNotWait, null);
        CompareImages(mapStagingTexture, $"{PathToAssets}/DotNetCubeTexture.png", isOpenGL);
        m_DeviceContext.UnmapTextureSubresource(stagingTexture, 0, 0);
        m_DeviceContext.InvalidateState();
    }

    [Fact]
    public void DearchiveCubeGraphicsAssets()
    {
        using var dearchiver = m_EngineFactory.CreateDearchiver(new());
        Assert.NotNull(dearchiver);

        var binaryData = File.ReadAllBytes($"{PathToAssets}/DotNetArchive.bin");
        Assert.NotNull(binaryData);

        using var dataBlob = m_EngineFactory.CreateDataBlob(binaryData);
        Assert.NotNull(dataBlob);

        var result = dearchiver.LoadArchive(dataBlob, ContentVersion, false);
        Assert.True(result);

        using var pipeline = dearchiver.UnpackPipelineState(new()
        {
            Device = m_RenderDevice,
            Name = "Cube Graphics PSO",
            PipelineType = PipelineType.Graphics,

        });
        Assert.NotNull(pipeline);
        Assert.Equal("Cube Graphics PSO", pipeline.GetDesc().Name);

        using var vs = dearchiver.UnpackShader(new()
        {
            Device = m_RenderDevice,
            Name = "Cube vertex shader",
        });
        Assert.NotNull(vs);
        Assert.Equal("Cube vertex shader", vs.GetDesc().Name);

        using var shaderModified = dearchiver.UnpackShader(new()
        {
            Device = m_RenderDevice,
            Name = "Cube pixel shader",
            ModifyShaderDesc = (ref ShaderDesc desc) =>
            {
                desc.Name = "ModifiedName";
            }
        });
        Assert.NotNull(shaderModified);
        Assert.Equal("ModifiedName", shaderModified.GetDesc().Name);

        using var pipelineModified = dearchiver.UnpackPipelineState(new()
        {
            Device = m_RenderDevice,
            Name = "Cube Graphics PSO",
            PipelineType = PipelineType.Graphics,
            ModifyPipelineStateCreateInfo = (createInfo) =>
            {
                createInfo.PSODesc.Name = "ModifiedName";
            }
        });
        Assert.NotNull(pipelineModified);
        Assert.Equal("ModifiedName", pipelineModified.GetDesc().Name);
    }

    [Fact]
    public void CheckBrokenShader()
    {
        const string shaderCode = @"
            struct PSInput
            {
                float4 Pos : SV_POSITION;
                float4 Color : COLOR0;
            };

            struct PSOutput
            {
                float4 Color : SV_TARGET;
            };

            PSOutput main(in PSInput PSIn)
            {
                error;
            }
        ";

        using var shader = m_RenderDevice.CreateShader(new()
        {
            Source = shaderCode,
            Desc = new()
            {
                Name = "Broken shader",
                ShaderType = ShaderType.Pixel,
                UseCombinedTextureSamplers = true
            },
            SourceLanguage = ShaderSourceLanguage.Hlsl
        }, out var compilerError);

        Assert.Null(shader);
        Assert.NotNull(compilerError);

        var compilerOutputStr = Marshal.PtrToStringAnsi(compilerError.GetDataPtr(), (int)compilerError.GetSize());
        Assert.Contains("error", compilerOutputStr);
        m_Output.WriteLine(compilerOutputStr);
    }

    public void Dispose()
    {
        m_DeviceContext?.Dispose();
        m_RenderDevice?.Dispose();
        m_SwapChain.Dispose();
        m_EngineFactory?.Dispose();
        m_Window?.Dispose();
    }
}
