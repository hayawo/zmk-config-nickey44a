# zmk-input-processor-pad-modes

Two small trackpad helpers.

- `zmk,input-processor-layer-drag`: while `layer` is active, the first
  cursor movement of a touch presses the drag button; it stays down across
  finger lifts until the layer is released. Taps (no movement) are left alone, so tap and two-finger tap
  keep working.
- `zmk,input-processor-mod-scaler`: scales REL_X/REL_Y by `multiplier /
  divisor` while any of the explicit modifiers in `mods` is held.
- `zmk,input-processor-report-throttle`: sends at most one movement report
  per `min-interval-ms`, merging the movement in between, so BLE reports do
  not queue up and lag when the host uses a long connection interval.
