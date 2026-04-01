#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#define GLFW_INCLUDE_VULKAN        // REQUIRED only for GLFW CreateWindowSurface. you could ditch the header right below this in exchange for this #define.

#if defined(__INTELLISENSE__) || !defined(USE_CPP20_MODULES)
#	include <vulkan/vulkan_raii.hpp>
#else
import vulkan_hpp;
#endif

#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_raii.hpp>
#include <tiny_obj_loader.h>

#define GLFW_EXPOSE_NATIVE_WIN32    // Required to define for the #include below -- allows GLFW to show Windows handles (HWND/hInstance access)
#include <GLFW/glfw3native.h>       // Gives GLFW Native interface for this OS (so allows Win32 Functions on GLFW windows)

#define VK_USE_PLATFORM_WIN32_KHR // Similarily with GLFW, this specifies to Vulkan that you want to get Vulkan's WindowsOS-specific functions (VkWin32SurfaceCreateInfoKHR/vkCreateWin32SurfaceKHR - access to these functions from Vulkan)

#include <tiny_obj_loader.h>

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <stdexcept>

constexpr uint32_t WIDTH = 800;
constexpr uint32_t HEIGHT = 600;


// Validation layers flag if debug. We have to define ourselves what validation layers we will actually want to use.
const std::vector<char const*> validationLayers = {
    "VK_LAYER_KHRONOS_validation"
};

#ifdef NDEBUG
constexpr bool enableValidationLayers = false;
#else
constexpr bool enableValidationLayers = true;
#endif

// This is nothing more than just a handy thingy to know in order to check extensions/libs... do whatever!
void some_handy_printer( vk::raii::Context* context, std::vector<const char*>* req_layers, std::vector<const char*>* req_extensions  )
{
    // For a list of all the supported extensions this app has available, use this.
    auto extensions = (*context).enumerateInstanceExtensionProperties();

    // For a list of all the supported layers this app uses has available, use this.
    auto layers = (*context).enumerateInstanceLayerProperties();

    std::cout << "extensions this app has available\n";
    for (const auto& extension : extensions) {
        std::cout << '\t' << extension.extensionName << '\n';
    }

    std::cout << "extensions actually required\n";
    for (const auto& requiredExtension : *req_extensions ) {
        std::cout << '\t' << requiredExtension << '\n';
    }

    std::cout << "layers this app has available\n";
    for (const auto& layer : layers ) {
        std::cout << '\t' << layer.layerName << '\n';
    }

    std::cout << "layers actually required\n";
    for (const auto& requiredLayer : *req_layers ) {
        std::cout << '\t' << requiredLayer << '\n';
    }
    std::cout << "\n\n";
}



class HelloTriangleApplication
{
  public:
	void run()
	{
		initWindow();
		initVulkan();
		mainLoop();
		cleanup();
	}

  private:
    GLFWwindow* window = nullptr; // GLFW Window Struct: this stores the handle (Win32's HWND... the instance object above!), size, position, input, etc.
    vk::raii::Context context; // Holds RAII context -- sets up vulkan's initialization
    // These two are equivalent: vk::raii::instance just has a destructor so we don't really have to manage memory lifetime and destroying it. They're both the same thing, though
    //VkInstance instance;
    vk::raii::Instance instance = nullptr; // Handle to the vulkan instance (like win32's HWND!)

    // Destruction order is declared in reverse from how theyre declared? debugMsg gets destroyed first, instance second
    // this tells Vulkan about the callback function. You don't touch this. Vulkan internally handles it. Once its created, Vulkan knows about the callback. If it goes out of scope, the debug callback won't occur anymore.
    vk::raii::DebugUtilsMessengerEXT debugMessenger = nullptr;

    // Vulkan isn't designed to interface directly with the window system on its own (GLFW, in our case).
    // In order to establish that connection between Vulkan and the window system, we need to use the WSI (aka Window System Integration) extension.
    // VkSurfaceKHR is an object that represents the type of surface to present rendered images to (there's other WSI objects, this is just one of them -- this is a INSTANCE level extension)
    // This is already enabled for our instance by us using glfwGetRequiredInstanceExtensions -- it's present in the list. Within the list, there's some other WSI extensions, too.
    // Create the window surface object AFTER our instance creation, because otherwise it can influence the physical device selection (if you're doing off-screen rendering, WSI isn't necessary btw)
    vk::raii::SurfaceKHR window_surface = nullptr; // the CREATION of it relies on OS window system details, like for Windows (os) it'll need the HWND/HMODULE from Win32, so it actually represents VK_KHR_win32_surface on the Windows OS
        // The USAGE of this variable is platform flexible, but the literal creation of it inherently relies on the OS: for linux, it represents/contains VK_KHR_xlib_surface (X11 OS)/VK_KHR_xcb_surface (XCB OS); on windows, VK_KHR_win32_surface.
            // Furthermore, the "extensions this application has available print" shows VK_KHR_win32_surface because a) we're using Windows and b) it's automatically/specifically given from glfwRequiredExtensions (on linux, it'd show one of the two)

    vk::raii::PhysicalDevice physicalDevice = nullptr; // the PHYSICAL device -- the GPU, this stores the handle
    vk::raii::Device logicalDevice = nullptr; // the LOGICAL device: it's an interface to communicate with the GPU, this stores the requested QUEUES and GPU SPECIFIC EXTENSIONS+FEATURES
    vk::raii::Queue graphicsQueue = nullptr;   // the queue is automatically created when we created the logical device (vk::DeviceCreateInfo), this is just the handle to interface/use them. They don't need to be manually destroyed, it's implicit, no cleanup() mention needed.


    // To enable extensions, you use this
    std::vector<const char*> requiredDeviceExtension = {
        vk::KHRSwapchainExtensionName }; // Originally, its a VK_KHR_swapchain macro, but the vk:: is just a wrapper -- theyre equivalent. Use the VK_KHR_SWAPCHAIN_EXTENSION_NAME macro to have the compiler check for misspellings
        /// the VK_KHR_swapchain extension (initialized below) is required for presenting rendered images from the device to the window.
            // See big_notes for some added information.

    // The swap chain is the series/chain (kinda like a queue, depends on presentation mode) of 'framebuffers', where GPUs draw to these individual framebuffers first for rendering, and then the swap chain appropriately sends (by choosing which framebuffer to display) images to the window surface.
    vk::raii::SwapchainKHR swapChain = nullptr;
    std::vector<vk::Image> swapChainImages; // the image container in the swap chain
    // There are 3 kinds of sub-properties of the swap chain we need to check in order to see/set if swap chain's properties are compatible with our window surface.
    // VkSurfaceCapabilitiesKHR capabilities;          // Basic surface capabilities (min/max number of images in swap chain, min/max width and height of images, resolution) (contains swap extent)
    // std::vector<VkSurfaceFormatKHR> formats;        // Surface formats (pixel format, color space)
    // std::vector<VkPresentModeKHR> presentModes;     // Available presentation mode
    vk::Extent2D swapChain_Extent_ImageResolution;
    vk::SurfaceFormatKHR swapChain_surfaceFormat;
    vk::PresentModeKHR swapChain_presentationMode;

    void initWindow() {

        // This initializes the GLFW library -- it starts it up -- sets up internal state, memory, platform specific resources, etc. Returns GLFW_TRUE if successful.
        // If you don't include it, GLFW will NOT work. It is REQUIRED.
        glfwInit();

        // GLFW: Vulkan doesn't support making windows itself, so we use GLFW.
        // it's intended to be used with OpenGL, so we use GLFW_NO_API to tell it not to create a OpenGL context later (as we are using another API -- vulkan)
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
        // glfwWindowHint's first parameter is the setting we're overriding (so, the CLIENT_API, or GLFW_RESIZEABLE), and the second is the actual value we're setting it (NO GLFW API and NO WINDOW RESIZE).

        // This creates the window, and by default, will show it -- you don't have to do ShowWindow like in win32.
        // glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE); To set its visibility: GLFW_FALSE == hidden, glfwShowWindow(window) to show it.
        window = glfwCreateWindow( WIDTH, HEIGHT, "Vulkan", nullptr, nullptr );
    }

    void initVulkan() {
        // The first step to using Vulkan is to initialize the Vulkan library by creating an instance.
        // The instance is the connection between your application and the Vulkan library.
        // Creating it involves specifying details about your application to the driver.
        createInstance();
        setupDebugMessenger(); // sets up our custom made debug callback

        createWindowSurface(); // actually creates our window surface to have Vulkan communicate with the Window

        pickPhysicalDevice(); // this picks the graphics card in the system that supports the features we need.
        createLogicalDevice(); // this describes what features we want to actually use, and what queues to create
        createSwapChain(); // create swap chain to have images actually render within the window
    }


    void mainLoop()
    {
        // GLFW's Win32 Message Loop equivalent (we're using GLFW for windowing -- vulkan can't natively). If an error or the closing of the window occurs, end the message loop.
        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();
        }
    }

    void cleanup()
    {
        // vkDestroyInstance(instance, nullptr); // Destroy instance object. Only needed for vkInstance (not vk::rai)
        // Second parameter is shared by all explicit destruction functions: pAllocator
        // It represents the way we're destroying it. 'nullptr' is default -- Vulkan handles it. It specifies a callback we can use for a custom memory allocator.
        // If we vkAllocateXXX'd, its counterpart is vkFreeXXX.

        // shits itself. destroy debug messenger first (its a child of instance). otherwise random error code 1.
        //debugMessenger = nullptr; // vkDestroyDebugUtilsMessengerEXT( instance, debugMessenger, nullptr); We're using RAII so we can't use this due to variable mismatch.
        //instance = nullptr; // switch them around and youll get a severe error message -- from callback!

        glfwDestroyWindow( window ); // Make sure to also destroy the GLFW objects since we also used a creation function on it
        glfwTerminate(); // Call this whenever we are COMPLETELY finished with the GLFW: it destroys EVERYTHING glfw related.
    }

    // broken garbage win32, fix later, maybe use the general, platform-agnostic one provided. (the example's only for learning purposes)
    void createWindowSurface()
    {
        // vk::Win32SurfaceCreateInfoKHR createInfo{ .hinstance = GetModuleHandle(nullptr), .hwnd      = glfwGetWin32Window(window) };
        // For WindowsOS specific, the above implementation is similar to VkSurfaceKHR but ONLY usable w/ WindowsOS (vkSurfaceKHR is a general, platform agnostic object, it contains linux's/windows/macs version of Win32SurfaceCreateInfoKHR)
        VkSurfaceKHR _surface;

        // First parameter is the instance to create the surface in; Second parameter is the pointer to the actual, specific window itself within that instance;
        // third is for custom allocators, google what it means; fourth is the output parameter that holds the handle of the created window surface.
        if ( glfwCreateWindowSurface(*instance, window, nullptr, &_surface) != 0 ) { // if the createFunction returns anything but 0, an error occurs ( 0 == VK_SUCCESS )
            throw std::runtime_error("failed to create window surface!");
        }
        // GLFW doesn't offer a special function for destroying the window surface, but wrapping it (encapsulating/using) with a vk::rai::SurfaceKHR object (window_surface) will let Vulkan automatically delete it with RAII when out of scope.
        window_surface = vk::raii::SurfaceKHR(instance, _surface);

    }


	void createInstance()
    {

        // in C++20, you can initialize a aggregate's member variables directly from initialization -- this is called designated initialization.
        // You can do it out of order of how they're defined, there's no real advantage outside of convenience, though
        // The nice thing is we can ignore things we don't care about (aka what we want defaulted, and only modify whatever we want)
        // For an example of pre-C++20, check BIG_NOTES_VULKAN.TXT for the equivalent.
        // Instead of passing data through function parameters, vulkan uses a lot of structs like this to pass stuff (instead of like a set_App_Name(), or something)
        constexpr vk::ApplicationInfo appInfo{.pApplicationName   = "Hello Triangle",
                                            .applicationVersion = VK_MAKE_VERSION( 1, 0, 0 ),
                                            .pEngineName        = "No Engine",
                                            .engineVersion      = VK_MAKE_VERSION( 1, 0, 0 ),
                                            .apiVersion         = vk::ApiVersion14 };

        // These are the extensions our program will actually need to use -- Get the required extensions
        auto requiredExtensions = getRequiredInstanceExtensions();

        // These are the layers our program will actually need to use -- Get the required layers.
        auto requiredLayers = getRequiredInstanceLayers();

        // Actually create the vulkan instance.
        // To see its full member variables, just google it. (theres more than im listing)
        // The first important one is 'flags' (not shown rn)
        vk::InstanceCreateInfo createInfo{
            .pApplicationInfo = &appInfo,                                               // The app info we just created: look at what appInfo holds to see what it is.
            .enabledLayerCount = static_cast<uint32_t>(requiredLayers.size()),          // This is just how many layers we need (count)
            .ppEnabledLayerNames = requiredLayers.data(),                               // This actually lists the layers' names
            .enabledExtensionCount = static_cast<uint32_t>(requiredExtensions.size()),  // This is just how many extensions we need (count)
            .ppEnabledExtensionNames = requiredExtensions.data()                        // This actually lists the extensions' names (UPDATE: THESE ARE VULKAN API EXTENSIONS, NOT GPU)
        };

        // This is like createInstance in win32: we now have all the required information, so actually create the instance.
        instance = vk::raii::Instance(context, createInfo);

        some_handy_printer( &context, &requiredLayers, &requiredExtensions );
    }


    bool checkDeviceQueueSupport_bitwise( vk::raii::PhysicalDevice const& physicalDevice )
    {
        // Check if our device can support whatever queue family (check the tutorial website for a lambda function version):
        // Everything in Vulkan uses a queue: from drawing to uploading textures, commands are submitted through a queue -- each family of queues allows only a subset of commands.
        auto availableQueueFamilies = physicalDevice.getQueueFamilyProperties(); // Check/contains what queue families are supported by the device -- what queue families we can use with this device.
        // vk::QueueFlags is a container for queue flag bits: we are saying "the required queues are eGraphics and eCompute" here -- we use | (BITWISE OR), it's weird.
        vk::QueueFlags requiredQueues = vk::QueueFlagBits::eGraphics | vk::QueueFlagBits::eCompute; // maybe move this vector variable someplace else like w/ extensions? idk
            // For bits, think of it | as "I'm turning this bit on", not "either or"; & is used for comparing and checking if a flag is enabled (it's kinda backwards)

        for ( const auto& availableQueueFamily : availableQueueFamilies )
        {
            if ( availableQueueFamily.queueFlags & requiredQueues ) { // bitmasks contain data for MULTIPLE bools (flag bits in our case), hence why we don't have to iterate over multiple requiredQueues like w/ _iterate() -- we just check if it contains all of them.
                std::cout << physicalDevice.getProperties().deviceName << " supports all our required queue families! (BITWISE)\n";
                return true;
            }
        }
        return false;
    }


    bool checkDeviceExtensionSupport( vk::raii::PhysicalDevice const& physicalDevice )
    {
        // Check if our device can support whatever extensions (Check the tutorial website for a lambda function version):
        auto availableDeviceExtensions = physicalDevice.enumerateDeviceExtensionProperties(); // Checks what extensions are supported by the device
        unsigned extension_count = 0; // just some counter im using to check if this physical device supports all the required extensions.
        for ( const auto& availableDeviceExtension : availableDeviceExtensions )
        {
            for ( const auto& requiredDeviceExtension : requiredDeviceExtension )
            {
                if ( strcmp( availableDeviceExtension.extensionName, requiredDeviceExtension ) == 0 ) {
                    extension_count++;
                    break;
                }
            }
        }
        if ( extension_count >= requiredDeviceExtension.size() ) {
            std::cout << physicalDevice.getProperties().deviceName << " supports all our required extensions!\n";
            return true;
        }
        return false;
    }



    bool isDeviceSuitable( vk::raii::PhysicalDevice const& physicalDevice )
    {
        auto deviceProperties = physicalDevice.getProperties();  // can .Name/.ID/.Type

        // Check if our device can support this vulkan's API version:
        bool supportsVulkan1_3 = deviceProperties.apiVersion >= vk::ApiVersion13;
        if ( supportsVulkan1_3 )
            std::cout << deviceProperties.deviceName << " supports ApiVersion13!\n";

        bool supports_required_queues = checkDeviceQueueSupport_bitwise( physicalDevice );

        bool supports_required_extensions = checkDeviceExtensionSupport( physicalDevice );

        // Check if our device can support whatever features
        // We need to grab ahold of this physical device's vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan13Features, vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>(), so we use a template function
        auto features = physicalDevice.template getFeatures2<vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan13Features, vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>();
        // Then we check if they actually exist for this physical device through features (these act as a sort of bool here -- if they're false, our device doesn't support it)
        bool supportsRequiredFeatures = features.template get<vk::PhysicalDeviceVulkan13Features>().dynamicRendering &&
                                        features.template get<vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>().extendedDynamicState;
        if ( supportsRequiredFeatures )
            std::cout << deviceProperties.deviceName << " supports our required features!\n";

        // Extensions and Features are similar: extensions are OPTIONAL addons and are not guaranteed to exist for every GPU, whereas features are CORE operations that the GPU can do, controlled by Vulkan.
        // Features are added with every Vulkan update. Features are like 64 bit floats, texture compression, etc.



        // See the function calls for clarification.
        bool is_device_suitable = supportsVulkan1_3 && supports_required_queues && supports_required_extensions && supportsRequiredFeatures;

        std::cout << deviceProperties.deviceName << ( is_device_suitable ? " supports everything!\n" : "isn't suitable.\n" );

        return is_device_suitable;
    }


	void pickPhysicalDevice()
	{
        std::vector<vk::raii::PhysicalDevice> physicalDevices = instance.enumeratePhysicalDevices();

        if ( physicalDevices.empty() ) { // Physical Devices is empty if there are no devices with vulkan support.
            throw std::runtime_error("failed to find GPUs with Vulkan support!");
        }

        // this uses a lambda function because the old one was causing errors.
		auto const devIter = std::ranges::find_if(physicalDevices, [&](auto const &physicalDevice) { return isDeviceSuitable(physicalDevice); });
		if (devIter == physicalDevices.end())
		{
			throw std::runtime_error("failed to find a suitable GPU!");
		}
		physicalDevice = *devIter; // If we have multiple GPUs, the recommended way is to filter them based on whats better -- see big_notes for some examples. We're just choosing the first one found.
        std::cout << "using " << physicalDevice.getProperties().deviceName << " as our physical device!\n";
	}


    void createLogicalDevice()
    {
        std::vector<vk::QueueFamilyProperties> queueFamilyProperties = physicalDevice.getQueueFamilyProperties(); // We're reusing this: we just need to see what our selected physical device can queue.

        // get the first index into queueFamilyProperties which supports both graphics and present
        uint32_t queueIndex = ~0;
        for ( uint32_t qfpIndex = 0; qfpIndex < queueFamilyProperties.size(); qfpIndex++ )
        {
        if ( (queueFamilyProperties[qfpIndex].queueFlags & vk::QueueFlagBits::eGraphics ) && // If the queue family supports eGraphics,
                physicalDevice.getSurfaceSupportKHR( qfpIndex, *window_surface ) )           // and this queue family supports presenting images to the window surface...
            {
                // then we've found a queue family that supports both graphics and present, set queueIndex as that queue we just found.
                queueIndex = qfpIndex;
                break;
            }
        }
        if (queueIndex == ~0) {
            throw std::runtime_error("Could not find a queue for graphics and present -> terminating");
        }


        // to actually specify the queues to be created, we need to feed data into a vk::DeviceQueueCreateInfo struct, similarily with createInfo/createDebugMessenger creation -- common thing in Vulkan.
        float queuePriority = 0.5f; // Vulkan allows us to assign priorities for the queue to affect scheduling -- this is required even if we've a single queue. (range of 0.0 - 1.0)
        vk::DeviceQueueCreateInfo deviceQueueCreateInfo { .queueFamilyIndex = queueIndex, // This also says what this queueFamily creation represents (so eGraphics for this example) [ALL THIS DOES IS SET THE ACTUAL QUEUE FAMILY TYPE]
                                                          .queueCount = 1, // how many queues we want to create in this family
                                                          .pQueuePriorities = &queuePriority }; // Queue Priority only matters within the same family, not across different families:
                                    // pQueuePriorities IS A VECTOR/ARRAY THAT HOLDS MULTIPLE QUEUE PRIORITIES; so this is valid { 0.8, 0.5, 1.0 } ORDER MATTERS, FIRST QUEUE IS 0.8, THIRD IS 1.0
        // End

        // Not needed for now. Come back to this when stuff gets interesting (i guess?) -- initializes itself to vk::False
        // It specifies the used device features.
        vk::PhysicalDeviceFeatures deviceFeatures;

        // vk::PhysicalDeviceFeatures2 is the container for EVERYTHING including and beyond vulkan version 1.1 -- vk::PhysicalDeviceFeatures is 1.0
        // vk::PhysicalDeviceVulkan13Features SPECIFICALLY only contains vulkan v1.3 features.
        // vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT contain Extension-specified features
        // in order to enable any feature, Vulkan uses a concept of "structure chaining", where each feature struct (the <> feature structs specified below)
        // has a pNext field that can point to another unrelated feature struct, which is a "chain of feature requests" -- the vulkan C++ API provides a helper template vk::StructureChain to make this easier.
        // First step: we create a vk::StructureChain with 3 different feature structs, and for each different struct, we provide an initializer, assign them below with {}, seperating w/ comma
        vk::StructureChain<vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan13Features, vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT> featureChain = {
            {},                               // vk::PhysicalDeviceFeatures2 (empty for now)
            {.dynamicRendering = true },      // vk::PhysicalDeviceVulkan13Features - enable the 'dynamic rendering' feature from Vulkan 1.3
            {.extendedDynamicState = true }   // vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT - enable the 'extended dynamic state' feature from the extension struct
        }; // vk::StructureChain automatically connects these structs together by setting up the pNext pointer between them, so now we have one object containing all 3 structs and their requested extensions (even if they're unrelated to one another)!
        // As a result of them being chained together, when actually creating the logical device later, we just pass a pointer to the first structure in this chain, which will then trickle down to allow Vulkan seeing the other 2.
            // An added thing: the function expects PhysicalDeviceFeatures (base) to be FIRST in the chain, but the order of the other structs dont matter
                // vk::StructureChain<vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT, vk::PhysicalDeviceVulkan13Features> works too -- just the first one matters!

        // After we handled the features(vulkan base api)/extensions(optional device capabilities), and specifying the queues, we can finally create the logical device.
        vk::DeviceCreateInfo deviceCreateInfo{
            .pNext = &featureChain.get<vk::PhysicalDeviceFeatures2>(),                      // what features we're enabling -- Points to the start of the chain (which is base vk::PhysicalDeviceFeatures2) due to pNext linking the other structs thanks to vk::StructureChain
            .queueCreateInfoCount = 1,                                                      // the amount of queues we are creating
            .pQueueCreateInfos = &deviceQueueCreateInfo,                                    // Queue creation info for EACH family (YOU HAVE TO PASS A VECTOR/ARRAY OF vk::DeviceQueueCreateInfo.data(), SO APPEND EACH QUEUE FAMILY INTO A CENTRAL VECTOR, then .data() the vector)
            .enabledExtensionCount = static_cast<uint32_t>(requiredDeviceExtension.size()), // the number of device extensions we're enabling (FOR EXTENSIONS, IT'S VERY SIMILAR TO vk::InstanceCreateInfo's EXTENSION)
            .ppEnabledExtensionNames = requiredDeviceExtension.data() };                    // the list of the names of the extensions to enable (HOWEVER, THIS DIFFERS FROM vk::InstanceCreateInfo BECAUSE THESE EXTENSIONS ARE GPU EXTENSIONS, NOT VULKAN API EXTENSIONS.)
            // some vulkan update made device-specific and instance validation layers inseperable, so they're now all vulkan instance layers (so adding .enabledLayerCount + ppEnabledLayerNames is obsolete, those go in instance creation)
                // the EXTENSIONS are seperate, the LAYERS are not (layers are not shown above w/ vk::DeviceCreateInfo, its just an added member variable).

        // The final step to create the logical device is to just give our original vk::raii::Device device (member variable to this struct) value!
        // the first param is the physical device, and the second is the info we just gave it (specifying what queues, and device-specific extensions and features to use)
        logicalDevice = vk::raii::Device( physicalDevice, deviceCreateInfo ); // logical devices operate through the physical device and as a result don't interact directly with the instance, which is why we're not passing instance as a param.
            // instance = the ENTIRE Vulkan API instance; physical device = the actual graphics card; logical device = the interface of the GPU.

        // First parameter is the logical device which we created the queue with (the queue exists, we're just giving it a handle, it's stored inside logical_device)
        // The second parameter is an int which represents what family index we want (so we want graphicsIndex here, we changed it to queueIndex to check if that queue family supports both eGraphics+Presenting Images to surfaces)
        // Third parameter: queue familys can have multiple queues, we're just selecting what exact queue we want in that family (starts from 0, we have 1 .QueueCount within the eGraphics family, so 0)
        graphicsQueue = vk::raii::Queue( logicalDevice, queueIndex, 0 ); // P.S, we're passing the second param from earlier (not from the logical_device itself) because vulkan doesn't store it.
    }


    void createSwapChain()
    {
        // It is important that we only try to query for swap chain support after verifying that the extension is available.
        // If we don't have the required extension to begin with (VK_KHR_swapchain), it'll be undefined behaviour (potential crashes) as we're checking... garbage.
        // PhysicalDevice was already determined to have the support, so we can freely continue -- if we didn't have a supported GPU, our program would crash from throw std::runtime_error("failed to find a suitable GPU!"); in pickPhysicalDevice()

        // vkGetPhysicalDeviceSurfaceCapabilitiesKHR( physicalDevice, window_surface, &details.capabilities ); // IF using this w/ a VK::RAII object, like window_surface is VK::RAI, use *window_surface to convert window_surface to a non-vk-raii object.
            // The non-RAII way of getting ahold of these. third parameter is an output just to store a SurfaceCapabilitiesKHR object -- you'd do the same but vkGetPhysicalDeviceSurfaceFormats/PresentModesKHR() for the 2 others.

        // The idea is that we're getting the supported properties of the specific window surface on the specific physical device -- we're choosing the settings for our swap chain image's
        // adding operator*window_surface is equivalent if window_surface is a non-pointer, vk::rai object.
        // this is because it makes the VK::RAII window_surface object back to its non VK::RAII object
        // so, (object type vk::raii::SurfaceKHR) *window_surface is implicitly converted to (object type vkSurfaceKHR) window_surface
            // For this application alone (drawing a triangle), swap chain support is enough if, for the window surface, there's atleast one supported image format, and one supported presentation mode.
                // In order to optimally choose the best settings for our swap chain to utilize, we have to consider, then choose the best from these 3 types of settings:
                // 1. Surface format (color depth); 2. Presentation mode (conditions for "swapping" images to the screen); 3. Swap extent (resolution of images in swapchain).

        vk::SurfaceCapabilitiesKHR windowSurface_Capabilities = physicalDevice.getSurfaceCapabilitiesKHR( window_surface );
        swapChain_Extent_ImageResolution = chooseSwapChain_SwapExtent( windowSurface_Capabilities ); // this gets the image resolution of the swap chain's framebuffers
        uint32_t minImageCount = chooseSwapChain_ImageCount( windowSurface_Capabilities ); // the minimum image count our swap chain must have: Vulkan requires a swap chain must ALWAYS have the minimum or more number of images in it.

        std::vector<vk::SurfaceFormatKHR> availableFormats = physicalDevice.getSurfaceFormatsKHR( window_surface );
        swapChain_surfaceFormat  = chooseSwapChain_SurfaceFormat( availableFormats ); // choose the swap chain's format ( pixel color and color space )

        std::vector<vk::PresentModeKHR> availablePresentModes = physicalDevice.getSurfacePresentModesKHR( window_surface );
        swapChain_presentationMode = chooseSwapChain_PresentationMode( availablePresentModes ); // choose the swap chain's presentation mode ( order of images -- fifo, immediate, mailbox )

        vk::SwapchainCreateInfoKHR swapChainCreateInfo
        {
            .surface          = *window_surface, // the window surface to present the swap chain's images onto
            .minImageCount    = minImageCount,
            .imageFormat      = swapChain_surfaceFormat.format,
            .imageColorSpace  = swapChain_surfaceFormat.colorSpace,
            .imageExtent      = swapChain_Extent_ImageResolution,
            .imageArrayLayers = 1, // specifies how many layers (stacked 2D images) per one swap chain image (always 1 unless developing stereoscopic 3D app)
            .imageUsage       = vk::ImageUsageFlagBits::eColorAttachment, // specifies how we want to use our image (color in a render directly to our swap chain for our example, but also as storage, sampled, texture, copying, transferring, etc); if we use the image outside of what we declared its usageImage, it's undefined behaviour.
            .imageSharingMode = vk::SharingMode::eExclusive, // specifies if the image is to be used by multiple different queue families:
                // eExclusive means one queue family per image (image ownership must also be explicitly transferred before using it in another queue family, but better performance);
                // eConcurrent means multiple queue families CAN affect an image and thus don't have to explicitly transfer ownership (HOWEVER, you have to specify IN ADVANCE what queue families share ownership thru queueFamilyIndexCount + pQueueFamilyIndices, and you have to specify at minimum 2 families).
            .preTransform     = windowSurface_Capabilities.currentTransform, // how the image should be transformed before it's sent to the swap chain (rotation/flipped/mirrored, etc -- currentTransform specifies no transforming)
            .compositeAlpha   = vk::CompositeAlphaFlagBitsKHR::eOpaque, // the alpha channel of the image RELATIVE to the window surface (you can blend different windows with another, too) -- you can make the WINDOW'S BACKGROUND (like the user's desktop) visible. eOpaque prevents it, though.
            .presentMode      = swapChain_presentationMode,
            .clipped          = true // specifies if it should render non-visible pixels: if true, Vulkan won't render pixels that aren't visible (offscreen or obscured by something else); if false, Vulkan renders EVERY pixel even if it's not visible.
            // One more mentioned member: .oldSwapchain (for now set is = nullptr), it's touched upon later but if the swap chain is invalid/unoptimized, we need to create a new one and additionally point to the (now unoptimized/invalid) old swap chain.
        };

        // we give the logical device because a swap chain object is created for that specific, logical (and thus physical) device -- we're using the capabilities of the physical device, and by proxy the logical device's
            // and obviously we're giving the create info as w/ every other creation object to actually give the swap chain value.
        swapChain = vk::raii::SwapchainKHR( logicalDevice, swapChainCreateInfo );
        swapChainImages = swapChain.getImages();
    }


    uint32_t chooseSwapChain_ImageCount( const vk::SurfaceCapabilitiesKHR& availableSurfaceCapabilities )
    {
        // vk::SurfaceCapabilitiesKHR::minImageCount is the minimum amount of images the swap chain must have (so as a direct result its the minimum amount of framebuffers, too)
        // 3u is for triple buffering, which is just a convention, so we use it as a "default"; if our minimum image count requires it to be higher though, we use that instead.
        auto minImageCount = std::max(3u, availableSurfaceCapabilities.minImageCount);

        // Ensure the swap chain's minimum image count cannot go above the physical device's maximum image count -- 0 is a special value in this context which means no maximum, so we have to check if its actually max capped by checking over 0.
        if ( ( availableSurfaceCapabilities.maxImageCount > 0 ) && ( availableSurfaceCapabilities.maxImageCount < minImageCount ) )
            minImageCount = availableSurfaceCapabilities.maxImageCount;

        return minImageCount;
    }


    // Function to choose the swap chain's resolution of the images being displayed, out of this device's available capabilities -- typically equal to the resolution of the window we're drawing to in pixels
    vk::Extent2D chooseSwapChain_SwapExtent( const vk::SurfaceCapabilitiesKHR& availableSurfaceCapabilities )
    {
        // the struct, vk::SurfaceCapabilitiesKHR, tells us the range of possible resolutions
            // Vulkan usually recommends matching the resolution of the window, this is done by setting the width and height of its member variable .currentExtent
            // However, to indicate special treatment, set .currentExtent to the maximum value of uint32_t. If done, we're picking a resolution through usage of members .minImageExtent and .maxImageExtent

        // Whenever the Operating Systems determines the size of the window, Vulkan sets the currentExtent.width to be below the maximum value and appropriately sized for that window surface.
        // As a result, we use the currentExtent as our swap chain's image resolution because (essentially) the OS dictates it for us, and Vulkan'll 'fix' (essentially choose) it for us.
        if ( availableSurfaceCapabilities.currentExtent.width != std::numeric_limits<uint32_t>::max() )
            return availableSurfaceCapabilities.currentExtent;
        // if it's false, we are free to choose ourselves between the minimum and maximum resolution that this physical devices will allow.

        // Used to hold the framebuffer's width and height
        int width;
        int height;

        // a Framebuffer is an area in memory where the GPU 'draws' the image before it's displayed: images first go through the framebuffer, and then to the window surface. (its like a background canvas)
        // the Framebuffer is, most of the time, equal to the window surface.
        // However, it's possible the framebuffer can be LARGER (rarely smaller) than the window surface because its in PHYSICAL pixels, while the window surface is in LOGICAL pixels
        // Physical pixels can be larger than logical pixels because of a user's high DPI (dots per inch);
        // as a result, the image is downscaled (or upscaled if the framebuffer is smaller, where upscaling gives poor image quality -- bad) to match the window surface.
        glfwGetFramebufferSize(window, &width, &height); // glfwGetFramebufferSize() gets the framebuffer's resolution in PHYSICAL PIXELS.
            // if the window has high DPI (due to user settings), the mismatch between window surface and framebuffer occurs.
            // this function takes IN the window handle/pointer w/ the first parameter, and OUTPUTs its width and height through the second/third params.


        vk::Extent2D dimensions;
        // Clamp just ensures the output (first parameter) is within the ranges of second parameter (minimum allowed value) to third parameter (maximum allowed value).
        // if the first parameter is greater than the maximum allowed, return the maximum allowed; if the first parameter is less than the minimum allowed, return the minimum allowed.
        // if the first parameter is within the range of minimum and maximum, return the first parameter.
        // the reason we're using clamp is so our dimensions of our swap chain images aren't the over the maximum, or under the minimum, dimensions of this physical device's capabilities.
        dimensions.width = std::clamp<uint32_t>(width, availableSurfaceCapabilities.minImageExtent.width, availableSurfaceCapabilities.maxImageExtent.width);
        dimensions.height = std::clamp<uint32_t>(height, availableSurfaceCapabilities.minImageExtent.height, availableSurfaceCapabilities.maxImageExtent.height);

        // Return the swap chain's clamp'd (between-ish) image resolution.
        return dimensions;
    }



    // Function to choose the swap chain's presentation mode out of this device's available present modes
    vk::PresentModeKHR chooseSwapChain_PresentationMode( const std::vector<vk::PresentModeKHR>& availablePresentModes )
    {
        // The presentation mode is the actual conditions for showing images to the screen via the swap chain. There's a total of 4 options.
            // vk::PresentModeKHR::eImmediate: Images submitted by your application are immediately sent
            // vk::PresentModeKHR::eFifo: First in, first out - images are put in a queue (swap chain), presenting an image every refresh cycle (vSync) -- EVERY vulkan supported device has eFifo support
            // vk::PresentModeKHR::eFifoRelaxed: eFifo, but if we miss a refresh, present an image immediately
            // vk::PresentModeKHR::eMailbox: only one frame is kept in storage, new frames replace old ones (high energy usage, but generally best)

            for ( const auto& availablePresentMode : availablePresentModes )
            {
                if ( availablePresentMode == vk::PresentModeKHR::eMailbox )
                    return vk::PresentModeKHR::eMailbox; // could also return availablePresentMode, but whatever, literally doesn't matter
            }
        return vk::PresentModeKHR::eFifo; // EVERY Vulkan supported graphics card has eFifo available as a presentation mode, so it's our default if we don't find/choose anything else.
    }


    // Function to choose the swap chain's surface format out of this device's available formats
    vk::SurfaceFormatKHR chooseSwapChain_SurfaceFormat( const std::vector<vk::SurfaceFormatKHR>& availableFormats )
    {
        assert(!availableFormats.empty());

        // vk::SurfaceFormatKHR contains a 'format' and 'colorSpace' member variable.
            // format - stores the color channel and types. (kinda like that thing in win32 where it stores color + alpha type)
                // for example, 'vk::Format::eB8G8R8A8Srgb' means we're storing Blue, Green, Red, and Alpha channels (in that order) with 8bit unsigned integers, totalling 32bits per pixel.
                    // The final portion (Srgb) is the color space + gamma curve for interpreting pixels, but the member variable colorSpace is still present because Vulkan seperates them.
            // colorSpace - which affects the presentation and interpretation on the window: sets a colorSpace to use
                // the colorSpace we're using is probably gonna be SRGB (because it's results are better, so if it's available, use it): vk::Format::eB8G8R8A8Srgb utilizes SRGB (end of it - Srgb)

        // We search through this device's available formats, and if we find a format that supports color format eB8G8R8A8Srgb and the SRGB color space, return in.
        for ( const auto& availableFormat : availableFormats )
        {
            if ( availableFormat.format == vk::Format::eB8G8R8A8Srgb &&
                availableFormat.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear ) {
                return availableFormat;
            }
        }

        return availableFormats[0];
    }


    std::vector<const char*> getRequiredInstanceExtensions()
    {
        uint32_t glfwExtensionCount = 0;
        // Get the required instance extensions from GLFW.
        // This is GLFW EXTENSIONS ONLY. It's zero (right now) because glfw doesn't require any extensions.
        auto glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount); // Just like win32, the parameter is an out: it gives value back to glfwExtensionCount

        std::vector extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

        if ( enableValidationLayers )
        {
            extensions.push_back(vk::EXTDebugUtilsExtensionName); // extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME); // Equivalent. Original is a neater macro.
        }

        // Check if the required GLFW extensions are supported by the Vulkan implementation. See equivalent to this with ranges + lambda in big notes (aka check the website for lambda function).
        auto extensionProperties = context.enumerateInstanceExtensionProperties(); // Currently, context is empty because we didn't give it anything.
        for (uint32_t i = 0; i < glfwExtensionCount; ++i)
        {
            bool found = false;
            for ( auto& extensionProperty : extensionProperties )
            {
                // strcmp is a standard C++ function that compares two strings:
                // 0 if the two strings are equal; < 0 if A < b; > 0 if A > B.
                if ( strcmp(extensionProperty.extensionName, glfwExtensions[i]) == 0 )
                {
                    found = true;
                    break;
                }
            }

            if ( !found ) {
                throw std::runtime_error("Required GLFW extension not supported: " + std::string(glfwExtensions[i]));
            }
        }

        return extensions;
    }


    std::vector<const char*> getRequiredInstanceLayers()
    {
        // These are the layers our program WANTS to use
        std::vector<char const*> requiredLayers;
        if ( enableValidationLayers ) {
            requiredLayers.assign(validationLayers.begin(), validationLayers.end());
        }

        // These are the layers we have within this vulkan implementation.
        auto layerProperties = context.enumerateInstanceLayerProperties();

        // if a required layer ISNT found within the current layers we have, throw an error (check the tutorial website for a lambda version)
        for ( const auto& requiredLayer: requiredLayers )
        {
            bool found = false;
            for ( auto& layerProperty : layerProperties )
            {
                //strcmp is a standard C++ function that compares two strings:
                // 0 if the two strings are equal; < 0 if A < b; > 0 if A > B.
                if ( strcmp(layerProperty.layerName, requiredLayer) == 0 )
                {
                    found = true;
                    break;
                }
            }
            if ( !found ) {
                throw std::runtime_error("Required layer not supported: " + std::string(requiredLayer));
            }
        }

        return requiredLayers;
    }


    void setupDebugMessenger()
    {
        if (!enableValidationLayers) return;

        vk::DebugUtilsMessageSeverityFlagsEXT severityFlags(vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError);

        vk::DebugUtilsMessageTypeFlagsEXT messageTypeFlags( vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation);

        vk::DebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCreateInfoEXT{.messageSeverity = severityFlags,    // Specifies what types of severities we want our callback to be called for.
                                                                              .messageType     = messageTypeFlags, // Similarily, specifies what messages we want our callback to be called for.
                                                                              .pfnUserCallback = &debugCallback};  // Specifies the function we're using to callback -- points to that function.
                                                                                                                   // One more parameter: .pUserData, which is the pointer to whatever data we want to send over to the callback function.
                                                                                                                   // For example, you can pass a pointer to this HelloTriangleApplication class, or whatever
                                                                                                                        // (its like that one thing w/ win32, forgot the name, but its the same principle).
        // createDebugUtilsMessengerEXT() just assigns the value of debugMessenger, so its creating it so it actually does something -- the param we pass is the thing right above which contains all the data we want regarding the callback.
        debugMessenger = instance.createDebugUtilsMessengerEXT( debugUtilsMessengerCreateInfoEXT );
    }



    static VKAPI_ATTR vk::Bool32 VKAPI_CALL debugCallback(vk::DebugUtilsMessageSeverityFlagBitsEXT       severity,      // This specifies the severity of the message -- see big_notes.
                                                          vk::DebugUtilsMessageTypeFlagsEXT              type,          // The message type of the error -- see big_notes.
                                                          const vk::DebugUtilsMessengerCallbackDataEXT*  pCallbackData, // What data/object caused this error -- see big_notes.
                                                          void*                                          pUserData)     // You can pass your own data here.
    {
        std::cerr << "validation layer: type " << to_string(type) << " msg: " << pCallbackData->pMessage << std::endl;

        if (severity >= vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning) {
            std::cerr << "this error is severe enough to show!\n";
        }

        return vk::False; // this returns a bool (vk::Bool32 is a type of bool just formatted by Vulkan differently) -- return false here as a general rule of thumb.
    }
};

int main()
{
	try
	{
		HelloTriangleApplication app;
		app.run();
	}
	catch (const std::exception &e)
	{
		std::cerr << e.what() << std::endl;
		return EXIT_FAILURE;
	}

    std::cout << "\tSuccessfully Ended!\n";
	return EXIT_SUCCESS;
}