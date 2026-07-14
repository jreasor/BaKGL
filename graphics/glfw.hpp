#pragma once

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <string_view>

#include <memory>

namespace Graphics {

struct DestroyGlfwWindow
{
    void operator()(GLFWwindow* window)
    {
        glfwDestroyWindow(window);
        glfwTerminate();
    }
};

std::unique_ptr<GLFWwindow, DestroyGlfwWindow> MakeGlfwWindow(
    unsigned height,
    unsigned width,
    std::string_view title,
    // ===== BAK_AGENT (removable automation harness) =====
    // true => GLFW_DECORATED=false (borderless window). Default keeps every
    // existing caller unchanged; main3d passes config.mGraphics.mBorderlessWindow.
    bool borderless = false);
    // ===== END BAK_AGENT =====

}
