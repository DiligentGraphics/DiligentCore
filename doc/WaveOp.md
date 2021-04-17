# Wave operations

## Common names

| GLSL | HLSL | Description |
|---|---|---|
| Subgroup | Wave | a set of lanes (threads) executed simultaneously in the processor. Other names: wavefront (AMD), warp (NVidia). |
| Invocation | Lane | a single thread of execution |
| ..Inclusive.. | - | function with these suffix includes lanes from 0 to `LaneIndex` |
| ..Exclusive.. | ..Prefix.. | function with these suffix includes lanes from 0 to `LaneIndex` but current lane is not included |


## WAVE_FEATURE_BASIC

| GLSL | HLSL | Description |
|---|---|---|
| uint&nbsp;gl_SubgroupSize | uint&nbsp;WaveGetLaneCount() | size of the wave, `WaveSize` will be used as an alias for both of them |
| uint&nbsp;gl_SubgroupInvocationID | uint&nbsp;WaveGetLaneIndex() | lane index within the wave, `LaneIndex` will be used as an alias for both of them|
| bool&nbsp;subgroupElect() | bool&nbsp;WaveIsFirstLane() | exactly one lane within the wave will return true, the others will return false. The lane that returns true is always the one that is active with the lowest `LaneIndex` |
 
 
## WAVE_FEATURE_VOTE

| GLSL | HLSL | Description |
|---|---|---|
| bool&nbsp;subgroupAny(bool&nbsp;value) | bool&nbsp;WaveActiveAnyTrue(bool&nbsp;value) | returns true if any active lane has `value == true` |
| bool&nbsp;subgroupAll(bool&nbsp;value) | bool&nbsp;WaveActiveAllTrue(bool&nbsp;value) | returns true if all active lanes have `value == true` |
| bool&nbsp;subgroupAllEqual(T&nbsp;value) | bool&nbsp;WaveActiveAllEqual(T&nbsp;value) | returns true if all active lanes have a `value` that is equal |
 
 
## WAVE_FEATURE_ARITHMETIC

| GLSL | HLSL | Description |
|---|---|---|
| T&nbsp;subgroupAdd(T&nbsp;value) | T&nbsp;WaveActiveSum(T&nbsp;value) | returns the summation of all active lanes `value`'s across the wave |
| T&nbsp;subgroupMul(T&nbsp;value) | T&nbsp;WaveActiveProduct(T&nbsp;value) | returns the multiplication of all active lanes `value`'s across the wave |
| T&nbsp;subgroupMin(T&nbsp;value) | T&nbsp;WaveActiveMin(T&nbsp;value) | returns the minimum value of all active lanes `value`'s across the wave |
| T&nbsp;subgroupMax(T&nbsp;value) | T&nbsp;WaveActiveMax(T&nbsp;value) | returns the maximum value of all active lanes `value`'s across the wave |
| T&nbsp;subgroupAnd(T&nbsp;value) | T&nbsp;WaveActiveBitAnd(T&nbsp;value) | returns the binary AND of all active lanes `value`'s across the wave |
| T&nbsp;subgroupOr(T&nbsp;value)  | T&nbsp;WaveActiveBitOr(T&nbsp;value) | returns the binary OR of all active lanes `value`'s across the wave |
| T&nbsp;subgroupXor(T&nbsp;value) | T&nbsp;WaveActiveBitXor(T&nbsp;value) | returns the binary XOR of all active lanes `value`'s across the wave |
| T&nbsp;subgroupInclusiveAdd(T&nbsp;value) | - | returns the inclusive scan summation of all active lanes `value`'s across the wave |
| T&nbsp;subgroupInclusiveMul(T&nbsp;value) | - | returns the inclusive scan the multiplication of all active lanes `value`'s across the wave |
| T&nbsp;subgroupInclusiveMin(T&nbsp;value) | - | returns the inclusive scan the minimum value of all active lanes `value`'s across the wave |
| T&nbsp;subgroupInclusiveMax(T&nbsp;value) | - | returns the inclusive scan the maximum value of all active lanes `value`'s across the wave |
| T&nbsp;subgroupInclusiveAnd(T&nbsp;value) | - | returns the inclusive scan the binary AND of all active lanes `value`'s across the wave |
| T&nbsp;subgroupInclusiveOr(T&nbsp;value)  | - | returns the inclusive scan the binary OR of all active lanes `value`'s across the wave |
| T&nbsp;subgroupInclusiveXor(T&nbsp;value) | - |  returns the inclusive scan the binary XOR of all active lanes `value`'s across the wave |
| T&nbsp;subgroupExclusiveAdd(T&nbsp;value) | T&nbsp;WavePrefixSum(T&nbsp;value) | returns the exclusive scan summation of all active lanes `value`'s across the wave |
| T&nbsp;subgroupExclusiveMul(T&nbsp;value) | T&nbsp;WavePrefixProduct(T&nbsp;value) | returns the exclusive scan the multiplication of all active lanes `value`'s across the wave |
| T&nbsp;subgroupExclusiveMin(T&nbsp;value) | - | returns the exclusive scan the minimum value of all active lanes `value`'s across the wave |
| T&nbsp;subgroupExclusiveMax(T&nbsp;value) | - | returns the exclusive scan the maximum value of all active lanes `value`'s across the wave |
| T&nbsp;subgroupExclusiveAnd(T&nbsp;value) | - | returns the exclusive scan the binary AND of all active lanes `value`'s across the wave |
| T&nbsp;subgroupExclusiveOr(T&nbsp;value)  | - | returns the exclusive scan the binary OR of all active lanes `value`'s across the wave |
| T&nbsp;subgroupExclusiveXor(T&nbsp;value) | - | returns the exclusive scan the binary XOR of all active lanes `value`'s across the wave |
 
 
## WAVE_FEATURE_BALLOUT

| GLSL | HLSL | Description |
|---|---|---|
| uvec4&nbsp;subgroupBallot(bool&nbsp;value) | uint4&nbsp;WaveActiveBallot(bool&nbsp;value) | each lane contributes a single bit to the resulting `uvec4` corresponding to `value` |
| T&nbsp;subgroupBroadcast(T&nbsp;value,&nbsp;uint&nbsp;id) | T&nbsp;WaveReadLaneAt(T&nbsp;value,&nbsp;uint&nbsp;id) | broadcasts the `value` whose `LaneIndex == id` to all other lanes (id must be a compile time constant) |
| T&nbsp;subgroupBroadcastFirst(T&nbsp;value) | T&nbsp;WaveReadLaneFirst(T&nbsp;value) | broadcasts the `value` whose `LaneIndex` is the lowest active to all other lanes |
| bool&nbsp;subgroupInverseBallot(uvec4&nbsp;value) | - | returns true if this lanes bit in `value` is true |
| bool&nbsp;subgroupBallotBitExtract(uvec4&nbsp;value,&nbsp;uint&nbsp;index) | - | returns true if the bit corresponding to `index` is set in `value` |
| uint&nbsp;subgroupBallotBitCount(uvec4&nbsp;value) | - | returns the number of bits set in `value`, only counting the bottom `WaveSize` bits |
| uint&nbsp;subgroupBallotInclusiveBitCount(uvec4&nbsp;value) | - | returns the inclusive scan of the number of bits set in `value`, only counting the bottom `WaveSize` bits (we'll cover what an inclusive scan is later) |
| uint&nbsp;subgroupBallotExclusiveBitCount(uvec4&nbsp;value) | - | returns the exclusive scan of the number of bits set in `value`, only counting the bottom `WaveSize` bits (we'll cover what an exclusive scan is later) |
| uint&nbsp;subgroupBallotFindLSB(uvec4&nbsp;value) | - | returns the lowest bit set in `value`, only counting the bottom `WaveSize` bits |
| uint&nbsp;subgroupBallotFindMSB(uvec4&nbsp;value) | - | returns the highest bit set in `value`, only counting the bottom `WaveSize` bits |
| uint&nbsp;subgroupBallotBitCount( subgroupBallot(bool&nbsp;value)) | uint&nbsp;WaveActiveCountBits(bool&nbsp;value) | counts the number of boolean variables which evaluate to true across all active lanes in the current wave, and replicates the result to all lanes in the wave |
| uint&nbsp;subgroupBallotExclusiveBitCount( subgroupBallot(bool&nbsp;value)) | uint&nbsp;WavePrefixCountBits(bool&nbsp;value) | returns the sum of all the specified boolean variables set to true across all active lanes with indices smaller than the current lane |


## WAVE_FEATURE_SHUFFLE

| GLSL | HLSL | Description |
|---|---|---|
| T&nbsp;subgroupShuffle(T&nbsp;value,&nbsp;uint&nbsp;index) | - | returns the `value` whose `LaneIndex` is equal to `index` |
| T&nbsp;subgroupShuffleXor(T&nbsp;value,&nbsp;uint&nbsp;mask) | - | returns the `value` whose `LaneIndex` is equal to the current lanes `LaneIndex` xor'ed with `mask` |
 
 
## WAVE_FEATURE_SHUFFLE_RELATIVE

| GLSL | HLSL | Description |
|---|---|---|
| T&nbsp;subgroupShuffleUp(T&nbsp;value,&nbsp;uint&nbsp;delta) | - | returns the `value` whose `LaneIndex` is equal to the current lanes `LaneIndex` minus `delta` |
| T&nbsp;subgroupShuffleDown(T&nbsp;value,&nbsp;uint&nbsp;delta) | - | returns the `value` whose `LaneIndex` is equal to the current lanes `LaneIndex` plus `delta` |
 
 
## WAVE_FEATURE_CLUSTERED

| GLSL | HLSL | Description |
|---|---|---|
| T&nbsp;subgroupClusteredAdd(T&nbsp;value,&nbsp;uint&nbsp;clusterSize) | - | returns the summation of all active lanes `value`'s across clusters of size `clusterSize` |
| T&nbsp;subgroupClusteredMul(T&nbsp;value,&nbsp;uint&nbsp;clusterSize) | - | returns the multiplication of all active lanes `value`'s across clusters of size `clusterSize` |
| T&nbsp;subgroupClusteredMin(T&nbsp;value,&nbsp;uint&nbsp;clusterSize) | - | returns the minimum value of all active lanes `value`'s across clusters of size `clusterSize` |
| T&nbsp;subgroupClusteredMax(T&nbsp;value,&nbsp;uint&nbsp;clusterSize) | - | returns the maximum value of all active lanes `value`'s across clusters of size `clusterSize` |
| T&nbsp;subgroupClusteredAnd(T&nbsp;value,&nbsp;uint&nbsp;clusterSize) | - | returns the binary AND of all active lanes `value`'s across clusters of size `clusterSize` |
| T&nbsp;subgroupClusteredOr(T&nbsp;value,&nbsp;uint&nbsp;clusterSize)  | - | returns the binary OR of all active lanes `value`'s across clusters of size `clusterSize` |
| T&nbsp;subgroupClusteredXor(T&nbsp;value,&nbsp;uint&nbsp;clusterSize) | - | returns the binary XOR of all active lanes `value`'s across clusters of size `clusterSize` |
 
 
## WAVE_FEATURE_QUAD
Quad operations executes on 2x2 grid in pixel and compute shaders.

| GLSL | HLSL | Description |
|---|---|---|
| T&nbsp;subgroupQuadBroadcast(T&nbsp;value,&nbsp;uint&nbsp;id) | T&nbsp;QuadReadLaneAt(T&nbsp;value,&nbsp;uint&nbsp;id) | returns the `value` in the quad whose `LaneIndex` modulus 4 is equal to `id` |
| T&nbsp;subgroupQuadSwapHorizontal(T&nbsp;value) | T&nbsp;QuadReadAcrossX(T&nbsp;value) | swaps `value`'s within the quad horizontally |
| T&nbsp;subgroupQuadSwapVertical(T&nbsp;value) | T&nbsp;QuadReadAcrossY(T&nbsp;value) | swaps `value`'s within the quad vertically |
| T&nbsp;subgroupQuadSwapDiagonal(T&nbsp;value) | T&nbsp;QuadReadAcrossDiagonal(T&nbsp;value) | swaps `value`'s within the quad diagonally |
 
 
## References

GLSL<br/>
https://www.khronos.org/blog/vulkan-subgroup-tutorial<br/>
https://raw.githubusercontent.com/KhronosGroup/GLSL/master/extensions/khr/GL_KHR_shader_subgroup.txt<br/>

HLSL<br/>
https://docs.microsoft.com/en-us/windows/win32/direct3dhlsl/hlsl-shader-model-6-0-features-for-direct3d-12<br/>
https://github.com/Microsoft/DirectXShaderCompiler/wiki/Wave-Intrinsics<br/>
