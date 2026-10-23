# Embeds SceneWriterFramework in a host plugin:
#
#   include(path/to/SceneWriterFramework/cmake/SWFEmbed.cmake)
#   target_link_libraries(MyPlugin PRIVATE swf::embedded)
#
# then call SWF_Embed_OnPostLoad() (swf/swf_embed.h) from the SKSE kPostLoad handler.
include_guard(GLOBAL)

include("${CMAKE_CURRENT_LIST_DIR}/SWFVersion.cmake")
get_filename_component(SWF_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)

if(NOT TARGET xbyak::xbyak)
	find_package(xbyak CONFIG REQUIRED)
endif()
if(NOT DETOURS_LIBRARY)
	# vcpkg's detours port: the release library, and the debug one when the triplet builds it.
	find_path(DETOURS_INCLUDE_DIRS "detours/detours.h" REQUIRED)
	find_library(DETOURS_LIBRARY_RELEASE detours PATHS "${DETOURS_INCLUDE_DIRS}/../lib" NO_DEFAULT_PATH REQUIRED)
	find_library(DETOURS_LIBRARY_DEBUG detours PATHS "${DETOURS_INCLUDE_DIRS}/../debug/lib" NO_DEFAULT_PATH)
	if(DETOURS_LIBRARY_DEBUG)
		set(DETOURS_LIBRARY optimized "${DETOURS_LIBRARY_RELEASE}" debug "${DETOURS_LIBRARY_DEBUG}")
	else()
		set(DETOURS_LIBRARY "${DETOURS_LIBRARY_RELEASE}")
	endif()
endif()

set(SWF_CORE_SOURCES
	"${SWF_ROOT}/src/core/Candidate.cpp"
	"${SWF_ROOT}/src/core/Dispatch.cpp"
	"${SWF_ROOT}/src/core/Log.cpp"
	"${SWF_ROOT}/src/core/Provider.cpp"
	"${SWF_ROOT}/src/families/Calls.cpp"
	"${SWF_ROOT}/src/families/Families.cpp"
	"${SWF_ROOT}/src/families/Moves.cpp"
	"${SWF_ROOT}/src/families/Stores.cpp"
	"${SWF_ROOT}/src/families/Structure.cpp"
	"${SWF_ROOT}/src/hooking/Hooks.cpp"
	"${SWF_ROOT}/src/hooking/Memory.cpp"
	"${SWF_ROOT}/src/tables/Ae_1_6_1170.cpp"
	"${SWF_ROOT}/src/tables/Ae_1_6_1170_Hooks.cpp")

# Applies SWF's private include path, version defines and warnings to `target`.
# VERSION overrides the implementation version (tests build several versions at once).
function(swf_internal_compile target)
	cmake_parse_arguments(ARG "" "VERSION" "DEFINITIONS" ${ARGN})
	if(ARG_VERSION)
		string(REPLACE "." ";" parts "${ARG_VERSION}")
		list(GET parts 0 major)
		list(GET parts 1 minor)
		list(GET parts 2 patch)
		list(GET parts 3 build)
	else()
		set(major ${SWF_VERSION_MAJOR})
		set(minor ${SWF_VERSION_MINOR})
		set(patch ${SWF_VERSION_PATCH})
		set(build ${SWF_VERSION_BUILD})
	endif()
	target_include_directories(${target} PRIVATE "${SWF_ROOT}/src")
	target_compile_features(${target} PRIVATE cxx_std_20)
	target_compile_definitions(${target} PRIVATE
		SWF_VERSION_MAJOR=${major} SWF_VERSION_MINOR=${minor}
		SWF_VERSION_PATCH=${patch} SWF_VERSION_BUILD=${build}
		SWF_NAMESPACE=v${major}_${minor}_${patch}_${build}
		WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE
		${ARG_DEFINITIONS})
	if(MSVC)
		target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8 /Zc:preprocessor)
	endif()
endfunction()

# Creates a static SWF core library named `name`.
function(swf_embedded_library name)
	cmake_parse_arguments(ARG "" "VERSION" "DEFINITIONS" ${ARGN})
	add_library(${name} STATIC ${SWF_CORE_SOURCES})
	target_include_directories(${name} PUBLIC "${SWF_ROOT}/include")
	target_compile_features(${name} PUBLIC cxx_std_20)
	target_link_libraries(${name} PRIVATE xbyak::xbyak ${DETOURS_LIBRARY})
	target_include_directories(${name} PRIVATE ${DETOURS_INCLUDE_DIRS})
	swf_internal_compile(${name} VERSION "${ARG_VERSION}" DEFINITIONS ${ARG_DEFINITIONS})
endfunction()

if(NOT TARGET swf_embedded)
	swf_embedded_library(swf_embedded)
	add_library(swf::embedded ALIAS swf_embedded)
endif()
