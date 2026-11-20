# zmk-input-processor-pad-modes

Two small trackpad helpers.

- `zmk,input-processor-layer-drag`: while `layer` is active, the first
  cursor movement of a touch presses the drag button; lifting the finger
  releases it. Taps (no movement) are left alone, so tap and two-finger tap
  keep working.
- `zmk,input-processor-mod-scaler`: scales REL_X/REL_Y by `multiplier /
  divisor` while any of the explicit modifiers in `mods` is held.
