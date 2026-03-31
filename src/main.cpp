#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#define GLFW_INCLUDE_VULKAN


#if defined(__INTELLISENSE__) || !defined(USE_CPP20_MODULES)
#	include <vulkan/vulkan_raii.hpp>
#else
import vulkan_hpp;
#endif

#define GLFW_INCLUDE_VULKAN        // REQUIRED only for GLFW CreateWindowSurface.

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
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

    // Destruction order is declared in reverse from how theyre declared? debugMsg gets destroyed first, instance second.
    // this tells Vulkan about the callback function. You don't touch this. Vulkan internally handles it. Once its created, Vulkan knows about the callback. If it goes out of scope, the debug callback won't occur anymore.
    vk::raii::DebugUtilsMessengerEXT debugMessenger = nullptr;

    vk::raii::PhysicalDevice physicalDevice = nullptr; // the PHYSICAL device -- the GPU, this stores the handle
    vk::raii::Device logicalDevice = nullptr; // the LOGICAL device: it's an interface to communicate with the GPU, this stores the requested QUEUES and GPU SPECIFIC EXTENSIONS+FEATURES
    vk::raii::Queue graphicsQueue = nullptr;   // the queue is automatically created when we created the logical device (vk::DeviceCreateInfo), this is just the handle to interface/use them. They don't need to be manually destroyed, it's implicit, no cleanup() mention needed.

    // To enable extensions, you use this, but this isn't being elaborated on until later. Come back later again.
    std::vector<const char*> requiredDeviceExtension = {
        vk::KHRSwapchainExtensionName }; // Originally, its a VK_KHR_swapchain macro, but the vk:: is just a wrapper -- theyre equivalent.
        /// the VK_KHR_swapchain extension is required for presenting rendered images from the device to the window.


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
        pickPhysicalDevice(); // this picks the graphics card in the system that supports the features we need.
        createLogicalDevice(); // this describes what features we want to actually use, and what queues to create
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


    bool isDeviceSuitable( vk::raii::PhysicalDevice const& physicalDevice )
    {
        auto deviceProperties = physicalDevice.getProperties();  // can .Name/.ID/.Type
        auto deviceFeatures = physicalDevice.getFeatures();      // for optional features like 64 bit floats, texture compression, etc.


        // Check if our device can support this vulkan's API version:
        bool supportsVulkan1_3 = deviceProperties.apiVersion >= vk::ApiVersion13;
        if ( supportsVulkan1_3 )
            std::cout << deviceProperties.deviceName << " supports ApiVersion13!\n";


        // Check if our device can support whatever queue family (check the tutorial website for a lambda function version):
        // Everything in Vulkan uses a queue: from drawing to uploading textures, commands are submitted through a queue -- each family of queues allows only a subset of commands.
        auto queueFamilies = physicalDevice.getQueueFamilyProperties(); // Check/contains what queue families are supported by the device -- what queue families we can use with this device.
        bool device_supports_our_graphics = false;
        for ( const auto& queueFamily : queueFamilies )
        {
            if ( queueFamily.queueFlags & vk::QueueFlagBits::eGraphics ) { // & check if eGraphics bit is present within queueFamily.QueueFlags (which is a bitmask* of integers representing states)
                device_supports_our_graphics = true; // If we find that corresponding bit (eGraphics), our device supports our graphics (doesnt have to be eGraphics -- example)
                std::cout << deviceProperties.deviceName << " supports our queue family!\n";
                break; // *A bitmask is just a integer where each bit/integer represents a flag -- it's just a way to pack a series of bools/flags quickly and w/o taking too much space.
            } // we use & here (not ==) because & is able to check whether or not that specific flag is present within that bitmask, where bitmasks contain a bunch of other stuff -- acts as a filter.
        }


        // Check if our device can support whatever extensions (Check the tutorial website for a lambda function version):
        auto availableDeviceExtensions = physicalDevice.enumerateDeviceExtensionProperties(); // Check/contains what extensions are supported by the device
        unsigned extension_count = 0; // just some counter im using.
        bool supportsAllRequiredExtensions = false;
        for ( const auto& availableDeviceExtension : availableDeviceExtensions )
        {
            for ( const auto& requiredDeviceExtension : requiredDeviceExtension )
            {
                if ( strcmp( availableDeviceExtension.extensionName, requiredDeviceExtension ) == 0 )
                {
                    extension_count++;
                    break;
                }
            }
        }
        if ( extension_count == requiredDeviceExtension.size() )
        {
            supportsAllRequiredExtensions = true;
            std::cout << deviceProperties.deviceName << " supports our required extensions!\n";
        }


        // Check if our device can support whatever features
        // We need to grab ahold of this physical device's vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan13Features, vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>(), so we use a template function
        auto features = physicalDevice.template getFeatures2<vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan13Features, vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>();
        // Then we check if they actually exist for this physical device through features (these act as a sort of bool here -- if they're false, our device doesn't support it)
        bool supportsRequiredFeatures = features.template get<vk::PhysicalDeviceVulkan13Features>().dynamicRendering &&
                                        features.template get<vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>().extendedDynamicState;
        if ( supportsRequiredFeatures )
            std::cout << deviceProperties.deviceName << " supports our required features!\n";


        // Extensions and Features are similar: extensions are OPTIONAL addons and are not guaranteed to exist for every GPU, whereas features are CORE operations that the GPU can do, controlled by Vulkan.
        // Features are added with every Vulkan update.

        bool is_device_suitable = supportsVulkan1_3 && device_supports_our_graphics && supportsAllRequiredExtensions && supportsRequiredFeatures;
        std::cout << deviceProperties.deviceName << ( is_device_suitable ? " supports everything!\n" : "isn't suitable\n" );

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
	}


    void createLogicalDevice()
    {
        std::vector<vk::QueueFamilyProperties> queueFamilyProperties = physicalDevice.getQueueFamilyProperties(); // We're reusing this: we just need to see what our selected physical device can queue.

        // Start -- Check the tutorial website for a lambda function version.
        vk::QueueFamilyProperties* graphicsQueueFamilyProperty = nullptr;
        for ( vk::QueueFamilyProperties& queueFamily : queueFamilyProperties )
        {
            if ( ( queueFamily.queueFlags & vk::QueueFlagBits::eGraphics) != static_cast<vk::QueueFlags>(0) ) {
                graphicsQueueFamilyProperty = &queueFamily;
                break;
            }
        }

        if ( !graphicsQueueFamilyProperty ) {
            std::cout << "The physical device doesn't support the eGraphics queue family\n";
            return;
        }

        // IF we want to check for another queue family with the physical device, like eCompute or whatever (idk), repeat this exact same logic -- the code above only returns a single queue family (whatever specified -- eGraphics)
        // that ALSO includes the vk::DeviceQueueCreateInfo creation: make a seperate thing (like vk::DeviceQueueCreateInfo deviceComputeQueueCreateInfo) and assign it.
        // the creation only makes ONE specific queue family -- if we want another queue family, we'd have to make two vk::DeviceQueueCreateInfo objects -- see // Start -> // End for the whole logic of one. Repeat for eCompute/another.
        auto graphicsIndex = static_cast<uint32_t>( graphicsQueueFamilyProperty - queueFamilyProperties.data() ); // get the index by subtracting
            // Keep your eyes peeled here, me.

        // to actually specify the queues to be created, we need to feed data into a vk::DeviceQueueCreateInfo struct, similarily with createInfo/createDebugMessenger creation -- common thing in Vulkan.
        float queuePriority = 0.5f; // Vulkan allows us to assign priorities for the queue to affect scheduling -- this is required even if we've a single queue. (range of 0.0 - 1.0)
        vk::DeviceQueueCreateInfo deviceQueueCreateInfo { .queueFamilyIndex = graphicsIndex, // This also says what this queueFamily creation represents (so eGraphics for this example)
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
        // The second parameter is an int which represents what family index we want (so we want graphicsIndex here)
        // Third parameter: queue familys can have multiple queues, we're just selecting what exact queue we want in that family (starts from 0, we have 1 .QueueCount within the eGraphics family, so 0)
        graphicsQueue = vk::raii::Queue( logicalDevice, graphicsIndex, 0 ); // P.S, we're passing the second param from earlier (not from the logical_device itself) because vulkan doesn't store it.
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