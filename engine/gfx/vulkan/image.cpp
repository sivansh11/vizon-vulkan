#include "image.hpp"

#include "core/log.hpp"

#include <stb_image/stb_image.hpp>

#include <set>

namespace gfx {

namespace vulkan {

Image::Builder& Image::Builder::mipMaps() {
    enableMipMaps = true;
    return *this;
}

Image::Builder& Image::Builder::setCompareOp(VkCompareOp compareOp) {
    enableCompareOp = true;
    Builder::compareOp = compareOp;
    return *this;
}

std::shared_ptr<Image> Image::Builder::build2D(std::shared_ptr<Context> context, uint32_t width, uint32_t height, VkFormat format, VkImageUsageFlags imageUsageFlags, VkMemoryPropertyFlags memoryTypeIndex) {
    VkImageCreateInfo imageCreateInfo{};
    imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
    imageCreateInfo.extent.width = width;
    imageCreateInfo.extent.height = height;
    imageCreateInfo.extent.depth = 1;
    if (enableMipMaps) 
        imageCreateInfo.mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(width, height)))) + 1;
    else    
        imageCreateInfo.mipLevels = 1;  
    imageCreateInfo.arrayLayers = 1; // ???
    imageCreateInfo.format = format;
    imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageCreateInfo.usage = imageUsageFlags;
    imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;  // TODO: add multisampling support ???

    VkImage image{};

    if (vkCreateImage(context->device(), &imageCreateInfo, nullptr, &image) != VK_SUCCESS) {
        ERROR("Failed to create image");
        std::terminate();        
    }

    VkMemoryRequirements memoryRequirements{};
    vkGetImageMemoryRequirements(context->device(), image, &memoryRequirements);

    VkMemoryAllocateInfo memoryAllocateInfo{};
    memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memoryAllocateInfo.allocationSize = memoryRequirements.size;
    memoryAllocateInfo.memoryTypeIndex = context->findMemoryType(memoryRequirements.memoryTypeBits, memoryTypeIndex);

    VkDeviceMemory deviceMemory{};

    if (vkAllocateMemory(context->device(), &memoryAllocateInfo, nullptr, &deviceMemory) != VK_SUCCESS) {
        ERROR("Failed to allocate memory");
        std::terminate();
    }

    vkBindImageMemory(context->device(), image, deviceMemory, 0);

    VkImageView imageView{};

    VkImageViewCreateInfo imageViewCreateInfo{};
    imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    imageViewCreateInfo.image = image;
    imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    imageViewCreateInfo.format = format;
    imageViewCreateInfo.subresourceRange.aspectMask = getImageAspect(format);
    imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
    imageViewCreateInfo.subresourceRange.levelCount = imageCreateInfo.mipLevels;
    imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
    imageViewCreateInfo.subresourceRange.layerCount = 1;

    if (vkCreateImageView(context->device(), &imageViewCreateInfo, nullptr, &imageView) != VK_SUCCESS) {
        ERROR("Failed to create image view");
        std::terminate();
    }

    VkSampler sampler{};

    VkSamplerCreateInfo samplerCreateInfo{};
    samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
    samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
    samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;

    samplerCreateInfo.anisotropyEnable = VK_FALSE;

    samplerCreateInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;

    samplerCreateInfo.unnormalizedCoordinates = VK_FALSE;
    samplerCreateInfo.compareEnable = enableCompareOp ? VK_TRUE : VK_FALSE;
    samplerCreateInfo.compareOp = compareOp;

    samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerCreateInfo.mipLodBias = 0;
    samplerCreateInfo.minLod = 0;
    samplerCreateInfo.maxLod = VK_LOD_CLAMP_NONE;

    if (vkCreateSampler(context->device(), &samplerCreateInfo, nullptr, &sampler) != VK_SUCCESS) {
        ERROR("Failed to create sampler");
        std::terminate();
    }

    ImageInfo imageInfo{};
    imageInfo.image = image;
    imageInfo.imageView = imageView;
    imageInfo.sampler = sampler;
    imageInfo.deviceMemory = deviceMemory;
    imageInfo.currentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.format = format;
    imageInfo.mipLevels = imageCreateInfo.mipLevels;
    imageInfo.width = width;
    imageInfo.height = height;
    return std::make_shared<Image>(context, imageInfo);
}

std::shared_ptr<Image> Image::Builder::loadFromPath(std::shared_ptr<Context> context, const std::filesystem::path& filePath, VkFormat format) {
    int width, height, channels;
    stbi_set_flip_vertically_on_load(true);  
    stbi_uc *pixels = stbi_load(filePath.string().c_str(), &width, &height, &channels, STBI_rgb_alpha);

    VkDeviceSize imageSize = width * height * 4;

    if (!pixels) {
        ERROR("Failed to read file {}", filePath.string());
        ERROR("{}", stbi_failure_reason());
        std::terminate();
    }

    auto stagingBuffer = Buffer::Builder{}
        .build(context, imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    auto map = stagingBuffer->map();
    std::memcpy(map, pixels, imageSize);
    stagingBuffer->unmap();

    stbi_image_free(pixels);
    
    mipMaps();
    auto image = build2D(context, width, height, format, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    image->transitionLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    Image::copyBufferToImage(context, *stagingBuffer, *image, VkBufferImageCopy{
        .bufferOffset = 0,
        .bufferRowLength = 0,
        .bufferImageHeight = 0,
        .imageSubresource = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .mipLevel = 0,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
        .imageOffset = {0, 0, 0},
        .imageExtent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1}
        });
    image->genMipMaps();



    return image;
}

VkImageAspectFlags Image::Builder::getImageAspect(VkFormat format) {
    static std::set<VkFormat> depthFormats = {
        VK_FORMAT_D16_UNORM,
        VK_FORMAT_D16_UNORM_S8_UINT,
        VK_FORMAT_D24_UNORM_S8_UINT,
        VK_FORMAT_D32_SFLOAT,
        VK_FORMAT_D32_SFLOAT_S8_UINT,
    };

    if (depthFormats.contains(format)) 
        return VK_IMAGE_ASPECT_DEPTH_BIT;
    return VK_IMAGE_ASPECT_COLOR_BIT;
}

Image::Image(std::shared_ptr<Context> context, const ImageInfo& imageInfo) 
  : m_context(context),
    m_imageInfo(imageInfo) {
    TRACE("Created image");
}

Image::~Image() {
    vkDestroyImage(m_context->device(), m_imageInfo.image, nullptr);
    vkFreeMemory(m_context->device(), m_imageInfo.deviceMemory, nullptr);
    if (m_imageInfo.imageView != VK_NULL_HANDLE) {
        vkDestroyImageView(m_context->device(), m_imageInfo.imageView, nullptr);
    }
    if (m_imageInfo.sampler != VK_NULL_HANDLE) {
        vkDestroySampler(m_context->device(), m_imageInfo.sampler, nullptr);
    }
    TRACE("Destroyed image");
}

void Image::transitionLayout(VkImageLayout newLayout) {
    auto commandBuffer = m_context->startSingleUseCommandBuffer();

    VkImageMemoryBarrier imageMemoryBarrier{};
    imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    imageMemoryBarrier.oldLayout = m_imageInfo.currentLayout;
    imageMemoryBarrier.newLayout = newLayout;

    imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;  // not transfering image ownership
    imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;  // not transfering image ownership

    imageMemoryBarrier.image = m_imageInfo.image;
    imageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    imageMemoryBarrier.subresourceRange.baseMipLevel = 0; 
    imageMemoryBarrier.subresourceRange.levelCount = m_imageInfo.mipLevels;
    imageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
    imageMemoryBarrier.subresourceRange.layerCount = 1;
    imageMemoryBarrier.srcAccessMask = 0;
    imageMemoryBarrier.dstAccessMask = 0;
    
    VkPipelineStageFlags sourceStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    VkPipelineStageFlags destinationStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;

    switch (m_imageInfo.currentLayout) {
        case VK_IMAGE_LAYOUT_UNDEFINED:
            // Image layout is undefined (or does not matter)
            // Only valid as initial layout
            // No flags required, listed only for completeness
            imageMemoryBarrier.srcAccessMask = 0;
            break;

        case VK_IMAGE_LAYOUT_PREINITIALIZED:
            // Image is preinitialized
            // Only valid as initial layout for linear images, preserves memory contents
            // Make sure host writes have been finished
            imageMemoryBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
            break;

        case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
            // Image is a color attachment
            // Make sure any writes to the color buffer have been finished
            imageMemoryBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            break;

        case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
            // Image is a depth/stencil attachment
            // Make sure any writes to the depth/stencil buffer have been finished
            imageMemoryBarrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            break;

        case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
            // Image is a transfer source
            // Make sure any reads from the image have been finished
            imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            break;

        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
            // Image is a transfer destination
            // Make sure any writes to the image have been finished
            imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            break;

        case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
            // Image is read by a shader
            // Make sure any shader reads from the image have been finished
            imageMemoryBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
            break;
        default:
            // Other source layouts aren't handled (yet)
            ERROR("not handled transition");
            std::terminate();
            break;
    }

    switch (newLayout) {
        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
            // Image will be used as a transfer destination
            // Make sure any writes to the image have been finished
            imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            break;

        case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
            // Image will be used as a transfer source
            // Make sure any reads from the image have been finished
            imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            break;

        case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
            // Image will be used as a color attachment
            // Make sure any writes to the color buffer have been finished
            imageMemoryBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            break;

        case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
            // Image layout will be used as a depth/stencil attachment
            // Make sure any writes to depth/stencil buffer have been finished
            imageMemoryBarrier.dstAccessMask = imageMemoryBarrier.dstAccessMask | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            break;

        case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
            // Image will be read in a shader (sampler, input attachment)
            // Make sure any writes to the image have been finished
            if (imageMemoryBarrier.srcAccessMask == 0) {
                imageMemoryBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
            }
            imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            break;
        default:
            // Other source layouts aren't handled (yet)
            ERROR("not handled transition");
            std::terminate();
            break;
    };

    imageMemoryBarrier.srcAccessMask = 0;
    imageMemoryBarrier.dstAccessMask = 0;

    vkCmdPipelineBarrier(commandBuffer, sourceStage, destinationStage, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier);

    m_context->endSingleUseCommandBuffer(commandBuffer);

    m_imageInfo.currentLayout = newLayout;
}

void Image::genMipMaps() {
    auto commandBuffer = m_context->startSingleUseCommandBuffer();

    VkImageMemoryBarrier imageMemoryBarrier{};
    imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    imageMemoryBarrier.srcAccessMask = 0;
    imageMemoryBarrier.dstAccessMask = 0;
    imageMemoryBarrier.oldLayout = {};
    imageMemoryBarrier.newLayout = {};
    imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageMemoryBarrier.image = m_imageInfo.image;
    imageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    imageMemoryBarrier.subresourceRange.baseMipLevel = {};
    imageMemoryBarrier.subresourceRange.levelCount = 1;
    imageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
    imageMemoryBarrier.subresourceRange.layerCount = 1;

    uint32_t mipWidth = m_imageInfo.width;
    uint32_t mipHeight = m_imageInfo.height;

    for (uint32_t i = 1; i < m_imageInfo.mipLevels; i++) {
        imageMemoryBarrier.subresourceRange.baseMipLevel = i - 1;
        imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier);

        VkImageBlit imageBlit{};
        imageBlit.srcSubresource = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .mipLevel = i - 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        };
        imageBlit.srcOffsets[0] = {0, 0, 0};
        imageBlit.srcOffsets[1] = {static_cast<int32_t>(mipWidth), static_cast<int32_t>(mipHeight), 1};
        imageBlit.dstSubresource = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .mipLevel = i,
            .baseArrayLayer = 0,
            .layerCount = 1,
        };
        imageBlit.dstOffsets[1] = {0, 0, 0};
        imageBlit.dstOffsets[1] = {mipWidth > 1 ? static_cast<int32_t>(mipWidth / 2) : 1, mipHeight > 1 ? static_cast<int32_t>(mipHeight / 2) : 1, 1};

        vkCmdBlitImage(commandBuffer, m_imageInfo.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, m_imageInfo.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &imageBlit, VK_FILTER_LINEAR);
        
        imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier);

        if (mipWidth > 1) mipWidth /= 2;
        if (mipHeight > 1) mipHeight /= 2;
    }

    imageMemoryBarrier.subresourceRange.baseMipLevel = m_imageInfo.mipLevels - 1;
    imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier);

    m_context->endSingleUseCommandBuffer(commandBuffer);
}

void Image::copyBufferToImage(std::shared_ptr<Context> context, Buffer& buffer, Image& image, VkBufferImageCopy bufferImageCopy) {
    auto commandBuffer = context->startSingleUseCommandBuffer();

    vkCmdCopyBufferToImage(commandBuffer, buffer.buffer(), image.image(), image.m_imageInfo.currentLayout, 1, &bufferImageCopy);

    context->endSingleUseCommandBuffer(commandBuffer);
}

} // namespace vulkan

} // namespace gfx
