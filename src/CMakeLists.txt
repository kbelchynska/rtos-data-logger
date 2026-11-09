# Усі .c з src/ (PlatformIO і idf.py збирають той самий код)
FILE(GLOB_RECURSE app_sources ${CMAKE_CURRENT_LIST_DIR}/*.c)

idf_component_register(SRCS ${app_sources}
                       INCLUDE_DIRS ".")
