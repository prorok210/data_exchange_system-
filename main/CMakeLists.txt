# Register component with specific sources and include directories
idf_component_register(
    SRCS 
        "main.c" 
        "event_handler.c"
        "mesh_light.c"
        "msg_handler.c"
    INCLUDE_DIRS 
        "include"
)

# Add specific compile options if needed
# target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-unused-variable)
