#include "../headers/swapChain.hpp"

void SwapChain::setImageResolution( const vk::SurfaceCapabilitiesKHR& availableSurfaceCapabilities, const Window& window )
{
    // Whenever the Operating Systems determines the size of the window, Vulkan sets the currentExtent.width to be below the maximum value and appropriately sized for that window surface.
    // As a result, we use the currentExtent as our swap chain's image resolution because (essentially) the OS dictates it for us, and Vulkan'll 'fix' (essentially choose) it for us.
    // However, if it's false, we are free to choose ourselves between the minimum and maximum resolution that this physical devices will allow.
    if ( availableSurfaceCapabilities.currentExtent.width != std::numeric_limits<uint32_t>::max() ) {
        imageResolution = availableSurfaceCapabilities.currentExtent;
        return;
    }

    int height, width;
    glfwGetFramebufferSize( window.window, &width, &height );

    imageResolution.width = std::clamp<uint32_t>(width, availableSurfaceCapabilities.minImageExtent.width, availableSurfaceCapabilities.maxImageExtent.width);
    imageResolution.height = std::clamp<uint32_t>(height, availableSurfaceCapabilities.minImageExtent.height, availableSurfaceCapabilities.maxImageExtent.height);
}

void SwapChain::setSurfaceFormat( const std::vector<vk::SurfaceFormatKHR>& availableFormats )
{
    for ( const auto& availableFormat : availableFormats )
    {
        if ( availableFormat.format == vk::Format::eB8G8R8A8Srgb && availableFormat.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear ) {
            surfaceFormat = availableFormat;
            return;
        }
    }
    throw std::runtime_error("Could not find a supported surface format for the swap chain images!");
}

void SwapChain::setPresentationMode( const std::vector<vk::PresentModeKHR>& availablePresentModes )
{
    for ( const auto& availablePresentMode : availablePresentModes )
    {
        if ( availablePresentMode == vk::PresentModeKHR::eMailbox ) {
            presentationMode = vk::PresentModeKHR::eMailbox;
            return;
        }
    }
    presentationMode = vk::PresentModeKHR::eFifo; // EVERY Vulkan supported graphics card has eFifo available as a presentation mode, so it's our default if we don't find/choose anything else.
}

uint32_t SwapChain::choose_MinImageCount( const vk::SurfaceCapabilitiesKHR& availableSurfaceCapabilities )
{
    // vk::SurfaceCapabilitiesKHR::minImageCount is the minimum amount of images the swap chain must have (so as a direct result its the minimum amount of framebuffers, too)
    // 3u is for triple buffering, which is just a convention, so we use it as a "default"; if our minimum image count requires it to be higher though, we use that instead.
    auto minImageCount = std::max(3u, availableSurfaceCapabilities.minImageCount);

    // Ensure the swap chain's minimum image count cannot go above the physical device's maximum image count -- 0 is a special value in this context which means no maximum, so we have to check if its actually max capped by checking over 0.
    if ( ( availableSurfaceCapabilities.maxImageCount > 0 ) && ( availableSurfaceCapabilities.maxImageCount < minImageCount ) )
        minImageCount = availableSurfaceCapabilities.maxImageCount;

    return minImageCount;
}

void SwapChain::createSwapChain( const LogicalDevice& device, const Window& window )
{
    vk::SurfaceCapabilitiesKHR GPU_availableWindowCapabilities = device.physicalDevice.getSurfaceCapabilitiesKHR( window.window_surface ); // 1. Basic surface capabilities (min/max number of images in swap chain, min/max width and height of images, resolution)
    std::vector<vk::SurfaceFormatKHR> GPU_availableFormats = device.physicalDevice.getSurfaceFormatsKHR( window.window_surface );          // 2. Surface formats (pixel format, color space)
    std::vector<vk::PresentModeKHR> GPU_availablePresentModes = device.physicalDevice.getSurfacePresentModesKHR( window.window_surface );  // 3. Available presentation mode

    setImageResolution( GPU_availableWindowCapabilities, window );
    setSurfaceFormat( GPU_availableFormats );
    setPresentationMode( GPU_availablePresentModes );

    vk::SwapchainCreateInfoKHR swapChainCreateInfo {
        .surface          = window.window_surface,
        .minImageCount    = choose_MinImageCount( GPU_availableWindowCapabilities ),
        .imageFormat      = surfaceFormat.format,
        .imageColorSpace  = surfaceFormat.colorSpace,
        .imageExtent      = imageResolution,
        .imageArrayLayers = 1,
        .imageUsage       = vk::ImageUsageFlagBits::eColorAttachment,
        .imageSharingMode = vk::SharingMode::eExclusive,
        .preTransform     = GPU_availableWindowCapabilities.currentTransform, // how the image should be transformed before it's sent to the swap chain (rotation/flipped/mirrored, etc -- currentTransform specifies no pre transforming)
        .compositeAlpha   = vk::CompositeAlphaFlagBitsKHR::eOpaque,
        .presentMode      = presentationMode,
        .clipped          = true };

    swapChain = vk::raii::SwapchainKHR( device.logicalDevice, swapChainCreateInfo );

    std::vector<vk::Image> empty_SwapChain_framebuffers = swapChain.getImages();
    swapChainImages.clear();
    for ( auto& swapChainImage : empty_SwapChain_framebuffers )
    {
        Image img { .image = swapChainImage };
        img.createImageView( device.logicalDevice, surfaceFormat.format, vk::ImageAspectFlagBits::eColor, 1 );
        swapChainImages.emplace_back( std::move( img ) );
    }
}

void SwapChain::cleanupSwapChainViews()
{
    for ( auto& swapChainImage : swapChainImages )
    {
        swapChainImage.imageView = nullptr;
    }
    swapChain = nullptr;
}