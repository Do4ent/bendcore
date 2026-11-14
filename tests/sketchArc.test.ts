import { arcFromEndpoints, hitTestArc } from '../src/ui/sketchArc';

// Reference values: SVG 1.1, appendix F.6.5 worked by hand.
describe('arcFromEndpoints', () => {
  it('quarter arc (100,0)->(0,100), r=100: centre selected by the flags', () => {
    const a = arcFromEndpoints(100, 0, 0, 100, 100, false, true)!;
    expect(a.cx).toBeCloseTo(0, 12);
    expect(a.cy).toBeCloseTo(0, 12);
    expect(a.r).toBeCloseTo(100, 12);
    expect(a.start).toBeCloseTo(0, 12);
    expect(a.delta).toBeCloseTo(Math.PI / 2, 12);

    const b = arcFromEndpoints(100, 0, 0, 100, 100, false, false)!;
    expect(b.cx).toBeCloseTo(100, 12);
    expect(b.cy).toBeCloseTo(100, 12);
    expect(b.delta).toBeCloseTo(-Math.PI / 2, 12);

    const c = arcFromEndpoints(100, 0, 0, 100, 100, true, true)!;
    expect(c.cx).toBeCloseTo(100, 12);
    expect(c.cy).toBeCloseTo(100, 12);
    expect(c.delta).toBeCloseTo((3 * Math.PI) / 2, 12);
  });

  it('radius smaller than half the chord is scaled up to a semicircle', () => {
    const a = arcFromEndpoints(0, 0, 100, 0, 10, false, true)!;
    expect(a.r).toBeCloseTo(50, 12);
    expect(a.cx).toBeCloseTo(50, 12);
    expect(a.cy).toBeCloseTo(0, 12);
    expect(Math.abs(a.delta)).toBeCloseTo(Math.PI, 12);
  });

  it('coincident endpoints or zero radius give no arc', () => {
    expect(arcFromEndpoints(5, 5, 5, 5, 10, false, true)).toBeNull();
    expect(arcFromEndpoints(0, 0, 10, 0, 0, false, true)).toBeNull();
  });
});

describe('hitTestArc', () => {
  const quarter = arcFromEndpoints(100, 0, 0, 100, 100, false, true)!;
  const s = 100 / Math.SQRT2;

  it('hits points on the arc and its endpoints', () => {
    expect(hitTestArc(s, s, quarter, 5)).toBe(true);
    expect(hitTestArc(100, 0, quarter, 5)).toBe(true);
    expect(hitTestArc(0, 100, quarter, 5)).toBe(true);
    expect(hitTestArc(s + 3, s, quarter, 5)).toBe(true);
  });

  it('misses the rest of the circle and points off the radius', () => {
    expect(hitTestArc(-s, -s, quarter, 5)).toBe(false);
    expect(hitTestArc(-100, 0, quarter, 5)).toBe(false);
    expect(hitTestArc(s * 0.8, s * 0.8, quarter, 5)).toBe(false);
  });

  it('a clockwise arc covers the complementary side', () => {
    const cw = arcFromEndpoints(100, 0, 0, 100, 100, true, false)!; // centre (0,0), 270 deg
    expect(cw.cx).toBeCloseTo(0, 12);
    expect(hitTestArc(-s, -s, cw, 5)).toBe(true);
    expect(hitTestArc(s, s, cw, 5)).toBe(false);
  });
});
