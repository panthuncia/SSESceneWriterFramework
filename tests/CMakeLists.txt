set(SWF_TEST_BIN "${CMAKE_BINARY_DIR}/tests/bin")

add_executable(swf_abi_layout abi_layout.cpp)
target_include_directories(swf_abi_layout PRIVATE "${SWF_ROOT}/include")
add_test(NAME abi_layout COMMAND swf_abi_layout)

# Four hosts embedding different SWF versions. b and c tie on version, d loads late.
set(hosts a b c d)
set(host_versions 0.1.0.0 0.2.0.0 0.2.0.0 0.3.0.0)
foreach(host version IN ZIP_LISTS hosts host_versions)
	swf_embedded_library(swf_test_core_${host} VERSION ${version} DEFINITIONS SWF_TESTING)
	add_library(swf_host_${host} SHARED election/TestHost.cpp)
	target_link_libraries(swf_host_${host} PRIVATE swf_test_core_${host})
	swf_internal_compile(swf_host_${host} VERSION ${version} DEFINITIONS SWF_TESTING)
	set_target_properties(swf_host_${host} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${SWF_TEST_BIN}")
	list(APPEND host_targets swf_host_${host})
endforeach()

add_library(swf_client_only SHARED election/TestClient.cpp)
target_include_directories(swf_client_only PRIVATE "${SWF_ROOT}/include")
target_compile_definitions(swf_client_only PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
set_target_properties(swf_client_only PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${SWF_TEST_BIN}")

add_executable(swf_election_tests election/Runner.cpp)
target_include_directories(swf_election_tests PRIVATE "${SWF_ROOT}/include")
target_compile_definitions(swf_election_tests PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
set_target_properties(swf_election_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${SWF_TEST_BIN}")
add_dependencies(swf_election_tests ${host_targets} swf_client_only)

set(scenarios order_abc order_cba fallback all_fail pin_host pin_version disabled
	unsupported_runtime late_candidate client_only dispatch)
foreach(scenario IN LISTS scenarios)
	add_test(NAME election_${scenario} COMMAND swf_election_tests ${scenario}
		WORKING_DIRECTORY "${SWF_TEST_BIN}")
endforeach()

# The store families' stubs, run against a synthetic image (links host a's core).
add_executable(swf_store_tests stores/StoreTests.cpp)
target_link_libraries(swf_store_tests PRIVATE swf_test_core_a xbyak::xbyak)
swf_internal_compile(swf_store_tests VERSION 0.1.0.0 DEFINITIONS SWF_TESTING)
add_test(NAME store_stubs COMMAND swf_store_tests)
