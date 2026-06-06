idf_component_register(
    SRCS "main.c" "../src/htu21d.c"
    INCLUDE_DIRS "." "../include"
    REQUIRES driver
)