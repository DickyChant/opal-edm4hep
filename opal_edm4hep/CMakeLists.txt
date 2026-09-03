cmake_minimum_required(VERSION 3.24)
project(opal_edm4hep LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

# This project does not pin a key4hep stack: choosing one is the caller's job,
# exactly as in the DELPHI converter this package is modelled on. Configure
# fails early with the commands to run rather than half-way through a build.
option(KEY4HEP_STACK_REQUIRED "Fail if no key4hep stack is sourced" ON)
if(KEY4HEP_STACK_REQUIRED AND NOT DEFINED ENV{KEY4HEP_STACK})
  message(FATAL_ERROR
    "No key4hep stack sourced. Run:\n"
    "  source /cvmfs/sw.hsf.org/key4hep/setup.sh\n"
    "then re-run cmake. Pass -DKEY4HEP_STACK_REQUIRED=OFF to bypass.")
endif()

find_package(ROOT 6.28 REQUIRED COMPONENTS Core RIO Tree TreePlayer)
find_package(podio 1.0 REQUIRED)
find_package(EDM4HEP 1.0 REQUIRED)

message(STATUS "opal_edm4hep: key4hep stack $ENV{KEY4HEP_STACK}")
message(STATUS "opal_edm4hep: ROOT ${ROOT_VERSION}, podio ${podio_VERSION}, EDM4hep ${EDM4HEP_VERSION}")

add_library(opal_edm4hep
  src/NtupleReader.cpp
  src/ScalarPassthrough.cpp
  src/Helix.cpp
  src/Converter.cpp)

target_include_directories(opal_edm4hep PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>)

target_link_libraries(opal_edm4hep PUBLIC
  ROOT::Core ROOT::RIO ROOT::Tree ROOT::TreePlayer
  podio::podio podio::podioRootIO
  EDM4HEP::edm4hep)

add_executable(opal_ntuple_pass bin/opal_ntuple_pass.cpp)
target_link_libraries(opal_ntuple_pass PRIVATE opal_edm4hep)

install(TARGETS opal_edm4hep opal_ntuple_pass
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib)
install(DIRECTORY include/ DESTINATION include)

include(CTest)
if(BUILD_TESTING)
  add_subdirectory(tests)
endif()
