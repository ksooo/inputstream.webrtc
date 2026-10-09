# - Find LibDataChannel
# Find the static libdatachannel WebRTC library and the libraries it depends on
#
#   LIBDATACHANNEL_FOUND        - True if libdatachannel and all its dependencies were found.
#   LIBDATACHANNEL_INCLUDE_DIRS - where to find rtc/rtc.hpp, etc.
#   LIBDATACHANNEL_LIBRARIES    - the libraries to link against, in link order.
#   LIBDATACHANNEL_DEFINITIONS  - the compile definitions for using the static library.
#

find_path(LIBDATACHANNEL_INCLUDE_DIR rtc/rtc.hpp)
find_library(LIBDATACHANNEL_LIBRARY datachannel)
find_library(LIBDATACHANNEL_JUICE_LIBRARY juice)
find_library(LIBDATACHANNEL_USRSCTP_LIBRARY usrsctp)
find_library(LIBDATACHANNEL_SRTP_LIBRARY srtp2)
find_library(LIBDATACHANNEL_MBEDTLS_LIBRARY mbedtls)
find_library(LIBDATACHANNEL_MBEDX509_LIBRARY mbedx509)
find_library(LIBDATACHANNEL_MBEDCRYPTO_LIBRARY mbedcrypto)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LibDataChannel REQUIRED_VARS LIBDATACHANNEL_INCLUDE_DIR
                                                               LIBDATACHANNEL_LIBRARY
                                                               LIBDATACHANNEL_JUICE_LIBRARY
                                                               LIBDATACHANNEL_USRSCTP_LIBRARY
                                                               LIBDATACHANNEL_SRTP_LIBRARY
                                                               LIBDATACHANNEL_MBEDTLS_LIBRARY
                                                               LIBDATACHANNEL_MBEDX509_LIBRARY
                                                               LIBDATACHANNEL_MBEDCRYPTO_LIBRARY)

if(LIBDATACHANNEL_FOUND)
  set(LIBDATACHANNEL_INCLUDE_DIRS ${LIBDATACHANNEL_INCLUDE_DIR})
  set(LIBDATACHANNEL_LIBRARIES ${LIBDATACHANNEL_LIBRARY}
                               ${LIBDATACHANNEL_JUICE_LIBRARY}
                               ${LIBDATACHANNEL_USRSCTP_LIBRARY}
                               ${LIBDATACHANNEL_SRTP_LIBRARY}
                               ${LIBDATACHANNEL_MBEDTLS_LIBRARY}
                               ${LIBDATACHANNEL_MBEDX509_LIBRARY}
                               ${LIBDATACHANNEL_MBEDCRYPTO_LIBRARY})
  set(LIBDATACHANNEL_DEFINITIONS RTC_STATIC)
  if(WIN32)
    list(APPEND LIBDATACHANNEL_LIBRARIES ws2_32 iphlpapi bcrypt)
  endif()
endif()

mark_as_advanced(LIBDATACHANNEL_INCLUDE_DIR
                 LIBDATACHANNEL_LIBRARY
                 LIBDATACHANNEL_JUICE_LIBRARY
                 LIBDATACHANNEL_USRSCTP_LIBRARY
                 LIBDATACHANNEL_SRTP_LIBRARY
                 LIBDATACHANNEL_MBEDTLS_LIBRARY
                 LIBDATACHANNEL_MBEDX509_LIBRARY
                 LIBDATACHANNEL_MBEDCRYPTO_LIBRARY)
