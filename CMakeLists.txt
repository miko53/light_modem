cmake_minimum_required(VERSION 3.13)

project(lmodem C)

#for installation 
include(GNUInstallDirs)
#for package
include(CMakePackageConfigHelpers)

set(CMAKE_DEBUG_POSTFIX d)

if (MODEM_DEBUG)
add_definitions(-DLMODEM_TRACE)
endif()

### Std compiler options
if(UNIX)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} --std=gnu99 -W -Wall -Wshadow -Wno-aggregate-return -Wno-suggest-attribute=format -Wno-undef -fms-extensions")  #-Wno-discarded-qualifiers
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -O0 -fstack-protector-all -Wstack-protector -fno-omit-frame-pointer")
endif()

if (COVERAGE)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} --coverage")
endif()

#include_directories(include)
add_subdirectory(src)

target_include_directories(lxymodem
                           PUBLIC 
                            "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
                            "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>")

install(TARGETS lxymodem
        EXPORT lxymodemTarget
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
install(FILES include/lmodem.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(FILES include/crc16.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT lxymodemTarget
        FILE lxymodemTarget.cmake
        NAMESPACE lxymodem::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/lxymodem)


include(CMakePackageConfigHelpers)
     
# set version
set(version 0.0.0)

set_property(TARGET lxymodem PROPERTY VERSION ${version})
set_property(TARGET lxymodem PROPERTY SOVERSION 0)
set_property(TARGET lxymodem PROPERTY
  INTERFACE_lxymodem_MAJOR_VERSION 0)
set_property(TARGET lxymodem APPEND PROPERTY
  COMPATIBLE_INTERFACE_STRING lxymodem_MAJOR_VERSION
)

# generate the version file for the config file
write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/lxymodemConfigVersion.cmake"
  VERSION "${version}"
  COMPATIBILITY AnyNewerVersion
)

configure_package_config_file(${CMAKE_CURRENT_SOURCE_DIR}/Config.cmake.in
  "${CMAKE_CURRENT_BINARY_DIR}/lxymodemConfig.cmake"
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/lxymodem
)
install(FILES
          "${CMAKE_CURRENT_BINARY_DIR}/lxymodemConfig.cmake"
          "${CMAKE_CURRENT_BINARY_DIR}/lxymodemConfigVersion.cmake"
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/lxymodem
)

export(EXPORT lxymodemTarget
       FILE "${CMAKE_CURRENT_BINARY_DIR}/cmake/lxymodemTarget.cmake"
       NAMESPACE lxymodem::
)

include(InstallRequiredSystemLibraries)
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE.txt")
set(CPACK_PACKAGE_VERSION_MAJOR "0")
set(CPACK_PACKAGE_VERSION_MINOR "0")
include(CPack)

add_subdirectory(tools)
