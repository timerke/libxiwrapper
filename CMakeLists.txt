cmake_minimum_required(VERSION 2.8)
project(xiwrapper CXX)
if(${CMAKE_SYSTEM_NAME} STREQUAL Darwin)
	# It is required to build a rpath framework
	cmake_policy(SET CMP0042 NEW)
endif()
if(NO_RPATH_PACKAGING)
	set(CMAKE_SKIP_RPATH TRUE)
endif()

OPTION(BUILD_SHARED_LIBS "Build shared libraries" ON)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Detect correct C++11 flag
if(NOT ${CMAKE_SYSTEM_NAME} STREQUAL Windows)
include(CheckCXXCompilerFlag)
CHECK_CXX_COMPILER_FLAG("-std=c++11" COMPILER_SUPPORTS_CXX11)
CHECK_CXX_COMPILER_FLAG("-std=c++0x" COMPILER_SUPPORTS_CXX0X)
if(COMPILER_SUPPORTS_CXX11)
	add_definitions(-DCOMPILER_SUPPORTS_CXX11)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
elseif(COMPILER_SUPPORTS_CXX0X)
	add_definitions(-DCOMPILER_SUPPORTS_CXX0X)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x")
else()
	message(STATUS "The compiler ${CMAKE_CXX_COMPILER} has no C++11 support. Please use a different C++ compiler.")
endif()
endif()

# Findy shared bindy library in BINDY_PATH
find_path(BINDY_INCLUDE_DIR bindy.h PATHS ${BINDY_PATH})
find_library(BINDY_LIBRARY NAMES bindy PATHS ${BINDY_PATH} ${BINDY_PATH}/Release ${BINDY_PATH}/Debug)
include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(BINDY DEFAULT_MSG BINDY_INCLUDE_DIR BINDY_LIBRARY)
mark_as_advanced(BINDY_INCLUDE_DIR BINDY_LIBRARY)

if(NOT BINDY_FOUND)
	message(FATAL_ERROR "Bindy could not be found in BINDY_PATH")
endif()

if(${CMAKE_SYSTEM_NAME} STREQUAL Linux)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")
endif()
if(${CMAKE_SYSTEM_NAME} STREQUAL FreeBSD)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++ -pthread")
endif()
if(${CMAKE_SYSTEM_NAME} STREQUAL Darwin)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
endif()

include_directories(${BINDY_INCLUDE_DIR})

if (BUILD_SHARED_LIBS)
  add_library(xiwrapper SHARED wrapper.cpp revealer_1.cpp)
else()
  add_library(xiwrapper STATIC wrapper.cpp)
endif()
target_link_libraries(xiwrapper ${BINDY_LIBRARY})
