cmake_minimum_required(VERSION 3.18)
project(inputstream.webrtc)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(BUILD_TESTING "Build the unit tests." ON)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_SOURCE_DIR})

find_package(Kodi REQUIRED)
find_package(LibDataChannel REQUIRED)

include_directories(${KODI_INCLUDE_DIR}/.. # Hack way with "/..", need bigger Kodi cmake rework to match right include ways
                    ${LIBDATACHANNEL_INCLUDE_DIRS}
                    ${PROJECT_SOURCE_DIR}/src)
add_compile_definitions(${LIBDATACHANNEL_DEFINITIONS})

# Sources that do not depend on Kodi; shared with the unit tests
set(WEBRTC_CORE_SOURCES src/utils/RtcLog.cpp)

set(WEBRTC_CORE_HEADERS src/utils/Log.h
                        src/utils/RtcLog.h)

set(WEBRTC_SOURCES ${WEBRTC_CORE_SOURCES}
                   src/Addon.cpp
                   src/InputStream.cpp
                   src/utils/Log.cpp)

set(WEBRTC_HEADERS ${WEBRTC_CORE_HEADERS}
                   src/Addon.h
                   src/InputStream.h)

list(APPEND DEPLIBS ${LIBDATACHANNEL_LIBRARIES})

find_package(Threads REQUIRED)
list(APPEND DEPLIBS Threads::Threads)

addon_version(inputstream.webrtc WEBRTC)
add_definitions(-DWEBRTC_VERSION=${WEBRTC_VERSION})

build_addon(inputstream.webrtc WEBRTC DEPLIBS)

if(NOT CMAKE_CROSSCOMPILING AND BUILD_TESTING)
  list(APPEND CMAKE_CTEST_ARGUMENTS "-V")
  enable_testing()
  find_package(Gtest REQUIRED)
  add_subdirectory(tests)
endif()

include(CPack)
