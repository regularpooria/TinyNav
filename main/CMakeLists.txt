
idf_component_register(
    SRCS
        main.cc
        main_functions.cc
        constants.cc
        output_handler.cc
        model.cc
        drive_system/drive_system.cc
    PRIV_REQUIRES
        spi_flash
    REQUIRES
        esp_driver_gpio
        esp_driver_mcpwm
        esp_adc
)