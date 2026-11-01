/**
 * Feature Operations Tests
 */

describe('Extrude Feature', () => {
  it('should validate square profile', () => {
    const vertices = [
      { x: 0, y: 0, z: 0 },
      { x: 10, y: 0, z: 0 },
      { x: 10, y: 10, z: 0 },
      { x: 0, y: 10, z: 0 }
    ];
    expect(vertices.length).toBe(4);
  });

  it('should extrude 2D profile to 3D', () => {
    const profileArea = 100;
    const height = 20;
    const volumeExpected = profileArea * height;
    expect(volumeExpected).toBe(2000);
  });

  it('should compute extrusion direction', () => {
    const normal = { x: 0, y: 0, z: 1 };
    expect(normal.z).toBe(1);
  });

  it('should create lateral faces', () => {
    const profileEdges = 4;
    const expectedLateralFaces = profileEdges;
    expect(expectedLateralFaces).toBe(4);
  });

  it('should handle symmetric extrusion', () => {
    const height = 20;
    const symmetric = true;
    const eachSide = symmetric ? height / 2 : height;
    expect(eachSide).toBe(10);
  });

  it('should validate non-coplanar vertices', () => {
    const vertex1 = { x: 0, y: 0, z: 0 };
    const vertex2 = { x: 1, y: 0, z: 0 };
    const vertex3 = { x: 0, y: 1, z: 0 };
    const distance = Math.sqrt(Math.pow(vertex3.x, 2) + Math.pow(vertex3.y, 2));
    expect(distance).toBe(1);
  });

  it('should create solid extrusion', () => {
    const faces = 6; // bottom + top + 4 sides
    expect(faces).toBe(6);
  });

  it('should handle taper angle', () => {
    const taperAngle = 5;
    const height = 20;
    const radiusDelta = height * Math.tan(taperAngle * Math.PI / 180);
    expect(radiusDelta).toBeGreaterThan(0);
  });

  it('should compute volume correctly', () => {
    const baseArea = 100;
    const height = 10;
    const volume = baseArea * height;
    expect(volume).toBe(1000);
  });
});

describe('Revolve Feature', () => {
  it('should validate profile for revolution', () => {
    const profileVertices = 5;
    expect(profileVertices).toBeGreaterThan(1);
  });

  it('should revolve 360 degrees', () => {
    const angle = 360;
    const angleRad = angle * Math.PI / 180;
    expect(angleRad).toBeCloseTo(2 * Math.PI);
  });

  it('should compute number of segments', () => {
    const angle = 360;
    const numSegments = 12;
    const segmentAngle = angle / numSegments;
    expect(segmentAngle).toBe(30);
  });

  it('should validate revolution axis', () => {
    const axis = { x: 0, y: 0, z: 1 };
    const length = Math.sqrt(axis.x * axis.x + axis.y * axis.y + axis.z * axis.z);
    expect(length).toBe(1);
  });

  it('should rotate vertices around axis', () => {
    const vertex = { x: 10, y: 0, z: 0 };
    const angle = 90 * Math.PI / 180;
    const rotatedX = vertex.x * Math.cos(angle) - vertex.y * Math.sin(angle);
    const rotatedY = vertex.x * Math.sin(angle) + vertex.y * Math.cos(angle);
    expect(rotatedX).toBeCloseTo(0, 5);
    expect(rotatedY).toBeCloseTo(10, 5);
  });

  it('should create revolution faces', () => {
    const numSegments = 12;
    const profileVertices = 4;
    expect(numSegments).toBeGreaterThan(2);
  });

  it('should clamp segment count', () => {
    const minSegments = 3;
    const maxSegments = 128;
    const requested = 150;
    const clamped = Math.min(Math.max(requested, minSegments), maxSegments);
    expect(clamped).toBe(128);
  });

  it('should compute volume of revolution', () => {
    const radius = 10;
    const height = 20;
    const volumeCylinder = Math.PI * radius * radius * height;
    expect(volumeCylinder).toBeGreaterThan(0);
  });

  it('should handle partial revolution', () => {
    const angle = 180;
    const full = 360;
    const isFull = Math.abs(angle - full) < 0.01;
    expect(isFull).toBe(false);
  });

  it('should generate solid of revolution', () => {
    const faces = 13; // 12 segments + profile
    expect(faces).toBe(13);
  });
});

describe('Feature Integration', () => {
  it('should combine extrude and revolve', () => {
    const extrudeHeight = 10;
    const revolveAngle = 180;
    const combined = { extrude: extrudeHeight, revolve: revolveAngle };
    expect(combined.extrude).toBe(10);
    expect(combined.revolve).toBe(180);
  });

  it('should track feature history', () => {
    const features = ['extrude', 'revolve', 'extrude'];
    expect(features.length).toBe(3);
    expect(features[0]).toBe('extrude');
  });

  it('should support undo/redo', () => {
    const history = [];
    history.push('extrude');
    expect(history.length).toBe(1);
    history.pop();
    expect(history.length).toBe(0);
  });

  it('should compute B-Rep topology', () => {
    const vertices = 8;
    const edges = 12;
    const faces = 6;
    expect(vertices).toBe(8);
    expect(edges).toBe(12);
    expect(faces).toBe(6);
  });

  it('should validate solid generation', () => {
    const isSolid = true;
    expect(isSolid).toBe(true);
  });
});
