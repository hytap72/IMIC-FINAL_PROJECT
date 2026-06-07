set(SOURCES
    main.c
    ../src/htu21d.c
    ../src/bh_1750.c
    ../src/max17043.c
    ../src/driver_motor.c
    ../src/ble_manager.c
)

idf_component_register(
    SRCS ${SOURCES}
    INCLUDE_DIRS "." "../include"
    REQUIRES driver bt nvs_flash esp_wifi esp_event
)