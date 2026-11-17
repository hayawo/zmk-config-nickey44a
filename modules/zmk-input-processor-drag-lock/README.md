# zmk-input-processor-drag-lock

Drag lock for trackpads whose driver reports a long press as a held
button (e.g. the Azoteq TPS43 `press-and-hold` gesture).

- A button press that lasts longer than `tap-ms` is a drag. When it is
  released (finger lifted), the release is held back for `lock-ms`.
- Touching again within `lock-ms` keeps the button down, so moving the
  finger continues the drag. Repeat to drag farther than the pad allows.
- A tap while locked drops immediately; otherwise the button is released
  when `lock-ms` passes without a touch.
