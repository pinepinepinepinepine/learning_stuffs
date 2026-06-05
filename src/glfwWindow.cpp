#include "../headers/glfwWindow.hpp"
#include "../headers/externState.hpp"

bool held_w = false;
bool held_a = false;
bool held_s = false;
bool held_d = false;
bool held_space = false;
bool held_ctrl = false;

bool held_q = false;
bool held_e = false;

bool held_x = false;

bool toggle_r = false;
bool toggle_t = false;

bool toggle_c = false;

void glfw_keyCallback( GLFWwindow *window, int key, int scancode, int action, int mods )
{
    if ( action == GLFW_REPEAT )
        return;

    if ( action == GLFW_PRESS )
    {
        switch ( key )
        {
            case ( GLFW_KEY_W ):
            {
                held_w = true;
                break;
            }
            case ( GLFW_KEY_S ):
            {
                held_s = true;
                break;
            }
            case ( GLFW_KEY_A ):
            {
                held_a = true;
                break;
            }
            case ( GLFW_KEY_D ):
            {
                held_d = true;
                break;
            }
            case ( GLFW_KEY_SPACE ):
            {
                held_space = true;
                break;
            }
            case ( GLFW_KEY_LEFT_CONTROL ):
            {
                held_ctrl = true;
                break;
            }
            case ( GLFW_KEY_Q ):
            {
                held_q = true;
                break;
            }
            case ( GLFW_KEY_E ):
            {
                held_e = true;
                break;
            }
            case ( GLFW_KEY_X ):
            {
                held_x = true;
                break;
            }
            case ( GLFW_KEY_R ):
            {
                if ( toggle_r )
                    toggle_r = false;
                else
                    toggle_r = true;
                break;
            }
            case ( GLFW_KEY_T ):
            {
                if ( toggle_t )
                    toggle_t = false;
                else
                    toggle_t = true;
                break;
            }
            case ( GLFW_KEY_C ):
            {
                if ( toggle_c )
                    toggle_c = false;
                else
                    toggle_c = true;
                break;
            }
            default:
                break;
        }
    }
    else
    {
        switch ( key )
        {
            case ( GLFW_KEY_W ):
            {
                held_w = false;
                break;
            }
            case ( GLFW_KEY_S ):
            {
                held_s = false;
                break;
            }
            case ( GLFW_KEY_A ):
            {
                held_a = false;
                break;
            }
            case ( GLFW_KEY_D ):
            {
                held_d = false;
                break;
            }
            case ( GLFW_KEY_SPACE ):
            {
                held_space = false;
                break;
            }
            case ( GLFW_KEY_LEFT_CONTROL ):
            {
                held_ctrl = false;
                break;
            }
            case ( GLFW_KEY_Q ):
            {
                held_q = false;
                break;
            }
            case ( GLFW_KEY_E ):
            {
                held_e = false;
                break;
            }
            case ( GLFW_KEY_X ):
            {
                held_x = false;
                break;
            }
            default:
                break;
        }
    }
}

bool rightClickLock = false;

glm::vec2 current_cursor_position(0,0);
glm::vec2 right_click_lock_at(0,0);
glm::vec2 cursor_clicked_at;
bool held_click = false;
bool moving_cursor = false;

void glfw_cursorCallback( GLFWwindow *window, double xpos, double ypos )
{
    current_cursor_position = glm::vec2(xpos, ypos);
    moving_cursor = true;
}

void glfw_clickCallback( GLFWwindow *window, int button, int action, int mods )
{
    if ( button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS )
    {
        held_click = true;
        cursor_clicked_at = glm::vec2( current_cursor_position.x, current_cursor_position.y );
    }
    if ( button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE )
    {
        held_click = false;
    }

    if ( button == GLFW_MOUSE_BUTTON_RIGHT )
    {
        if ( action == GLFW_PRESS )
        {
            rightClickLock = true;
            right_click_lock_at = current_cursor_position;
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        }
        else if ( action == GLFW_RELEASE )
        {
            rightClickLock = false;
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        }
    }


}


void Window::initWindow()
{
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    window = glfwCreateWindow( WIDTH, HEIGHT, "vulk stuff", nullptr, nullptr );

    glfwSetWindowUserPointer( window, this );

    glfwSetFramebufferSizeCallback( window, framebufferResizeCallback );

    glfwSetKeyCallback( window, glfw_keyCallback );
    glfwSetCursorPosCallback( window, glfw_cursorCallback ); // SOLELY the position. this does NOT include mouse button inputs.
    glfwSetMouseButtonCallback( window, glfw_clickCallback );
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