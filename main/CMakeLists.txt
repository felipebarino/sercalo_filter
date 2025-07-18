idf_component_register(SRCS "main.c"
                    PRIV_REQUIRES spi_flash
                    INCLUDE_DIRS "."
                    REQUIRES driver sercalo_i2c_driver)
