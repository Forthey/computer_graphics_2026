#pragma once

#ifdef _MSC_VER
#pragma warning(push, 0)
#endif
#include <dxgiformat.h>
#ifdef _MSC_VER
#pragma warning(pop)
#endif

#include <cstddef>
#include <cstdint>
#include <vector>

struct TextureSubresourceLayout {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint32_t rowPitch = 0;
    std::uint32_t slicePitch = 0;
    std::size_t dataOffset = 0;
};

struct TextureDescription {
    DXGI_FORMAT fmt = DXGI_FORMAT_UNKNOWN;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint32_t mipmapsCount = 0;
    std::uint32_t arraySize = 1;
    bool isCubeMap = false;
    std::vector<TextureSubresourceLayout> subresources;
    std::vector<std::byte> data;
};

bool loadDDS(const wchar_t* path, TextureDescription& textureDescription);