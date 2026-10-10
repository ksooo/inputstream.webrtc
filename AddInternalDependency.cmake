# Builds a dependency defined in depends/common with ExternalProject, for builds outside of Kodi's
# add-on build system such as Debian packages. Like Kodi's add-on build system, it builds the
# dependencies in deps.txt first and passes the CMake options in flags.txt. Downloads are skipped
# for archives already present in INTERNAL_DEPENDS_DOWNLOAD_DIR.
#
#   add_internal_dependency(<name> [BUILD_BYPRODUCTS <files>])
#
# Installs static libraries into INTERNAL_DEPENDS_PREFIX.

include(ExternalProject)

set(INTERNAL_DEPENDS_PREFIX ${CMAKE_BINARY_DIR}/depends)
set(INTERNAL_DEPENDS_DOWNLOAD_DIR ${CMAKE_BINARY_DIR}/download
    CACHE PATH "Where the archives of internally built dependencies are downloaded to")

function(add_internal_dependency name)
  cmake_parse_arguments(ARG "" "" "BUILD_BYPRODUCTS" ${ARGN})
  if(TARGET ${name})
    return()
  endif()

  set(dir ${PROJECT_SOURCE_DIR}/depends/common/${name})
  file(STRINGS ${dir}/${name}.txt definition LIMIT_COUNT 1)
  string(REGEX REPLACE "^${name}[ \t]+([^ \t]+).*$" "\\1" url "${definition}")
  file(STRINGS ${dir}/${name}.sha256 sha256 LIMIT_COUNT 1)

  set(depends)
  if(EXISTS ${dir}/deps.txt)
    file(STRINGS ${dir}/deps.txt depends)
    foreach(dependency ${depends})
      add_internal_dependency(${dependency})
    endforeach()
  endif()

  set(flags)
  if(EXISTS ${dir}/flags.txt)
    file(STRINGS ${dir}/flags.txt flags LIMIT_COUNT 1)
    separate_arguments(flags)
  endif()

  set(patch_command)
  file(GLOB patches ${dir}/*.patch)
  if(patches)
    find_program(PATCH_PROGRAM patch REQUIRED)
    list(SORT patches)
    foreach(patch ${patches})
      list(APPEND patch_command COMMAND ${PATCH_PROGRAM} -p1 -i ${patch})
    endforeach()
    list(REMOVE_AT patch_command 0)
  endif()

  externalproject_add(${name}
                      URL ${url}
                      URL_HASH SHA256=${sha256}
                      DOWNLOAD_DIR ${INTERNAL_DEPENDS_DOWNLOAD_DIR}
                      PREFIX ${CMAKE_BINARY_DIR}/build/${name}
                      PATCH_COMMAND ${patch_command}
                      DEPENDS ${depends}
                      CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${INTERNAL_DEPENDS_PREFIX}
                                 -DCMAKE_INSTALL_LIBDIR=lib
                                 -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
                                 -DBUILD_SHARED_LIBS=OFF
                                 -DCMAKE_POSITION_INDEPENDENT_CODE=ON
                                 -DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}
                                 ${flags}
                      CMAKE_CACHE_ARGS -DCMAKE_PREFIX_PATH:STRING=${INTERNAL_DEPENDS_PREFIX};${CMAKE_PREFIX_PATH}
                      BUILD_BYPRODUCTS ${ARG_BUILD_BYPRODUCTS})
endfunction()
