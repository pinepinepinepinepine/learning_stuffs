#include "../headers/glfwWindow.hpp"

void Window::initWindow()
{
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    window = glfwCreateWindow( WIDTH, HEIGHT, "vulk stuff", nullptr, nullptr );

    glfwSetWindowUserPointer( window, this );

    glfwSetFramebufferSizeCallback( window, framebufferResizeCallback );
}

void Window::framebufferResizeCallback(GLFWwindow* window, int width, int height)
{
    auto app = reinterpret_cast<Window*>( glfwGetWindowUserPointer( window ) );
    app->framebufferResized = true;
}

void Window::createWindowSurface( vk::raii::Instance& instance )
{
    VkSurfaceKHR _surface;

    if ( glfwCreateWindowSurface(*instance, window, nullptr, &_surface) != 0 )
        throw std::runtime_error("failed to create window surface!");

    // GLFW doesn't offer a special function for destroying the window surface, but wrapping it (encapsulating/using) with a vk::rai::SurfaceKHR object (window_surface) will let Vulkan automatically delete it with RAII when out of scope.
    window_surface = VkSurfaceKHR( _surface );
}