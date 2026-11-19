# 0015. Outputs: parts list, panel printout, generated single-line diagram

- **Status:** Accepted
- **Date:** 2026-11-19

## Context

A finished design must be usable for a real installation: buying parts and
handing the design to an electrician. Electricians and shops in the CIS work
from a single-line diagram (однолинейная схема). No prices are shown
anywhere (product decision).

## Decision

- **Parts list:** generated in core from the model as rows
  `{designation, type, parameters, brand/model text, module width,
  quantity}`, grouped by identical items, plus wire totals per cross-section
  and color (meters, from wire lengths) and the enclosure. Shown in a
  dockable table; exported to **PDF** and **CSV** (UTF-8 with BOM so Excel
  opens Cyrillic correctly). No price column.
- **Designations** are assigned automatically per CIS convention (QF1… for
  breakers, QD1… for RCDs, QFD1… for RCBOs, QS for switch-disconnectors) and
  editable in engineer mode. They are shown on the panel picture, the parts
  list and the diagram.
- **Panel printout:** the canvas rendered via `QGraphicsScene::render` to
  **PNG** and **PDF**, with a title block (project name, date).
- **Single-line diagram:** core traces the wiring from the supply through
  devices to loads and builds a tree of
  `{device, rating, designation, outgoing line: wire section, length,
  connected loads, total power}`. The app draws it with a fixed layout
  (supply on top, main devices, then one column per outgoing line) using
  graphic symbols in the style of ГОСТ 2.755 / ГОСТ 2.710 and a table row
  under each line (line purpose, section, power, current). Exported to PDF
  and PNG. Strict standard compliance (frames, stamps per ГОСТ 21.613) is not
  a goal.
- If the wiring is not a tree (meshes, a load fed from two breakers, an
  unconnected device), the diagram is still drawn for the traceable part and
  lists the problem items in a warning above it.
- PDF output via `QPdfWriter`; printing via `QPrinter` / `QPrintDialog` with
  the same painting code.

## Rejected alternatives

- **Hand-drawn single-line diagram editor** — duplicates the work already
  done on the panel and can contradict it.
- **Export to DXF/DWG** — useful for designers, but a new format and likely
  dependency; can be an ADR later.
- **Excel `.xlsx` export** — needs a library; CSV opens in Excel.

## Consequences

- Wire lengths matter for the parts list, so the load-line default (15 m)
  is visible and editable in the basic properties of a wire to a load.
- Tracing code is shared by the diagram, the parts list grouping by line
  and tutorial checks.
