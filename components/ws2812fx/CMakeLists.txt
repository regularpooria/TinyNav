idf_component_register(SRCS "src/modes_funcs.cpp" "src/modes.cpp" "src/WS2812FX.cpp"
  INCLUDE_DIRS "include"
  REQUIRES esp_timer led_strip
)
