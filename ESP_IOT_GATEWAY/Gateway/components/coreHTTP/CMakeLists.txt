file(GLOB component_srcs 
    "source/*.c" 
    "source/dependency/3rdparty/llhttp/src/*.c"
)

idf_component_register(
    SRCS "${component_srcs}"
    INCLUDE_DIRS 
        "source/include"
        "source/interface"
        "source/dependency/3rdparty/llhttp/include"
)

target_compile_definitions(${COMPONENT_LIB} PUBLIC CORE_HTTP_DO_NOT_USE_CUSTOM_CONFIG=1)