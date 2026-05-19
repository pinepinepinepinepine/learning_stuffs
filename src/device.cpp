#include "../headers/device.hpp"

void VulkanInstance::setRequiredInstance_Extensions()
{
    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    std::vector<const char*> extensions( glfwExtensions, glfwExtensions + glfwExtensionCount ); // () initializes with the vector's Pointer range constructor -- arithmetics: contains [first element to last element]

    if ( enableValidationLayers )
        extensions.push_back(vk::EXTDebugUtilsExtensionName);

    auto extensionProperties = context.enumerateInstanceExtensionProperties(); // Currently, context is empty because we didn't give it anything.
    for (uint32_t i = 0; i < glfwExtensionCount; ++i)
    {
        bool found = false;
        for ( auto& extensionProperty : extensionProperties )
        {
            if ( strcmp(extensionProperty.extensionName, glfwExtensions[i]) == 0 )
            {
                found = true;
                break;
            }
        }

        if ( !found )
            throw std::runtime_error("Required GLFW extension not supported: " + std::string(glfwExtensions[i]));
    }
    vulkanExtensions = extensions;
}

void VulkanInstance::setRequiredInstance_Layers()
{
    std::vector<char const*> requiredLayers;
    if ( enableValidationLayers )
        requiredLayers = { "VK_LAYER_KHRONOS_validation" };

    auto layerProperties = context.enumerateInstanceLayerProperties();

    for ( const auto& requiredLayer: requiredLayers )
    {
        bool found = false;
        for ( auto& layerProperty : layerProperties )
        {
            if ( strcmp(layerProperty.layerName, requiredLayer) == 0 )
            {
                found = true;
                break;
            }
        }

        if ( !found )
            throw std::runtime_error("Required layer not supported: " + std::string(requiredLayer));
    }
    vulkanLayers = requiredLayers;
}

void VulkanInstance::createVulkanInstanceHandle()
{
    try
    {
        constexpr vk::ApplicationInfo appInfo{
            .pApplicationName   = "Basic Renderer",
            .applicationVersion = VK_MAKE_VERSION( 1, 0, 0 ),
            .pEngineName        = "No Engine",
            .engineVersion      = VK_MAKE_VERSION( 1, 0, 0 ),
            .apiVersion         = vk::ApiVersion14 };

        vk::InstanceCreateInfo createInfo {
            .pApplicationInfo = &appInfo,
            .enabledLayerCount = static_cast<uint32_t>(vulkanLayers.size()),
            .ppEnabledLayerNames = vulkanLayers.data(),                               // THESE ARE VULKAN LAYERS (DEBUG TOOLS)
            .enabledExtensionCount = static_cast<uint32_t>(vulkanExtensions.size()),
            .ppEnabledExtensionNames = vulkanExtensions.data() };                     // THESE ARE VULKAN API EXTENSIONS, NOT GPU

        instance = vk::raii::Instance(context, createInfo);
    }
    catch ( const vk::SystemError& err ) { throw std::runtime_error( std::string("Failed to create Vulkan instance!") + err.what() ); }
}

void VulkanInstance::setupDebugMessenger()
{
    if ( !enableValidationLayers )
        return;

    vk::DebugUtilsMessageSeverityFlagsEXT severityFlags(vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError);
    vk::DebugUtilsMessageTypeFlagsEXT messageTypeFlags( vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation);

    vk::DebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCreateInfoEXT{
        .messageSeverity = severityFlags,            // Specifies what types of severities we want our callback to be called for.
        .messageType     = messageTypeFlags,         // Similarily, specifies what messages we want our callback to be called for.
        .pfnUserCallback = &debugCallbackFunction }; // Specifies the function we're using to callback -- points to that function.
            // One more parameter: .pUserData, which is the pointer to whatever data we want to send over to the callback function.

    debugMessenger = instance.createDebugUtilsMessengerEXT( debugUtilsMessengerCreateInfoEXT );
}

VKAPI_ATTR vk::Bool32 VKAPI_CALL VulkanInstance::debugCallbackFunction(
    vk::DebugUtilsMessageSeverityFlagBitsEXT       severity,
    vk::DebugUtilsMessageTypeFlagsEXT              type,
    const vk::DebugUtilsMessengerCallbackDataEXT*  pCallbackData,
    void*                                          pUserData)
{
    std::cerr << "\nvalidation layer: type " << to_string(type) << " msg: " << pCallbackData->pMessage << std::endl;
    if (severity >= vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning) {
        std::cerr << "this error is severe enough to show!\n";
    }
    return vk::False;
}

void VulkanInstance::createVulkanInstance()
{
    setRequiredInstance_Extensions();
    setRequiredInstance_Layers();
    createVulkanInstanceHandle();
    setupDebugMessenger();
}


bool PhysicalDevice::deviceSupportsReq_Queues( const vk::raii::PhysicalDevice& graphicDevice )
{
    auto availableQueueFamilies = graphicDevice.getQueueFamilyProperties();
    vk::QueueFlags requiredQueues = vk::QueueFlagBits::eGraphics | vk::QueueFlagBits::eCompute;

    for ( const auto& availableQueueFamily : availableQueueFamilies )
    {
        if ( ( availableQueueFamily.queueFlags & requiredQueues ) == requiredQueues )
            return true;
    }
    return false;
}

bool PhysicalDevice::deviceSupportsReq_GPUExtensions( const vk::raii::PhysicalDevice& graphicDevice )
{
    auto availableDeviceExtensions = graphicDevice.enumerateDeviceExtensionProperties();
    unsigned extension_count = 0;

    for ( const auto& availableDeviceExtension : availableDeviceExtensions )
    {
        for ( const auto& requiredDeviceExtension : requiredGPUDeviceExtension )
        {
            if ( strcmp( availableDeviceExtension.extensionName, requiredDeviceExtension ) == 0 ) {
                extension_count++;
                break;
            }
        }
    }

    if ( extension_count >= requiredGPUDeviceExtension.size() )
        return true;
    return false;
}

bool PhysicalDevice::deviceSupportsReq_GPUFeatures( const vk::raii::PhysicalDevice& graphicDevice )
{
    auto features = graphicDevice.template getFeatures2<vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan11Features, vk::PhysicalDeviceVulkan13Features,
    vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT,vk::PhysicalDeviceTimelineSemaphoreFeaturesKHR>();

    bool supportsRequiredFeatures =
        features.template get<vk::PhysicalDeviceFeatures2>().features.samplerAnisotropy && features.template get<vk::PhysicalDeviceFeatures2>().features.vertexPipelineStoresAndAtomics &&
        features.template get<vk::PhysicalDeviceVulkan11Features>().shaderDrawParameters &&
        features.template get<vk::PhysicalDeviceVulkan13Features>().dynamicRendering &&
        features.template get<vk::PhysicalDeviceVulkan13Features>().synchronization2 &&
        features.template get<vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>().extendedDynamicState &&
        features.template get<vk::PhysicalDeviceTimelineSemaphoreFeaturesKHR>().timelineSemaphore;

    return supportsRequiredFeatures;
}

bool PhysicalDevice::isDeviceSuitable( const vk::raii::PhysicalDevice& graphicDevice )
{
    bool supports_reqVulkanAPI = graphicDevice.getProperties().apiVersion >= vk::ApiVersion13;
    bool supports_reqQueues = deviceSupportsReq_Queues( graphicDevice );
    bool supports_reqGPUExtensions = deviceSupportsReq_GPUExtensions( graphicDevice );
    bool supports_reqFeatures = deviceSupportsReq_GPUFeatures( graphicDevice );

    return ( supports_reqVulkanAPI && supports_reqQueues && supports_reqGPUExtensions && supports_reqFeatures );
}

vk::Format PhysicalDevice::findSupportedFormat( const std::vector<vk::Format>& candidates, vk::ImageTiling tiling, vk::FormatFeatureFlags features ) const
{
    for ( const auto format : candidates )
    {
        vk::FormatProperties props = physicalDevice.getFormatProperties( format );
        if ( tiling == vk::ImageTiling::eLinear && ( props.linearTilingFeatures & features ) == features )
            return format;
        if ( tiling == vk::ImageTiling::eOptimal && ( props.optimalTilingFeatures & features ) == features )
            return format;
    }
    throw std::runtime_error("failed to find supported format!");
}

void PhysicalDevice::setMaxUsableSampleCount()
{
    vk::PhysicalDeviceProperties physicalDeviceProperties = physicalDevice.getProperties();
    vk::SampleCountFlags counts = physicalDeviceProperties.limits.framebufferColorSampleCounts & physicalDeviceProperties.limits.framebufferDepthSampleCounts;

    if (counts & vk::SampleCountFlagBits::e64)  { msaaSamples = vk::SampleCountFlagBits::e64; return; }
    if (counts & vk::SampleCountFlagBits::e32)  { msaaSamples = vk::SampleCountFlagBits::e32; return; }
    if (counts & vk::SampleCountFlagBits::e16)  { msaaSamples = vk::SampleCountFlagBits::e16; return; }
    if (counts & vk::SampleCountFlagBits::e8)   { msaaSamples = vk::SampleCountFlagBits::e8;  return; }
    if (counts & vk::SampleCountFlagBits::e4)   { msaaSamples = vk::SampleCountFlagBits::e4;  return; }
    if (counts & vk::SampleCountFlagBits::e2)   { msaaSamples = vk::SampleCountFlagBits::e2;  return; }
    msaaSamples = vk::SampleCountFlagBits::e1;
}

void PhysicalDevice::setPhysicalGPUDevice()
{
    std::vector<vk::raii::PhysicalDevice> physicalDevicesList = instance.enumeratePhysicalDevices();

    if ( physicalDevicesList.empty() )  // Physical Devices is empty if there are no devices with vulkan support.
        throw std::runtime_error("failed to find GPUs with Vulkan support!");

    auto const devIter = std::ranges::find_if(physicalDevicesList, [&](auto const &physDevice) { return isDeviceSuitable(physDevice); });
    if (devIter == physicalDevicesList.end())
        throw std::runtime_error("failed to find a suitable GPU!");

    physicalDevice = *devIter; // If we have multiple GPUs, the recommended way is to filter them based on whats better -- see big_notes for some examples. We're just choosing the first one found.

    setMaxUsableSampleCount();
}



void LogicalDevice::createLogicalDevice( const VkSurfaceKHR* window_surface )
{
    setPhysicalGPUDevice();

    std::vector<vk::QueueFamilyProperties> queueFamilyProperties = physicalDevice.getQueueFamilyProperties();
    for ( uint32_t qfpIndex = 0; qfpIndex < queueFamilyProperties.size(); qfpIndex++ )
    {
        auto flags = queueFamilyProperties[qfpIndex].queueFlags;
        if ( ( flags & vk::QueueFlagBits::eGraphics ) && ( flags & vk::QueueFlagBits::eCompute ) && physicalDevice.getSurfaceSupportKHR( qfpIndex, *window_surface ) ) {
            queueIndex = qfpIndex;
            break;
        }
    }
    if (queueIndex == ~0) throw std::runtime_error("Cannot find a queue family supporting graphics, computing, and presenting to the window");

    float queuePriority = 0.5f;
    vk::DeviceQueueCreateInfo deviceQueueCreateInfo {
        .queueFamilyIndex = queueIndex,
        .queueCount = 1,
        .pQueuePriorities = &queuePriority }; // has to be a pointer-to-variable

    vk::StructureChain<vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan11Features, vk::PhysicalDeviceVulkan13Features, vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT, vk::PhysicalDeviceTimelineSemaphoreFeaturesKHR> featureChain = {
        { .features = { .sampleRateShading = true, .samplerAnisotropy = true,
            .vertexPipelineStoresAndAtomics = true } },   // vk::PhysicalDeviceFeatures2:
        { .shaderDrawParameters = true},                                            // vk::PhysicalDeviceVulkan11Features
        { .synchronization2 = true, .dynamicRendering = true},                      // vk::PhysicalDeviceVulkan13Features
        { .extendedDynamicState = true },                                           // vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT
        { .timelineSemaphore = true } };                                            // vk::PhysicalDeviceTimelineSemaphoreFeaturesKHR

    vk::DeviceCreateInfo logicalDeviceCreateInfo {
        .pNext = &featureChain.get<vk::PhysicalDeviceFeatures2>(),                          // what features we're enabling
        .queueCreateInfoCount = 1,                                                          // the amount of queues we are creating
        .pQueueCreateInfos = &deviceQueueCreateInfo,                                        // Queue creation info for EACH family (YOU HAVE TO PASS A VECTOR/ARRAY OF vk::DeviceQueueCreateInfo.data(), SO APPEND EACH QUEUE FAMILY INTO A CENTRAL VECTOR, then .data() the vector)
        .enabledExtensionCount = static_cast<uint32_t>(requiredGPUDeviceExtension.size()),  // the number of GPU extensions we're enabling
        .ppEnabledExtensionNames = requiredGPUDeviceExtension.data() };                     // the list of the names of the extensions to enable (HOWEVER, THIS DIFFERS FROM vk::InstanceCreateInfo BECAUSE THESE EXTENSIONS ARE GPU EXTENSIONS, NOT VULKAN API EXTENSIONS.)

    logicalDevice = vk::raii::Device( physicalDevice, logicalDeviceCreateInfo );
    queue = vk::raii::Queue( logicalDevice, queueIndex, 0 );
}