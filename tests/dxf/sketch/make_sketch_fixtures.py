"""DXF fixtures for the DXF import into a sketch (tests/wasm/dxf_sketch.test.mjs,
docs/ui/SKETCH_EDITOR.md, "Импорт DXF").

The files are written by ezdxf (MIT), independently of the bendcore reader and writer:

    python3 tests/dxf/sketch/make_sketch_fixtures.py tests/dxf/sketch

The expected areas and volumes are formulas in the node test, not values read back from here.

plate_inch.dxf  R2000, $INSUNITS = 1 (inch). Layer CONTOUR: 4 x 2 in rectangle drawn as four
                separate LINE entities, one reversed, end points 2e-5 in (0.000508 mm) apart.
                Layer HOLES: circle r = 0.25 in at (1, 1). Layer DIMS: a line and a text.
                Layer FROZEN (frozen): a line. Paper space: a line on CONTOUR.
slot_mm.dxf     R2018, $INSUNITS = 4 (mm). SLOT: closed LWPOLYLINE stadium 50 x 20 with two
                bulge-1 ends, counter-clockwise. SLOT_CW: the same stadium at y = 100 drawn
                clockwise (bulges -1). ROUNDED: 60 x 40 rectangle with R5 corners from LINE and
                ARC entities at x = 100. CURVES: a SPLINE and an ELLIPSE (not read by bendcore_dxf).
outline_r12.dxf R12 (no $INSUNITS), code page ANSI_1251. Layer "КОНТУР": closed POLYLINE square
                30 x 30 whose left side is an arc with bulge 0.5 (bulging outward).
"""
import os
import sys

import ezdxf

out = sys.argv[1] if len(sys.argv) > 1 else os.path.dirname(os.path.abspath(__file__))

# --- plate_inch.dxf
doc = ezdxf.new("R2000")
doc.units = ezdxf.units.IN
doc.header["$MEASUREMENT"] = 0
for name, color in (("CONTOUR", 7), ("HOLES", 3), ("DIMS", 2), ("FROZEN", 1)):
    doc.layers.add(name, color=color)
doc.layers.get("FROZEN").freeze()
msp = doc.modelspace()
g = 2e-5
msp.add_line((0, 0), (4, 0), dxfattribs={"layer": "CONTOUR"})
msp.add_line((4 + g, 0), (4, 2 - g), dxfattribs={"layer": "CONTOUR"})
msp.add_line((0, 2), (4, 2), dxfattribs={"layer": "CONTOUR"})  # reversed direction
msp.add_line((0, 2), (g, 0), dxfattribs={"layer": "CONTOUR"})
msp.add_circle((1, 1), 0.25, dxfattribs={"layer": "HOLES"})
msp.add_line((0, -0.5), (4, -0.5), dxfattribs={"layer": "DIMS"})
msp.add_text("4.000", dxfattribs={"layer": "DIMS", "height": 0.1, "insert": (2, -0.4)})
msp.add_line((0, 0), (4, 2), dxfattribs={"layer": "FROZEN"})
doc.layout("Layout1").add_line((0, 0), (10, 10), dxfattribs={"layer": "CONTOUR"})
doc.saveas(os.path.join(out, "plate_inch.dxf"))

# --- slot_mm.dxf
doc = ezdxf.new("R2018")
doc.units = ezdxf.units.MM
for name in ("SLOT", "SLOT_CW", "ROUNDED", "CURVES"):
    doc.layers.add(name)
msp = doc.modelspace()
msp.add_lwpolyline([(0, 0, 0.0), (50, 0, 1.0), (50, 20, 0.0), (0, 20, 1.0)], format="xyb", close=True,
                   dxfattribs={"layer": "SLOT"})
msp.add_lwpolyline([(0, 120, 0.0), (50, 120, -1.0), (50, 100, 0.0), (0, 100, -1.0)], format="xyb", close=True,
                   dxfattribs={"layer": "SLOT_CW"})
x0, y0, w, h, r = 100.0, 0.0, 60.0, 40.0, 5.0
a = {"layer": "ROUNDED"}
msp.add_line((x0 + r, y0), (x0 + w - r, y0), dxfattribs=a)
msp.add_line((x0 + w, y0 + r), (x0 + w, y0 + h - r), dxfattribs=a)
msp.add_line((x0 + w - r, y0 + h), (x0 + r, y0 + h), dxfattribs=a)
msp.add_line((x0, y0 + h - r), (x0, y0 + r), dxfattribs=a)
msp.add_arc((x0 + w - r, y0 + r), r, 270, 0, dxfattribs=a)
msp.add_arc((x0 + w - r, y0 + h - r), r, 0, 90, dxfattribs=a)
msp.add_arc((x0 + r, y0 + h - r), r, 90, 180, dxfattribs=a)
msp.add_arc((x0 + r, y0 + r), r, 180, 270, dxfattribs=a)
msp.add_spline([(0, 200), (10, 210), (20, 200), (30, 215)], dxfattribs={"layer": "CURVES"})
msp.add_ellipse((50, 200), major_axis=(10, 0), ratio=0.5, dxfattribs={"layer": "CURVES"})
doc.saveas(os.path.join(out, "slot_mm.dxf"))

# --- outline_r12.dxf
doc = ezdxf.new("R12")
doc.encoding = "cp1251"
doc.layers.add("КОНТУР", color=5)
msp = doc.modelspace()
msp.add_polyline2d([(0, 0, 0, 0, 0.0), (30, 0, 0, 0, 0.0), (30, 30, 0, 0, 0.0), (0, 30, 0, 0, 0.5)],
                   format="xyseb", close=True, dxfattribs={"layer": "КОНТУР"})
doc.saveas(os.path.join(out, "outline_r12.dxf"))
if "$INSUNITS" in ezdxf.readfile(os.path.join(out, "outline_r12.dxf")).header:
    raise SystemExit("outline_r12.dxf must not have $INSUNITS")
