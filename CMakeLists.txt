cmake_minimum_required(VERSION 3.20)
cmake_policy(SET CMP0135 NEW)
set(CMAKE_OSX_DEPLOYMENT_TARGET 11.0 CACHE STRING "")
project(QuadMapper LANGUAGES C CXX)
if(APPLE)
  enable_language(OBJC OBJCXX)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

include(FetchContent)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(glfw URL https://github.com/glfw/glfw/archive/refs/tags/3.4.tar.gz)
FetchContent_Declare(imgui URL https://github.com/ocornut/imgui/archive/refs/tags/v1.92.3.tar.gz)
FetchContent_MakeAvailable(glfw imgui)

add_library(imgui STATIC
  ${imgui_SOURCE_DIR}/imgui.cpp
  ${imgui_SOURCE_DIR}/imgui_draw.cpp
  ${imgui_SOURCE_DIR}/imgui_tables.cpp
  ${imgui_SOURCE_DIR}/imgui_widgets.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp)
target_include_directories(imgui PUBLIC ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)
target_link_libraries(imgui PUBLIC glfw)

add_library(glad STATIC third_party/glad/src/gl.c)
target_include_directories(glad PUBLIC third_party/glad/include)

# OS / GL independent
add_library(qm_core STATIC src/core/Homography.cpp src/core/Settings.cpp)
target_include_directories(qm_core PUBLIC src third_party/json)

add_executable(QuadMapper MACOSX_BUNDLE WIN32 src/main.cpp src/render/Renderer.cpp src/render/Image.cpp)
target_include_directories(QuadMapper PRIVATE third_party/stb)
target_link_libraries(QuadMapper PRIVATE qm_core glad imgui glfw)

# Test pattern images: every build mirrors patterns/ into the app, so deleted files disappear too.
if(APPLE)
  set(PATTERN_DIR "$<TARGET_BUNDLE_CONTENT_DIR:QuadMapper>/Resources/patterns")
else()
  set(PATTERN_DIR "$<TARGET_FILE_DIR:QuadMapper>/patterns")
endif()
add_custom_target(copy_patterns ALL
  COMMAND ${CMAKE_COMMAND} -E rm -rf ${PATTERN_DIR}
  COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/patterns ${PATTERN_DIR})
add_dependencies(copy_patterns QuadMapper)

if(APPLE)
  # Syphon is compiled in statically (OpenGL client only, no Metal).
  FetchContent_Declare(syphon
    GIT_REPOSITORY https://github.com/Syphon/Syphon-Framework.git
    GIT_TAG f4761677a45b8034a3c2069ec0f3d2553da81fba)
  FetchContent_MakeAvailable(syphon)
  file(GLOB SYPHON_SRC ${syphon_SOURCE_DIR}/*.m ${syphon_SOURCE_DIR}/*.c)
  list(FILTER SYPHON_SRC EXCLUDE REGEX "Metal")
  # Syphon headers include each other as <Syphon/...>
  file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/syphon_include)
  file(CREATE_LINK ${syphon_SOURCE_DIR} ${CMAKE_BINARY_DIR}/syphon_include/Syphon SYMBOLIC)
  add_library(syphon STATIC ${SYPHON_SRC})
  target_compile_options(syphon PRIVATE -fobjc-arc -w -include ${syphon_SOURCE_DIR}/Syphon_Prefix.pch)
  target_compile_definitions(syphon PRIVATE SYPHON_CORE_SHARE)
  target_include_directories(syphon PUBLIC ${CMAKE_BINARY_DIR}/syphon_include PRIVATE ${syphon_SOURCE_DIR})
  target_link_libraries(syphon PUBLIC "-framework Cocoa" "-framework OpenGL" "-framework IOSurface")

  target_sources(QuadMapper PRIVATE src/platform/mac/PlatformMac.mm)
  target_compile_options(QuadMapper PRIVATE $<$<COMPILE_LANGUAGE:OBJCXX>:-fobjc-arc>)
  target_link_libraries(QuadMapper PRIVATE syphon)
  set_target_properties(QuadMapper PROPERTIES
    MACOSX_BUNDLE_BUNDLE_NAME QuadMapper
    MACOSX_BUNDLE_GUI_IDENTIFIER com.quadmapper.app)

  # Receive test: publishes an animated timecode pattern (not distributed).
  add_executable(SyphonTestSender tools/SyphonTestSender.mm)
  target_compile_options(SyphonTestSender PRIVATE -fobjc-arc)
  target_link_libraries(SyphonTestSender PRIVATE syphon glfw)
elseif(WIN32)
  set(SPOUT_BUILD_LIBRARY OFF CACHE BOOL "" FORCE)
  set(SKIP_INSTALL_ALL ON CACHE BOOL "" FORCE)
  FetchContent_Declare(spout URL https://github.com/leadedge/Spout2/archive/refs/tags/2.007.017.tar.gz)
  FetchContent_MakeAvailable(spout)
  target_sources(QuadMapper PRIVATE src/platform/win/PlatformWin.cpp)
  target_link_libraries(QuadMapper PRIVATE Spout_static)
  if(MSVC)
    target_link_options(QuadMapper PRIVATE /ENTRY:mainCRTStartup)
  endif()
endif()

enable_testing()
add_executable(test_homography tests/test_homography.cpp)
target_link_libraries(test_homography PRIVATE qm_core)
add_test(NAME homography COMMAND test_homography)
