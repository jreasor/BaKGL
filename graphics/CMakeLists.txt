enable_testing()

add_library(graphics
    IGuiElement.hpp IGuiElement.cpp
    cube.hpp cube.cpp
    framebuffer.hpp framebuffer.cpp
    glfw.hpp glfw.cpp
    glm.hpp
    graphicsConfig.hpp graphicsConfig.cpp
    guiRenderer.hpp guiRenderer.cpp
    guiTypes.hpp guiTypes.cpp
    inputHandler.hpp inputHandler.cpp
    line.hpp line.cpp
    meshObject.hpp meshObject.cpp
    opengl.hpp opengl.cpp
    quad.hpp quad.cpp
    renderData.hpp renderData.cpp
    shaderProgram.hpp shaderProgram.cpp
    sphere.hpp sphere.cpp
    sprites.hpp sprites.cpp
    texture.hpp texture.cpp
    textureUpload.hpp textureUpload.cpp
    types.hpp
    vramTracker.hpp vramTracker.cpp
)

target_link_libraries(
    graphics
    com
    shaders
    ${LINK_3D_LIBRARIES})
