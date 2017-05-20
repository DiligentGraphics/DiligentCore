Core module implements key engine functionality. It provides the engine implementation using Direct3D11, Direct3D12, OpenGL and OpenGLES as well as basic platform-specific utilities. For more information, please visit [diligentgraphics.com/diligent-engine](http://diligentgraphics.com/diligent-engine/).

## Version History

### v1.0.0

Initial release

### v2.0.alpha

Alpha release of Diligent Engine 2.0. The engine has been updated to take advantages of Direct3D12:

* Pipeline State Object encompasses all coarse-grain state objects like Depth-Stencil State, Blend State, Rasterizer State, shader states etc.
* New shader resource binding model implemented to leverage Direct3D12

Release notes:

* Diligent Engine 2.0 also implements OpenGL and Direct3D11 back-ends
* Alpha release is only available on Windows platform
* Direct3D11 back-end is very thoroughly optimized and has very low overhead compared to native D3D11 implementation
* Direct3D12 implementation, to the contrary, is preliminary and not yet optimized.

For more details on the release, please visit [diligentgraphics.com](http://diligentgraphics.com/2016/03/17/diligent-engine-2-0-powered-by-direct3d12/)

## License

Copyright 2015-2017 Egor Yusov.
Licensed under the [Apache License, Version 2.0](License.txt)

## Resources
Visit [Diligent Engine Web Site](http://diligentgraphics.com), follow us on [Twitter](https://twitter.com/diligentengine) and [Facebook](https://www.facebook.com/DiligentGraphics)
