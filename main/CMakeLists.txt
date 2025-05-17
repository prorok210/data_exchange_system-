# Register component with specific sources and include directories
file(GLOB SRC_FILES "src/*.c")

idf_component_register(
    SRCS ${SRC_FILES}
    INCLUDE_DIRS 
        "include"
        "../components/mavlink"
)

if(DEFINED ENV{TEST_BUILD} AND "$ENV{TEST_BUILD}" STREQUAL "1")
    message(STATUS "Компиляция в тестовом режиме: TEST_BUILD определен")
    add_compile_definitions(TEST_BUILD)
endif()

# Add compiler flags to handle format warnings properly and ignore warnings from MAVLink
target_compile_options(${COMPONENT_LIB} PRIVATE 
    -Wno-format 
    -Wno-address-of-packed-member  # Игнорировать предупреждения о доступе к упакованным членам
    -Wno-attributes                # Игнорировать предупреждения об атрибутах
)
