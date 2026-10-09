cmake_minimum_required(VERSION 3.18)
project(inputstream.webrtc)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(BUILD_TESTING "Build the unit tests." ON)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_SOURCE_DIR})

find_package(Kodi REQUIRED)
find_package(LibDataChannel REQUIRED)
find_package(NlohmannJSON REQUIRED)

include_directories(${KODI_INCLUDE_DIR}/.. # Hack way with "/..", need bigger Kodi cmake rework to match right include ways
                    ${LIBDATACHANNEL_INCLUDE_DIRS}
                    ${NLOHMANNJSON_INCLUDE_DIRS}
                    ${PROJECT_SOURCE_DIR}/src)
add_compile_definitions(${LIBDATACHANNEL_DEFINITIONS})

# Sources that do not depend on Kodi; shared with the unit tests
set(WEBRTC_CORE_SOURCES src/StreamProperties.cpp
                        src/ha/HaSignaling.cpp
                        src/session/Candidates.cpp
                        src/session/IceServers.cpp
                        src/session/RtpReceiver.cpp
                        src/session/Session.cpp
                        src/stream/Codecs.cpp
                        src/stream/MediaStream.cpp
                        src/stream/RtpClock.cpp
                        src/stream/SequenceTracker.cpp
                        src/stream/StreamBuffer.cpp
                        src/utils/Base64.cpp
                        src/utils/CaBundle.cpp
                        src/utils/RtcLog.cpp
                        src/utils/StringUtils.cpp
                        src/utils/Url.cpp
                        src/whep/WhepClient.cpp)

set(WEBRTC_CORE_HEADERS src/StreamProperties.h
                        src/ha/HaSignaling.h
                        src/session/Candidates.h
                        src/session/IceServers.h
                        src/session/RtpReceiver.h
                        src/session/Session.h
                        src/session/Signaling.h
                        src/stream/Codecs.h
                        src/stream/MediaStream.h
                        src/stream/RtpClock.h
                        src/stream/SequenceTracker.h
                        src/stream/StreamBuffer.h
                        src/utils/Base64.h
                        src/utils/CaBundle.h
                        src/utils/Log.h
                        src/utils/RtcLog.h
                        src/utils/StringUtils.h
                        src/utils/Url.h
                        src/whep/HttpTransport.h
                        src/whep/WhepClient.h)

set(WEBRTC_SOURCES ${WEBRTC_CORE_SOURCES}
                   src/Addon.cpp
                   src/InputStream.cpp
                   src/utils/Log.cpp
                   src/whep/KodiHttpTransport.cpp)

set(WEBRTC_HEADERS ${WEBRTC_CORE_HEADERS}
                   src/Addon.h
                   src/InputStream.h
                   src/whep/KodiHttpTransport.h)

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
