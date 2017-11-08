cmake_minimum_required(VERSION 3.0)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/CMakeModules")
set(CMAKE_CONFIGURATION_TYPES "Debug;Release")
project(life)

set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
file(GLOB_RECURSE SRC "${SRC_DIR}/*.cpp" "${SRC_DIR}/*.c" "${SRC_DIR}/*.h" "${SRC_DIR}/*.hpp")

set(SHADERS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/shaders)
file(GLOB_RECURSE SHADERS
     "${SHADERS_DIR}/*.vert"
     "${SHADERS_DIR}/*.tesc"
     "${SHADERS_DIR}/*.tese"
     "${SHADERS_DIR}/*.geom"
     "${SHADERS_DIR}/*.frag"
     "${SHADERS_DIR}/*.comp")

if(CMAKE_COMPILER_IS_GNUCXX)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
elseif(MSVC)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")
endif()

if(UNIX)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")
endif()

find_package(bp REQUIRED)
find_package(bpScene REQUIRED)
find_package(bpView REQUIRED)

if (NOT BP_FOUND)
	message(SEND_ERROR "bp not found")
endif()
if (NOT BP_SCENE_FOUND)
	message(SEND_ERROR "bpScene not found")
endif()
if (NOT BP_VIEW_FOUND)
	message(SEND_ERROR "bpView not found")
endif()

include_directories(${BP_INCLUDE_DIRS}
		    ${BP_SCENE_INCLUDE_DIRS}
		    ${BP_VIEW_INCLUDE_DIRS})

add_executable(life ${SRC})
target_link_libraries(life
		      ${BP_LIBRARIES}
		      ${BP_SCENE_LIBRARIES}
		      ${BP_VIEW_LIBRARIES})

set(GLSLANG_VALIDATOR glslangValidator)
foreach(SHADER ${SHADERS})
	get_filename_component(FILE_NAME ${SHADER} NAME)
	set(SPIRV "${PROJECT_BINARY_DIR}/spv/${FILE_NAME}.spv")
	add_custom_command(
		OUTPUT ${SPIRV}
		COMMAND ${CMAKE_COMMAND} -E make_directory "${PROJECT_BINARY_DIR}/spv/"
		COMMAND ${GLSLANG_VALIDATOR} -V ${SHADER} -o ${SPIRV}
		DEPENDS ${SHADER})
	list(APPEND SPIRV_BINARY_FILES ${SPIRV})
endforeach(SHADER)

add_custom_target(shaders DEPENDS ${SPIRV_BINARY_FILES})
add_dependencies(life shaders)