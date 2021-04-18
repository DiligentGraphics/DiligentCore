VULKAN_SDK_VER="1.2.162.1"

export VK_SDK_DMG=vulkansdk-macos-$VULKAN_SDK_VER.dmg
wget -O $VK_SDK_DMG https://sdk.lunarg.com/sdk/download/$VULKAN_SDK_VER/mac/$VK_SDK_DMG?Human=true &&
hdiutil attach $VK_SDK_DMG
echo "VULKAN_SDK=/Volumes/vulkansdk-macos-$VULKAN_SDK_VER" >> $GITHUB_ENV
