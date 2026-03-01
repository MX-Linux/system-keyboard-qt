cmake_minimum_required(VERSION 3.16)

project(system-keyboard-qt)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
include(GNUInstallDirs)

# Detect distribution at build time
if(EXISTS "/etc/arch-release")
    set(DISTRO "ARCH")
    add_definitions(-DDISTRO_ARCH)
    message(STATUS "Building for Arch Linux")
elseif(EXISTS "/etc/debian_version")
    set(DISTRO "DEBIAN")
    add_definitions(-DDISTRO_DEBIAN)
    message(STATUS "Building for Debian/Ubuntu")
else()
    set(DISTRO "UNKNOWN")
    add_definitions(-DDISTRO_DEBIAN)
    message(WARNING "Unknown distribution, defaulting to Debian behavior")
endif()

find_package(Qt6 REQUIRED COMPONENTS Core Widgets)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

set(SOURCES
    cmd.cpp
    main.cpp
    window.cpp
    keyboardlayouts.cpp
    selectlayoutdialog.cpp
    posixconfigparser.cpp
    translation.cpp
    filterablecombobox.cpp
)

set(HEADERS
    cmd.h
    window.h
    keyboardlayouts.h
    selectlayoutdialog.h
    translation.h
    posixconfigparser.h
    filterablecombobox.h
)

set(FORMS
    window.ui
    selectlayoutdialog.ui
)

set(RESOURCES
    system-keyboard-qt-resource.qrc
)

qt6_add_executable(system-keyboard-qt ${SOURCES} ${HEADERS} ${FORMS} ${RESOURCES})

target_include_directories(system-keyboard-qt PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(system-keyboard-qt PRIVATE Qt::Core Qt::Widgets)

install(TARGETS system-keyboard-qt
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
