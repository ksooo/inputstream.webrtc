set(BINARY ${CMAKE_PROJECT_NAME}_test)

list(TRANSFORM WEBRTC_CORE_SOURCES PREPEND ${PROJECT_SOURCE_DIR}/)

add_executable(${BINARY} Base64Test.cpp
                         CaBundleTest.cpp
                         CandidatesTest.cpp
                         CodecsTest.cpp
                         HaSignalingTest.cpp
                         IceServersTest.cpp
                         LogStub.cpp
                         MediaStreamTest.cpp
                         RtcLogTest.cpp
                         RtpClockTest.cpp
                         SequenceTrackerTest.cpp
                         SessionTest.cpp
                         StreamBufferTest.cpp
                         StreamPropertiesTest.cpp
                         TestMain.cpp
                         UrlTest.cpp
                         WhepClientTest.cpp
                         ${WEBRTC_CORE_SOURCES})

target_include_directories(${BINARY} PRIVATE ${GTEST_INCLUDE_DIRS})
target_link_libraries(${BINARY} PRIVATE ${GTEST_LIBRARIES} ${DEPLIBS})

add_test(NAME ${BINARY} COMMAND ${BINARY})
