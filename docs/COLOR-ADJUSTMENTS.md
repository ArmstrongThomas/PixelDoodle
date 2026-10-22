# Hue, Saturation and Brightness Adjustment

Open **Menu > Adjust** to shift colors without choosing a new foreground color.
The page provides three signed controls:

- **Hue**: -180 to +180 degrees.
- **Saturation**: -100% to +100%, added to the current HSV saturation.
- **Brightness**: -100% to +100%, added to the current HSV value.

Tap the minus and plus buttons for five-point steps, or tap and drag either
track for a precise value. The top screen shows side-by-side **Before** and
**After** views of the actual layer stack, including layer order, opacity and
blend modes. While dragging, After retains the last complete preview and is
marked as adjusting; lifting the stylus refreshes it. No layer or undo data is
changed until Apply.

## Scope

- **Selection** changes only pixels inside the current selection. The selection
  is bound to the current layer; missing or stale selections are refused.
- **Current** changes every nontransparent pixel on the active layer.
- **Visible** changes every nontransparent pixel on every enabled visible
  layer independently.

Visible-layer adjustment preserves layer order, names, visibility, opacity,
blend modes, alpha bytes and transparency. It does not flatten the image.
Locked target layers are refused. Alpha Lock does not block these controls
because the operation changes RGB and preserves alpha.

The entire operation is one undo transaction, including when Visible changes
multiple layers. If the changed pixels cannot fit together in the history
budget, the operation is refused transactionally and leaves every layer
unchanged.
