#include "vertex.cpp"

constexpr uint32_t WIDTH = 800;
constexpr uint32_t HEIGHT = 600;

std::ofstream outputFile("../garbage_dump.txt");

// Frames in Flight refers to having multiple frames be rendered at once -- the rendering of one frame doesn't interfere with another.
// This constant defines how many frames should be processed simulatenously
constexpr int MAX_FRAMES_IN_FLIGHT = 2;
    // We chose 2 because we don't want the CPU to get too far ahead of the GPU -- it's a tradeoff, but generally this is ideal, it allows the CPU and GPU to be working on their own tasks at the same time.
        // If the CPU finishes early, then it'll wait for the GPU to finish rendering before submitting more work.
    // With 3 or more frames in flight, the CPU can get ahead of the GPU, adding frames of latency, which isn't ideal.

// Validation layers flag if debug. We have to define ourselves what validation layers we will actually want to use.
const std::vector<char const*> validationLayers = {
    "VK_LAYER_KHRONOS_validation"
};

#ifdef NDEBUG
constexpr bool enableValidationLayers = false;
#else
constexpr bool enableValidationLayers = true;
#endif

class HelloTriangleApplication
{
  public:
	void run()
	{
		initWindow();
        outputFile << get_current_time() << " | Finished setting up the GLFW window" << std::endl; // endL flushes it out while making a new line over \n.
		initVulkan();
        outputFile << get_current_time() << " | Finished setting up Vulkan" << std::endl;
		mainLoop();
        outputFile << get_current_time() << " | Exited the Main Loop" << std::endl;
		cleanup();
        outputFile << get_current_time() << " | Cleaned up the objects before exiting" << std::endl;
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


    // To enable extensions, you use this
    std::vector<const char*> requiredDeviceExtension = {
        vk::KHRSwapchainExtensionName }; // Originally, its a VK_KHR_swapchain macro, but the vk:: is just a wrapper -- theyre equivalent. Use the VK_KHR_SWAPCHAIN_EXTENSION_NAME macro to have the compiler check for misspellings
        /// the VK_KHR_swapchain extension (initialized below) is required for presenting rendered images from the device to the window.
            // See big_notes for some added information.

    vk::raii::PhysicalDevice physicalDevice = nullptr; // the PHYSICAL device -- the GPU, this stores the handle
    // we CANNOT get the queueIndex directly from the graphicsQueue object (even though we're constructing the queue w/ it), we need to track it ourselves.
        // this is needed for the command pool, hence why it's a member.
        // used to get (and then store) the first index into queueFamilyProperties which supports both graphics and present
    uint32_t queueIndex = ~0;
    vk::raii::Device logicalDevice = nullptr; // the LOGICAL device: it's an interface to communicate with the GPU, this stores the requested QUEUES and GPU SPECIFIC EXTENSIONS+FEATURES
    vk::raii::Queue graphicsQueue = nullptr;   // the queue is automatically created when we created the logical device (vk::DeviceCreateInfo), this is just the handle to interface/use them. They don't need to be manually destroyed, it's implicit, no cleanup() mention needed.

    // The swap chain is the series/chain (kinda like a queue, depends on presentation mode) of 'framebuffers', where GPUs draw to these individual framebuffers first for rendering, and then the swap chain appropriately sends (by choosing which framebuffer to display) images to the window surface.
    vk::raii::SwapchainKHR swapChain = nullptr;
    // There are 3 kinds of sub-properties of the swap chain we need to check in order to see/set if swap chain's properties are compatible with our window surface.
    vk::Extent2D swapChain_Extent_ImageResolution;  // The swap chain's extent (which is image resolution)
        // VkSurfaceCapabilitiesKHR capabilities;          // 1. Basic surface capabilities (min/max number of images in swap chain, min/max width and height of images, resolution) (Capabilities struct contains the Extent -- struct above)
    vk::SurfaceFormatKHR swapChain_surfaceFormat;   // 2. Surface formats (pixel format, color space)
    vk::PresentModeKHR swapChain_presentationMode;  // 3. Available presentation mode
    std::vector<vk::Image> swapChainImages; // the image container in the swap chain
    std::vector<vk::raii::ImageView> swapChainImageViews; // how we access the image

    // At this point, we're required to make a pipeline layout but won't be using it until a few chapter, so we're making it empty.
    vk::raii::PipelineLayout pipelineLayout = nullptr;
    vk::raii::Pipeline graphicsPipeline = nullptr; // the pipeline ITSELF!

    // A command pool manages the memory that is used to store the command buffers, and the command buffers are allocated from the command pool.
    vk::raii::CommandPool commandPool = nullptr;
    // Command Buffers are objects that are used to record commands which can be submitted through the device's queue for the command buffer commands' execution.
    // Command Buffers are automatically destroyed whenever the Command Pool is destroyed.
    // https://docs.vulkan.org/spec/latest/chapters/cmdbuffers.html
    std::vector<vk::raii::CommandBuffer> commandBuffers;
        // commandBuffer(s) is now a vector due to us wanting to have multiple frames in flight: each frame needs its own command buffer.

    vk::raii::Buffer vertexBuffer = nullptr; // the buffer that'll be used to store/house vertex data (duh) -- this does not necessarily store anything, just references memory (in our case, vertexBufferMemory)
    vk::raii::DeviceMemory vertexBufferMemory = nullptr; // the handle to the allocated GPU memory reserved for the vertex buffer -- this will ACTUALLY contain the vertex data

    // see big_notes for an elaboration.
    // semaphore = forces GPU to wait; fence = forces CPU to wait.
    std::vector<vk::raii::Semaphore> presentCompleteSemaphore; // To signal an image has been grabbed from the swap chain, and is ready for rendering
    std::vector<vk::raii::Semaphore> renderFinishedSemaphore;  // To signal an image has finished rendering and that presentation can occur.
    std::vector<vk::raii::Fence> drawFence;                    // To ensure only one frame is rendered and presented at a time.
        // Similarily with a vector of command buffers, every frame needs its own respective semaphore and fence to track it, hence we're making it a vector.
    uint32_t wait_frameIndex = 0; // To assign semaphores their respective frames to track.

    bool framebufferResized = false;

    void initWindow() {

        // This initializes the GLFW library -- it starts it up -- sets up internal state, memory, platform specific resources, etc. Returns GLFW_TRUE if successful.
        // If you don't include it, GLFW will NOT work. It is REQUIRED.
        glfwInit();

        // GLFW: Vulkan doesn't support making windows itself, so we use GLFW.
        // it's intended to be used with OpenGL, so we use GLFW_NO_API to tell it not to create a OpenGL context later (as we are using another API -- vulkan)
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
        // glfwWindowHint's first parameter is the setting we're overriding (so, the CLIENT_API, or GLFW_RESIZEABLE), and the second is the actual value we're setting it (NO GLFW API and NO WINDOW RESIZE).

        // This creates the window, and by default, will show it -- you don't have to do ShowWindow like in win32.
        // glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE); To set its visibility: GLFW_FALSE == hidden, glfwShowWindow(window) to show it.
        window = glfwCreateWindow( WIDTH, HEIGHT, "Vulkan", nullptr, nullptr );

        // With win32, you can store pointers inside a window instance to transmit data ( SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)this) ) -- this is the same thing but with GLFW
            // We're doing this so we can refer to this (second param - this class, helloTriangleApplication) framebufferResized inside of the callback
        glfwSetWindowUserPointer(window, this);
        // this function is pretty much equivalent to specifying what happens in a window after WM_SIZE is passed within the message loop w/ Win32
            // Essentially: whenever this window (first param) sends WM_SIZE (window gets resized, glfwSetFramebufferSizeCallback), call framebufferResizeCallback() (second parameter)
            // An important note is we're passing the function as a pointer to glfwSetFramebufferSizeCallback ( hence the lack of () ) -- GLFW calls framebufferResizeCallback() itself and passes window + width/height.
        glfwSetFramebufferSizeCallback( window, framebufferResizeCallback );

    }

    // the callback is static because GLFW can't call a member function with a this-> pointer (we're inside class HelloTriangleApplication, functions implicitly have this->)
    static void framebufferResizeCallback(GLFWwindow* window, int width, int height)
    {
        // same thing as w/ win32 ( reinterpret_cast<HelloTriangleApplication*>(GetWindowLongPtr(hwnd, GWLP_USERDATA)) ): its just grabbing the user data pointer from this window
        auto app = reinterpret_cast<HelloTriangleApplication*>( glfwGetWindowUserPointer( window ) );
        app->framebufferResized = true;

        // this technically does nothing for us (specifically me) because our we check (result == vk::Result::eSuboptimalKHR) || (result == vk::Result::eErrorOutOfDateKHR) already covers window resizes
        // We're just being explicit: if the GLFW window sends WM_SIZE (not literally... kinda, but win32 equivalent for GLFW), present the last image as normal (suboptimal, itll work)
        // and also recreate the swap chain for the next images to actually be sized appropriately
        // We are just being explicit. Also, some platforms/drivers lack the vk::Result::eSuboptimalKHR and eError results, so it's good practice.
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
        createImageViews();

        createGraphicsPipeline(); // For an explanation of the graphics pipeline, see BIG_NOTES

        createCommandPool(); // see function for elaboration

        createVertexBuffer(); // see function for elaboration

        createCommandBuffers(); // see function for elaboration

        createSyncObjects(); // see big_notes for what sync objects are

    }


    void mainLoop()
    {
        // GLFW's Win32 Message Loop equivalent (we're using GLFW for windowing -- vulkan can't natively). If an error or the closing of the window occurs, end the message loop.
        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();
            drawFrame();
        }

        // Wait for the logicalDevice to finish operations before exiting the main loop.
        logicalDevice.waitIdle();
    }

    void drawFrame()
    {

        outputFile << get_current_time() << " | Beginning drawFrame" << std::endl;

        // Waits for the logicalDevice's fence state to be signaled (true) before continuing past this line.
            // The first parameter is an array of fences (we only have one so we just pass one fence object) that we want to wait upon
            // The second parameter specifies if we want to wait for either: one of the fences to be signaled, or for all the fences to be signaled BEFORE returning, resulting in a wait within this line.
            // Third parameter is a fail safe: if we  waited for the maximum integer number in nanoseconds with this wait, disable the timeout (because it means something went wrong) and continue
        auto fenceResult = logicalDevice.waitForFences( *drawFence[wait_frameIndex], vk::True, UINT64_MAX );

        if (fenceResult != vk::Result::eSuccess)
			throw std::runtime_error("failed to wait for fence!"); // Self explanatory: if for some reason waitForFences() didn't wait (it is expected for it to wait on the line above -- alternatively, we reached 64bit limit), we throw an exception because something went wrong.

        // First param is the timeout: wait 64bit integer limit in nanoseconds before letting us pass
        // Second param is the semaphore: whenever we've acquired the next image (and finished executing acquireNextImage), signal the semaphore
        // Third param is the fence: we're not using a fence as a signal, so we use nullptr. Same logic as above.
        // the function itself returns two values: vk::Result, and the index of the vk::image within the swapChainImages array.
            // We then use that index to pick the vk::raii:FrameBuffer, and then use the command buffer on that framebuffer.
        auto [result, imageIndex] = swapChain.acquireNextImage(UINT64_MAX, *presentCompleteSemaphore[wait_frameIndex], nullptr);

        // Note: We could also recreate the swap chain if an image acquired is eSuboptimalKHR, but we're just... not. whatever.
        if ( result == vk::Result::eErrorOutOfDateKHR ) // If we return this, this means the swap chain is incompatible with the new surface and can no longer be used for rendering.
            // if including #define VULKAN_HPP_HANDLE_ERROR_OUT_OF_DATE_AS_SUCCESS, instead of throwing an exception (crashing the program if it isn't catched), eErrorOutOfDateKHR will be treated as a success code and actually execute as if it's a success
            // Right now, since we never defined it, it'll never get to this point because it'll just crash the program.
        {
            std::cout << "HOW'D YOU GET HERE?\n";
            recreateSwapChain(); // And yeah, won't ever get here, but if it does, we need to recreate the swap chain (duh) because the swap chain no longer works.
            return; // note: we return here because we don't want to present the image (UNLIKE BELOW W/ SUBOPTIMAL)
        }

        if ( result != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR ) // If the acquisition of an image failed, just stop the program, nothing special.
        {
            assert(result == vk::Result::eTimeout || result == vk::Result::eNotReady);
            throw std::runtime_error("failed to acquire swap chain image!");
        }

        // UPDATE: if result is eErrorOutOfDateKHR (it CANNOT OCCUR RIGHT NOW BECAUSE WE DIDN'T DEFINE IT AND SUBSEQUENTLY CATCH IT -- RIGHT NOW IT'LL CRASH THE PROGRAM), we MUST set this to unsignaled AFTER we've checked for eErrorOutOfDateKHR
        // because OTHERWISE the return (from above) causes the next drawFrame to wait till time-expiry due to drawFrame being unsignaled
        logicalDevice.resetFences( *drawFence[wait_frameIndex] ); // Signal the fence back to unsignaled so that the next iteration can run as expected (OTHERWISE, drawFence is set to signaled, causing waitForFences to never wait)

        recordCommandBuffer( imageIndex );
        // Notice: we rerecord the command buffer each time, different each time depending on the image
            // Essentially, we record the commands we want to occur onto that image, hence why we re-record it each time -- it's image specific!

        graphicsQueue.waitIdle();
        // VULKAN NOTE: for simplicity, wait for the queue to be idle before starting the frame
		    // In the next chapter you see how to use multiple frames in flight and fences to sync
        // ME: removing this causes a validation layer error.
            // and still does after the next chapter.

        vk::PipelineStageFlags waitDestinationStageMask( vk::PipelineStageFlagBits::eColorAttachmentOutput );

        // the vk::SubmitInfo configures the queue submission and its synchronization through its members.
        const vk::SubmitInfo submitInfo {
            .waitSemaphoreCount   = 1, // the number of semaphores we're waiting on before execution.
            .pWaitSemaphores      = &*presentCompleteSemaphore[wait_frameIndex], // the actual Semaphores to wait upon before execution.
            .pWaitDstStageMask    = &waitDestinationStageMask, // what stages of the pipeline where we need to wait for.
                // We want to wait for writing colours to the image before we submit it, so we specify to to wait during the color attachment stage which is responsible for colour.
                // This means that the vertex shader and such can execute upon other images while this image is not available yet for submission.
            .commandBufferCount   = 1, // the number of command buffers we want to execute.
            .pCommandBuffers      = &*commandBuffers[wait_frameIndex], // the command buffer itself.
            .signalSemaphoreCount = 1, // the number of Semaphores to signal whenever the command buffer(s) specified above finish executing
            .pSignalSemaphores    = &*renderFinishedSemaphore[wait_frameIndex] // the semaphores themselves to signal.
        };

        // we can now submit the command buffer to the graphics queue (submitInfo contains the command buffer, which in turn contains the image we just recorded onto it, so thats how we ultimately display images!)
        // First parameter takes an array of vk::SubmitInfo structs (an array because it's more efficient for much larger workloads)
        // Second parameter is the fence that'll be signaled whenever the command buffer finishes execution. (Which will be waited upon for the next frame -- next call to drawFrame()!)
        graphicsQueue.submit( submitInfo, *drawFence[wait_frameIndex] ); // Fence gets sigaled whenever GPU fully finishes w/ graphics queue, so next call to drawFrame isn't blocked.



        // the FINAL step is to submit the image back to the swap chain to have it eventually show up on the screen!
        // The presentation is configured through vk::PresentInfoKHR.
        const vk::PresentInfoKHR presentInfoKHR {
            .waitSemaphoreCount = 1, // the number of semaphores to wait upon before presenting an image
            .pWaitSemaphores    = &*renderFinishedSemaphore[wait_frameIndex], // the semaphores themselves to wait upon. (we want to wait for the command buffer to finish executing: we want to wait for the rendering/drawing of a photo to finish before presenting)
            .swapchainCount     = 1, // the number of swap chains to present this image to
            .pSwapchains        = &*swapChain, // the swap chains themselves to present this image to
            .pImageIndices      = &imageIndex // the image's index assigned for each swap chain.
        };

        // vk::raii::Queue::presentKHR submits the request to present an image to the swap chain.
        result = graphicsQueue.presentKHR( presentInfoKHR );
        outputFile << get_current_time() << " | Presenting a new image" << std::endl;

        // you can technically remove the result checks and just use framebufferResized as a check, but i'm keeping it just for the sake of learning/keeping as is.
        if ( (result == vk::Result::eSuboptimalKHR) || (result == vk::Result::eErrorOutOfDateKHR) || framebufferResized ) // Suboptimal still works to present images to the screen, but just isn't 100% perfect; w/ eErrorOutOfDate, same gimmick as above
        {
            if ( result == vk::Result::eErrorOutOfDateKHR ) // we didn't define the macro above, so it should never get here (it'll crash.)
                std::cout << "WHAT???\n";

            framebufferResized = false;
            outputFile << get_current_time() << " | Recreating the swap chain due to resizing" << std::endl;
            recreateSwapChain(); // if we don't recreate the swap chain, the swap chain images aren't rendered properly because
        }
        else
        {
            // There are no other success codes than eSuccess; on any error code, presentKHR already threw an exception.
            assert(result == vk::Result::eSuccess);
        }
        // note: even after the recreation of the swapChain, it applies to the NEXT call to drawFrame() -- the "suboptimal" image was drawn and presented, we didn't skip its presentation.

        // We just need to increment the wait_frameIndex to cycle through the index to assign each semaphore a unique index so that it isn't tracking the same thing.
        // Modulo is used cleverly here, remember it: the moment it reaches the cap (like w/ the BLP!), it resets back to zero. It's equivalent to just doing if (wait_frameIndex >= FramesFlight) wait_frameIndex=0;
        wait_frameIndex = (wait_frameIndex + 1) % MAX_FRAMES_IN_FLIGHT;

        outputFile << get_current_time() << " | Exiting drawFrame" << std::endl;
    }


    void cleanup()
    {
        cleanupSwapChain(); // just destroys the swap chain + swap chain image views

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

        // Check if our device can support whatever features -- WE ARE NOT ENABLING THEM HERE, JUST QUERYING IF THEY'RE SUPPORTED -- WE ENABLE THEM IN createLogicalDevice()
        // We need to grab ahold of this physical device's vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan13Features, vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>(), so we use a template function
        auto features = physicalDevice.template getFeatures2<vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan11Features, vk::PhysicalDeviceVulkan13Features, vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>();
        // Then we check if they actually exist for this physical device through features (these act as a sort of bool here -- if they're false, our device doesn't support it)
        bool supportsRequiredFeatures = features.template get<vk::PhysicalDeviceVulkan11Features>().shaderDrawParameters &&
                                        features.template get<vk::PhysicalDeviceVulkan13Features>().dynamicRendering &&
                                        features.template get<vk::PhysicalDeviceVulkan13Features>().synchronization2 &&
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


            // We ACTUALLY enable features/extensions here after querying them within isDeviceSuitable()
        // vk::PhysicalDeviceFeatures2 is the container for EVERYTHING including and beyond vulkan version 1.1 -- vk::PhysicalDeviceFeatures is 1.0
        // vk::PhysicalDeviceVulkan13Features SPECIFICALLY only contains vulkan v1.3 features.
        // vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT contain Extension-specified features
        // in order to enable any feature, Vulkan uses a concept of "structure chaining", where each feature struct (the <> feature structs specified below)
        // has a pNext field that can point to another unrelated feature struct, which is a "chain of feature requests" -- the vulkan C++ API provides a helper template vk::StructureChain to make this easier.
        // First step: we create a vk::StructureChain with 3 different feature structs, and for each different struct, we provide an initializer, assign them below with {}, seperating w/ comma
        vk::StructureChain<vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan11Features, vk::PhysicalDeviceVulkan13Features, vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT> featureChain = {
            {},                               // vk::PhysicalDeviceFeatures2 (empty for now)
            {.shaderDrawParameters = true},   // vk::PhysicalDeviceVulkan11Features - UNMENTIONED IN DOCS: needed for shader creation otherwise warning -- we're just enabling it (we query'd support in isDeviceSuitable)
            { .synchronization2 = true, .dynamicRendering = true},      // vk::PhysicalDeviceVulkan13Features - enable the 'dynamic rendering' feature from Vulkan 1.3
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

    void createImageViews()
    {
        // Make sure the image view container is empty as we're creating it here.
        assert(swapChainImageViews.empty());

        vk::ImageViewCreateInfo imageViewCreateInfo {
            .viewType         = vk::ImageViewType::e2D,                         // Specifies we're rending to a 2D screen. (:e3D is 3d; :e1D is 1d)
            .format           = swapChain_surfaceFormat.format,                 // The color format
            .subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 } // Describe's the image's purpose and which part of the image should be accessed (we're specifying Color)
        }; // There's also a .components field which mixes color channels around: imageViewCreateInfo.components = { vk::ComponentSwizzle::eIdentity, vk::ComponentSwizzle::eIdentity, vk::ComponentSwizzle::eIdentity, vk::ComponentSwizzle::eIdentity}; for an example (no clue)
        // Hypothetically, if we had vk::SwapchainCreateInfoKHR::imageArrayLayers above 1, we should make multiple image views for each different layer to access them individually -- the maximum amount of image views is generally 16.

        for (auto &swapChainImage : swapChainImages)
        {
            // ImageViews objects are referencing the original Image object (similarily to std::string_view): we're just saying this this imageView's Info is pointing to this swapChainImage.
            // Then, we just push the imageView we just properly created (signalled by assigning .Image) into the swapChainImageViews member variable.
            imageViewCreateInfo.image = swapChainImage;
            swapChainImageViews.emplace_back( logicalDevice, imageViewCreateInfo );
                // notice how for every vk::raii::ImageView in swapChainImageView, we're using the same imageViewCreateInfo -- every image view has the same properties
        }
    }

    // A function that ensures the swap chain and its image views are destroyed before re-creating them.
    void cleanupSwapChain()
    {
        swapChainImageViews.clear();
        swapChain = nullptr;
    }


    // If the size of the window changes, our swap chain becomes incompatible because the swap chain is specifically tailored to a surface size.
        // otherwise, it'll produce wrong results as it's using the old surface size.
    void recreateSwapChain()
    {
        outputFile << get_current_time() << " | Beginning the Recreation of the swap chain" << std::endl;

        int width = 0, height = 0;
        glfwGetFramebufferSize(window, &width, &height); // initial check for the size. recreateSwapChain is called whenever the window size changes, hence why it's here.
            // Window doesn't necessarily have to be minimized here, but if you collapse the window to be zero, it'll also run this loop (aka you drag the window down)
        while (width == 0 || height == 0) {
            glfwGetFramebufferSize(window, &width, &height); // grabs the size while inside of the loop: if we're inside this loop, our screen's minimized (0w,0h)
                // Whenever opening the screen back up again, the size changes to be non-zero, so by setting width+height > 0, we exit the loop (which again, is given from this function)
            glfwWaitEvents(); // puts GLFW to sleep
        }


        logicalDevice.waitIdle(); // blocks the CPU (won't go beneath this line): waits for the GPU to not be executing anything (be idle) before recreating the entire swap chain.

        // Clean up the swap chain and its image views before re-creating them below.
        cleanupSwapChain();

        createSwapChain();
        // As a result of changing the swap chain, we also need to change our image views as the image views are supposed to be referencing the current swap chain's images, not the old one.
        createImageViews();

        outputFile << get_current_time() << " | Finished the Recreation of the swap chain" << std::endl;
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
            .minImageCount    = minImageCount, // it's odd but in this swap chain that we're creating, the maximum number of framebuffers within this swap chain is the .minImageCount (name convention is inhereted from SurfaceCapabilitiesKHR) -- fixed after creation.
                // After creation of the swap chain, in GPU Memory, the number of empty canvases is equal to .minImageCount's value, which will then be used as framebuffers.
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
                // In cases where we need to create a new swap chain, pass the old Swap Chain in .oldSwapchain, and then destroy the old swap chain whenever it's finished its work. (you can know if it's finished working w/ a semaphore)
        };

        // we give the logical device because a swap chain object is created for that specific, logical (and thus physical) device -- we're using the capabilities of the physical device, and by proxy the logical device's
            // and obviously we're giving the create info as w/ every other creation object to actually give the swap chain value.
        swapChain = vk::raii::SwapchainKHR( logicalDevice, swapChainCreateInfo );
        swapChainImages = swapChain.getImages(); // returns a vector of images (which currently are empty canvases, formally framebuffers), where the number of images (or canvases, formally framebuffers) within is equal to chooseSwapChain_ImageCount()'s return;
    }


    uint32_t chooseSwapChain_ImageCount( const vk::SurfaceCapabilitiesKHR& availableSurfaceCapabilities )
    {
        // vk::SurfaceCapabilitiesKHR::minImageCount is the minimum amount of images the swap chain must have (so as a direct result its the minimum amount of framebuffers, too)
        // 3u is for triple buffering, which is just a convention, so we use it as a "default"; if our minimum image count requires it to be higher though, we use that instead.
        auto minImageCount = std::max(3u, availableSurfaceCapabilities.minImageCount);

        // Ensure the swap chain's minimum image count cannot go above the physical device's maximum image count -- 0 is a special value in this context which means no maximum, so we have to check if its actually max capped by checking over 0.
        if ( ( availableSurfaceCapabilities.maxImageCount > 0 ) && ( availableSurfaceCapabilities.maxImageCount < minImageCount ) )
            minImageCount = availableSurfaceCapabilities.maxImageCount;

        // return the total image count
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


    void createGraphicsPipeline()
    {
        auto shaderCode = readFile_SPIRVShaders("../shaders/slang.spv");
        std::cout << "ShaderCode Size: " << shaderCode.size() << "\n";

        /* PROGRAMMABLE PIPELINE FUNCTIONS */

        // the shader module is just wrapping around the shader bytecode in another file.
        // after we finished creating the graphics pipeline, it's no longer needed. hence, local to createGraphicsPipeline() instead of being a class member.
        vk::raii::ShaderModule shaderModule = createShaderModule( shaderCode );

        vk::PipelineShaderStageCreateInfo vertexShader_StageInfo {
            .stage = vk::ShaderStageFlagBits::eVertex, // identifies this structure: what stage of the graphic pipeline this shader is for
            .module = shaderModule,                    // the shader module's code to use
            .pName = "vertMain"                        // the entry point to use (vertMain is our entry point here because this is a vertex shader)
        };

        // something kinda interesting is that shaderModule contains the code for the fragment shader as well (hence why it's being used for the following fragment shader's module creation).
        // but by specifying what our entry point is by .pName, we guarantee we don't run fragment shader and vertex shader code together as we're calling its respective, independant function.
            // this also means that we can make different functions to act as entry points in case we want to make something specific or whatever for one of these modules.
        vk::PipelineShaderStageCreateInfo fragmentShader_StageInfo {
            .stage = vk::ShaderStageFlagBits::eFragment,
            .module = shaderModule,
            .pName = "fragMain"
        };
        // To make the Tesselation and Geometry shaders, it's the exact same process. For this program, it's not needed, so we're skipping them.
            // Just store it in a vector for now as we're gonna be referencing the general shader stages later.
        vk::PipelineShaderStageCreateInfo shaderStages[] { vertexShader_StageInfo, fragmentShader_StageInfo };


        /* FIXED PIPELINE FUNCTIONS (STATES) */

            // Vertex Input
        // this struct/state describes the format of the vertex data that'll be passed onto the Vertex Shader.
        // This happens in two ways: Bindings and Attribute Descriptions -- see vertex.cpp's Attribute and Binding Descriptions functions for an elaboration.
        auto bindingDescription    = Vertex::getBindingDescription();       // Bindings: specifies whether the data is per-vertex or per-instance (per group of vertices), and the spacing between data.
        auto attributeDescriptions = Vertex::getAttributeDescriptions();    // Attribute Descriptions: the vertex's attributes (position, format/color, and what binding to load them from and which offset)
        vk::PipelineVertexInputStateCreateInfo vertexInputInfo {
            .vertexBindingDescriptionCount   = 1,                   // the number of bindings we're using
            .pVertexBindingDescriptions      = &bindingDescription, // the bindings themselves
            .vertexAttributeDescriptionCount = static_cast<uint32_t>( attributeDescriptions.size() ), // the number of attributes we have per vertex
            .pVertexAttributeDescriptions    = attributeDescriptions.data()                           // the attribute data themselves -- again, we pass .data() to allow for pointer arithmetics
        }; // Old comment, but relevant: the .pVertexBindingDescriptions and .pVertexAttributeDescriptions members point to an array of structs that describe details for loading vertex data.

            // Input Assembly
        // this struct describes what kind of geometry will be drawn from the vertices, and if primitive restart (how vertices are grouped into primitives/shapes) should be enabled (can ONLY be enabled for Strip topologies).
            // to specify the geometry from vertices, use vk::PrimitiveTopology: these can be triangles (for every 3 vertices), lines (for every 2), or points -- we're making a triangle so eTriangleList
        // Normally, vertices are loaded from the vertex buffer by index in sequential order. However with an 'element buffer', you can specify the specific indices to use.
            // Enabling the .primitiveRestartEnable member to vk::True allows us to break up the lines and triangles with the vk::PrimitiveTopology::e<Line/Triangle>Strip using
            // a special index of 0xFFFF or 0xFFFFFFFF (which allows us to create our own lines between vertices), which tells the GPU to make a new primitive strip.
        vk::PipelineInputAssemblyStateCreateInfo inputAssembly {
            .topology = vk::PrimitiveTopology::eTriangleList // specifies what topology we're using to make primitives (shapes: triangles/lines/points) by connecting vertices together
        };

            // Viewport and Scissor
        // For a VERY nice visual: https://docs.vulkan.org/tutorial/latest/_images/images/viewports_scissors.png
        // A viewport is the region of the framebuffer where the exact image will be rendered to, where at this step, the image itself is geometrically unmodified, but it's positioning and scale is modified depending on the viewpoint's area.
        // the scissor state will discard every pixel outside its rectangle
        // It goes like this: viewport state -> rasterization -> scissor state
        vk::Viewport viewport{
            0.0f, 0.0f, // X and Y: the viewport's upper left corner (higher Y level = downwards -- a little weird but whatever)
            static_cast<float>( swapChain_Extent_ImageResolution.width ),   // the viewport's width (so how right it goes relative to the X)
            static_cast<float>( swapChain_Extent_ImageResolution.height ),  // the viewport's height (so how down it goes relative to the Y)
            0.0f, 1.0f // min and max depth range (range of 0.0 -> 1.0, generally keep these 0.0f + 1.0f if youre not doing anything special).
        };
        // Same logic as the viewport's rectangle (X, Y + width, height): first parameter is the upper left corner of the scissor rectangle, second parameter is how right/down it goes. (we gave it the size of the entire framebuffer, so it's 1:1)
        vk::Rect2D scissor{ vk::Offset2D{ 0, 0 }, swapChain_Extent_ImageResolution };
            // this is relative to the FRAMEBUFFER (NOT THE VIEWPORT), so 0,0 is the 0,0 of the framebuffer, NOT to the viewport if say the viewport starts at x100,y100 of the framebuffer, scissor w/ x0,y0'll start at 0,0 on the framebuffer.
        // We are currently at pipeline creation time within this function (createGraphicsPipeline()).
        // With a dynamic state, we just need to declare their count of pipeline creation, but due to it being set as dynamic, the viewport and scissor rectangles themselves are setup at drawing time.
            // (we feed .pScissors and .pViewports as with any other createInfo -- the data itself, all we're doing now is giving the number of, but nothing related to the actual rectangles)
        // Without a dynamic state, we would have to declare the rectangles themselves at pipeline creation (right here), thus making them immutable (unless you create a new pipeline altogether)
            // vk::PipelineViewportStateCreateInfo viewportState{.viewportCount = 1, .pViewports = &viewport, .scissorCount = 1, .pScissors = &scissor};
        vk::PipelineViewportStateCreateInfo viewportState{.viewportCount = 1, .scissorCount = 1};
        // The scissor and viewport states can either be static or dynamic, but generally people prefer them to be dynamic for flexibility -- we set them to be dynamic with the dynamicState vector at the bottom.

            // Rasterizer
        // Rasterizer takes the geometry formed by the vertices' connecting lines, and then turns it into fragments to later be coloured by the fragment shader.
            // It also performs depth testing (whether or not a fragment is behind/infront an object, deciding visibility), face culling (removes front/back facing triangles), and the scissor test;
                // if a fragment fails the depth test by being outside the far/near planes (viewport's minDepth + maxDepth), or if it's behind another fragment (in turn object), it'll be discarded (or clamped depending on if .depthClampEnable is true).
            // also can be assigned to just fill the edges (wireframe), or fill entire polygons with fragments -- all this is specified with the _createInfo struct below.
        vk::PipelineRasterizationStateCreateInfo rasterizer {
            .depthClampEnable        = vk::False, // if true, if a fragment's depth isn't between the near and far planes, its depth is clamped (to vk::Viewport's minDepth + maxDepth) instead of discarded (requires enabling a GPU feature + is useful for shadow mapping).
                // this doesn't color anything. this determines whether or not a fragment is present at all based upon the viewport's minDepth and maxDepth and if it's behind an object
                // this is nice for shadow mapping because those fragments which would previously be discarded are present for shadows
            .rasterizerDiscardEnable = vk::False, // if true, geometry doesn't pass through the rasterizer stage (skipping it entirely), which disables output to the framebuffer (so nothing is visible)
            .polygonMode             = vk::PolygonMode::eFill, // determines how fragments are generated for geometry (eFill = entire area of polygon w/ fragments; eLine = edges are drawn as lines (wireframe); ePoint = vertices are drawn as points)
                // Any other polygon mode aside from eFill requires enabling a GPU feature (make sure to first query for it first!)
            .cullMode                = vk::CullModeFlagBits::eBack, // Determines what faces to cull: can cut out the front, back, both, or disable it entirely. (as a result, whatever facing triangles won't be shown)
            .frontFace               = vk::FrontFace::eClockwise, // Specifies what vertex order is 'front facing': either clockwise or counter-clockwise (if a triangle is back-facing, it's likely behind something, and so not visible)
            .depthBiasEnable         = vk::False, // The rasterizer can alter the depth values by either adding a constant, or biasing them based upon a fragment's slope in order to "skew" the depth test. This is sometimes used for shadowmapping.
            .lineWidth               = 1.0f // the thickness of the lines (1.0f = a line 1.0 pixel thick): the maximum width is depends on hardware, and having this value be above 1.0f requires enabling the wideLines gpu feature.
        };



            // Multisampling
        // this struct configures multisampling, which is one of the ways to perform anti-aliasing
            // it works by combining multiple polygon's fragments that rasterize and populate a single pixel.
            // furthermore, these are called "SAMPLES" of the same pixel: one pixel contains multiple samples (if .rasterizationSamples > 1) to choose a colour that reduces jagged edges -- so it blends together.
                // as an example, a pixel is a jar, and samples are the colours inside that jar; individual samples get their color from the base fragment (the defaulted color) , or from depth (whether or not its covered)
                // for a fragment to get its FINAL colour, the samples are averaged for that one individual fragment.
        vk::PipelineMultisampleStateCreateInfo multisampling {
            .rasterizationSamples = vk::SampleCountFlagBits::e1, // We are saying "we only want 1 sample per fragment", which means multi-sampling is disabled
            .sampleShadingEnable = vk::False // determines whether the fragment shader runs for every fragment (=false) or every sample (=true) -- true has nicer colours and smoother edges, but it's greatly more expensive as it runs per SAMPLE (and every fragment can have multiple samples)
        }; // we're revisiting this in another chapter.


            // Depth and Stencil Testing (TO DO: ELABORATED UPON LATER, we're passing nullptr within the pipeline creation)
        // this also discards fragments (two way filter process through this and rasterization)
        // vk::PipelineDepthStencilStateCreateInfo;


            // Colour Blending
        // Whenever a fragment shader returns a colour on a certain fragment, it needs to be combined with the already existing colour in the framebuffer. This transformation is called Colour Blending, where there's two methods to achieve this:
            // 1. Mix the old and new colour to make a final colour, or 2. Combine the old and new colour using a bitwise operation
        // There's two structs to configure colour blending: 1. vk::PipelineColorBlendAttachmentState is for configurating colour blending per framebuffer,
        // while 2. vk::PipelineColorBlendStateCreateInfo is for configuring colour blending globally. (which also means it'll affect per-framebuffer)
        vk::PipelineColorBlendAttachmentState colorBlendAttachment { // see big_notes for an elaboration on how these members interact (line 240)
            .blendEnable         = vk::True, // Whether or not this kind of blending is enabled: if enabled, it'll blend the fragment's colour with whatever's already in the framebuffer at that fragment's location; if disabled, the new fragment colour is unmodified (no blending w/ the previous).
            .srcColorBlendFactor = vk::BlendFactor::eSrcAlpha, // Selects the blend factor for the source (new) colour
            .dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha, // Selects the blend factor for the destination (old) colour
            .colorBlendOp        = vk::BlendOp::eAdd,  // how to combine the two colours
            .srcAlphaBlendFactor = vk::BlendFactor::eOne, // Selects the blend factor for the source (new) colour's alpha
            .dstAlphaBlendFactor = vk::BlendFactor::eZero, // Selects the blend factor for the destination (old) colour's alpha
            .alphaBlendOp        = vk::BlendOp::eAdd, // how to combine the two colours' alpha
            .colorWriteMask      = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA // what colour channels are allowed to be written (if we exclude one of these, we won't see/use its respective colour at all)
        };
        // The create info of the colour blending:
        vk::PipelineColorBlendStateCreateInfo colorBlending {
            .logicOpEnable = vk::False, // For the second way to configure colour blending (BITWISE): Whether or not we're using bitwise combinations for our colours -- this WILL set .blendEnable to vk::False (choose only one)
            .logicOp = vk::LogicOp::eCopy, // the bitwise operation to use to combine colours whenever we're using bitwise to combine colours (this only matters if logicOpEnable = vk::True)
            .attachmentCount = 1, // The number of PipelineColorBlendAttachmentStates
            .pAttachments = &colorBlendAttachment // the PipelineColorBlendAttachmentState themselves.
        };


        /*---*/

        // As mentioned, we're making an empty pipeline layout as we're required to -- elaborated upon later.
        // a pipeline layout contains uniform values (essentially global objects shared across) which can alter the behaviour of our shaders without having to recreate them,
        // and push constants, which are a way of passing dynamic values to shaders, so allows uniform values to affect shaders.
        vk::PipelineLayoutCreateInfo pipelineLayoutInfo{ .setLayoutCount = 0, .pushConstantRangeCount = 0 };
        pipelineLayout = vk::raii::PipelineLayout(logicalDevice, pipelineLayoutInfo);

        // States control how data flows/are processed throughout the pipeline stages -- these are the FIXED FUNCTION STAGES of the graphic pipeline.
            // Such as Viewport (the modifiable section -- what to modify in this space), Rasterization mode (how the rasterization stage behaves), Depth, Color Blend, Line Width, and scissor state (what part of the canvas is to be 'cut' outside the specified space -- unmodified)
        // While most of the pipeline states are baked into the pipeline (and thus cannot be changed), a small amount of states can be changed (are dynamic) without having to recreate the entire pipeline.
            // Some examples are the size of the viewport (the window surface in our case), the line width, and blend constants.
        // These are called Dynamic States.
        std::vector<vk::DynamicState> dynamicStates { vk::DynamicState::eViewport, vk::DynamicState::eScissor }; // simply specify what states we want to be dynamic
        // then fill in the dynamic state's create info as with any other _createinfo struct.
        vk::PipelineDynamicStateCreateInfo dynamicState {
            .dynamicStateCount = static_cast<uint32_t>( dynamicStates.size() ), // how many states we want to make dynamic,
            .pDynamicStates = dynamicStates.data()                              // and the data themselves
        }; // Identically to vk::ShaderModuleCreateInfo::pCode, the full contents of dynamicStates is accessed using pointer arithmetic, utilizing the first element of dynamicStates ( .data() )
        // This will result in the specified states' default configuration to be entirely ignored (if not specified yourself, it's undefined behaviour), and you will have to specify what happens with the data at drawing time.
            // This is common for viewport and scissor state as they should be flexible (otherwise it's a far more complex setup whenever baked in) because often they are changed per frame or per draw call.


        vk::PipelineRenderingCreateInfo renderingPipeline_CreateInfo {
            .colorAttachmentCount = 1,  // The number of color formats we're using
            .pColorAttachmentFormats = &swapChain_surfaceFormat.format // the color formats
        };

        vk::GraphicsPipelineCreateInfo graphicsPipeline_CreateInfo {
        .stageCount          = 2,   // The number of programmable stages we're using within this pipeline (we're using vertex and fragment shaders)
        .pStages             = shaderStages, // the programmable stages themselves
        .pVertexInputState   = &vertexInputInfo, // the following members are just the states (fixed functions stages) of the pipeline
        .pInputAssemblyState = &inputAssembly,
        .pViewportState      = &viewportState,
        .pRasterizationState = &rasterizer,
        .pMultisampleState   = &multisampling,
        .pColorBlendState    = &colorBlending,
        .pDynamicState       = &dynamicState,
        .layout              = pipelineLayout, // currently empty, elaborated upon later (.layout receives a handle, not a struct pointer)
        .renderPass          = nullptr // renderPass is nullptr here because we're using DYNAMIC rendering instead of a traditional render pass, which is what this field is.
        }; // There's two more members: .basePipelineHandle and .basePipelineIndex, which are used to create a new graphics pipeline by deriving from an already existing pipeline (idea is it's less expensive and pipelines can have common traits)
            // To use it, set vk::GraphicsPipelineCreateInfo::flags = vk::PipelineCreateFlagBits::eDerivative

        // So far, we have:
        // the Shader Stages: shader modules that are the programmable stages of the graphics pipeline. (such as vertex and fragment shader)
        // the Fixed-Function States: structures that are the fixed-function stages of the graphics pipeline. (such as input assembly, rasterizer, viewport, and color blending)
        // the Pipeline Layout (empty as of right now): containing uniform and push values that impact the shader at draw time.
        // Dynamic Rendering: the color formats of the images which we'll use during rendering.


        // In the tutorial, https://docs.vulkan.org/tutorial/latest/03_Drawing_a_triangle/02_Graphics_pipeline_basics/04_Conclusion.html, pipeline_info isn't written like this, but I think it's easier.
        vk::StructureChain<vk::GraphicsPipelineCreateInfo, vk::PipelineRenderingCreateInfo> pipeline_info { graphicsPipeline_CreateInfo, renderingPipeline_CreateInfo };

        // Second Parameter is an optional vk::raii::PipelineCache object, which can store and reuse relevant pipeline creation data across multiple pipeline constructions -- elaborated later within the 'pipeline cache' chapter.
        // Third parameter, vk::StructureChain sets up pNext between the structs (ctrl f above), all we have to do is point to the first within its chain and Vulkan'll point to the following struct via pNext!
        graphicsPipeline = vk::raii::Pipeline( logicalDevice, nullptr, pipeline_info.get<vk::GraphicsPipelineCreateInfo>() );
    }


    // [[nodiscard]] means the return object of this function (vk::raii::ShaderModule) has to be stored into another object vk::raii::ShaderModule),
    // if we don't use (store) it, the compiler'll give a warning (not an error, will still compile) -- we do this because we could mess up our pipeline w/ unintended behaviour (just making a note so we know).
    // This function takes in the byte-code inside of the buffer we made with readFile_SPIRVShaders(), and creates/returns a vk::raii::ShaderModule object from it.
    [[nodiscard]] vk::raii::ShaderModule createShaderModule(const std::vector<char>& code) const
    {
        // To the required info for a shader module's creation is the size of the code (in bytes)
        vk::ShaderModuleCreateInfo ShaderModule_createInfo {
            .codeSize = code.size() * sizeof(char), // Redundant? sizeof(char) is 1, and code.size is equal to bytes already? I suppose just for... clarity? whatever.
            .pCode = reinterpret_cast<const uint32_t*>( code.data() ) // .pCode takes in uint32, so just cast it, no biggie
                // data() returns a pointer to the first element, through this, .pCode infers the entirety of the code's vector by pointing to the next address through something formally called "Pointer Arithmetic".
                // this address is achieved by adding sizeof(uint32_t)*[ElementNumber] and code.data() (the first element) together as the address of the next element will always follow that continuous pattern (pointer arithmetic).
                // We HAVE to set .codeSize here because otherwise it won't know when to stop (when it's out of range).
                    // the reason we ultimately have to cast to uint32_t is because Vulkan requires it (even though char is cheaper, it's 1 byte instead of u32's 4), so we comply;
                    // otherwise, if we pass code.data() as char, the pointer arithmetic is off because it's adding by 4 when the vector's addresses are seperated by 1.
        };

        // as with all the other gpu related objects/structs, we pass logical device because we're creating it for this GPU
        // and as with a bunch of other objects in Vulkan (common pattern), we first create a _createInfo (holding a bunch of relevant members) object to then pass to the real object.
        vk::raii::ShaderModule shaderModule{ logicalDevice, ShaderModule_createInfo };

        return shaderModule;
    }


    // Loads slang.spv (which is in bytecode format --not human readable-- containing shader data)
    static std::vector<char> readFile_SPIRVShaders(const std::string& filename)
    {
        // Opens the specified file (filename) with 2 flags: read the file in binary mode (ISN'T HUMAN READABLE TEXT) (std::ios::binary) and (|) start reading at the end of the file (std::ios::ate)
        std::ifstream file(filename, std::ios::ate | std::ios::binary);
            // we start from the END of the file because we can use the READ POSITION (which is the end of the text from ::ate) to determine the file's size,
            // and thus allocate a buffer (pre-allocated memory) through the vector right below.

        if ( !file.is_open() ) { // just error checking.
            throw std::runtime_error("failed to open file!");
        }

        // pre-allocates a vector of the file's byte size. (creating a buffer)
        std::vector<char> buffer( file.tellg() ); // .tellg() returns the current position of the read cursor THROUGH bytes (essentially telling us the file size in bytes)
            // initializing a vector with () is used to pre-allocate empty elements within that vector: vector(3) means it has 3 (pre-allocated) elements within.
            // 1 byte == 1 element for a vector; therefore a 100 byte file pre-allocates 100 vector elements.

        // then, we go to the beginning of the file: move the cursor 0 (first parameter) elements beyond the beginning (second parameter) (so right where the file begins)
        file.seekg(0, std::ios::beg);

        // instead of looping, .read() handles it so you can "read it all at once"
        // all this does it set buffer[n] equal to a single byte of memory within that file
        file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
            // after reading, buffer contains 1 byte in each of its elements (where each byte is 8 bits), which, in our context, contains the code in SPIR-V format

        file.close(); // close the file then return it.
        return buffer;
    }


    void createCommandPool()
    {
        vk::CommandPoolCreateInfo commandPoolInfo{
        .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer, // .flags can be set to contain two flags (bitmask | for and):
            // vk::CommandPoolCreateFlagBits::eTransient: specify that command buffers are rerecorded/changed with new commands very often (which may change the memory allocation behaviour), and vk::CommandPoolCreateFlagBits::eResetCommandBuffer, which allows command buffers to be rerecorded/changed individually (without this flag, you'd have to reset their commands all together)
            // We're going to be recording a command buffer every frame, so we want to reset and rerecord over it -- hence eResetCommandBuffer.
        .queueFamilyIndex = queueIndex // Command buffers are actually executed by running them through a device's queue, like the graphics and presentation queues from earlier.
            // Each command pool can only allocate command buffers where the command buffers are run through a single type of queue (pool's queue family must be the same as the buffer's queue family). We're recording commands for drawing, which is why we've chosen the graphics queue family.
        };

        // As with all the other objects, use CommandPool::constructor fed w/ the logical device and pool info itself to construct it.
        commandPool = vk::raii::CommandPool(logicalDevice, commandPoolInfo);
    }

    void createCommandBuffers()
    {
        vk::CommandBufferAllocateInfo commandBuffer_allocationInfo {
            .commandPool = commandPool, // the command pool from where buffers are allocated from (specifies which queues the command buffer can utilize, for our case, our buffers will run through eGraphics and eCompute)
            .level = vk::CommandBufferLevel::ePrimary, // Specifies if the allocated command buffers are primary or secondary command buffers:
                // ePrimary: Submits to a queue for execution, but cannot be called/referenced by other command buffers.
                // eSecondary: Cannot be submitted to a queue directly, but can be called/referenced by primary command buffers. (kinda like a helper function to reuse common operations)
            .commandBufferCount = MAX_FRAMES_IN_FLIGHT // Number of command buffers to allocate (Update: we want to create a command buffer count equal to our max frames in flight because each frame requires it's own command buffer)
        };

        // As with everything, We feed it the logicalDevice to specify we're utilizing this GPU, then pass the actual creation information.
        commandBuffers = vk::raii::CommandBuffers( logicalDevice, commandBuffer_allocationInfo );
    }

    // recordCommandBuffer() will write (record) the commands that we want to contain inside a command buffer.
        // We'll use the commandBuffer that we want to contain the command, and the index of the current swap chain image that we want to write/draw to.
    void recordCommandBuffer( uint32_t swapChain_imageIndex )
    {
        // Since the addition of the vector to commandBuffer, auto &commandBuffer = commandBuffers[frameIndex]; to use commandBuffer directly instead of specifying commandBuffers[frameIndex] each time is handy, but whatever.

        // {} is vk::CommandBufferBeginInfo, it's empty as we don't have ANY members we'd want to use right now, but it contains members .flags/.pInheritanceInfo, which we don't want to use.
            // vk::CommandBufferBeginInfo::pInheritanceInfo is only relevant for secondary command buffers, which specifies what state to inherent from the calling primary command buffer.
        // Here's possible vk::CommandBufferBeginInfo::flags
            // vk::CommandBufferUsageFlagBits::eOneTimeSubmit: command buffer'll be re-recorded (changed) right after executing it once.
            // vk::CommandBufferUsageFlagBits::eRenderPassContinue: a secondary command buffer that's only used within a single render pass
            // vk::CommandBufferUsageFlagBits::eSimultaneousUse: the command buffer can be re-ran while it is already executing.
        commandBuffers[wait_frameIndex].begin({}); // tutorial mistakenly uses operator-> but ok... we'll use operator. This signals the start of the command buffer's commands.
            // vk::raii::CommandBuffer::begin() implicitly resets the already existing commands inside the command buffer, if we've already recorded a command once.


        transition_image_layout (
            swapChain_imageIndex, vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal, // We are now transitioning the swapChain's image (swapChain_imageIndex) to go from eUndefined (no previous layout) -> eColorAttachmentOptimal (where eColorAttachmentOptimal is optimal for rendering)
            {}, vk::AccessFlagBits2::eColorAttachmentWrite, // Bitmasks that define what access rights: our old image (SOURCE) format's access right is empty because it's eUndefined,
            // but our new image (DESTINATION) format has vk::AccessFlagBits2::eColorAttachmentWrite, which grants permissions to specify pixel data into this image (again, specified by swapChain_imageIndex).
            vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::PipelineStageFlagBits2::eColorAttachmentOutput
            // the specified old (src - param one) image's stage must've finished before beginning the new desination's (param two) specified stage (vk::PipelineStageFlagBits2::eColorAttachmentOutput)
                // the old stage is EMPTY because we've not made an image layout yet, so it's a little misleading -- it's considered finished so it's kinda redundant.
        );


        // Then we're setting up the image's color (color attachment)
        vk::ClearValue clearColor = vk::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f); // A clearColor is what the color attachment (the color of the image view, this is pixel-by-pixel) is cleared to (filled fully with) -- 1.0f means opaque black.
        vk::RenderingAttachmentInfo attachmentInfo {
            .imageView   = swapChainImageViews[ swapChain_imageIndex ], // what image view we're rendering to
            .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,    // the image layout the image will be in during rendering.
            .loadOp      = vk::AttachmentLoadOp::eClear,                // specifies what we'll do to our image BEFORE rendering (clearing it, which is filling it in fully)
            .storeOp     = vk::AttachmentStoreOp::eStore,               // what we'll do to the image AFTER rendering (we're just storing it for later use)
            .clearValue  = clearColor                                   // the color used for the eClear operation (the screen will first be rendered fully opaque black)
        };


        vk::RenderingInfo renderingInfo {
            .renderArea = {                                 // this is what we actually render to inside of the specific image. (defines the size of the rendering rectangle/area)
                .offset = { 0, 0 },                         // the first point of the rectangle (0,0 being upper top left)
                .extent = swapChain_Extent_ImageResolution  // we're passing the image resolution here to specify the .extent (how far down and right it goes) -- we're covering the entirety of the swap chain's canvas
            },
            .layerCount           = 1,                  // the number of layers within this image view we're rendering to (we only have one layer)
            .colorAttachmentCount = 1,                  // the attachment count
            .pColorAttachments    = &attachmentInfo     // the attachment data itself
        };

        commandBuffers[wait_frameIndex].beginRendering( renderingInfo ); // actually begin rendering here

        // bind the graphics pipeline (what pipeline we're using)
        // first parameter specifies if the pipeline object is a Graphics or Compute pipeline: we're drawing a triangle, so we're making a graphic.
        commandBuffers[wait_frameIndex].bindPipeline( vk::PipelineBindPoint::eGraphics, *graphicsPipeline );

        // Since we specified the viewport and scissor states of the pipeline to be dynamic, we now have to set them within the command buffer before issuing the draw command.
        commandBuffers[wait_frameIndex].setViewport( 0, // We're using the first viewport (0 = first index) ( > 0 is for different screen stuff, confusing)
            vk::Viewport(
                0.0f, 0.0f, // x and y (offset)
                static_cast<float>( swapChain_Extent_ImageResolution.width ), static_cast<float>( swapChain_Extent_ImageResolution.height ), // width and height
                0.0f, 1.0f // min and max depth
            )
        );

        commandBuffers[wait_frameIndex].setScissor( 0, vk::Rect2D( vk::Offset2D( 0, 0 ), swapChain_Extent_ImageResolution ) ); // same thing as the above, just formatted a little differently, but it's a rectangle of size 0,0 -> width, height

        // this tells the command buffer where to read vertex data from (second parameter)
        commandBuffers[wait_frameIndex].bindVertexBuffers( 0, *vertexBuffer, {0} );
            // vertexBuffer experiences these series of events:
                // vertexBuffer.bindMemory( *vertexBufferMemory, 0 ); -> void* data = vertexBufferMemory.mapMemory( 0, bufferInfo.size ); -> memcpy( data, vertices.data(), bufferInfo.size );
                // We copy the vertices.data() into void* data pointer which is pointing to vertexBufferMemory (giving vertexBufferMemory the vertices), and due to us binding vertexBufferMemory into vertexBuffer (meaning vertexBuffer is referencing vertexBufferMemory, which contains actual data)
                // we can use *vertexBuffer as our second parameter w/ bindVertexBuffers to read the original vertex data -- we don't pass the memory itself (we pass the buffer) because it's how bindVertexBuffer is set up to read it (the buffer is referencing the memory anyway, so it's no problem)

        commandBuffers[wait_frameIndex].draw( static_cast<uint32_t>( vertices.size() ), 1, 0, 0 ); // actually draw the thing.
            // First parameter: vertexCount - how many vertices we're drawing (we have a vertex buffer, so we're just seeing how many vertices are in our vertices container)
            // Second: instanceCount - Instanced rendering (use 1 if we're not doing that)
            // Third: firstVertex - Used as an offset into the vertex buffer, defines the lowest value of SV_VertexId -- if it's above 0, we're skipping some vertices to not be shaded.
            // Fourth: firstInstance - Used as an offset for instanced rendering, defines the lowest value of SV_InstanceID -- if it's above 0, we're skipping some instances to not be shaded (we have only one instance, so...)

        commandBuffers[wait_frameIndex].endRendering(); // end the rendering here.

        // After rendering, transition the swapchain image to vk::ImageLayout::ePresentSrcKHR
        transition_image_layout(
            swapChain_imageIndex,                                   // again, what image we're referring to.
            vk::ImageLayout::eColorAttachmentOptimal,               // transition this image layout from eColorAttachmentOptimal...
            vk::ImageLayout::ePresentSrcKHR,                        // to ePresentSrcKHR for presenting images to the screen.
            vk::AccessFlagBits2::eColorAttachmentWrite,             // the previous access rights
            {},                                                     // The destination (ePresentSrcKHR) doesn't need any access rights.
            vk::PipelineStageFlagBits2::eColorAttachmentOutput,     // the source's stage (finish this stage before transitioning to eBottomOfPipe)
            vk::PipelineStageFlagBits2::eBottomOfPipe               // the desination's stage: the bottom of the pipe means the graphic pipeline has FULLY finished (NO MORE PIPELINE WORK).
        );

        commandBuffers[wait_frameIndex].end(); // we've finished recording the command buffer: signal its end.
    }

    // This function is used to transition the image layout before and after rendering
    // Different image layouts are optimized for different operations. For example, an image's layout can be optimal for presenting to the screen, or an optimal layout being used as a colour attachment (dictates presented colour)
    // Before we start rendering an image, we need to transition its image layout to one that is suitable for rendering: vk::ImageLayout::eColorAttachmentOptimal.
        // The old tutorial skips this, and the new one doesn't elaborate upon this thing https://vulkan-tutorial.com/Drawing_a_triangle/Drawing/Command_buffers
        // However it's pre 1.3 and makes a separate render pass, meaning it doesn't render DIRECTLY through the image view, instead it'll render through a seperate render pass.
            // see big_notes.
        // Not to mention it isn't included in this section's code at the bottom (though, in the next one, yes) -- just awful.
    // this is ONLY necessary in this context for dynamic rendering.
    void transition_image_layout( uint32_t imageIndex, vk::ImageLayout old_layout, vk::ImageLayout new_layout, vk::AccessFlags2 src_access_mask,
	    vk::AccessFlags2 dst_access_mask, vk::PipelineStageFlags2 src_stage_mask, vk::PipelineStageFlags2 dst_stage_mask )
    {
        // Specified a bit of the fields within the call to this in recordCommandBuffer()
        vk::ImageMemoryBarrier2 barrier = {
		    .srcStageMask        = src_stage_mask,
		    .srcAccessMask       = src_access_mask,
		    .dstStageMask        = dst_stage_mask,
		    .dstAccessMask       = dst_access_mask,
		    .oldLayout           = old_layout,
		    .newLayout           = new_layout,
		    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		    .image               = swapChainImages[imageIndex],
		    .subresourceRange    = {
		        .aspectMask     = vk::ImageAspectFlagBits::eColor,
		        .baseMipLevel   = 0,
		        .levelCount     = 1,
		        .baseArrayLayer = 0,
		        .layerCount     = 1
            }
        };

        vk::DependencyInfo dependency_info = {
		    .dependencyFlags         = {},
		    .imageMemoryBarrierCount = 1,
		    .pImageMemoryBarriers    = &barrier
        };

        commandBuffers[wait_frameIndex].pipelineBarrier2( dependency_info );
    }

    void createVertexBuffer()
    {
        // You should know this by now, but I'm just regurgitating it.
        // A buffer is a chunk of memory, which starts off empty, but gets populated by some arbitrary data. We're making a buffer here to store vertex data, and the reason we're allocating buffer memory ourselves here is because it isn't automatic.
            // We are responsible for memory management, including buffers.

        vk::BufferCreateInfo bufferInfo {
            .size        = sizeof( vertices[0] ) * vertices.size(), // .size sets the buffer's reserved memory in bytes: gets the size of a single vertex in bytes, and multiply it by the amount of vertices in the container.
            .usage       = vk::BufferUsageFlagBits::eVertexBuffer,  // .usage specifies which purpose the data inside of the buffer will be used for -- you can specify multiple purposes with bitwise OR.
            .sharingMode = vk::SharingMode::eExclusive              // similarily with images inside of the swap chain, buffers can be owned by an exclusive queue family, or concurrently be owned by multiple queue families. We're only using the eGraphics queue, so it's exclusive.
        };      // Note: we are NOT assigning the queue family here, we're just saying it'll be exclusive/owned by a single queue.
            // There's also a .flags member, which is used to configure sparse buffer memory. We're not needing it right now, so we're leaving it as be, defaulted to 0.

        // and similiarily with a bunch of other constructors for Vulkan objects, give the device and the information we just specified above (buffer info) to actually create a vertex buffer.
        vertexBuffer = vk::raii::Buffer( logicalDevice, bufferInfo );
            // an IMPORTANT notice: for GPU buffers, you NEED to explicitly allocate memory towards it, unlike with CPU's commandPool + commandBuffers -- it is automatic after object creation.
            // right now, all we have is a handle to a vertexBuffer which information of what is supposed to be inside of it: we need to actually allocate the memory inside the GPU.

        // The first step to ACTUALLY allocate memory inside the GPU is to get the memory requirements of the buffer. (REMEMBER: THIS IS ONLY NEEDED FOR GPU BUFFERS)
        // struct vk::MemoryRequirements contains three member variables:
            // .size: the size of the required memory in bytes (it MAY differ from bufferInfo.size)
            // .alignment: the offset in bytes from where the buffer actually begins in the allocated region of memory (depends on bufferInfo's .usage and .flags)
                //        this does NOT mean it shrinks the buffer memory size to "pad", it just shifts the address by X bytes to then begin the buffer itself at that shifted address
            // .memoryTypeBits: the bit field of memory types that are suitable for the buffer -- this is NOT specified by .usage and isn't manually set, vulkan sets this automatically
                // graphics card themselves offer different types of memory to allocate from, and each type of memory varies in terms of allowed operations and performance characteristics, and it's up to us to find the right memory type.
        vk::MemoryRequirements memoryRequirements = vertexBuffer.getMemoryRequirements();
        // Then properly fill out the Info struct with whatever requirements.
        vk::MemoryAllocateInfo memoryAllocateInfo {
            .allocationSize  = memoryRequirements.size, // the byte size to actually allocate for our buffer
            .memoryTypeIndex = findGPUBufferMemoryType( memoryRequirements.memoryTypeBits, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent )
                // for the first parameter of findGPUBufferMemoryType(), we're just passing the basic buffer memory requirements (NOT including the special properties we want, which is param2) to find the barebones support for this buffer.
                // the second param is the special properties we require of the memory type -- we want to map the memory type to the CPU for access to our vertices, hence vk::MemoryPropertyFlagBits::eHostVisible
        };

        // Then the second step is to construct vk::raii::DeviceMemory to actually allocate the memory within the GPU!
        // at this point, we've allocated memory onto the GPU, but the vertexBuffer object isn’t using it yet, hence vk::raii::Buffer::bindBufferMemory
        vertexBufferMemory = vk::raii::DeviceMemory( logicalDevice, memoryAllocateInfo );
        // first parameter specifies the memory handle to 'bind' the buffer to -- where this buffer's data exists.
        // second parameter is the offset within this region of memory (just keep it zero) -- if the offset is non-zero, then it is required to be divisible by memRequirements.alignment
        vertexBuffer.bindMemory( *vertexBufferMemory, 0 ); // Without this, vertexBuffer is NOT referencing ANY memory, and thus won't be useful.

        // a pointer to our vertex buffer's memory (left of .mapMemory) -- we need to do this to actually grab ahold of the vertex data, otherwise it's inaccessible.
        // first parameter is the offset (here, zero -- we want the whole thing), and the second parameter is the byte size of the region (within vertexBufferMemory) we want to grab ahold of (here, size of the entire butter -- we want the whole thing)
        // think of it as a rectangular window: we start at the first point (offset 0, first param) of the rectangle, and cover the entire area of the rectangle (.size of the buffer, second param)
        void* data = vertexBufferMemory.mapMemory( 0, bufferInfo.size ); // this begins the access of vertexBufferMemory within the CPU
        // memcpy copies a block of memory from source (2nd param) to destination (1st param)
            // the exact number of bytes to copy is specified by the third param (so we're grabbing the entirety of the buffer -- there is no byte offset, copy from the start of the buffer)
        memcpy( data, vertices.data(), bufferInfo.size );
            // so all this does is store the vertices.data() inside vertexBufferMemory through the void* data pointer.
        vertexBufferMemory.unmapMemory(); // this ends the access of vertexBufferMemory within the CPU
            // an analogy for mapMemory and unmapMemory (aside from other similar functions like beginRendering + endRendering) is like a treasure chest.
            // once we mapMemory, the treasure chest is open and we can access the contents of the chest within CPU memory.
            // once we unmapMemory, the treasure chest is sealed (but the contents itself are still within the chest, we just can't open the chest), preventing us from accessing the contents of the chest within CPU memory.
                // this also means that the void* data pointer is now invalid and pointing to garbage -- DO NOT use data after this point


    }

    // finds the right type of memory to use whenever allocating memory into GPU buffers.
        // typeFilter is a bitmask of memory types that are suitable/requested.

    uint32_t findGPUBufferMemoryType( uint32_t typeFilter, vk::MemoryPropertyFlags properties )
    {
        // query this GPU for its available types of memory to choose from whenever allocating GPU buffers
        vk::PhysicalDeviceMemoryProperties memProperties = physicalDevice.getMemoryProperties();
        // struct vk::PhysicalDeviceMemoryProperties contains two array member variables:
            // .memoryTypes: the GPU's supported memory types
            // .memoryHeaps: distinct memory resources, such as dedicated VRAM -- if the dedicated VRAM runs out, we can manually swap memory/space with standard RAM. The different types of memory exist within these heaps.
                // just heads up: heaps matters for performance, but they don't elaborate upon it -- study it yourself later!

        for ( uint32_t i = 0; i < memProperties.memoryTypeCount; i++ )
        {
            // operator<< shifts the bits by the right-hand value (i), starting from the left-hand element in the bit (however, the index 0 is at the right of the byte, 7 is at the left)
            // the proper terminology is from most significant bit (instead of the left-most) to least significant (instead of the right-most bit), and the indexing of a bit starts from the least important to the most important.
            // therefore, a bit index of 0 is the least significant bit (the right-most)
            // it's a little weird, but for creating bitmasks to check them against another bitmask, we start with 1 instead of 0 (whenever using <<).
            // typeFilter & ( 1 << i ) creates a temporary bitmask for comparison: it only sets the i-th least significant of typeFilter, leaving the rest to zero.
            // if ( typeFilter & ( 1 << i ) ) is non-zero, it means the bit flag at index i is a suitable bit flag (the rest of the flags are zero due to << usage), so return the index (which in turn tells us the memory type to use).
                // this entire ordeal just tells us IF the memory type is suitable to begin with for the buffer we want (in our case, a vertex buffer)

            // the second part specifies the desired properties bits ASIDE from just looking for a suitable canditate that'll simply work.
            // memoryTypes[] consists of individual vk::MemoryType structs, structs which specify the heap and properties of each memory type.
            // the "properties of each memory type" define the special features of the memory type, such as being able to map it (AKA be able to point to it with a pointer) so we can access it within the CPU
                // the flag bit that allows mapping the memory type is vk::MemoryPropertyFlagBits::eHostVisible
                // we also need another flag bit, vk::MemoryPropertyFlagBits::eHostCoherent -- EXPLAINED LATER WHEN WE MAP THE MEMORY.

            // so in turn, the entire if statement reads like so:
            // if this memory type is suitable (left-most of &&) for this buffer, AND (&&) supports these special, required features (right-most of &&), return its bit index, i, to use it as our buffer's allocated memory type.
                // the reason we == properties instead of > 0 is because we might have more than one required memory type properties, so we need the support of ALL properties instead of just one
                    // param properties is a bitmask of ALL zero (00000000), EXCEPT when assigning desired flag bits, it changes that bit flag (within the currently empty bitmask) to be 1, indicating that desired flag is present.
            if ( ( typeFilter & ( 1 << i ) ) && ( ( memProperties.memoryTypes[i].propertyFlags & properties ) == properties ) )
                return i;
        }
        throw std::runtime_error("failed to find suitable memory type!");
    }

    void createSyncObjects()
    {
        // We're passing an empty vk::SemaphoreCreateInfo struct because we don't have any relevant fields to specify.
            // Future versions (FUTURE.. JUST IN PLANNING) of Vulkan may add .flags (like w/ fences) and .pNext for vk::SemaphoreCreateInfo, similarily to other structures.

        // For every frame in flight, we need a semaphore to track/signal the presentation of it.
        for ( size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++ )
        {
            presentCompleteSemaphore.emplace_back( logicalDevice, vk::SemaphoreCreateInfo() ); // semaphore is unsignaled on creation because, unlike fence, we don't need it to be signaled (gpu handles this garbage)
            drawFence.emplace_back( logicalDevice, vk::FenceCreateInfo( { .flags = vk::FenceCreateFlagBits::eSignaled } ) ); // .FLAGS IS KINDA NEEDED HERE BECAUSE OTHERWISE THE FENCE IS UNSIGNALED -- THIS ENSURES FOR THE FIRST TIME WE ENTER DRAWFRAME(), IT'S SIGNALED (SO IT DOESNT WAIT ON THE TIMELIMIT EXPIRY)
        }

        // We make a semaphore for every image within the swap chain to signal whether that image has finished rendering.
            // "Whenever renderFinished becomes signaled, send the rendered image to the GPU for presentation, and presentation waits on a separate presentComplete semaphore to actually display"
            // All this renderFinishedSemaphore does is track if its respective image has finished rendering, hence why we make renderFinishedSemaphore the size of swapChainImages.size() and not MAX_FRAMES_IN_FLIGHT,
            // because this semaphore is solely focusing on whether or not the images within the swap chain are rendered -- NOTHING relating to presenting per say.
        for ( size_t i = 0; i < swapChainImages.size(); i++ ) {
            renderFinishedSemaphore.emplace_back( vk::raii::Semaphore( logicalDevice, vk::SemaphoreCreateInfo() ) ); // the explicit type is redundant since we're emplacing, just showing.
        }
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
        outputFile << get_current_time() << " | Starting" << std::endl;
		HelloTriangleApplication app;
		app.run();
	}
	catch (const std::exception &e)
	{
		std::cerr << e.what() << std::endl;
		return EXIT_FAILURE;
	}

    outputFile << get_current_time() << " | Exiting" << std::endl;
    std::cout << "\tSuccessfully Ended!\n";
	return EXIT_SUCCESS;
}


// Probably a good idea to each sub-category into separate files because it's KINDA much. (device creation in one, pipeline in another, swap chain etc)