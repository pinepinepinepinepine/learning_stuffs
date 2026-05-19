#pragma once

#include "includes.hpp"

constexpr uint32_t WIDTH = 800;
constexpr uint32_t HEIGHT = 600;

struct Window
{
    GLFWwindow* window = nullptr;
    VkSurfaceKHR window_surface = nullptr;
    bool framebufferResized = false;

    void initWindow();
    static void framebufferResizeCallback(GLFWwindow* window, int width, int height);
    void createWindowSurface( vk::raii::Instance& instance );
};