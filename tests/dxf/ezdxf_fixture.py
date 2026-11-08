"""Независимая фикстура для tests/dxf/test_dxf_reader_roundtrip.cpp.

Запуск: python ezdxf_fixture.py <версия DXF> <выходной .dxf> <файл ожиданий>.
Файл DXF пишет ezdxf (MIT); ожидания получаются повторным чтением этого файла ezdxf и
функцией ezdxf.math.bulge_to_arc, т. е. независимо от читателя bendcore.
Формат ожиданий: одна запись на строку, поля через пробел; строки текста — в hex UTF-8.
"""
import math
import sys

import ezdxf
from ezdxf.math import bulge_to_arc
from ezdxf.tools.text import plain_mtext, plain_text

version, out_path, expect_path = sys.argv[1], sys.argv[2], sys.argv[3]

doc = ezdxf.new(version)
doc.units = ezdxf.units.IN if version == "R2000" else ezdxf.units.MM
if version == "R12":
    doc.encoding = "cp1251"
doc.layers.add("GEOM", color=3)
doc.layers.add("Оси" if version != "R12" else "AXIS", color=1)
msp = doc.modelspace()

msp.add_line((1.25, -3.5), (101.125, 47.0625), dxfattribs={"layer": "GEOM"})
msp.add_line((0, 0), (-12.5, 7.75), dxfattribs={"color": 5})
msp.add_circle((50, 25), 12.345678, dxfattribs={"layer": "GEOM"})
msp.add_arc((10, 20), 7.5, 30, 300)
msp.add_arc((-5, 5), 2.0, 350, 10)
msp.add_polyline2d([(0, 0, 0, 0, 0.5), (10, 0, 0, 0, 0), (10, 5, 0, 0, -1.0)],
                   format="xyseb", close=True)
msp.add_text("Деталь Ø20 %%c5", dxfattribs={"height": 3.5, "rotation": 15.0,
                                             "insert": (5, 6), "style": "Standard"})
if version != "R12":
    msp.add_lwpolyline([(0, 0, 0.0), (20, 0, 1.0), (20, 10, -0.41421356237309503), (0, 10, 0.0)],
                       format="xyb", close=True, dxfattribs={"layer": "GEOM"})
    msp.add_lwpolyline([(1, 1), (2, 3), (4, 1)], format="xy")
    mt = msp.add_mtext("{\\fArial|b1;Технические требования}\\P1. Размеры для справок\\P2. Ra 6,3 мкм",
                       dxfattribs={"char_height": 2.5, "insert": (30, 40), "attachment_point": 1})
    mt.dxf.rotation = 90.0
doc.saveas(out_path)

# Ожидания — по повторному чтению ezdxf.
rd = ezdxf.readfile(out_path)
m = rd.modelspace()
lines = []
hx = lambda s: ezdxf.decode_dxf_unicode(s).encode("utf-8").hex() or "-"
f = lambda v: repr(float(v))
lines.append(f"VERSION {rd.dxfversion}")
lines.append(f"INSUNITS {rd.header.get('$INSUNITS', 0)}")
for layer in rd.layers:
    lines.append(f"LAYER {hx(layer.dxf.name)} {layer.dxf.color}")
for e in m:
    t = e.dxftype()
    if t == "LINE":
        s, n = e.dxf.start, e.dxf.end
        lines.append(f"LINE {s.x!r} {s.y!r} {n.x!r} {n.y!r} {hx(e.dxf.layer)} {e.dxf.color}")
    elif t == "CIRCLE":
        c = e.dxf.center
        lines.append(f"CIRCLE {c.x!r} {c.y!r} {e.dxf.radius!r}")
    elif t == "ARC":
        c = e.dxf.center
        lines.append(f"ARC {c.x!r} {c.y!r} {e.dxf.radius!r} {e.dxf.start_angle % 360.0!r} {e.dxf.end_angle % 360.0!r}")
    elif t in ("LWPOLYLINE", "POLYLINE"):
        if t == "LWPOLYLINE":
            pts = [(p[0], p[1], p[4]) for p in e.get_points("xyseb")]
            closed = e.closed
        else:
            pts = [(v.dxf.location.x, v.dxf.location.y, v.dxf.bulge) for v in e.vertices]
            closed = e.is_closed
        lines.append(f"POLY {int(t == 'LWPOLYLINE')} {int(closed)} {len(pts)} " +
                     " ".join(f"{f(x)} {f(y)} {f(b)}" for x, y, b in pts))
        n = len(pts)
        for i, (x, y, b) in enumerate(pts):
            if b == 0 or (i == n - 1 and not closed):
                continue
            x2, y2, _ = pts[(i + 1) % n]
            c, a0, a1, r = bulge_to_arc((x, y), (x2, y2), b)
            lines.append(f"BULGE {f(c.x)} {f(c.y)} {f(r)}")
    elif t == "TEXT":
        p = e.dxf.insert
        lines.append(f"TEXT {hx(plain_text(ezdxf.decode_dxf_unicode(e.dxf.text)))} {p.x!r} {p.y!r} {e.dxf.height!r} {e.dxf.rotation!r}")
    elif t == "MTEXT":
        p = e.dxf.insert
        lines.append(f"MTEXT {hx(plain_mtext(ezdxf.decode_dxf_unicode(e.text)))} {p.x!r} {p.y!r} {e.dxf.char_height!r} {e.get_rotation()!r}")
with open(expect_path, "w", encoding="ascii") as f:
    f.write("\n".join(lines) + "\n")
