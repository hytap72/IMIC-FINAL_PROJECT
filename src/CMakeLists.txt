set(SOURCES
    main.c
    ../src/htu21d.c
    ../src/bh_1750.c
)

idf_component_register(
    SRCS ${SOURCES}
    INCLUDE_DIRS "." "../include"
    REQUIRES driver
)