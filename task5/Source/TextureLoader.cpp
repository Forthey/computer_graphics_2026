#include "TextureLoader.h"

#ifdef _MSC_VER
#pragma warning(push, 0)
#endif
#include <Windows.h>
#ifdef _MSC_VER
#pragma warning(pop)
#endif

#include <algorithm>
#include <array>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

namespace {
constexpr std::uint32_t kDdsMagic = 0x20534444u;
constexpr std::uint32_t kDdsPixelFormatFourCc = 0x00000004u;
constexpr std::uint32_t kDdsPixelFormatRgb = 0x00000040u;
constexpr std::uint32_t kDdsCaps2Cubemap = 0x00000200u;
constexpr std::uint32_t kDdsResourceMiscTextureCube = 0x4u;
constexpr std::uint32_t kDdsResourceDimensionTexture2D = 3u;

constexpr std::uint32_t makeFourCc(char a, char b, char c, char d) {
    return static_cast<std::uint32_t>(static_cast<unsigned char>(a)) |
           (static_cast<std::uint32_t>(static_cast<unsigned char>(b)) << 8u) |
           (static_cast<std::uint32_t>(static_cast<unsigned char>(c)) << 16u) |
           (static_cast<std::uint32_t>(static_cast<unsigned char>(d)) << 24u);
}

std::wstring getExecutableDirectory() {
    wchar_t executablePath[MAX_PATH]{};
    const DWORD length = GetModuleFileNameW(nullptr, executablePath, MAX_PATH);
    if (length == 0u || length == MAX_PATH) {
        return {};
    }

    std::wstring directory(executablePath, executablePath + length);
    const std::size_t separatorIndex = directory.find_last_of(L"\\/");
    if (separatorIndex == std::wstring::npos) {
        return {};
    }

    directory.resize(separatorIndex + 1u);
    return directory;
}

#pragma pack(push, 1)
struct DdsPixelFormatHeader {
    std::uint32_t size;
    std::uint32_t flags;
    std::uint32_t fourCC;
    std::uint32_t rgbBitCount;
    std::uint32_t rBitMask;
    std::uint32_t gBitMask;
    std::uint32_t bBitMask;
    std::uint32_t aBitMask;
};

struct DdsHeader {
    std::uint32_t size;
    std::uint32_t flags;
    std::uint32_t height;
    std::uint32_t width;
    std::uint32_t pitchOrLinearSize;
    std::uint32_t depth;
    std::uint32_t mipMapCount;
    std::uint32_t reserved1[11];
    DdsPixelFormatHeader pixelFormat;
    std::uint32_t caps;
    std::uint32_t caps2;
    std::uint32_t caps3;
    std::uint32_t caps4;
    std::uint32_t reserved2;
};

struct DdsHeaderDx10 {
    DXGI_FORMAT dxgiFormat;
    std::uint32_t resourceDimension;
    std::uint32_t miscFlag;
    std::uint32_t arraySize;
    std::uint32_t miscFlags2;
};
#pragma pack(pop)

struct FormatInfo {
    DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
    bool isBlockCompressed = false;
    std::uint32_t unitBytes = 0;
};

bool readFileBytes(const wchar_t* path, std::vector<std::byte>& fileBytes) {
    std::vector<std::wstring> candidates = {
        std::wstring(path),
        std::wstring(L"..\\..\\") + path,
    };

    const std::wstring executableDirectory = getExecutableDirectory();
    if (!executableDirectory.empty()) {
        candidates.push_back(executableDirectory + path);
        candidates.push_back(executableDirectory + L"..\\..\\" + path);
    }

    for (const std::wstring& candidate : candidates) {
        std::ifstream input(candidate, std::ios::binary | std::ios::ate);
        if (!input) {
            continue;
        }

        const std::streamsize fileSize = input.tellg();
        if (fileSize <= 0) {
            continue;
        }

        fileBytes.resize(static_cast<std::size_t>(fileSize));
        input.seekg(0, std::ios::beg);
        input.read(reinterpret_cast<char*>(fileBytes.data()), fileSize);
        if (input) {
            return true;
        }
    }

    return false;
}

FormatInfo getSupportedFormatInfo(DXGI_FORMAT format) {
    if (format == DXGI_FORMAT_R8G8B8A8_UNORM || format == DXGI_FORMAT_B8G8R8A8_UNORM ||
        format == DXGI_FORMAT_B8G8R8X8_UNORM) {
        return {format, false, 4u};
    }

    if (format == DXGI_FORMAT_BC1_UNORM) {
        return {format, true, 8u};
    }

    if (format == DXGI_FORMAT_BC2_UNORM || format == DXGI_FORMAT_BC3_UNORM) {
        return {format, true, 16u};
    }

    return {};
}

FormatInfo decodeLegacyFormat(const DdsPixelFormatHeader& pixelFormat) {
    if ((pixelFormat.flags & kDdsPixelFormatFourCc) != 0u) {
        switch (pixelFormat.fourCC) {
            case makeFourCc('D', 'X', 'T', '1'):
                return getSupportedFormatInfo(DXGI_FORMAT_BC1_UNORM);
            case makeFourCc('D', 'X', 'T', '3'):
                return getSupportedFormatInfo(DXGI_FORMAT_BC2_UNORM);
            case makeFourCc('D', 'X', 'T', '5'):
                return getSupportedFormatInfo(DXGI_FORMAT_BC3_UNORM);
            default:
                return {};
        }
    }

    if ((pixelFormat.flags & kDdsPixelFormatRgb) == 0u || pixelFormat.rgbBitCount != 32u) {
        return {};
    }

    if (pixelFormat.rBitMask == 0x000000FFu && pixelFormat.gBitMask == 0x0000FF00u &&
        pixelFormat.bBitMask == 0x00FF0000u && pixelFormat.aBitMask == 0xFF000000u) {
        return getSupportedFormatInfo(DXGI_FORMAT_R8G8B8A8_UNORM);
    }

    if (pixelFormat.rBitMask == 0x00FF0000u && pixelFormat.gBitMask == 0x0000FF00u &&
        pixelFormat.bBitMask == 0x000000FFu && pixelFormat.aBitMask == 0xFF000000u) {
        return getSupportedFormatInfo(DXGI_FORMAT_B8G8R8A8_UNORM);
    }

    if (pixelFormat.rBitMask == 0x00FF0000u && pixelFormat.gBitMask == 0x0000FF00u &&
        pixelFormat.bBitMask == 0x000000FFu && pixelFormat.aBitMask == 0x00000000u) {
        return getSupportedFormatInfo(DXGI_FORMAT_B8G8R8X8_UNORM);
    }

    return {};
}

bool computeSubresourceLayout(const FormatInfo& formatInfo, std::uint32_t width, std::uint32_t height,
                              TextureSubresourceLayout& layout) {
    if (formatInfo.format == DXGI_FORMAT_UNKNOWN || formatInfo.unitBytes == 0u || width == 0u || height == 0u) {
        return false;
    }

    layout.width = width;
    layout.height = height;

    if (formatInfo.isBlockCompressed) {
        const std::uint32_t blockWidth = (std::max)(1u, (width + 3u) / 4u);
        const std::uint32_t blockHeight = (std::max)(1u, (height + 3u) / 4u);
        layout.rowPitch = blockWidth * formatInfo.unitBytes;
        layout.slicePitch = layout.rowPitch * blockHeight;
    } else {
        layout.rowPitch = width * formatInfo.unitBytes;
        layout.slicePitch = layout.rowPitch * height;
    }

    return true;
}
}  // namespace

bool loadDDS(const wchar_t* path, TextureDescription& textureDescription) {
    textureDescription = {};
    if (path == nullptr) {
        return false;
    }

    std::vector<std::byte> fileBytes;
    if (!readFileBytes(path, fileBytes) || fileBytes.size() < sizeof(std::uint32_t) + sizeof(DdsHeader)) {
        return false;
    }

    std::uint32_t magic = 0;
    std::memcpy(&magic, fileBytes.data(), sizeof(magic));
    if (magic != kDdsMagic) {
        return false;
    }

    DdsHeader header{};
    std::memcpy(&header, fileBytes.data() + sizeof(std::uint32_t), sizeof(header));
    if (header.size != 124u || header.pixelFormat.size != 32u || header.width == 0u || header.height == 0u) {
        return false;
    }

    std::size_t dataOffset = sizeof(std::uint32_t) + sizeof(DdsHeader);
    std::uint32_t arraySize = 1u;
    bool isCubeMap = (header.caps2 & kDdsCaps2Cubemap) != 0u;
    FormatInfo formatInfo{};

    if ((header.pixelFormat.flags & kDdsPixelFormatFourCc) != 0u &&
        header.pixelFormat.fourCC == makeFourCc('D', 'X', '1', '0')) {
        if (fileBytes.size() < dataOffset + sizeof(DdsHeaderDx10)) {
            return false;
        }

        DdsHeaderDx10 headerDx10{};
        std::memcpy(&headerDx10, fileBytes.data() + dataOffset, sizeof(headerDx10));
        if (headerDx10.resourceDimension != kDdsResourceDimensionTexture2D || headerDx10.arraySize == 0u) {
            return false;
        }

        formatInfo = getSupportedFormatInfo(headerDx10.dxgiFormat);
        arraySize = headerDx10.arraySize;
        isCubeMap = (headerDx10.miscFlag & kDdsResourceMiscTextureCube) != 0u;
        dataOffset += sizeof(DdsHeaderDx10);
    } else {
        formatInfo = decodeLegacyFormat(header.pixelFormat);
    }

    if (formatInfo.format == DXGI_FORMAT_UNKNOWN) {
        return false;
    }

    textureDescription.fmt = formatInfo.format;
    textureDescription.width = header.width;
    textureDescription.height = header.height;
    textureDescription.mipmapsCount = (std::max)(1u, header.mipMapCount);
    textureDescription.arraySize = arraySize;
    textureDescription.isCubeMap = isCubeMap;
    textureDescription.subresources.resize(static_cast<std::size_t>(textureDescription.arraySize) *
                                           textureDescription.mipmapsCount);

    std::size_t currentOffset = dataOffset;
    for (std::uint32_t arrayIndex = 0; arrayIndex < textureDescription.arraySize; ++arrayIndex) {
        std::uint32_t mipWidth = textureDescription.width;
        std::uint32_t mipHeight = textureDescription.height;
        for (std::uint32_t mipIndex = 0; mipIndex < textureDescription.mipmapsCount; ++mipIndex) {
            TextureSubresourceLayout layout{};
            if (!computeSubresourceLayout(formatInfo, mipWidth, mipHeight, layout)) {
                return false;
            }

            if (currentOffset + layout.slicePitch > fileBytes.size()) {
                return false;
            }

            layout.dataOffset = currentOffset - dataOffset;
            textureDescription
                .subresources[static_cast<std::size_t>(arrayIndex) * textureDescription.mipmapsCount + mipIndex] =
                layout;
            currentOffset += layout.slicePitch;
            mipWidth = (std::max)(1u, mipWidth / 2u);
            mipHeight = (std::max)(1u, mipHeight / 2u);
        }
    }

    const std::size_t textureSize = currentOffset - dataOffset;
    textureDescription.data.resize(textureSize);
    std::memcpy(textureDescription.data.data(), fileBytes.data() + dataOffset, textureSize);
    return true;
}