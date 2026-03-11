cmake_minimum_required(VERSION 3.16)
project(swanos_gui LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Network Core)

set(SOURCES
    main.cpp
    pipe_connection.cpp
    serial_worker.cpp
    llm_client.cpp
    boot_splash.cpp
    login_screen.cpp
    terminal.cpp
    sidebar.cpp
    desktop.cpp
    swanos_window.cpp
)

set(HEADERS
    colors.h
    particle.h
    pipe_connection.h
    serial_worker.h
    llm_client.h
    boot_splash.h
    login_screen.h
    terminal.h
    sidebar.h
    desktop.h
    swanos_window.h
)

add_executable(${PROJECT_NAME} WIN32 ${SOURCES} ${HEADERS})

target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Widgets
    Qt6::Network
    Qt6::Core
)

# On Windows, link win32 pipe APIs
if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE ws2_32)
endif()
