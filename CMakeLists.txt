cmake_minimum_required(VERSION 3.7)

set(VERSION 4.0)

project(gxde-launcher)

#set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_FLAGS "-g -Wall")

if (DEFINED ENABLE_MIEEE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mieee")
endif()

if (DEFINED DISABLE_DRAG_ANIMATION)
    add_definitions(-DDISABLE_DRAG_ANIMATION)
endif ()

if (DEFINED WITHOUT_UNINSTALL_APP)
    add_definitions(-DWITHOUT_UNINSTALL_APP)
endif ()

set(BIN_NAME gxde-launcher)

# Sources files
file(GLOB_RECURSE SRCS "src/*.h" "src/*.cpp")

# Install settings
if (CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX /usr)
endif ()

if (NOT (${CMAKE_BUILD_TYPE} MATCHES "Debug"))
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Ofast")

    # generate qm
    execute_process(COMMAND bash "translate_generation.sh"
                    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
endif ()

# dev
file(GLOB INTERFACES "src/dbusinterface/*.h")

# Find the library
find_package(PkgConfig REQUIRED)
find_package(Qt6Widgets REQUIRED)
find_package(Qt6Concurrent REQUIRED)
find_package(Qt6DBus REQUIRED)
find_package(Qt6Svg REQUIRED)
find_package(Dtk2widget REQUIRED)
find_package(Dtk6Core REQUIRED)
find_package(Dtk6Gui REQUIRED)
find_package(DFrameworkdbusQt6 REQUIRED)
find_package(LayerShellQt REQUIRED)

pkg_check_modules(XCB_EWMH REQUIRED xcb-ewmh x11)
pkg_check_modules(QGSettings REQUIRED gsettings-qt6)

add_executable(${BIN_NAME} ${SRCS} ${INTERFACES} src/skin.qrc src/widgets/images.qrc)
target_include_directories(${BIN_NAME} PUBLIC ${XCB_EWMH_INCLUDE_DIRS}
                                              ${DFrameworkdbusQt6_INCLUDE_DIRS}
                                              /usr/include/libdframeworkdbus-qt6-6.0
                                              ${Qt6Gui_PRIVATE_INCLUDE_DIRS}
                                              ${QGSettings_INCLUDE_DIRS}
                                              ${Dtk6Core_INCLUDE_DIRS}
                                              ${Dtk6Gui_INCLUDE_DIRS}
                                              ${PROJECT_BINARY_DIR})

target_link_libraries(${BIN_NAME} PRIVATE
    ${XCB_EWMH_LIBRARIES}
    DFrameworkdbusQt6::DFrameworkdbusQt6
    ${Dtk2widget_LIBRARIES}
    Dtk6::Gui
    ${Qt6Widgets_LIBRARIES}
    ${Qt6Concurrent_LIBRARIES}
    ${Qt6DBus_LIBRARIES}
    ${QGSettings_LIBRARIES}
    ${Qt6Svg_LIBRARIES}
    LayerShellQt::Interface
)

## qm files
file(GLOB QM_FILES "translations/*.qm")
install(FILES ${QM_FILES} DESTINATION share/gxde-launcher/translations)

## dev files
install (FILES ${INTERFACES} DESTINATION include/gxde-launcher)

## services files
install(FILES src/dbusservices/com.deepin.dde.Launcher.service DESTINATION /usr/share/dbus-1/services)

## icon
install(FILES data/deepin-launcher.svg DESTINATION /usr/share/icons/hicolor/scalable/apps)

## Launcher Chooser
#install(FILES data/gxde-launcher DESTINATION /usr/bin)

# bin
install(TARGETS ${BIN_NAME} DESTINATION bin)

