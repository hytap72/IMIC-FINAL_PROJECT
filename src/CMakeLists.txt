set(SOURCES
    main.c
    ../src/htu21d.c
    ../src/bh_1750.c
)

idf_component_register(
    SRCS main.c htu21d.c bh_1750.c max17043.c i2c_bus.c
    INCLUDE_DIRS "." "../include"
    REQUIRES driver esp_driver_i2c
)