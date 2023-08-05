#include "image.hpp"

#include "core/log.hpp"

#include <stb_image/stb_image.hpp>

#include <set>

namespace gfx {

namespace vulkan {

image_builder_t& image_builder_t::mip_maps() {
    enable_mip_maps = true;
    return *this;
}

image_builder_t& image_builder_t::set_tiling(VkImageTiling image_tiling) {
    image_builder_t::image_tiling = image_tiling;
    return *this;
}

image_builder_t& image_builder_t::set_initial_layout(VkImageLayout initial_layout) {
    image_builder_t::initial_layout = initial_layout;
    return *this;
}

image_builder_t& image_builder_t::set_compare_op(VkCompareOp compare_op) {
    enable_compare_op = true;
    image_builder_t::compare_op = compare_op;
    return *this;
}

uint32_t numChannels(VkFormat format) {
    static std::set<VkFormat> formats_with_4_channels{
        VK_FORMAT_R8G8B8A8_SINT,
        VK_FORMAT_R8G8B8A8_SNORM,
        VK_FORMAT_R8G8B8A8_SRGB,
        VK_FORMAT_R8G8B8A8_SSCALED,
        VK_FORMAT_R8G8B8A8_UINT,
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_FORMAT_R8G8B8A8_USCALED
    };

    if (formats_with_4_channels.contains(format)) return 4;
    if (format == VK_FORMAT_D32_SFLOAT) return 4;
    ERROR("Failed to guess number of channels for format");
    // std::terminate();
    return 0;
}

core::ref<image_t> image_builder_t::build2D(core::ref<context_t> context, uint32_t width, uint32_t height, VkFormat format, VkImageUsageFlags image_usage_flags, VkMemoryPropertyFlags memory_type_index) {
    VkImageCreateInfo image_create_info{};
    image_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_create_info.imageType = VK_IMAGE_TYPE_2D;
    image_create_info.extent.width = width;
    image_create_info.extent.height = height;
    image_create_info.extent.depth = 1;
    if (enable_mip_maps) {
        VkImageFormatProperties image_format_properties{};
        vkGetPhysicalDeviceImageFormatProperties(context->physical_device(), VK_FORMAT_R8G8B8A8_SRGB, VkImageType::VK_IMAGE_TYPE_2D, VkImageTiling::VK_IMAGE_TILING_LINEAR, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, 0, &image_format_properties);
        image_create_info.mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(width, height)))) + 1;
        image_create_info.mipLevels = std::min(image_format_properties.maxMipLevels, image_create_info.mipLevels);
    } else    
        image_create_info.mipLevels = 1;  
    image_create_info.arrayLayers = 1; // ???
    image_create_info.format = format;
    image_create_info.tiling = image_tiling;
    image_create_info.initialLayout = initial_layout;
    image_create_info.usage = image_usage_flags;
    image_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    image_create_info.samples = VK_SAMPLE_COUNT_1_BIT;  // TODO: add multisampling support ???

    VkImage image{};

    if (vkCreateImage(context->device(), &image_create_info, nullptr, &image) != VK_SUCCESS) {
        ERROR("Failed to create image");
        std::terminate();        
    }

    VkMemoryRequirements memory_requirements{};
    vkGetImageMemoryRequirements(context->device(), image, &memory_requirements);

    VkMemoryAllocateInfo memory_allocate_info{};
    memory_allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memory_allocate_info.allocationSize = memory_requirements.size;
    memory_allocate_info.memoryTypeIndex = context->find_memory_type(memory_requirements.memoryTypeBits, memory_type_index);

    VkDeviceMemory device_memory{};

    if (vkAllocateMemory(context->device(), &memory_allocate_info, nullptr, &device_memory) != VK_SUCCESS) {
        ERROR("Failed to allocate memory");
        std::terminate();
    }

    vkBindImageMemory(context->device(), image, device_memory, 0);

    VkImageView imageView{};

    VkImageViewCreateInfo image_view_create_info{};
    image_view_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    image_view_create_info.image = image;
    image_view_create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    image_view_create_info.format = format;
    image_view_create_info.subresourceRange.aspectMask = get_image_aspect(format);
    image_view_create_info.subresourceRange.baseMipLevel = 0;
    image_view_create_info.subresourceRange.levelCount = image_create_info.mipLevels;
    image_view_create_info.subresourceRange.baseArrayLayer = 0;
    image_view_create_info.subresourceRange.layerCount = 1;

    if (vkCreateImageView(context->device(), &image_view_create_info, nullptr, &imageView) != VK_SUCCESS) {
        ERROR("Failed to create image view");
        std::terminate();
    }

    VkSampler sampler{};

    VkSamplerCreateInfo sampler_create_info{};
    sampler_create_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sampler_create_info.magFilter = VK_FILTER_LINEAR;
    sampler_create_info.minFilter = VK_FILTER_LINEAR;
    sampler_create_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampler_create_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampler_create_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;

    sampler_create_info.anisotropyEnable = VK_FALSE;

    sampler_create_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;

    sampler_create_info.unnormalizedCoordinates = VK_FALSE;
    sampler_create_info.compareEnable = enable_compare_op ? VK_TRUE : VK_FALSE;
    sampler_create_info.compareOp = compare_op;

    sampler_create_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    sampler_create_info.mipLodBias = 0;
    sampler_create_info.minLod = 0;
    sampler_create_info.maxLod = VK_LOD_CLAMP_NONE;

    if (vkCreateSampler(context->device(), &sampler_create_info, nullptr, &sampler) != VK_SUCCESS) {
        ERROR("Failed to create sampler");
        std::terminate();
    }

    image_info_t image_info{};
    image_info.image = image;
    image_info.image_view = imageView;
    image_info.sampler = sampler;
    image_info.device_memory = device_memory;
    image_info.current_layout = VK_IMAGE_LAYOUT_UNDEFINED;
    image_info.format = format;
    image_info.mip_levels = image_create_info.mipLevels;
    image_info.width = width;
    image_info.height = height;
    image_info.size = width * height * numChannels(format);
    
    return core::make_ref<image_t>(context, image_info);
}

core::ref<image_t> image_builder_t::loadFromPath(core::ref<context_t> context, const std::filesystem::path& file_path, VkFormat format) {
    int width, height, channels;
    stbi_set_flip_vertically_on_load(true);  
    stbi_uc *pixels = stbi_load(file_path.string().c_str(), &width, &height, &channels, STBI_rgb_alpha);

    VkDeviceSize image_size = width * height * 4;

    if (!pixels) {
        ERROR("Failed to read file {}", file_path.string());
        ERROR("{}", stbi_failure_reason());
        std::terminate();
    }

    auto staging_buffer = buffer_builder_t{}
        .build(context, image_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    auto map = staging_buffer->map();
    std::memcpy(map, pixels, image_size);
    staging_buffer->unmap();

    stbi_image_free(pixels);
    
    mip_maps();
    auto image = build2D(context, width, height, format, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    image->transition_layout(initial_layout, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    image_t::copy_buffer_to_image(context, *staging_buffer, *image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VkBufferImageCopy{
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
    image->genMipMaps(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);


    return image;
}

VkImageAspectFlags image_builder_t::get_image_aspect(VkFormat format) {
    static std::set<VkFormat> depth_formats = {
        VK_FORMAT_D16_UNORM,
        VK_FORMAT_D16_UNORM_S8_UINT,
        VK_FORMAT_D24_UNORM_S8_UINT,
        VK_FORMAT_D32_SFLOAT,
        VK_FORMAT_D32_SFLOAT_S8_UINT,
    };

    if (depth_formats.contains(format)) 
        return VK_IMAGE_ASPECT_DEPTH_BIT;
    return VK_IMAGE_ASPECT_COLOR_BIT;
}

image_t::image_t(core::ref<context_t> context, const image_info_t& image_info) 
  : _context(context),
    _imageInfo(image_info) {
    TRACE("Created image");
}

image_t::~image_t() {
    vkDestroyImage(_context->device(), _imageInfo.image, nullptr);
    vkFreeMemory(_context->device(), _imageInfo.device_memory, nullptr);
    if (_imageInfo.image_view != VK_NULL_HANDLE) {
        vkDestroyImageView(_context->device(), _imageInfo.image_view, nullptr);
    }
    if (_imageInfo.sampler != VK_NULL_HANDLE) {
        vkDestroySampler(_context->device(), _imageInfo.sampler, nullptr);
    }
    TRACE("Destroyed image");
}

void image_t::transition_layout(VkImageLayout old_layout, VkImageLayout new_layout) {
    auto commandbuffer = _context->start_single_use_commandbuffer();

    transition_layout(commandbuffer, old_layout, new_layout);
    
    _context->end_single_use_commandbuffer(commandbuffer);
}

void image_t::transition_layout(VkCommandBuffer commandbuffer, VkImageLayout old_layout, VkImageLayout new_layout) {
    VkImageMemoryBarrier image_memory_barrier{};
    image_memory_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    image_memory_barrier.oldLayout = old_layout;
    image_memory_barrier.newLayout = new_layout;

    image_memory_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;  // not transfering image ownership
    image_memory_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;  // not transfering image ownership

    image_memory_barrier.image = _imageInfo.image;
    image_memory_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    image_memory_barrier.subresourceRange.baseMipLevel = 0; 
    image_memory_barrier.subresourceRange.levelCount = _imageInfo.mip_levels;
    image_memory_barrier.subresourceRange.baseArrayLayer = 0;
    image_memory_barrier.subresourceRange.layerCount = 1;
    image_memory_barrier.srcAccessMask = 0;
    image_memory_barrier.dstAccessMask = 0;
    
    VkPipelineStageFlags source_stage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    VkPipelineStageFlags destination_stage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;

    switch (old_layout) {
        case VK_IMAGE_LAYOUT_UNDEFINED:
            // Image layout is undefined (or does not matter)
            // Only valid as initial layout
            // No flags required, listed only for completeness
            image_memory_barrier.srcAccessMask = 0;
            break;

        case VK_IMAGE_LAYOUT_PREINITIALIZED:
            // Image is preinitialized
            // Only valid as initial layout for linear images, preserves memory contents
            // Make sure host writes have been finished
            image_memory_barrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
            break;

        case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
            // Image is a color attachment
            // Make sure any writes to the color buffer have been finished
            image_memory_barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            break;

        case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
            // Image is a depth/stencil attachment
            // Make sure any writes to the depth/stencil buffer have been finished
            image_memory_barrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            break;

        case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
            // Image is a transfer source
            // Make sure any reads from the image have been finished
            image_memory_barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            break;

        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
            // Image is a transfer destination
            // Make sure any writes to the image have been finished
            image_memory_barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            break;

        case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
            // Image is read by a shader
            // Make sure any shader reads from the image have been finished
            image_memory_barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
            break;
        default:
            // Other source layouts aren't handled (yet)
            ERROR("not handled transition");
            std::terminate();
            break;
    }

    switch (new_layout) {
        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
            // Image will be used as a transfer destination
            // Make sure any writes to the image have been finished
            image_memory_barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            break;

        case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
            // Image will be used as a transfer source
            // Make sure any reads from the image have been finished
            image_memory_barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            break;

        case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
            // Image will be used as a color attachment
            // Make sure any writes to the color buffer have been finished
            image_memory_barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            break;

        case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
            // Image layout will be used as a depth/stencil attachment
            // Make sure any writes to depth/stencil buffer have been finished
            image_memory_barrier.dstAccessMask = image_memory_barrier.dstAccessMask | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            break;

        case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
            // Image will be read in a shader (sampler, input attachment)
            // Make sure any writes to the image have been finished
            if (image_memory_barrier.srcAccessMask == 0) {
                image_memory_barrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
            }
            image_memory_barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            break;
        
        case VK_IMAGE_LAYOUT_GENERAL:
            image_memory_barrier.srcAccessMask = 0;
            image_memory_barrier.dstAccessMask = 0;
            break;

        default:
            // Other source layouts aren't handled (yet)
            ERROR("not handled transition");
            std::terminate();
            break;
    };

    vkCmdPipelineBarrier(commandbuffer, source_stage, destination_stage, 0, 0, nullptr, 0, nullptr, 1, &image_memory_barrier);
}

void image_t::genMipMaps(VkCommandBuffer commandbuffer, VkImageLayout old_layout, VkImageLayout new_layout) {
    VkImageMemoryBarrier image_memory_barrier{};
    image_memory_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    image_memory_barrier.srcAccessMask = 0;
    image_memory_barrier.dstAccessMask = 0;
    image_memory_barrier.oldLayout = {};
    image_memory_barrier.newLayout = {};
    image_memory_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    image_memory_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    image_memory_barrier.image = _imageInfo.image;
    image_memory_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    image_memory_barrier.subresourceRange.baseMipLevel = {};
    image_memory_barrier.subresourceRange.levelCount = 1;
    image_memory_barrier.subresourceRange.baseArrayLayer = 0;
    image_memory_barrier.subresourceRange.layerCount = 1;

    uint32_t mipWidth = _imageInfo.width;
    uint32_t mipHeight = _imageInfo.height;

    for (uint32_t i = 1; i < _imageInfo.mip_levels; i++) {
        image_memory_barrier.subresourceRange.baseMipLevel = i - 1;
        image_memory_barrier.oldLayout = old_layout;
        image_memory_barrier.newLayout = old_layout != VK_IMAGE_LAYOUT_GENERAL ? VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL : VK_IMAGE_LAYOUT_GENERAL;
        image_memory_barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        image_memory_barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

        vkCmdPipelineBarrier(commandbuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &image_memory_barrier);

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

        vkCmdBlitImage(commandbuffer, _imageInfo.image, old_layout != VK_IMAGE_LAYOUT_GENERAL ? VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL : VK_IMAGE_LAYOUT_GENERAL, _imageInfo.image, new_layout != VK_IMAGE_LAYOUT_GENERAL ? VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL : VK_IMAGE_LAYOUT_GENERAL, 1, &imageBlit, VK_FILTER_LINEAR);
        
        image_memory_barrier.oldLayout = new_layout != VK_IMAGE_LAYOUT_GENERAL ? VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL : VK_IMAGE_LAYOUT_GENERAL;
        image_memory_barrier.newLayout = new_layout;
        image_memory_barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        image_memory_barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        vkCmdPipelineBarrier(commandbuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &image_memory_barrier);

        if (mipWidth > 1) mipWidth /= 2;
        if (mipHeight > 1) mipHeight /= 2;
    }

    image_memory_barrier.subresourceRange.baseMipLevel = _imageInfo.mip_levels - 1;
    image_memory_barrier.oldLayout = old_layout;
    image_memory_barrier.newLayout = new_layout;
    image_memory_barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    image_memory_barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(commandbuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &image_memory_barrier);
}

void image_t::genMipMaps(VkImageLayout old_layout, VkImageLayout new_layout) {
    auto commandbuffer = _context->start_single_use_commandbuffer();
    genMipMaps(commandbuffer, old_layout, new_layout);
    _context->end_single_use_commandbuffer(commandbuffer);
}

void image_t::copy_buffer_to_image(core::ref<context_t> context, buffer_t& buffer, image_t& image, VkImageLayout image_layout, VkBufferImageCopy buffer_image_copy) {
    auto commandbuffer = context->start_single_use_commandbuffer();

    copy_buffer_to_image(commandbuffer, buffer, image, image_layout, buffer_image_copy);

    context->end_single_use_commandbuffer(commandbuffer);
}

void image_t::copy_buffer_to_image(VkCommandBuffer commandbuffer, buffer_t& buffer, image_t& image, VkImageLayout image_layout, VkBufferImageCopy buffer_image_copy) {
    vkCmdCopyBufferToImage(commandbuffer, buffer.buffer(), image.image(), image_layout, 1, &buffer_image_copy);
}

void image_t::invalidate(VkDeviceSize offset, VkDeviceSize size) {
    VkMappedMemoryRange mapped_memory_range{};
    mapped_memory_range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
    mapped_memory_range.memory = _imageInfo.device_memory;
    mapped_memory_range.offset = offset;
    mapped_memory_range.size = size;
    vkInvalidateMappedMemoryRanges(_context->device(), 1, &mapped_memory_range);
}

void image_t::flush(VkDeviceSize offset, VkDeviceSize size) {
    VkMappedMemoryRange mapped_memory_range{};
    mapped_memory_range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
    mapped_memory_range.memory = _imageInfo.device_memory;
    mapped_memory_range.offset = offset;
    mapped_memory_range.size = size;
    vkFlushMappedMemoryRanges(_context->device(), 1, &mapped_memory_range);
}

void *image_t::map(VkDeviceSize offset, VkDeviceSize size) {
    void *data;
    vkMapMemory(_context->device(), _imageInfo.device_memory, offset, size, 0, &data);
    return data;
}

void image_t::unmap() {
    vkUnmapMemory(_context->device(), _imageInfo.device_memory);
}

} // namespace vulkan

} // namespace gfx
