cmake_minimum_required(VERSION 3.20)
project(ofl VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# CEF paths
set(CEF_ROOT "/tmp/cef" CACHE PATH "CEF binary distribution root")
set(CEF_LIB_DIR "${CEF_ROOT}/Release")
set(CEF_RES_DIR "${CEF_ROOT}/Resources")
set(CEF_INCLUDE_DIR "${CEF_ROOT}/include")
set(CEF_WRAPPER_LIB "${CEF_ROOT}/build/libcef_dll_wrapper/libcef_dll_wrapper.a")

# System dependencies
find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
pkg_check_modules(APPINDICATOR REQUIRED ayatana-appindicator3-0.1)
pkg_check_modules(LIBNOTIFY REQUIRED libnotify)
pkg_check_modules(NSS REQUIRED nss)
pkg_check_modules(ALSA REQUIRED alsa)

add_executable(ofl
    src/main.cc
    src/app.cc
    src/client.cc
    src/config.cc
    src/window.cc
    src/tray.cc
    src/notifications.cc
)

target_include_directories(ofl PRIVATE
    ${CEF_ROOT}
    ${CEF_INCLUDE_DIR}/..
    ${GTK3_INCLUDE_DIRS}
    ${APPINDICATOR_INCLUDE_DIRS}
    ${LIBNOTIFY_INCLUDE_DIRS}
    ${NSS_INCLUDE_DIRS}
    ${ALSA_INCLUDE_DIRS}
    src
)

target_link_libraries(ofl PRIVATE
    ${CEF_WRAPPER_LIB}
    ${CEF_LIB_DIR}/libcef.so
    ${GTK3_LIBRARIES}
    ${APPINDICATOR_LIBRARIES}
    ${LIBNOTIFY_LIBRARIES}
    ${NSS_LIBRARIES}
    ${ALSA_LIBRARIES}
    pthread
    dl
)

target_link_directories(ofl PRIVATE
    ${CEF_LIB_DIR}
)

target_compile_options(ofl PRIVATE -Wall -Wextra -Wpedantic)

set_target_properties(ofl PROPERTIES
    BUILD_RPATH "${CEF_LIB_DIR};${CMAKE_CURRENT_BINARY_DIR}"
    INSTALL_RPATH "$ORIGIN"
)

# Copy CEF resources next to libcef.so for dev builds
add_custom_command(TARGET ofl POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CEF_RES_DIR}/icudtl.dat ${CEF_LIB_DIR}/icudtl.dat
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CEF_RES_DIR}/chrome_100_percent.pak ${CEF_LIB_DIR}/chrome_100_percent.pak
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CEF_RES_DIR}/chrome_200_percent.pak ${CEF_LIB_DIR}/chrome_200_percent.pak
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CEF_RES_DIR}/resources.pak ${CEF_LIB_DIR}/resources.pak
    COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different ${CEF_RES_DIR}/locales ${CEF_LIB_DIR}/locales
    COMMENT "Copying CEF resources next to libcef.so"
)

# Also symlink CEF libs/resources next to the binary for dev builds
add_custom_command(TARGET ofl POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E create_symlink ${CEF_LIB_DIR}/libcef.so ${CMAKE_CURRENT_BINARY_DIR}/libcef.so
    COMMAND ${CMAKE_COMMAND} -E create_symlink ${CEF_LIB_DIR}/libEGL.so ${CMAKE_CURRENT_BINARY_DIR}/libEGL.so
    COMMAND ${CMAKE_COMMAND} -E create_symlink ${CEF_LIB_DIR}/libGLESv2.so ${CMAKE_CURRENT_BINARY_DIR}/libGLESv2.so
    COMMAND ${CMAKE_COMMAND} -E create_symlink ${CEF_LIB_DIR}/libvk_swiftshader.so ${CMAKE_CURRENT_BINARY_DIR}/libvk_swiftshader.so
    COMMAND ${CMAKE_COMMAND} -E create_symlink ${CEF_LIB_DIR}/libvulkan.so.1 ${CMAKE_CURRENT_BINARY_DIR}/libvulkan.so.1
    COMMAND ${CMAKE_COMMAND} -E create_symlink ${CEF_LIB_DIR}/v8_context_snapshot.bin ${CMAKE_CURRENT_BINARY_DIR}/v8_context_snapshot.bin
    COMMAND ${CMAKE_COMMAND} -E create_symlink ${CEF_LIB_DIR}/vk_swiftshader_icd.json ${CMAKE_CURRENT_BINARY_DIR}/vk_swiftshader_icd.json
    COMMAND ${CMAKE_COMMAND} -E create_symlink ${CEF_LIB_DIR}/icudtl.dat ${CMAKE_CURRENT_BINARY_DIR}/icudtl.dat
    COMMAND ${CMAKE_COMMAND} -E create_symlink ${CEF_LIB_DIR}/chrome_100_percent.pak ${CMAKE_CURRENT_BINARY_DIR}/chrome_100_percent.pak
    COMMAND ${CMAKE_COMMAND} -E create_symlink ${CEF_LIB_DIR}/chrome_200_percent.pak ${CMAKE_CURRENT_BINARY_DIR}/chrome_200_percent.pak
    COMMAND ${CMAKE_COMMAND} -E create_symlink ${CEF_LIB_DIR}/resources.pak ${CMAKE_CURRENT_BINARY_DIR}/resources.pak
    COMMAND ${CMAKE_COMMAND} -E create_symlink ${CEF_LIB_DIR}/locales ${CMAKE_CURRENT_BINARY_DIR}/locales
    COMMENT "Symlinking CEF resources next to ofl binary for dev builds"
)

# Install targets
set(OFL_INSTALL_LIBDIR "lib/ofl" CACHE STRING "Install directory for ofl libs")

install(TARGETS ofl RUNTIME DESTINATION ${OFL_INSTALL_LIBDIR})

install(FILES
    ${CEF_LIB_DIR}/libcef.so
    ${CEF_LIB_DIR}/libEGL.so
    ${CEF_LIB_DIR}/libGLESv2.so
    ${CEF_LIB_DIR}/libvk_swiftshader.so
    ${CEF_LIB_DIR}/libvulkan.so.1
    ${CEF_LIB_DIR}/v8_context_snapshot.bin
    ${CEF_LIB_DIR}/vk_swiftshader_icd.json
    DESTINATION ${OFL_INSTALL_LIBDIR}
)

install(FILES
    ${CEF_RES_DIR}/icudtl.dat
    ${CEF_RES_DIR}/chrome_100_percent.pak
    ${CEF_RES_DIR}/chrome_200_percent.pak
    ${CEF_RES_DIR}/resources.pak
    DESTINATION ${OFL_INSTALL_LIBDIR}
)
install(DIRECTORY ${CEF_RES_DIR}/locales DESTINATION ${OFL_INSTALL_LIBDIR})

install(FILES data/ofl.desktop DESTINATION share/applications)
install(FILES data/dev.ofl.outlook-lite.appdata.xml DESTINATION share/metainfo)
install(FILES data/ofl-app.svg DESTINATION share/icons/hicolor/scalable/apps)
install(FILES data/ofl.svg DESTINATION share/icons/hicolor/scalable/apps)

file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/ofl-launcher
"#!/bin/sh\nexec \"${CMAKE_INSTALL_PREFIX}/${OFL_INSTALL_LIBDIR}/ofl\" \"$@\"\n")
install(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/ofl-launcher DESTINATION bin RENAME ofl)
