
# Shader Resource Variables

Diligent Engine uses three variable types:

- *Static* variables can be set only once in each pipeline state object or pipeline resource signature.
  Once bound, the resource can't be changed.
- *Mutable* variables can be set only once in each shader resource binding instance.
  Once bound, the resource can't be changed.
- *Dynamic* variables can be bound any number of times.

Internally, static and mutable variables are implemented in the same way. Static variable bindings are copied
from PSO or Signature to the SRB either when the SRB is created or when `IPipelineState::InitializeStaticSRBResources()`
method is called.

Dynamic variables introduce some overhead every time an SRB is committed (even if no bindings have changed). Prefer static/mutable
variables over dynamic ones whenever possible.


# Dynamic Buffers

`USAGE_DYNAMIC` buffers introduce some overhead in every draw or dispatch command that uses an SRB with dynamic buffers.
Note that extra work is performed even if none of the dynamic buffers have been mapped between the commands. If this
is the case, an application should use `DRAW_FLAG_DYNAMIC_RESOURCE_BUFFERS_INTACT` flag to tell the engine that
none of the dynamic buffers have been updated between the commands. Note that the first time an SRB is bound,
dynamic buffers are properly bound regardless of the `DRAW_FLAG_DYNAMIC_RESOURCE_BUFFERS_INTACT` flag.

Constant and structured buffers set with `IShaderResoruceBinding::SetBufferRange` method count as
dynamic buffers when the specified range does not cover the entire buffer *regardless of the buffer usage*,
unless the variable was created with `NO_DYNAMIC_BUFFERS` flag. Note that in the latter case,
`IShaderResoruceBinding::SetBufferOffset` method can't be used to set dynamic offset.
Similar to `USAGE_DYNAMIC` buffers, if an applications knows that none of the dynamic offsets have changed
between the draw calls, it may use `DRAW_FLAG_DYNAMIC_RESOURCE_BUFFERS_INTACT` flag.

An application should try to use as few dynamic buffers as possible. On some implementations, the number of dynamic
buffers may be limited by as few as 8 buffers. If an application knows that no dynamic buffers will be bound to
a shader resource variable, it should use the `SHADER_VARIABLE_FLAG_NO_DYNAMIC_BUFFERS` flag when defining
the variables through the PSO layout or `PIPELINE_RESOURCE_FLAG_NO_DYNAMIC_BUFFERS` when defining the variable
through pipeline resource signature. It is an error to bind `USAGE_DYNAMIC` buffer to a variable that was
created with `NO_DYNAMIC_BUFFERS` flag. Likewise, it is an error to set dynamic offset for such variable.

Try to optimize dynamic buffers usage, but don't strive to avoid them as they are usually the fastest way
to upload frequently changing data to the GPU.


# References

* [DirectX12 Do's And Don'ts](https://developer.nvidia.com/dx12-dos-and-donts) by NVidia
* [Vulkan Do's And Don'ts](https://developer.nvidia.com/blog/vulkan-dos-donts/) by NVidia
* [Arm Mali GPU Best Practices Developer Guide Version](https://developer.arm.com/documentation/101897/0201/Preface) by ARM
