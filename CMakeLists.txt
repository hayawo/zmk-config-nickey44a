# Bundled until the standalone touch-inertia module is published and SHA-pinned.
add_subdirectory(modules/zmk-input-processor-touch-inertia)
add_subdirectory(modules/zmk-input-processor-touch-swipe)
add_subdirectory(modules/zmk-input-processor-pinch-zoom)
add_subdirectory(modules/zmk-input-processor-pad-modes)

# zmk-driver-hires-dial builds its behaviors as a separate Zephyr library and
# expects ZMK's headers (drivers/behavior.h, zmk/*.h) on the global include
# path, as on ZMK main. ZMK v0.3 keeps them private to the app target.
zephyr_include_directories(${APPLICATION_SOURCE_DIR}/include)
