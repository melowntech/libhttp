define_module(BINARY http-clienttest DEPENDS http)

set(SRC_LIST
	main.cpp
)

add_executable(http-clienttest ${SRC_LIST})
buildsys_binary(http-clienttest)
target_link_libraries(http-clienttest ${MODULE_LIBRARIES})
buildsys_target_compile_definitions(http-clienttest ${MODULE_DEFINITIONS})

