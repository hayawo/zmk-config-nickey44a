# Bundled until the standalone touch-inertia module is published and SHA-pinned.
add_subdirectory(modules/zmk-input-processor-touch-inertia)
add_subdirectory(modules/zmk-input-processor-touch-swipe)
add_subdirectory(modules/zmk-input-processor-pinch-zoom)

# Test-only wiring diagnostic (enabled by the nickey44a_r_pinprobe build).
if(NICKEY_PIN_PROBE)
  target_sources(app PRIVATE ${CMAKE_CURRENT_LIST_DIR}/src/tps43_pin_probe.c)
endif()
