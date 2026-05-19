#pragma once

#include "includes.hpp"
#include "glfwWindow.hpp"
#include "image.hpp"


struct SwapChain
{
    vk::Extent2D imageResolution;
    vk::SurfaceFormatKHR surfaceFormat;
    vk::PresentModeKHR presentationMode;
    vk::raii::SwapchainKHR swapChain = nullptr;
    std::vector<Image> swapChainImages;

    void setImageResolution( const vk::SurfaceCapabilitiesKHR& availableSurfaceCapabilities, const Window& window );
    void setSurfaceFormat( const std::vector<vk::SurfaceFormatKHR>& availableFormats );
    void setPresentationMode( const std::vector<vk::PresentModeKHR>& availablePresentModes );
    uint32_t choose_MinImageCount( const vk::SurfaceCapabilitiesKHR& availableSurfaceCapabilities );
    void createSwapChain( const LogicalDevice& device, const Window& window );
    void cleanupSwapChainViews();
};