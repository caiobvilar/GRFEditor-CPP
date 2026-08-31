cmake_minimum_required(VERSION 3.25)
project(grfeditor VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

find_package(SDL3 REQUIRED CONFIG)
find_package(glad REQUIRED CONFIG)
find_package(imgui REQUIRED CONFIG)
find_package(ZLIB REQUIRED)
find_package(GTest REQUIRED)

# ---------------------------------------------------------------------------
# ImGui backends
# ---------------------------------------------------------------------------
set(IMGUI_BACKEND_DIR ${CMAKE_BINARY_DIR}/imgui_backends)

add_library(grfeditor_imgui_backends STATIC
    ${IMGUI_BACKEND_DIR}/imgui_impl_sdl3.cpp
    ${IMGUI_BACKEND_DIR}/imgui_impl_opengl3.cpp
)
target_include_directories(grfeditor_imgui_backends PUBLIC
    ${IMGUI_BACKEND_DIR}
)
target_compile_definitions(grfeditor_imgui_backends PUBLIC
    IMGUI_IMPL_OPENGL_LOADER_GLAD2
)
target_link_libraries(grfeditor_imgui_backends PUBLIC
    glad::glad
    SDL3::SDL3
    imgui::imgui
)

add_executable(grfeditor
    src/main.cpp
    src/app/Application.cpp
    src/app/Dockspace.cpp
    src/app/ArchivePanel.cpp
)
target_include_directories(grfeditor PRIVATE src)
target_link_libraries(grfeditor PRIVATE grfeditor_imgui_backends grfcore)

# ---------------------------------------------------------------------------
# grfcore: the GRF container engine (M1)
# ---------------------------------------------------------------------------
add_library(grfcore STATIC
    src/grfcore/GrfUtil.cpp
    src/grfcore/GrfHeader.cpp
    src/grfcore/DesCipher.cpp
    src/grfcore/Compression.cpp
    src/grfcore/FileEntry.cpp
    src/grfcore/FileTable.cpp
    src/grfcore/GrfWriter.cpp
    src/grfcore/Container.cpp
    src/grfcore/Thor.cpp
    src/grfcore/Rgz.cpp
    src/grfcore/GrfContainerProvider.cpp
)
target_include_directories(grfcore PUBLIC src/grfcore)
target_link_libraries(grfcore PUBLIC ZLIB::ZLIB)

# ---------------------------------------------------------------------------
# grfcl: command-line GRF tool
# ---------------------------------------------------------------------------
add_executable(grfcl
    src/grfcl/main.cpp
)
target_include_directories(grfcl PRIVATE src/grfcore)
target_link_libraries(grfcl PRIVATE grfcore)

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
add_executable(grfcore_tests
    tests/core_tests.cpp
)
target_include_directories(grfcore_tests PRIVATE src/grfcore)
target_link_libraries(grfcore_tests PRIVATE grfcore GTest::gtest_main)
enable_testing()
include(GoogleTest)
gtest_discover_tests(grfcore_tests)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(grfeditor PRIVATE -Wall -Wextra)
    target_compile_options(grfeditor_imgui_backends PRIVATE -w)
    target_compile_options(grfcore PRIVATE -Wall -Wextra)
    target_compile_options(grfcl PRIVATE -Wall -Wextra)
    target_compile_options(grfcore_tests PRIVATE -Wall -Wextra)
endif()

# ---------------------------------------------------------------------------
# Install
# ---------------------------------------------------------------------------
include(GNUInstallDirs)

install(TARGETS grfeditor grfcl
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(FILES packaging/grfeditor.desktop
    DESTINATION ${CMAKE_INSTALL_DATADIR}/applications
)
install(FILES resources/grfeditor-256.png
    DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/256x256/apps
)

# ---------------------------------------------------------------------------
# Packaging (CPack: .deb / .rpm)
# ---------------------------------------------------------------------------
set(CPACK_PACKAGE_NAME "grfeditor")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_VENDOR "caiobvilar")
set(CPACK_PACKAGE_CONTACT "GRFEditor-CPP contributors")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Editor for Ragnarok Online GRF/GPF/Thor archives")
set(CPACK_PACKAGE_DESCRIPTION_FILE "${CMAKE_CURRENT_SOURCE_DIR}/README.md")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "grfeditor")
set(CPACK_PACKAGE_FILE_NAME "grfeditor-${CPACK_PACKAGE_VERSION}-${CMAKE_SYSTEM_PROCESSOR}")

# .deb is cross-built here on Fedora, where dpkg-shlibdeps cannot resolve
# Debian package deps; declare them explicitly instead.
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS OFF)
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libc6, libgcc-s1, libstdc++6, libgl1, libegl1, libx11-6, libxrandr2, libxi6, libxinerama1, libxcursor1, libxss1, libxkbcommon0, libwayland-client0, libpulse0, zlib1g, libasound2t64")
set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "https://github.com/caiobvilar/GRFEditor-CPP")

set(CPACK_RPM_PACKAGE_LICENSE "CC-BY-NC-SA-4.0")
set(CPACK_RPM_PACKAGE_GROUP "Applications/Editors")
set(CPACK_RPM_PACKAGE_URL "https://github.com/caiobvilar/GRFEditor-CPP")

include(CPack)