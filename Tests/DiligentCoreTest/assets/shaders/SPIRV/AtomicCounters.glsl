// Test for atomic counters
// GLSL fragment shader for testing atomic counter resource reflection
// Note: Vulkan does not support atomic_uint (AtomicCounter storage class).
// We use a storage buffer with atomic operations to simulate atomic counters.
// However, this will be reflected as RWStorageBuffer, not AtomicCounter.
#version 450

layout(binding = 0) coherent buffer AtomicCounterBuffer
{
    uint counter;
} g_AtomicCounter;

layout(location = 0) out vec4 FragColor;

void main()
{
    // Increment atomic counter using atomicAdd
    // atomicAdd returns the value before the addition
    uint counterValue = atomicAdd(g_AtomicCounter.counter, 1u);

    // Create a color based on counter value
    vec4 color = vec4(
        float(counterValue & 0xFFu) / 255.0,
        float((counterValue >> 8u) & 0xFFu) / 255.0,
        float((counterValue >> 16u) & 0xFFu) / 255.0,
        1.0
    );

    FragColor = color;
}
