cmake_minimum_required(VERSION 3.15)
project(waservix VERSION 1.0.0 LANGUAGES C
        DESCRIPTION "WAservix WhatsApp API Gateway client for C and C++"
        HOMEPAGE_URL "https://tekservix.com/waservix/docs/")

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
  set(WASERVIX_TOP_LEVEL ON)
else()
  set(WASERVIX_TOP_LEVEL OFF)
endif()
option(WASERVIX_BUILD_TESTS "Build the tests" ${WASERVIX_TOP_LEVEL})
option(WASERVIX_BUILD_EXAMPLES "Build the examples" OFF)

find_package(CURL REQUIRED)

add_library(waservix src/waservix.c)
add_library(waservix::waservix ALIAS waservix)
target_include_directories(waservix PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_link_libraries(waservix PRIVATE CURL::libcurl)
set_target_properties(waservix PROPERTIES
  C_STANDARD 99
  VERSION ${PROJECT_VERSION}
  SOVERSION ${PROJECT_VERSION_MAJOR}
  C_VISIBILITY_PRESET hidden)
if(BUILD_SHARED_LIBS)
  target_compile_definitions(waservix PUBLIC WASERVIX_SHARED)
endif()
if(MSVC)
  target_compile_options(waservix PRIVATE /W4)
  target_compile_definitions(waservix PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
  target_compile_options(waservix PRIVATE -Wall -Wextra -Wpedantic)
endif()

install(TARGETS waservix EXPORT waservix-targets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT waservix-targets NAMESPACE waservix::
  DESTINATION ${CMAKE_INSTALL_DATADIR}/waservix)
configure_package_config_file(cmake/waservix-config.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/waservix-config.cmake
  INSTALL_DESTINATION ${CMAKE_INSTALL_DATADIR}/waservix)
write_basic_package_version_file(${CMAKE_CURRENT_BINARY_DIR}/waservix-config-version.cmake
  COMPATIBILITY SameMajorVersion)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/waservix-config.cmake
              ${CMAKE_CURRENT_BINARY_DIR}/waservix-config-version.cmake
  DESTINATION ${CMAKE_INSTALL_DATADIR}/waservix)
configure_file(cmake/waservix.pc.in ${CMAKE_CURRENT_BINARY_DIR}/waservix.pc @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/waservix.pc DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)

if(WASERVIX_BUILD_TESTS)
  enable_testing()
  add_executable(waservix_tests tests/test_waservix.c)
  target_link_libraries(waservix_tests PRIVATE waservix::waservix)
  add_test(NAME waservix_tests COMMAND waservix_tests)
endif()

if(WASERVIX_BUILD_EXAMPLES)
  add_executable(waservix_send examples/send.c)
  target_link_libraries(waservix_send PRIVATE waservix::waservix)
endif()
