add_executable(main
        main.c
)

target_link_libraries(main pico_stdlib hardware_adc oled1_lib freertos)
pico_add_extra_outputs(main)
