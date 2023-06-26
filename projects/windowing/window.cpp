#include "window.hpp"

static void ExtractAndConvertToRGBA(const SL::Screen_Capture::Image &img, unsigned char *dst, size_t dst_size) {
    assert(dst_size >= static_cast<size_t>(SL::Screen_Capture::Width(img) * SL::Screen_Capture::Height(img) * sizeof(SL::Screen_Capture::ImageBGRA)));
    auto imgsrc = StartSrc(img);
    auto imgdist = dst;
	for (auto h = 0; h < Height(img); h++) {
		auto startimgsrc = imgsrc;
		for (auto w = 0; w < Width(img); w++) {
			*imgdist++ = imgsrc->R;
			*imgdist++ = imgsrc->G;
			*imgdist++ = imgsrc->B;
			*imgdist++ = 1;
			imgsrc++;
		}
		imgsrc = SL::Screen_Capture::GotoNextRow(img, startimgsrc);
	}
}

Window::Window(std::shared_ptr<gfx::vulkan::Context> context, const std::string& name) 
  : m_context(context) {

	auto windows = SL::Screen_Capture::GetWindows();
    for (auto window : windows) {
        std::string_view window_name = window.Name;
        if (window_name.find(name) != std::string::npos) {
            selectedWindow.push_back(window);
			break;
        } 
    }

	assert(selectedWindow.size() > 0);

    m_image = gfx::vulkan::Image::Builder{}
		.mipMaps()
		.setTiling(VK_IMAGE_TILING_LINEAR)
        .build2D(context, selectedWindow[0].Size.x, selectedWindow[0].Size.y, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
	m_image->transitionLayout(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
	map = m_image->map();

	m_framegrabber = SL::Screen_Capture::CreateCaptureConfiguration([&]() { return this->selectedWindow; })
		->onNewFrame([&](const SL::Screen_Capture::Image& img, const SL::Screen_Capture::Window& window) {
			this->m_imgBufferChanged = true;
			ExtractAndConvertToRGBA(img, (uint8_t *)(map), this->selectedWindow[0].Size.x * this->selectedWindow[0].Size.y * sizeof(SL::Screen_Capture::ImageBGRA));
		})
		->start_capturing();
	m_framegrabber->setFrameChangeInterval(std::chrono::nanoseconds(50));
}

Window::~Window() {
	m_framegrabber->pause();
	m_framegrabber.reset();
	m_image->unmap();
}

void Window::update(VkCommandBuffer commandBuffer) {
	if (m_imgBufferChanged) {
		m_imgBufferChanged = true;
		m_image->flush();
		m_image->genMipMaps(commandBuffer, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL);
		VkImageMemoryBarrier imageMemoryBarrier{};
		imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		imageMemoryBarrier.image = m_image->image();
		imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
		imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
		imageMemoryBarrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
		imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		imageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imageMemoryBarrier.subresourceRange.baseMipLevel = {};
		imageMemoryBarrier.subresourceRange.levelCount = 1;
		imageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
		imageMemoryBarrier.subresourceRange.layerCount = 1; 
	    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier);

	}
}