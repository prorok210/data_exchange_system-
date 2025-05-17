# Register component with specific sources and include directories
file(GLOB SRC_FILES "src/*.c")

set(COMPONENT_REQUIRES mavlink)

idf_component_register(
    SRCS ${SRC_FILES}
    INCLUDE_DIRS "include"
    REQUIRES ${COMPONENT_REQUIRES}
)

if(DEFINED ENV{TEST_BUILD} AND "$ENV{TEST_BUILD}" STREQUAL "1")
    message(STATUS "Компиляция в тестовом режиме: TEST_BUILD определен")
    add_compile_definitions(TEST_BUILD)
endif()
