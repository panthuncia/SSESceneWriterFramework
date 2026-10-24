# SceneWriterFramework's headers, with no build dependencies:
#
#   include(path/to/SceneWriterFramework/cmake/SWFInterface.cmake)
#   target_link_libraries(MyPlugin PRIVATE swf::interface)  # MIT: swf/swf.h, swf/swf_loader.h, swf/swf.hpp
#   target_link_libraries(MyPlugin PRIVATE swf::commonlib)  # GPL: swf/swf_commonlib.h (link CommonLibSSE-NG yourself)
#
# swf::interface is all a plugin under another license may use: it connects to a provider at run time.
include_guard(GLOBAL)

get_filename_component(SWF_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)

add_library(swf_interface INTERFACE)
target_include_directories(swf_interface INTERFACE "${SWF_ROOT}/include")
target_compile_features(swf_interface INTERFACE cxx_std_20)
add_library(swf::interface ALIAS swf_interface)

add_library(swf_commonlib INTERFACE)
target_include_directories(swf_commonlib INTERFACE "${SWF_ROOT}/include-gpl")
target_link_libraries(swf_commonlib INTERFACE swf_interface)
add_library(swf::commonlib ALIAS swf_commonlib)
