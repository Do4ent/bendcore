/**
 * BendCore Geometry Rounding
 * Snap geometry values to standard sizes and tolerances
 * Phase 35E: Geometry rounding for standard components
 */

export class GeometryRounder {
  /**
   * Standard tube diameters (in inches)
   */
  private static readonly STANDARD_DIAMETERS = [
    0.1875, 0.25, 0.3125, 0.375, 0.5, 0.625, 0.75, 0.875,
    1.0, 1.125, 1.25, 1.375, 1.5, 1.75, 2.0, 2.5, 3.0
  ];

  /**
   * Round diameter to nearest standard
   */
  static roundDiameter(od: number): number {
    let closest = this.STANDARD_DIAMETERS[0];
    let minDiff = Math.abs(od - closest);

    for (const diameter of this.STANDARD_DIAMETERS) {
      const diff = Math.abs(od - diameter);
      if (diff < minDiff) {
        minDiff = diff;
        closest = diameter;
      }
    }

    return closest;
  }

  /**
   * Round radius to standard value
   */
  static roundRadius(radius: number): number {
    // Common radius factors
    const factors = [1, 1.5, 2, 2.5, 3, 4, 5, 6, 8, 10];

    let closest = factors[0];
    let minDiff = Math.abs(radius - closest);

    for (const factor of factors) {
      const diff = Math.abs(radius - factor);
      if (diff < minDiff) {
        minDiff = diff;
        closest = factor;
      }
    }

    return closest;
  }

  /**
   * Round angle to nearest 0.1 degrees
   */
  static roundAngle(angle: number): number {
    return Math.round(angle * 10) / 10;
  }

  /**
   * Round length to nearest 1mm (0.0394 inches)
   */
  static roundLength(length: number): number {
    const mmPerInch = 25.4;
    const lengthMm = length * mmPerInch;
    const rounded = Math.round(lengthMm);
    return rounded / mmPerInch;
  }

  /**
   * Round wall thickness to standard
   */
  static roundWallThickness(thickness: number): number {
    const standards = [0.020, 0.035, 0.049, 0.065, 0.083, 0.109, 0.120];

    let closest = standards[0];
    let minDiff = Math.abs(thickness - closest);

    for (const std of standards) {
      const diff = Math.abs(thickness - std);
      if (diff < minDiff) {
        minDiff = diff;
        closest = std;
      }
    }

    return closest;
  }

  /**
   * Check if angle is within tolerance
   */
  static isValidAngle(angle: number): boolean {
    return angle >= 0.5 && angle <= 179.5;
  }

  /**
   * Check if diameter is standard
   */
  static isStandardDiameter(od: number): boolean {
    return this.STANDARD_DIAMETERS.some(d => Math.abs(d - od) < 0.001);
  }

  /**
   * Get all standard diameters
   */
  static getStandardDiameters(): number[] {
    return [...this.STANDARD_DIAMETERS];
  }
}
