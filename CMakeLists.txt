cmake_minimum_required(VERSION 3.25)

include("${CMAKE_CURRENT_LIST_DIR}/cmake/SWFVersion.cmake")
project(SceneWriterFramework VERSION ${SWF_VERSION} LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(cmake/SWFEmbed.cmake)

option(SWF_BUILD_PLUGIN "Build the standalone SceneWriterFramework.dll" ${PROJECT_IS_TOP_LEVEL})
option(SWF_BUILD_TESTS "Build the SWF tests" ${PROJECT_IS_TOP_LEVEL})

if(SWF_BUILD_PLUGIN)
	add_library(SceneWriterFramework SHARED plugin/Plugin.cpp)
	target_link_libraries(SceneWriterFramework PRIVATE swf::embedded)
	swf_internal_compile(SceneWriterFramework)
	install(TARGETS SceneWriterFramework RUNTIME DESTINATION SKSE/Plugins)
	install(FILES $<TARGET_PDB_FILE:SceneWriterFramework> DESTINATION SKSE/Plugins OPTIONAL)
endif()

if(SWF_BUILD_TESTS)
	enable_testing()
	add_subdirectory(tests)
endif()
