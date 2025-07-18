# CMakeLists.txt para o componente sercalo_i2c_driver

idf_component_register(SRCS "sercalo_i2c.c"               # Arquivos fonte .c
                       INCLUDE_DIRS "include"             # Diretório de includes públicos
                       PRIV_INCLUDE_DIRS ""               # Diretório de includes privados (se houver)
                       REQUIRES "driver"                  # Dependências (driver I2C do ESP-IDF)
                       )