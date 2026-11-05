# zmk-input-processor-pinch-zoom

Turns a trackpad pinch (zoom) axis into Ctrl + mouse wheel, so pinch-to-zoom
works in browsers, Office, Explorer and most Windows apps.

- The zoom axis (`zoom-code`, e.g. `INPUT_REL_MISC` from the Azoteq TPS43
  driver) is accumulated; every `divisor` units become one wheel step.
- The modifier (`modifier`, default Left Ctrl) is pressed just before the first
  wheel step and released when the finger lifts (`touch-code` = 0) or after
  `release-ms` without zoom input.
- Place it last in the listener's processor chain so scroll inertia never sees
  the generated wheel events.
