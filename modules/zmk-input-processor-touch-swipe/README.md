# Touch swipe

Optional `override-layer`, `override-left-keycode`, and `override-right-keycode`
properties select alternate keycodes while the specified layer ID is active.
The layer is checked when the swipe threshold is reached. The same touch state
is retained across layer changes, so changing layers cannot retrigger a swipe
until the finger is lifted. The default `override-layer = -1` disables this feature.

Local MIT-licensed processor, ordered after orientation and before touch-inertia.
One instance must serve one input listener. The normal Zephyr input thread
serializes callbacks; there is no delayed work or shared gesture state.
The configuration repository adds the source via its root CMakeLists.txt.
Because Zephyr 3.5 module metadata accepts only one dts_root (already used by
touch-inertia), Nickey44A also carries an identical binding mirror in its
shield's dts/bindings directory, an automatic DTS root. Keep these YAML files
in sync. The standalone module uses its own binding; do not load it twice.

TOUCH down resets only on the idle-to-touch transition. Every TOUCH up fully
resets touching, fired, both signed accumulators and total vertical travel.
HWHEEL accumulates signed horizontal motion; WHEEL accumulates signed vertical
motion and absolute vertical travel. Using total vertical travel prevents
repeated up/down scrolling from cancelling the dominance guard.
Detection requires abs(horizontal) >= threshold AND
abs(horizontal) >= total_vertical_travel * axis-ratio. After firing, no further
samples are accumulated until all fingers release, including duplicate down
events and reversals. Integer arithmetic saturates safely at int32 limits and
uses int64 for magnitude and ratio calculations.

HWHEEL always returns ZMK_INPUT_PROC_STOP, even outside touch. WHEEL, TOUCH,
cursor and button events pass unchanged. Touch-inertia code and settings are
unchanged. Snipe still scales only cursor X/Y before the common chain.

left-keycode/right-keycode are required encoded ZMK keycodes; Nickey44A sets
C_AC_BACK/C_AC_FORWARD. The same keycode-state events used by &kp produce a
press then release through ZMK's HID listener. Arbitrary behavior bindings
are not implemented. Tab switching uses Ctrl+Tab/Ctrl+Shift+Tab; the TPS43 driver
is unchanged.

## Tuning and hardware validation

Nickey44A starts with threshold=8 and axis-ratio=2. The pinned TPS43 driver
outputs only its dominant scroll axis per sample, truncated as movement *
scroll-sensitivity / 100. At sensitivity=5, eight emitted units correspond
to approximately 160 controller movement units before per-sample truncation.
This is a starting candidate, not a measured physical distance or guarantee.
The processor can observe only the selected, transformed axis; discarded
diagonal components and future vertical motion cannot inform an earlier fire.
Very slow movements can truncate to zero. Confirm direction on hardware.

Edit threshold and axis-ratio in nickey44a_r.overlay and rebuild the right
firmware. Try threshold 12/16 for accidental fires, or 4/6 for shorter swipes;
axis-ratio 3 requires stronger horizontal dominance. Vertical-heavy touches
may need release before a horizontal gesture, deliberately.

Check left/right once per touch, same-direction continuation, reversal,
duplicate down, complete release/re-touch, tap/right-click, single-finger
cursor/tap/hold, repeated vertical scrolling and inertia, layer-1 Snipe,
deep sleep/wake, and USB/BLE Consumer reports in the target browser.
Build success does not establish physical gesture feel or sleep behavior.
