#include "Image.h"

#include "common/Log.h"
#include "common/ReaperRoot.h"

#include "SwapchainRendererBase.h"

#include "renderer/texture/GPUTextureProperties.h"
#include "renderer/vulkan/Debug.h"
#include "renderer/vulkan/api/VulkanStringConversion.h"

namespace Reaper
{
PixelFormat VulkanToPixelFormat(VkFormat format)
{
    switch (format)
    {
    case VK_FORMAT_R4G4_UNORM_PACK8:
        return PixelFormat::R4G4_UNORM_PACK8;
    case VK_FORMAT_R4G4B4A4_UNORM_PACK16:
        return PixelFormat::R4G4B4A4_UNORM_PACK16;
    case VK_FORMAT_B4G4R4A4_UNORM_PACK16:
        return PixelFormat::B4G4R4A4_UNORM_PACK16;
    case VK_FORMAT_R5G6B5_UNORM_PACK16:
        return PixelFormat::R5G6B5_UNORM_PACK16;
    case VK_FORMAT_B5G6R5_UNORM_PACK16:
        return PixelFormat::B5G6R5_UNORM_PACK16;
    case VK_FORMAT_R5G5B5A1_UNORM_PACK16:
        return PixelFormat::R5G5B5A1_UNORM_PACK16;
    case VK_FORMAT_B5G5R5A1_UNORM_PACK16:
        return PixelFormat::B5G5R5A1_UNORM_PACK16;
    case VK_FORMAT_A1R5G5B5_UNORM_PACK16:
        return PixelFormat::A1R5G5B5_UNORM_PACK16;
    case VK_FORMAT_R8_UNORM:
        return PixelFormat::R8_UNORM;
    case VK_FORMAT_R8_SNORM:
        return PixelFormat::R8_SNORM;
    case VK_FORMAT_R8_USCALED:
        return PixelFormat::R8_USCALED;
    case VK_FORMAT_R8_SSCALED:
        return PixelFormat::R8_SSCALED;
    case VK_FORMAT_R8_UINT:
        return PixelFormat::R8_UINT;
    case VK_FORMAT_R8_SINT:
        return PixelFormat::R8_SINT;
    case VK_FORMAT_R8_SRGB:
        return PixelFormat::R8_SRGB;
    case VK_FORMAT_R8G8_UNORM:
        return PixelFormat::R8G8_UNORM;
    case VK_FORMAT_R8G8_SNORM:
        return PixelFormat::R8G8_SNORM;
    case VK_FORMAT_R8G8_USCALED:
        return PixelFormat::R8G8_USCALED;
    case VK_FORMAT_R8G8_SSCALED:
        return PixelFormat::R8G8_SSCALED;
    case VK_FORMAT_R8G8_UINT:
        return PixelFormat::R8G8_UINT;
    case VK_FORMAT_R8G8_SINT:
        return PixelFormat::R8G8_SINT;
    case VK_FORMAT_R8G8_SRGB:
        return PixelFormat::R8G8_SRGB;
    case VK_FORMAT_R8G8B8_UNORM:
        return PixelFormat::R8G8B8_UNORM;
    case VK_FORMAT_R8G8B8_SNORM:
        return PixelFormat::R8G8B8_SNORM;
    case VK_FORMAT_R8G8B8_USCALED:
        return PixelFormat::R8G8B8_USCALED;
    case VK_FORMAT_R8G8B8_SSCALED:
        return PixelFormat::R8G8B8_SSCALED;
    case VK_FORMAT_R8G8B8_UINT:
        return PixelFormat::R8G8B8_UINT;
    case VK_FORMAT_R8G8B8_SINT:
        return PixelFormat::R8G8B8_SINT;
    case VK_FORMAT_R8G8B8_SRGB:
        return PixelFormat::R8G8B8_SRGB;
    case VK_FORMAT_B8G8R8_UNORM:
        return PixelFormat::B8G8R8_UNORM;
    case VK_FORMAT_B8G8R8_SNORM:
        return PixelFormat::B8G8R8_SNORM;
    case VK_FORMAT_B8G8R8_USCALED:
        return PixelFormat::B8G8R8_USCALED;
    case VK_FORMAT_B8G8R8_SSCALED:
        return PixelFormat::B8G8R8_SSCALED;
    case VK_FORMAT_B8G8R8_UINT:
        return PixelFormat::B8G8R8_UINT;
    case VK_FORMAT_B8G8R8_SINT:
        return PixelFormat::B8G8R8_SINT;
    case VK_FORMAT_B8G8R8_SRGB:
        return PixelFormat::B8G8R8_SRGB;
    case VK_FORMAT_R8G8B8A8_UNORM:
        return PixelFormat::R8G8B8A8_UNORM;
    case VK_FORMAT_R8G8B8A8_SNORM:
        return PixelFormat::R8G8B8A8_SNORM;
    case VK_FORMAT_R8G8B8A8_USCALED:
        return PixelFormat::R8G8B8A8_USCALED;
    case VK_FORMAT_R8G8B8A8_SSCALED:
        return PixelFormat::R8G8B8A8_SSCALED;
    case VK_FORMAT_R8G8B8A8_UINT:
        return PixelFormat::R8G8B8A8_UINT;
    case VK_FORMAT_R8G8B8A8_SINT:
        return PixelFormat::R8G8B8A8_SINT;
    case VK_FORMAT_R8G8B8A8_SRGB:
        return PixelFormat::R8G8B8A8_SRGB;
    case VK_FORMAT_B8G8R8A8_UNORM:
        return PixelFormat::B8G8R8A8_UNORM;
    case VK_FORMAT_B8G8R8A8_SNORM:
        return PixelFormat::B8G8R8A8_SNORM;
    case VK_FORMAT_B8G8R8A8_USCALED:
        return PixelFormat::B8G8R8A8_USCALED;
    case VK_FORMAT_B8G8R8A8_SSCALED:
        return PixelFormat::B8G8R8A8_SSCALED;
    case VK_FORMAT_B8G8R8A8_UINT:
        return PixelFormat::B8G8R8A8_UINT;
    case VK_FORMAT_B8G8R8A8_SINT:
        return PixelFormat::B8G8R8A8_SINT;
    case VK_FORMAT_B8G8R8A8_SRGB:
        return PixelFormat::B8G8R8A8_SRGB;
    case VK_FORMAT_A8B8G8R8_UNORM_PACK32:
        return PixelFormat::A8B8G8R8_UNORM_PACK32;
    case VK_FORMAT_A8B8G8R8_SNORM_PACK32:
        return PixelFormat::A8B8G8R8_SNORM_PACK32;
    case VK_FORMAT_A8B8G8R8_USCALED_PACK32:
        return PixelFormat::A8B8G8R8_USCALED_PACK32;
    case VK_FORMAT_A8B8G8R8_SSCALED_PACK32:
        return PixelFormat::A8B8G8R8_SSCALED_PACK32;
    case VK_FORMAT_A8B8G8R8_UINT_PACK32:
        return PixelFormat::A8B8G8R8_UINT_PACK32;
    case VK_FORMAT_A8B8G8R8_SINT_PACK32:
        return PixelFormat::A8B8G8R8_SINT_PACK32;
    case VK_FORMAT_A8B8G8R8_SRGB_PACK32:
        return PixelFormat::A8B8G8R8_SRGB_PACK32;
    case VK_FORMAT_A2R10G10B10_UNORM_PACK32:
        return PixelFormat::A2R10G10B10_UNORM_PACK32;
    case VK_FORMAT_A2R10G10B10_SNORM_PACK32:
        return PixelFormat::A2R10G10B10_SNORM_PACK32;
    case VK_FORMAT_A2R10G10B10_USCALED_PACK32:
        return PixelFormat::A2R10G10B10_USCALED_PACK32;
    case VK_FORMAT_A2R10G10B10_SSCALED_PACK32:
        return PixelFormat::A2R10G10B10_SSCALED_PACK32;
    case VK_FORMAT_A2R10G10B10_UINT_PACK32:
        return PixelFormat::A2R10G10B10_UINT_PACK32;
    case VK_FORMAT_A2R10G10B10_SINT_PACK32:
        return PixelFormat::A2R10G10B10_SINT_PACK32;
    case VK_FORMAT_A2B10G10R10_UNORM_PACK32:
        return PixelFormat::A2B10G10R10_UNORM_PACK32;
    case VK_FORMAT_A2B10G10R10_SNORM_PACK32:
        return PixelFormat::A2B10G10R10_SNORM_PACK32;
    case VK_FORMAT_A2B10G10R10_USCALED_PACK32:
        return PixelFormat::A2B10G10R10_USCALED_PACK32;
    case VK_FORMAT_A2B10G10R10_SSCALED_PACK32:
        return PixelFormat::A2B10G10R10_SSCALED_PACK32;
    case VK_FORMAT_A2B10G10R10_UINT_PACK32:
        return PixelFormat::A2B10G10R10_UINT_PACK32;
    case VK_FORMAT_A2B10G10R10_SINT_PACK32:
        return PixelFormat::A2B10G10R10_SINT_PACK32;
    case VK_FORMAT_R16_UNORM:
        return PixelFormat::R16_UNORM;
    case VK_FORMAT_R16_SNORM:
        return PixelFormat::R16_SNORM;
    case VK_FORMAT_R16_USCALED:
        return PixelFormat::R16_USCALED;
    case VK_FORMAT_R16_SSCALED:
        return PixelFormat::R16_SSCALED;
    case VK_FORMAT_R16_UINT:
        return PixelFormat::R16_UINT;
    case VK_FORMAT_R16_SINT:
        return PixelFormat::R16_SINT;
    case VK_FORMAT_R16_SFLOAT:
        return PixelFormat::R16_SFLOAT;
    case VK_FORMAT_R16G16_UNORM:
        return PixelFormat::R16G16_UNORM;
    case VK_FORMAT_R16G16_SNORM:
        return PixelFormat::R16G16_SNORM;
    case VK_FORMAT_R16G16_USCALED:
        return PixelFormat::R16G16_USCALED;
    case VK_FORMAT_R16G16_SSCALED:
        return PixelFormat::R16G16_SSCALED;
    case VK_FORMAT_R16G16_UINT:
        return PixelFormat::R16G16_UINT;
    case VK_FORMAT_R16G16_SINT:
        return PixelFormat::R16G16_SINT;
    case VK_FORMAT_R16G16_SFLOAT:
        return PixelFormat::R16G16_SFLOAT;
    case VK_FORMAT_R16G16B16_UNORM:
        return PixelFormat::R16G16B16_UNORM;
    case VK_FORMAT_R16G16B16_SNORM:
        return PixelFormat::R16G16B16_SNORM;
    case VK_FORMAT_R16G16B16_USCALED:
        return PixelFormat::R16G16B16_USCALED;
    case VK_FORMAT_R16G16B16_SSCALED:
        return PixelFormat::R16G16B16_SSCALED;
    case VK_FORMAT_R16G16B16_UINT:
        return PixelFormat::R16G16B16_UINT;
    case VK_FORMAT_R16G16B16_SINT:
        return PixelFormat::R16G16B16_SINT;
    case VK_FORMAT_R16G16B16_SFLOAT:
        return PixelFormat::R16G16B16_SFLOAT;
    case VK_FORMAT_R16G16B16A16_UNORM:
        return PixelFormat::R16G16B16A16_UNORM;
    case VK_FORMAT_R16G16B16A16_SNORM:
        return PixelFormat::R16G16B16A16_SNORM;
    case VK_FORMAT_R16G16B16A16_USCALED:
        return PixelFormat::R16G16B16A16_USCALED;
    case VK_FORMAT_R16G16B16A16_SSCALED:
        return PixelFormat::R16G16B16A16_SSCALED;
    case VK_FORMAT_R16G16B16A16_UINT:
        return PixelFormat::R16G16B16A16_UINT;
    case VK_FORMAT_R16G16B16A16_SINT:
        return PixelFormat::R16G16B16A16_SINT;
    case VK_FORMAT_R16G16B16A16_SFLOAT:
        return PixelFormat::R16G16B16A16_SFLOAT;
    case VK_FORMAT_R32_UINT:
        return PixelFormat::R32_UINT;
    case VK_FORMAT_R32_SINT:
        return PixelFormat::R32_SINT;
    case VK_FORMAT_R32_SFLOAT:
        return PixelFormat::R32_SFLOAT;
    case VK_FORMAT_R32G32_UINT:
        return PixelFormat::R32G32_UINT;
    case VK_FORMAT_R32G32_SINT:
        return PixelFormat::R32G32_SINT;
    case VK_FORMAT_R32G32_SFLOAT:
        return PixelFormat::R32G32_SFLOAT;
    case VK_FORMAT_R32G32B32_UINT:
        return PixelFormat::R32G32B32_UINT;
    case VK_FORMAT_R32G32B32_SINT:
        return PixelFormat::R32G32B32_SINT;
    case VK_FORMAT_R32G32B32_SFLOAT:
        return PixelFormat::R32G32B32_SFLOAT;
    case VK_FORMAT_R32G32B32A32_UINT:
        return PixelFormat::R32G32B32A32_UINT;
    case VK_FORMAT_R32G32B32A32_SINT:
        return PixelFormat::R32G32B32A32_SINT;
    case VK_FORMAT_R32G32B32A32_SFLOAT:
        return PixelFormat::R32G32B32A32_SFLOAT;
    case VK_FORMAT_R64_UINT:
        return PixelFormat::R64_UINT;
    case VK_FORMAT_R64_SINT:
        return PixelFormat::R64_SINT;
    case VK_FORMAT_R64_SFLOAT:
        return PixelFormat::R64_SFLOAT;
    case VK_FORMAT_R64G64_UINT:
        return PixelFormat::R64G64_UINT;
    case VK_FORMAT_R64G64_SINT:
        return PixelFormat::R64G64_SINT;
    case VK_FORMAT_R64G64_SFLOAT:
        return PixelFormat::R64G64_SFLOAT;
    case VK_FORMAT_R64G64B64_UINT:
        return PixelFormat::R64G64B64_UINT;
    case VK_FORMAT_R64G64B64_SINT:
        return PixelFormat::R64G64B64_SINT;
    case VK_FORMAT_R64G64B64_SFLOAT:
        return PixelFormat::R64G64B64_SFLOAT;
    case VK_FORMAT_R64G64B64A64_UINT:
        return PixelFormat::R64G64B64A64_UINT;
    case VK_FORMAT_R64G64B64A64_SINT:
        return PixelFormat::R64G64B64A64_SINT;
    case VK_FORMAT_R64G64B64A64_SFLOAT:
        return PixelFormat::R64G64B64A64_SFLOAT;
    case VK_FORMAT_B10G11R11_UFLOAT_PACK32:
        return PixelFormat::B10G11R11_UFLOAT_PACK32;
    case VK_FORMAT_E5B9G9R9_UFLOAT_PACK32:
        return PixelFormat::E5B9G9R9_UFLOAT_PACK32;
    case VK_FORMAT_D16_UNORM:
        return PixelFormat::D16_UNORM;
    case VK_FORMAT_X8_D24_UNORM_PACK32:
        return PixelFormat::X8_D24_UNORM_PACK32;
    case VK_FORMAT_D32_SFLOAT:
        return PixelFormat::D32_SFLOAT;
    case VK_FORMAT_S8_UINT:
        return PixelFormat::S8_UINT;
    case VK_FORMAT_D16_UNORM_S8_UINT:
        return PixelFormat::D16_UNORM_S8_UINT;
    case VK_FORMAT_D24_UNORM_S8_UINT:
        return PixelFormat::D24_UNORM_S8_UINT;
    case VK_FORMAT_D32_SFLOAT_S8_UINT:
        return PixelFormat::D32_SFLOAT_S8_UINT;
    case VK_FORMAT_BC1_RGB_UNORM_BLOCK:
        return PixelFormat::BC1_RGB_UNORM_BLOCK;
    case VK_FORMAT_BC1_RGB_SRGB_BLOCK:
        return PixelFormat::BC1_RGB_SRGB_BLOCK;
    case VK_FORMAT_BC1_RGBA_UNORM_BLOCK:
        return PixelFormat::BC1_RGBA_UNORM_BLOCK;
    case VK_FORMAT_BC1_RGBA_SRGB_BLOCK:
        return PixelFormat::BC1_RGBA_SRGB_BLOCK;
    case VK_FORMAT_BC2_UNORM_BLOCK:
        return PixelFormat::BC2_UNORM_BLOCK;
    case VK_FORMAT_BC2_SRGB_BLOCK:
        return PixelFormat::BC2_SRGB_BLOCK;
    case VK_FORMAT_BC3_UNORM_BLOCK:
        return PixelFormat::BC3_UNORM_BLOCK;
    case VK_FORMAT_BC3_SRGB_BLOCK:
        return PixelFormat::BC3_SRGB_BLOCK;
    case VK_FORMAT_BC4_UNORM_BLOCK:
        return PixelFormat::BC4_UNORM_BLOCK;
    case VK_FORMAT_BC4_SNORM_BLOCK:
        return PixelFormat::BC4_SNORM_BLOCK;
    case VK_FORMAT_BC5_UNORM_BLOCK:
        return PixelFormat::BC5_UNORM_BLOCK;
    case VK_FORMAT_BC5_SNORM_BLOCK:
        return PixelFormat::BC5_SNORM_BLOCK;
    case VK_FORMAT_BC6H_UFLOAT_BLOCK:
        return PixelFormat::BC6H_UFLOAT_BLOCK;
    case VK_FORMAT_BC6H_SFLOAT_BLOCK:
        return PixelFormat::BC6H_SFLOAT_BLOCK;
    case VK_FORMAT_BC7_UNORM_BLOCK:
        return PixelFormat::BC7_UNORM_BLOCK;
    case VK_FORMAT_BC7_SRGB_BLOCK:
        return PixelFormat::BC7_SRGB_BLOCK;
    case VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK:
        return PixelFormat::ETC2_R8G8B8_UNORM_BLOCK;
    case VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK:
        return PixelFormat::ETC2_R8G8B8_SRGB_BLOCK;
    case VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK:
        return PixelFormat::ETC2_R8G8B8A1_UNORM_BLOCK;
    case VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK:
        return PixelFormat::ETC2_R8G8B8A1_SRGB_BLOCK;
    case VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK:
        return PixelFormat::ETC2_R8G8B8A8_UNORM_BLOCK;
    case VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK:
        return PixelFormat::ETC2_R8G8B8A8_SRGB_BLOCK;
    case VK_FORMAT_EAC_R11_UNORM_BLOCK:
        return PixelFormat::EAC_R11_UNORM_BLOCK;
    case VK_FORMAT_EAC_R11_SNORM_BLOCK:
        return PixelFormat::EAC_R11_SNORM_BLOCK;
    case VK_FORMAT_EAC_R11G11_UNORM_BLOCK:
        return PixelFormat::EAC_R11G11_UNORM_BLOCK;
    case VK_FORMAT_EAC_R11G11_SNORM_BLOCK:
        return PixelFormat::EAC_R11G11_SNORM_BLOCK;
    case VK_FORMAT_ASTC_4x4_UNORM_BLOCK:
        return PixelFormat::ASTC_4x4_UNORM_BLOCK;
    case VK_FORMAT_ASTC_4x4_SRGB_BLOCK:
        return PixelFormat::ASTC_4x4_SRGB_BLOCK;
    case VK_FORMAT_ASTC_5x4_UNORM_BLOCK:
        return PixelFormat::ASTC_5x4_UNORM_BLOCK;
    case VK_FORMAT_ASTC_5x4_SRGB_BLOCK:
        return PixelFormat::ASTC_5x4_SRGB_BLOCK;
    case VK_FORMAT_ASTC_5x5_UNORM_BLOCK:
        return PixelFormat::ASTC_5x5_UNORM_BLOCK;
    case VK_FORMAT_ASTC_5x5_SRGB_BLOCK:
        return PixelFormat::ASTC_5x5_SRGB_BLOCK;
    case VK_FORMAT_ASTC_6x5_UNORM_BLOCK:
        return PixelFormat::ASTC_6x5_UNORM_BLOCK;
    case VK_FORMAT_ASTC_6x5_SRGB_BLOCK:
        return PixelFormat::ASTC_6x5_SRGB_BLOCK;
    case VK_FORMAT_ASTC_6x6_UNORM_BLOCK:
        return PixelFormat::ASTC_6x6_UNORM_BLOCK;
    case VK_FORMAT_ASTC_6x6_SRGB_BLOCK:
        return PixelFormat::ASTC_6x6_SRGB_BLOCK;
    case VK_FORMAT_ASTC_8x5_UNORM_BLOCK:
        return PixelFormat::ASTC_8x5_UNORM_BLOCK;
    case VK_FORMAT_ASTC_8x5_SRGB_BLOCK:
        return PixelFormat::ASTC_8x5_SRGB_BLOCK;
    case VK_FORMAT_ASTC_8x6_UNORM_BLOCK:
        return PixelFormat::ASTC_8x6_UNORM_BLOCK;
    case VK_FORMAT_ASTC_8x6_SRGB_BLOCK:
        return PixelFormat::ASTC_8x6_SRGB_BLOCK;
    case VK_FORMAT_ASTC_8x8_UNORM_BLOCK:
        return PixelFormat::ASTC_8x8_UNORM_BLOCK;
    case VK_FORMAT_ASTC_8x8_SRGB_BLOCK:
        return PixelFormat::ASTC_8x8_SRGB_BLOCK;
    case VK_FORMAT_ASTC_10x5_UNORM_BLOCK:
        return PixelFormat::ASTC_10x5_UNORM_BLOCK;
    case VK_FORMAT_ASTC_10x5_SRGB_BLOCK:
        return PixelFormat::ASTC_10x5_SRGB_BLOCK;
    case VK_FORMAT_ASTC_10x6_UNORM_BLOCK:
        return PixelFormat::ASTC_10x6_UNORM_BLOCK;
    case VK_FORMAT_ASTC_10x6_SRGB_BLOCK:
        return PixelFormat::ASTC_10x6_SRGB_BLOCK;
    case VK_FORMAT_ASTC_10x8_UNORM_BLOCK:
        return PixelFormat::ASTC_10x8_UNORM_BLOCK;
    case VK_FORMAT_ASTC_10x8_SRGB_BLOCK:
        return PixelFormat::ASTC_10x8_SRGB_BLOCK;
    case VK_FORMAT_ASTC_10x10_UNORM_BLOCK:
        return PixelFormat::ASTC_10x10_UNORM_BLOCK;
    case VK_FORMAT_ASTC_10x10_SRGB_BLOCK:
        return PixelFormat::ASTC_10x10_SRGB_BLOCK;
    case VK_FORMAT_ASTC_12x10_UNORM_BLOCK:
        return PixelFormat::ASTC_12x10_UNORM_BLOCK;
    case VK_FORMAT_ASTC_12x10_SRGB_BLOCK:
        return PixelFormat::ASTC_12x10_SRGB_BLOCK;
    case VK_FORMAT_ASTC_12x12_UNORM_BLOCK:
        return PixelFormat::ASTC_12x12_UNORM_BLOCK;
    case VK_FORMAT_ASTC_12x12_SRGB_BLOCK:
        return PixelFormat::ASTC_12x12_SRGB_BLOCK;
    case VK_FORMAT_G8B8G8R8_422_UNORM:
        return PixelFormat::G8B8G8R8_422_UNORM;
    case VK_FORMAT_B8G8R8G8_422_UNORM:
        return PixelFormat::B8G8R8G8_422_UNORM;
    case VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM:
        return PixelFormat::G8_B8_R8_3PLANE_420_UNORM;
    case VK_FORMAT_G8_B8R8_2PLANE_420_UNORM:
        return PixelFormat::G8_B8R8_2PLANE_420_UNORM;
    case VK_FORMAT_G8_B8_R8_3PLANE_422_UNORM:
        return PixelFormat::G8_B8_R8_3PLANE_422_UNORM;
    case VK_FORMAT_G8_B8R8_2PLANE_422_UNORM:
        return PixelFormat::G8_B8R8_2PLANE_422_UNORM;
    case VK_FORMAT_G8_B8_R8_3PLANE_444_UNORM:
        return PixelFormat::G8_B8_R8_3PLANE_444_UNORM;
    case VK_FORMAT_R10X6_UNORM_PACK16:
        return PixelFormat::R10X6_UNORM_PACK16;
    case VK_FORMAT_R10X6G10X6_UNORM_2PACK16:
        return PixelFormat::R10X6G10X6_UNORM_2PACK16;
    case VK_FORMAT_R10X6G10X6B10X6A10X6_UNORM_4PACK16:
        return PixelFormat::R10X6G10X6B10X6A10X6_UNORM_4PACK16;
    case VK_FORMAT_G10X6B10X6G10X6R10X6_422_UNORM_4PACK16:
        return PixelFormat::G10X6B10X6G10X6R10X6_422_UNORM_4PACK16;
    case VK_FORMAT_B10X6G10X6R10X6G10X6_422_UNORM_4PACK16:
        return PixelFormat::B10X6G10X6R10X6G10X6_422_UNORM_4PACK16;
    case VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16:
        return PixelFormat::G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16;
    case VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16:
        return PixelFormat::G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16;
    case VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16:
        return PixelFormat::G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16;
    case VK_FORMAT_G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16:
        return PixelFormat::G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16;
    case VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16:
        return PixelFormat::G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16;
    case VK_FORMAT_R12X4_UNORM_PACK16:
        return PixelFormat::R12X4_UNORM_PACK16;
    case VK_FORMAT_R12X4G12X4_UNORM_2PACK16:
        return PixelFormat::R12X4G12X4_UNORM_2PACK16;
    case VK_FORMAT_R12X4G12X4B12X4A12X4_UNORM_4PACK16:
        return PixelFormat::R12X4G12X4B12X4A12X4_UNORM_4PACK16;
    case VK_FORMAT_G12X4B12X4G12X4R12X4_422_UNORM_4PACK16:
        return PixelFormat::G12X4B12X4G12X4R12X4_422_UNORM_4PACK16;
    case VK_FORMAT_B12X4G12X4R12X4G12X4_422_UNORM_4PACK16:
        return PixelFormat::B12X4G12X4R12X4G12X4_422_UNORM_4PACK16;
    case VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16:
        return PixelFormat::G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16;
    case VK_FORMAT_G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16:
        return PixelFormat::G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16;
    case VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16:
        return PixelFormat::G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16;
    case VK_FORMAT_G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16:
        return PixelFormat::G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16;
    case VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16:
        return PixelFormat::G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16;
    case VK_FORMAT_G16B16G16R16_422_UNORM:
        return PixelFormat::G16B16G16R16_422_UNORM;
    case VK_FORMAT_B16G16R16G16_422_UNORM:
        return PixelFormat::B16G16R16G16_422_UNORM;
    case VK_FORMAT_G16_B16_R16_3PLANE_420_UNORM:
        return PixelFormat::G16_B16_R16_3PLANE_420_UNORM;
    case VK_FORMAT_G16_B16R16_2PLANE_420_UNORM:
        return PixelFormat::G16_B16R16_2PLANE_420_UNORM;
    case VK_FORMAT_G16_B16_R16_3PLANE_422_UNORM:
        return PixelFormat::G16_B16_R16_3PLANE_422_UNORM;
    case VK_FORMAT_G16_B16R16_2PLANE_422_UNORM:
        return PixelFormat::G16_B16R16_2PLANE_422_UNORM;
    case VK_FORMAT_G16_B16_R16_3PLANE_444_UNORM:
        return PixelFormat::G16_B16_R16_3PLANE_444_UNORM;
    case VK_FORMAT_PVRTC1_2BPP_UNORM_BLOCK_IMG:
        return PixelFormat::PVRTC1_2BPP_UNORM_BLOCK_IMG;
    case VK_FORMAT_PVRTC1_4BPP_UNORM_BLOCK_IMG:
        return PixelFormat::PVRTC1_4BPP_UNORM_BLOCK_IMG;
    case VK_FORMAT_PVRTC2_2BPP_UNORM_BLOCK_IMG:
        return PixelFormat::PVRTC2_2BPP_UNORM_BLOCK_IMG;
    case VK_FORMAT_PVRTC2_4BPP_UNORM_BLOCK_IMG:
        return PixelFormat::PVRTC2_4BPP_UNORM_BLOCK_IMG;
    case VK_FORMAT_PVRTC1_2BPP_SRGB_BLOCK_IMG:
        return PixelFormat::PVRTC1_2BPP_SRGB_BLOCK_IMG;
    case VK_FORMAT_PVRTC1_4BPP_SRGB_BLOCK_IMG:
        return PixelFormat::PVRTC1_4BPP_SRGB_BLOCK_IMG;
    case VK_FORMAT_PVRTC2_2BPP_SRGB_BLOCK_IMG:
        return PixelFormat::PVRTC2_2BPP_SRGB_BLOCK_IMG;
    case VK_FORMAT_PVRTC2_4BPP_SRGB_BLOCK_IMG:
        return PixelFormat::PVRTC2_4BPP_SRGB_BLOCK_IMG;
    case VK_FORMAT_UNDEFINED:
    default:
        AssertUnreachable();
    }

    return PixelFormat::Unknown;
}

VkFormat PixelFormatToVulkan(PixelFormat format)
{
    switch (format)
    {
    case PixelFormat::R4G4_UNORM_PACK8:
        return VK_FORMAT_R4G4_UNORM_PACK8;
    case PixelFormat::R4G4B4A4_UNORM_PACK16:
        return VK_FORMAT_R4G4B4A4_UNORM_PACK16;
    case PixelFormat::B4G4R4A4_UNORM_PACK16:
        return VK_FORMAT_B4G4R4A4_UNORM_PACK16;
    case PixelFormat::R5G6B5_UNORM_PACK16:
        return VK_FORMAT_R5G6B5_UNORM_PACK16;
    case PixelFormat::B5G6R5_UNORM_PACK16:
        return VK_FORMAT_B5G6R5_UNORM_PACK16;
    case PixelFormat::R5G5B5A1_UNORM_PACK16:
        return VK_FORMAT_R5G5B5A1_UNORM_PACK16;
    case PixelFormat::B5G5R5A1_UNORM_PACK16:
        return VK_FORMAT_B5G5R5A1_UNORM_PACK16;
    case PixelFormat::A1R5G5B5_UNORM_PACK16:
        return VK_FORMAT_A1R5G5B5_UNORM_PACK16;
    case PixelFormat::R8_UNORM:
        return VK_FORMAT_R8_UNORM;
    case PixelFormat::R8_SNORM:
        return VK_FORMAT_R8_SNORM;
    case PixelFormat::R8_USCALED:
        return VK_FORMAT_R8_USCALED;
    case PixelFormat::R8_SSCALED:
        return VK_FORMAT_R8_SSCALED;
    case PixelFormat::R8_UINT:
        return VK_FORMAT_R8_UINT;
    case PixelFormat::R8_SINT:
        return VK_FORMAT_R8_SINT;
    case PixelFormat::R8_SRGB:
        return VK_FORMAT_R8_SRGB;
    case PixelFormat::R8G8_UNORM:
        return VK_FORMAT_R8G8_UNORM;
    case PixelFormat::R8G8_SNORM:
        return VK_FORMAT_R8G8_SNORM;
    case PixelFormat::R8G8_USCALED:
        return VK_FORMAT_R8G8_USCALED;
    case PixelFormat::R8G8_SSCALED:
        return VK_FORMAT_R8G8_SSCALED;
    case PixelFormat::R8G8_UINT:
        return VK_FORMAT_R8G8_UINT;
    case PixelFormat::R8G8_SINT:
        return VK_FORMAT_R8G8_SINT;
    case PixelFormat::R8G8_SRGB:
        return VK_FORMAT_R8G8_SRGB;
    case PixelFormat::R8G8B8_UNORM:
        return VK_FORMAT_R8G8B8_UNORM;
    case PixelFormat::R8G8B8_SNORM:
        return VK_FORMAT_R8G8B8_SNORM;
    case PixelFormat::R8G8B8_USCALED:
        return VK_FORMAT_R8G8B8_USCALED;
    case PixelFormat::R8G8B8_SSCALED:
        return VK_FORMAT_R8G8B8_SSCALED;
    case PixelFormat::R8G8B8_UINT:
        return VK_FORMAT_R8G8B8_UINT;
    case PixelFormat::R8G8B8_SINT:
        return VK_FORMAT_R8G8B8_SINT;
    case PixelFormat::R8G8B8_SRGB:
        return VK_FORMAT_R8G8B8_SRGB;
    case PixelFormat::B8G8R8_UNORM:
        return VK_FORMAT_B8G8R8_UNORM;
    case PixelFormat::B8G8R8_SNORM:
        return VK_FORMAT_B8G8R8_SNORM;
    case PixelFormat::B8G8R8_USCALED:
        return VK_FORMAT_B8G8R8_USCALED;
    case PixelFormat::B8G8R8_SSCALED:
        return VK_FORMAT_B8G8R8_SSCALED;
    case PixelFormat::B8G8R8_UINT:
        return VK_FORMAT_B8G8R8_UINT;
    case PixelFormat::B8G8R8_SINT:
        return VK_FORMAT_B8G8R8_SINT;
    case PixelFormat::B8G8R8_SRGB:
        return VK_FORMAT_B8G8R8_SRGB;
    case PixelFormat::R8G8B8A8_UNORM:
        return VK_FORMAT_R8G8B8A8_UNORM;
    case PixelFormat::R8G8B8A8_SNORM:
        return VK_FORMAT_R8G8B8A8_SNORM;
    case PixelFormat::R8G8B8A8_USCALED:
        return VK_FORMAT_R8G8B8A8_USCALED;
    case PixelFormat::R8G8B8A8_SSCALED:
        return VK_FORMAT_R8G8B8A8_SSCALED;
    case PixelFormat::R8G8B8A8_UINT:
        return VK_FORMAT_R8G8B8A8_UINT;
    case PixelFormat::R8G8B8A8_SINT:
        return VK_FORMAT_R8G8B8A8_SINT;
    case PixelFormat::R8G8B8A8_SRGB:
        return VK_FORMAT_R8G8B8A8_SRGB;
    case PixelFormat::B8G8R8A8_UNORM:
        return VK_FORMAT_B8G8R8A8_UNORM;
    case PixelFormat::B8G8R8A8_SNORM:
        return VK_FORMAT_B8G8R8A8_SNORM;
    case PixelFormat::B8G8R8A8_USCALED:
        return VK_FORMAT_B8G8R8A8_USCALED;
    case PixelFormat::B8G8R8A8_SSCALED:
        return VK_FORMAT_B8G8R8A8_SSCALED;
    case PixelFormat::B8G8R8A8_UINT:
        return VK_FORMAT_B8G8R8A8_UINT;
    case PixelFormat::B8G8R8A8_SINT:
        return VK_FORMAT_B8G8R8A8_SINT;
    case PixelFormat::B8G8R8A8_SRGB:
        return VK_FORMAT_B8G8R8A8_SRGB;
    case PixelFormat::A8B8G8R8_UNORM_PACK32:
        return VK_FORMAT_A8B8G8R8_UNORM_PACK32;
    case PixelFormat::A8B8G8R8_SNORM_PACK32:
        return VK_FORMAT_A8B8G8R8_SNORM_PACK32;
    case PixelFormat::A8B8G8R8_USCALED_PACK32:
        return VK_FORMAT_A8B8G8R8_USCALED_PACK32;
    case PixelFormat::A8B8G8R8_SSCALED_PACK32:
        return VK_FORMAT_A8B8G8R8_SSCALED_PACK32;
    case PixelFormat::A8B8G8R8_UINT_PACK32:
        return VK_FORMAT_A8B8G8R8_UINT_PACK32;
    case PixelFormat::A8B8G8R8_SINT_PACK32:
        return VK_FORMAT_A8B8G8R8_SINT_PACK32;
    case PixelFormat::A8B8G8R8_SRGB_PACK32:
        return VK_FORMAT_A8B8G8R8_SRGB_PACK32;
    case PixelFormat::A2R10G10B10_UNORM_PACK32:
        return VK_FORMAT_A2R10G10B10_UNORM_PACK32;
    case PixelFormat::A2R10G10B10_SNORM_PACK32:
        return VK_FORMAT_A2R10G10B10_SNORM_PACK32;
    case PixelFormat::A2R10G10B10_USCALED_PACK32:
        return VK_FORMAT_A2R10G10B10_USCALED_PACK32;
    case PixelFormat::A2R10G10B10_SSCALED_PACK32:
        return VK_FORMAT_A2R10G10B10_SSCALED_PACK32;
    case PixelFormat::A2R10G10B10_UINT_PACK32:
        return VK_FORMAT_A2R10G10B10_UINT_PACK32;
    case PixelFormat::A2R10G10B10_SINT_PACK32:
        return VK_FORMAT_A2R10G10B10_SINT_PACK32;
    case PixelFormat::A2B10G10R10_UNORM_PACK32:
        return VK_FORMAT_A2B10G10R10_UNORM_PACK32;
    case PixelFormat::A2B10G10R10_SNORM_PACK32:
        return VK_FORMAT_A2B10G10R10_SNORM_PACK32;
    case PixelFormat::A2B10G10R10_USCALED_PACK32:
        return VK_FORMAT_A2B10G10R10_USCALED_PACK32;
    case PixelFormat::A2B10G10R10_SSCALED_PACK32:
        return VK_FORMAT_A2B10G10R10_SSCALED_PACK32;
    case PixelFormat::A2B10G10R10_UINT_PACK32:
        return VK_FORMAT_A2B10G10R10_UINT_PACK32;
    case PixelFormat::A2B10G10R10_SINT_PACK32:
        return VK_FORMAT_A2B10G10R10_SINT_PACK32;
    case PixelFormat::R16_UNORM:
        return VK_FORMAT_R16_UNORM;
    case PixelFormat::R16_SNORM:
        return VK_FORMAT_R16_SNORM;
    case PixelFormat::R16_USCALED:
        return VK_FORMAT_R16_USCALED;
    case PixelFormat::R16_SSCALED:
        return VK_FORMAT_R16_SSCALED;
    case PixelFormat::R16_UINT:
        return VK_FORMAT_R16_UINT;
    case PixelFormat::R16_SINT:
        return VK_FORMAT_R16_SINT;
    case PixelFormat::R16_SFLOAT:
        return VK_FORMAT_R16_SFLOAT;
    case PixelFormat::R16G16_UNORM:
        return VK_FORMAT_R16G16_UNORM;
    case PixelFormat::R16G16_SNORM:
        return VK_FORMAT_R16G16_SNORM;
    case PixelFormat::R16G16_USCALED:
        return VK_FORMAT_R16G16_USCALED;
    case PixelFormat::R16G16_SSCALED:
        return VK_FORMAT_R16G16_SSCALED;
    case PixelFormat::R16G16_UINT:
        return VK_FORMAT_R16G16_UINT;
    case PixelFormat::R16G16_SINT:
        return VK_FORMAT_R16G16_SINT;
    case PixelFormat::R16G16_SFLOAT:
        return VK_FORMAT_R16G16_SFLOAT;
    case PixelFormat::R16G16B16_UNORM:
        return VK_FORMAT_R16G16B16_UNORM;
    case PixelFormat::R16G16B16_SNORM:
        return VK_FORMAT_R16G16B16_SNORM;
    case PixelFormat::R16G16B16_USCALED:
        return VK_FORMAT_R16G16B16_USCALED;
    case PixelFormat::R16G16B16_SSCALED:
        return VK_FORMAT_R16G16B16_SSCALED;
    case PixelFormat::R16G16B16_UINT:
        return VK_FORMAT_R16G16B16_UINT;
    case PixelFormat::R16G16B16_SINT:
        return VK_FORMAT_R16G16B16_SINT;
    case PixelFormat::R16G16B16_SFLOAT:
        return VK_FORMAT_R16G16B16_SFLOAT;
    case PixelFormat::R16G16B16A16_UNORM:
        return VK_FORMAT_R16G16B16A16_UNORM;
    case PixelFormat::R16G16B16A16_SNORM:
        return VK_FORMAT_R16G16B16A16_SNORM;
    case PixelFormat::R16G16B16A16_USCALED:
        return VK_FORMAT_R16G16B16A16_USCALED;
    case PixelFormat::R16G16B16A16_SSCALED:
        return VK_FORMAT_R16G16B16A16_SSCALED;
    case PixelFormat::R16G16B16A16_UINT:
        return VK_FORMAT_R16G16B16A16_UINT;
    case PixelFormat::R16G16B16A16_SINT:
        return VK_FORMAT_R16G16B16A16_SINT;
    case PixelFormat::R16G16B16A16_SFLOAT:
        return VK_FORMAT_R16G16B16A16_SFLOAT;
    case PixelFormat::R32_UINT:
        return VK_FORMAT_R32_UINT;
    case PixelFormat::R32_SINT:
        return VK_FORMAT_R32_SINT;
    case PixelFormat::R32_SFLOAT:
        return VK_FORMAT_R32_SFLOAT;
    case PixelFormat::R32G32_UINT:
        return VK_FORMAT_R32G32_UINT;
    case PixelFormat::R32G32_SINT:
        return VK_FORMAT_R32G32_SINT;
    case PixelFormat::R32G32_SFLOAT:
        return VK_FORMAT_R32G32_SFLOAT;
    case PixelFormat::R32G32B32_UINT:
        return VK_FORMAT_R32G32B32_UINT;
    case PixelFormat::R32G32B32_SINT:
        return VK_FORMAT_R32G32B32_SINT;
    case PixelFormat::R32G32B32_SFLOAT:
        return VK_FORMAT_R32G32B32_SFLOAT;
    case PixelFormat::R32G32B32A32_UINT:
        return VK_FORMAT_R32G32B32A32_UINT;
    case PixelFormat::R32G32B32A32_SINT:
        return VK_FORMAT_R32G32B32A32_SINT;
    case PixelFormat::R32G32B32A32_SFLOAT:
        return VK_FORMAT_R32G32B32A32_SFLOAT;
    case PixelFormat::R64_UINT:
        return VK_FORMAT_R64_UINT;
    case PixelFormat::R64_SINT:
        return VK_FORMAT_R64_SINT;
    case PixelFormat::R64_SFLOAT:
        return VK_FORMAT_R64_SFLOAT;
    case PixelFormat::R64G64_UINT:
        return VK_FORMAT_R64G64_UINT;
    case PixelFormat::R64G64_SINT:
        return VK_FORMAT_R64G64_SINT;
    case PixelFormat::R64G64_SFLOAT:
        return VK_FORMAT_R64G64_SFLOAT;
    case PixelFormat::R64G64B64_UINT:
        return VK_FORMAT_R64G64B64_UINT;
    case PixelFormat::R64G64B64_SINT:
        return VK_FORMAT_R64G64B64_SINT;
    case PixelFormat::R64G64B64_SFLOAT:
        return VK_FORMAT_R64G64B64_SFLOAT;
    case PixelFormat::R64G64B64A64_UINT:
        return VK_FORMAT_R64G64B64A64_UINT;
    case PixelFormat::R64G64B64A64_SINT:
        return VK_FORMAT_R64G64B64A64_SINT;
    case PixelFormat::R64G64B64A64_SFLOAT:
        return VK_FORMAT_R64G64B64A64_SFLOAT;
    case PixelFormat::B10G11R11_UFLOAT_PACK32:
        return VK_FORMAT_B10G11R11_UFLOAT_PACK32;
    case PixelFormat::E5B9G9R9_UFLOAT_PACK32:
        return VK_FORMAT_E5B9G9R9_UFLOAT_PACK32;
    case PixelFormat::D16_UNORM:
        return VK_FORMAT_D16_UNORM;
    case PixelFormat::X8_D24_UNORM_PACK32:
        return VK_FORMAT_X8_D24_UNORM_PACK32;
    case PixelFormat::D32_SFLOAT:
        return VK_FORMAT_D32_SFLOAT;
    case PixelFormat::S8_UINT:
        return VK_FORMAT_S8_UINT;
    case PixelFormat::D16_UNORM_S8_UINT:
        return VK_FORMAT_D16_UNORM_S8_UINT;
    case PixelFormat::D24_UNORM_S8_UINT:
        return VK_FORMAT_D24_UNORM_S8_UINT;
    case PixelFormat::D32_SFLOAT_S8_UINT:
        return VK_FORMAT_D32_SFLOAT_S8_UINT;
    case PixelFormat::BC1_RGB_UNORM_BLOCK:
        return VK_FORMAT_BC1_RGB_UNORM_BLOCK;
    case PixelFormat::BC1_RGB_SRGB_BLOCK:
        return VK_FORMAT_BC1_RGB_SRGB_BLOCK;
    case PixelFormat::BC1_RGBA_UNORM_BLOCK:
        return VK_FORMAT_BC1_RGBA_UNORM_BLOCK;
    case PixelFormat::BC1_RGBA_SRGB_BLOCK:
        return VK_FORMAT_BC1_RGBA_SRGB_BLOCK;
    case PixelFormat::BC2_UNORM_BLOCK:
        return VK_FORMAT_BC2_UNORM_BLOCK;
    case PixelFormat::BC2_SRGB_BLOCK:
        return VK_FORMAT_BC2_SRGB_BLOCK;
    case PixelFormat::BC3_UNORM_BLOCK:
        return VK_FORMAT_BC3_UNORM_BLOCK;
    case PixelFormat::BC3_SRGB_BLOCK:
        return VK_FORMAT_BC3_SRGB_BLOCK;
    case PixelFormat::BC4_UNORM_BLOCK:
        return VK_FORMAT_BC4_UNORM_BLOCK;
    case PixelFormat::BC4_SNORM_BLOCK:
        return VK_FORMAT_BC4_SNORM_BLOCK;
    case PixelFormat::BC5_UNORM_BLOCK:
        return VK_FORMAT_BC5_UNORM_BLOCK;
    case PixelFormat::BC5_SNORM_BLOCK:
        return VK_FORMAT_BC5_SNORM_BLOCK;
    case PixelFormat::BC6H_UFLOAT_BLOCK:
        return VK_FORMAT_BC6H_UFLOAT_BLOCK;
    case PixelFormat::BC6H_SFLOAT_BLOCK:
        return VK_FORMAT_BC6H_SFLOAT_BLOCK;
    case PixelFormat::BC7_UNORM_BLOCK:
        return VK_FORMAT_BC7_UNORM_BLOCK;
    case PixelFormat::BC7_SRGB_BLOCK:
        return VK_FORMAT_BC7_SRGB_BLOCK;
    case PixelFormat::ETC2_R8G8B8_UNORM_BLOCK:
        return VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK;
    case PixelFormat::ETC2_R8G8B8_SRGB_BLOCK:
        return VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK;
    case PixelFormat::ETC2_R8G8B8A1_UNORM_BLOCK:
        return VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK;
    case PixelFormat::ETC2_R8G8B8A1_SRGB_BLOCK:
        return VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK;
    case PixelFormat::ETC2_R8G8B8A8_UNORM_BLOCK:
        return VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK;
    case PixelFormat::ETC2_R8G8B8A8_SRGB_BLOCK:
        return VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK;
    case PixelFormat::EAC_R11_UNORM_BLOCK:
        return VK_FORMAT_EAC_R11_UNORM_BLOCK;
    case PixelFormat::EAC_R11_SNORM_BLOCK:
        return VK_FORMAT_EAC_R11_SNORM_BLOCK;
    case PixelFormat::EAC_R11G11_UNORM_BLOCK:
        return VK_FORMAT_EAC_R11G11_UNORM_BLOCK;
    case PixelFormat::EAC_R11G11_SNORM_BLOCK:
        return VK_FORMAT_EAC_R11G11_SNORM_BLOCK;
    case PixelFormat::ASTC_4x4_UNORM_BLOCK:
        return VK_FORMAT_ASTC_4x4_UNORM_BLOCK;
    case PixelFormat::ASTC_4x4_SRGB_BLOCK:
        return VK_FORMAT_ASTC_4x4_SRGB_BLOCK;
    case PixelFormat::ASTC_5x4_UNORM_BLOCK:
        return VK_FORMAT_ASTC_5x4_UNORM_BLOCK;
    case PixelFormat::ASTC_5x4_SRGB_BLOCK:
        return VK_FORMAT_ASTC_5x4_SRGB_BLOCK;
    case PixelFormat::ASTC_5x5_UNORM_BLOCK:
        return VK_FORMAT_ASTC_5x5_UNORM_BLOCK;
    case PixelFormat::ASTC_5x5_SRGB_BLOCK:
        return VK_FORMAT_ASTC_5x5_SRGB_BLOCK;
    case PixelFormat::ASTC_6x5_UNORM_BLOCK:
        return VK_FORMAT_ASTC_6x5_UNORM_BLOCK;
    case PixelFormat::ASTC_6x5_SRGB_BLOCK:
        return VK_FORMAT_ASTC_6x5_SRGB_BLOCK;
    case PixelFormat::ASTC_6x6_UNORM_BLOCK:
        return VK_FORMAT_ASTC_6x6_UNORM_BLOCK;
    case PixelFormat::ASTC_6x6_SRGB_BLOCK:
        return VK_FORMAT_ASTC_6x6_SRGB_BLOCK;
    case PixelFormat::ASTC_8x5_UNORM_BLOCK:
        return VK_FORMAT_ASTC_8x5_UNORM_BLOCK;
    case PixelFormat::ASTC_8x5_SRGB_BLOCK:
        return VK_FORMAT_ASTC_8x5_SRGB_BLOCK;
    case PixelFormat::ASTC_8x6_UNORM_BLOCK:
        return VK_FORMAT_ASTC_8x6_UNORM_BLOCK;
    case PixelFormat::ASTC_8x6_SRGB_BLOCK:
        return VK_FORMAT_ASTC_8x6_SRGB_BLOCK;
    case PixelFormat::ASTC_8x8_UNORM_BLOCK:
        return VK_FORMAT_ASTC_8x8_UNORM_BLOCK;
    case PixelFormat::ASTC_8x8_SRGB_BLOCK:
        return VK_FORMAT_ASTC_8x8_SRGB_BLOCK;
    case PixelFormat::ASTC_10x5_UNORM_BLOCK:
        return VK_FORMAT_ASTC_10x5_UNORM_BLOCK;
    case PixelFormat::ASTC_10x5_SRGB_BLOCK:
        return VK_FORMAT_ASTC_10x5_SRGB_BLOCK;
    case PixelFormat::ASTC_10x6_UNORM_BLOCK:
        return VK_FORMAT_ASTC_10x6_UNORM_BLOCK;
    case PixelFormat::ASTC_10x6_SRGB_BLOCK:
        return VK_FORMAT_ASTC_10x6_SRGB_BLOCK;
    case PixelFormat::ASTC_10x8_UNORM_BLOCK:
        return VK_FORMAT_ASTC_10x8_UNORM_BLOCK;
    case PixelFormat::ASTC_10x8_SRGB_BLOCK:
        return VK_FORMAT_ASTC_10x8_SRGB_BLOCK;
    case PixelFormat::ASTC_10x10_UNORM_BLOCK:
        return VK_FORMAT_ASTC_10x10_UNORM_BLOCK;
    case PixelFormat::ASTC_10x10_SRGB_BLOCK:
        return VK_FORMAT_ASTC_10x10_SRGB_BLOCK;
    case PixelFormat::ASTC_12x10_UNORM_BLOCK:
        return VK_FORMAT_ASTC_12x10_UNORM_BLOCK;
    case PixelFormat::ASTC_12x10_SRGB_BLOCK:
        return VK_FORMAT_ASTC_12x10_SRGB_BLOCK;
    case PixelFormat::ASTC_12x12_UNORM_BLOCK:
        return VK_FORMAT_ASTC_12x12_UNORM_BLOCK;
    case PixelFormat::ASTC_12x12_SRGB_BLOCK:
        return VK_FORMAT_ASTC_12x12_SRGB_BLOCK;
    case PixelFormat::G8B8G8R8_422_UNORM:
        return VK_FORMAT_G8B8G8R8_422_UNORM;
    case PixelFormat::B8G8R8G8_422_UNORM:
        return VK_FORMAT_B8G8R8G8_422_UNORM;
    case PixelFormat::G8_B8_R8_3PLANE_420_UNORM:
        return VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM;
    case PixelFormat::G8_B8R8_2PLANE_420_UNORM:
        return VK_FORMAT_G8_B8R8_2PLANE_420_UNORM;
    case PixelFormat::G8_B8_R8_3PLANE_422_UNORM:
        return VK_FORMAT_G8_B8_R8_3PLANE_422_UNORM;
    case PixelFormat::G8_B8R8_2PLANE_422_UNORM:
        return VK_FORMAT_G8_B8R8_2PLANE_422_UNORM;
    case PixelFormat::G8_B8_R8_3PLANE_444_UNORM:
        return VK_FORMAT_G8_B8_R8_3PLANE_444_UNORM;
    case PixelFormat::R10X6_UNORM_PACK16:
        return VK_FORMAT_R10X6_UNORM_PACK16;
    case PixelFormat::R10X6G10X6_UNORM_2PACK16:
        return VK_FORMAT_R10X6G10X6_UNORM_2PACK16;
    case PixelFormat::R10X6G10X6B10X6A10X6_UNORM_4PACK16:
        return VK_FORMAT_R10X6G10X6B10X6A10X6_UNORM_4PACK16;
    case PixelFormat::G10X6B10X6G10X6R10X6_422_UNORM_4PACK16:
        return VK_FORMAT_G10X6B10X6G10X6R10X6_422_UNORM_4PACK16;
    case PixelFormat::B10X6G10X6R10X6G10X6_422_UNORM_4PACK16:
        return VK_FORMAT_B10X6G10X6R10X6G10X6_422_UNORM_4PACK16;
    case PixelFormat::G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16:
        return VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16;
    case PixelFormat::G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16:
        return VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16;
    case PixelFormat::G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16:
        return VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16;
    case PixelFormat::G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16:
        return VK_FORMAT_G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16;
    case PixelFormat::G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16:
        return VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16;
    case PixelFormat::R12X4_UNORM_PACK16:
        return VK_FORMAT_R12X4_UNORM_PACK16;
    case PixelFormat::R12X4G12X4_UNORM_2PACK16:
        return VK_FORMAT_R12X4G12X4_UNORM_2PACK16;
    case PixelFormat::R12X4G12X4B12X4A12X4_UNORM_4PACK16:
        return VK_FORMAT_R12X4G12X4B12X4A12X4_UNORM_4PACK16;
    case PixelFormat::G12X4B12X4G12X4R12X4_422_UNORM_4PACK16:
        return VK_FORMAT_G12X4B12X4G12X4R12X4_422_UNORM_4PACK16;
    case PixelFormat::B12X4G12X4R12X4G12X4_422_UNORM_4PACK16:
        return VK_FORMAT_B12X4G12X4R12X4G12X4_422_UNORM_4PACK16;
    case PixelFormat::G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16:
        return VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16;
    case PixelFormat::G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16:
        return VK_FORMAT_G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16;
    case PixelFormat::G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16:
        return VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16;
    case PixelFormat::G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16:
        return VK_FORMAT_G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16;
    case PixelFormat::G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16:
        return VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16;
    case PixelFormat::G16B16G16R16_422_UNORM:
        return VK_FORMAT_G16B16G16R16_422_UNORM;
    case PixelFormat::B16G16R16G16_422_UNORM:
        return VK_FORMAT_B16G16R16G16_422_UNORM;
    case PixelFormat::G16_B16_R16_3PLANE_420_UNORM:
        return VK_FORMAT_G16_B16_R16_3PLANE_420_UNORM;
    case PixelFormat::G16_B16R16_2PLANE_420_UNORM:
        return VK_FORMAT_G16_B16R16_2PLANE_420_UNORM;
    case PixelFormat::G16_B16_R16_3PLANE_422_UNORM:
        return VK_FORMAT_G16_B16_R16_3PLANE_422_UNORM;
    case PixelFormat::G16_B16R16_2PLANE_422_UNORM:
        return VK_FORMAT_G16_B16R16_2PLANE_422_UNORM;
    case PixelFormat::G16_B16_R16_3PLANE_444_UNORM:
        return VK_FORMAT_G16_B16_R16_3PLANE_444_UNORM;
    case PixelFormat::PVRTC1_2BPP_UNORM_BLOCK_IMG:
        return VK_FORMAT_PVRTC1_2BPP_UNORM_BLOCK_IMG;
    case PixelFormat::PVRTC1_4BPP_UNORM_BLOCK_IMG:
        return VK_FORMAT_PVRTC1_4BPP_UNORM_BLOCK_IMG;
    case PixelFormat::PVRTC2_2BPP_UNORM_BLOCK_IMG:
        return VK_FORMAT_PVRTC2_2BPP_UNORM_BLOCK_IMG;
    case PixelFormat::PVRTC2_4BPP_UNORM_BLOCK_IMG:
        return VK_FORMAT_PVRTC2_4BPP_UNORM_BLOCK_IMG;
    case PixelFormat::PVRTC1_2BPP_SRGB_BLOCK_IMG:
        return VK_FORMAT_PVRTC1_2BPP_SRGB_BLOCK_IMG;
    case PixelFormat::PVRTC1_4BPP_SRGB_BLOCK_IMG:
        return VK_FORMAT_PVRTC1_4BPP_SRGB_BLOCK_IMG;
    case PixelFormat::PVRTC2_2BPP_SRGB_BLOCK_IMG:
        return VK_FORMAT_PVRTC2_2BPP_SRGB_BLOCK_IMG;
    case PixelFormat::PVRTC2_4BPP_SRGB_BLOCK_IMG:
        return VK_FORMAT_PVRTC2_4BPP_SRGB_BLOCK_IMG;
    case PixelFormat::Unknown:
    default:
        AssertUnreachable();
    }

    return VK_FORMAT_UNDEFINED;
}

VkSampleCountFlagBits SampleCountToVulkan(u32 sampleCount)
{
    Assert(sampleCount > 0);
    Assert(sampleCount <= 64);
    Assert(isPowerOfTwo(sampleCount));
    return static_cast<VkSampleCountFlagBits>(sampleCount);
}

VkImageCreateFlags GetVulkanCreateFlags(const GPUTextureProperties& /*properties*/)
{
    return VK_FLAGS_NONE;
}

VkImageUsageFlags GetVulkanUsageFlags(u32 usageFlags)
{
    VkImageUsageFlags flags = 0;

    flags |= (usageFlags & GPUTextureUsage::TransferSrc) ? VK_IMAGE_USAGE_TRANSFER_SRC_BIT : 0;
    flags |= (usageFlags & GPUTextureUsage::TransferDst) ? VK_IMAGE_USAGE_TRANSFER_DST_BIT : 0;
    flags |= (usageFlags & GPUTextureUsage::Sampled) ? VK_IMAGE_USAGE_SAMPLED_BIT : 0;
    flags |= (usageFlags & GPUTextureUsage::Storage) ? VK_IMAGE_USAGE_STORAGE_BIT : 0;
    flags |= (usageFlags & GPUTextureUsage::ColorAttachment) ? VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT : 0;
    flags |= (usageFlags & GPUTextureUsage::DepthStencilAttachment) ? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT : 0;
    flags |= (usageFlags & GPUTextureUsage::TransientAttachment) ? VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT : 0;
    flags |= (usageFlags & GPUTextureUsage::InputAttachment) ? VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT : 0;

    return flags;
}

ImageInfo create_image(ReaperRoot& root, VkDevice device, const char* debug_string,
                       const GPUTextureProperties& properties, VmaAllocator& allocator)
{
    const VkExtent3D    extent = {properties.width, properties.height, properties.depth};
    const VkImageTiling tilingMode = VK_IMAGE_TILING_OPTIMAL;
    const VkFormat      vulkan_format = PixelFormatToVulkan(properties.format);

    const VkImageCreateInfo imageInfo = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                                         nullptr,
                                         GetVulkanCreateFlags(properties),
                                         VK_IMAGE_TYPE_2D,
                                         vulkan_format,
                                         extent,
                                         properties.mipCount,
                                         properties.layerCount,
                                         SampleCountToVulkan(properties.sampleCount),
                                         tilingMode,
                                         GetVulkanUsageFlags(properties.usageFlags),
                                         VK_SHARING_MODE_EXCLUSIVE,
                                         0,
                                         nullptr,
                                         VK_IMAGE_LAYOUT_UNDEFINED};

    VmaAllocationCreateInfo allocInfo = {};
    allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    log_debug(root, "vulkan: creating new image: extent = {}x{}x{}, format = {}", properties.width, properties.height,
              properties.depth, GetFormatToString(vulkan_format));
    log_debug(root, "- mips = {}, layers = {}, samples = {}", properties.mipCount, properties.layerCount,
              properties.sampleCount);

    VkImage       image;
    VmaAllocation allocation;
    Assert(vmaCreateImage(allocator, &imageInfo, &allocInfo, &image, &allocation, nullptr) == VK_SUCCESS);

    log_debug(root, "vulkan: created image with handle: {}", static_cast<void*>(image));

    VulkanSetDebugName(device, image, debug_string);

    return {image, allocation, properties};
}

VkImageView create_default_image_view(ReaperRoot& root, VkDevice device, const ImageInfo& image)
{
    VkImageSubresourceRange viewRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, image.properties.mipCount, 0,
                                         image.properties.layerCount};

    VkImageViewCreateInfo imageViewInfo = {
        VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        nullptr,
        0, // reserved
        image.handle,
        VK_IMAGE_VIEW_TYPE_2D,
        PixelFormatToVulkan(image.properties.format),
        VkComponentMapping{VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
                           VK_COMPONENT_SWIZZLE_IDENTITY},
        viewRange};

    VkImageView imageView = VK_NULL_HANDLE;
    Assert(vkCreateImageView(device, &imageViewInfo, nullptr, &imageView) == VK_SUCCESS);

    log_debug(root, "vulkan: created image view with handle: {}", static_cast<void*>(imageView));

    return imageView;
}

VkImageView create_depth_image_view(ReaperRoot& root, VkDevice device, const ImageInfo& image)
{
    VkImageSubresourceRange viewRange = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, image.properties.mipCount, 0,
                                         image.properties.layerCount};

    VkImageViewCreateInfo imageViewInfo = {
        VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        nullptr,
        0, // reserved
        image.handle,
        VK_IMAGE_VIEW_TYPE_2D,
        PixelFormatToVulkan(image.properties.format),
        VkComponentMapping{VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
                           VK_COMPONENT_SWIZZLE_IDENTITY},
        viewRange};

    VkImageView imageView = VK_NULL_HANDLE;
    Assert(vkCreateImageView(device, &imageViewInfo, nullptr, &imageView) == VK_SUCCESS);

    log_debug(root, "vulkan: created image view with handle: {}", static_cast<void*>(imageView));

    return imageView;
}

VkWriteDescriptorSet create_image_descriptor_write(VkDescriptorSet descriptorSet, u32 binding,
                                                   VkDescriptorType             descriptorType,
                                                   const VkDescriptorImageInfo* imageInfo)
{
    return {
        VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        nullptr,
        descriptorSet,
        binding,
        0,
        1,
        descriptorType,
        imageInfo,
        nullptr,
        nullptr,
    };
}
} // namespace Reaper
