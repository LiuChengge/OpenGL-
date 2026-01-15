cmake_minimum_required(VERSION 3.16)

project(endo_viewer_v4l)

set(CMAKE_CXX_STANDARD 11)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Find OpenCV
find_package(OpenCV REQUIRED)
if(${OpenCV_FOUND})
    message(STATUS "OpenCV library status:")
    message(STATUS "  version: ${OpenCV_VERSION}")
    message(STATUS "  libraries: ${OpenCV_LIBS}")
    message(STATUS "  include path: ${OpenCV_INCLUDE_DIRS}")
else(${OpenCV_FOUND})
    message(WARNING "Cannot found OpenCV.")
endif(${OpenCV_FOUND})

# Find OpenGL
find_package(OpenGL REQUIRED)
if(${OPENGL_FOUND})
    message(STATUS "OpenGL library status:")
    message(STATUS "  libraries: ${OPENGL_LIBRARIES}")
    message(STATUS "  include path: ${OPENGL_INCLUDE_DIR}")
else(${OPENGL_FOUND})
    message(WARNING "Cannot found OpenGL.")
endif(${OPENGL_FOUND})

# Build target
file(GLOB_RECURSE SRC_CPP ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)
file(GLOB_RECURSE SRC_C ${CMAKE_CURRENT_SOURCE_DIR}/src/*.c)
add_executable(${PROJECT_NAME}
    main.cpp
    ${SRC_CPP}
    ${SRC_C}
)
target_include_directories(${PROJECT_NAME}
    PUBLIC
        $<BUILD_INTERFACE:${OpenCV_INCLUDE_DIRS}>
        $<BUILD_INTERFACE:${OPENGL_INCLUDE_DIR}>
    PRIVATE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/>
        $<BUILD_INTERFACE:/opt/libjpeg-turbo/include/>

)
target_link_libraries(${PROJECT_NAME}
    ${OpenCV_LIBS}
    ${OPENGL_LIBRARIES}
    turbojpeg
    glfw
    dl
    pthread
    X11
    Xrandr
    Xi
    Xxf86vm
    Xcursor
)
