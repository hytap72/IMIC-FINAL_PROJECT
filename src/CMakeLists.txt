set(SOURCES
    main.c
    ../src/htu21d.c
    ../src/bh_1750.c
    ../src/max17043.c
    ../src/i2c_bus.c
    ../src/driver_motor.c
    ../src/ble_manager.c
    ../src/mqtt_manager.c
    ../src/aws_iot.c
    ../src/tcp_client.c
    ../src/udp_client.c
)

idf_component_register(
    SRCS ${SOURCES}
    INCLUDE_DIRS "." "../include"
    REQUIRES driver bt nvs_flash esp_wifi esp_event esp-tls mqtt json lwip esp_driver_i2c
)
