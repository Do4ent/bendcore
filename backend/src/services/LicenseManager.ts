/**
 * BendCore License Manager
 * Handles licensing, feature access control, and usage quotas
 */

/**
 * License types with pricing
 */
export enum LicenseType {
  FREE = 'free',
  STARTER = 'starter', // $29/месяц
  PROFESSIONAL = 'professional', // $99/месяц
  ENTERPRISE = 'enterprise', // $499+/месяц (custom)
}

/**
 * Feature access matrix
 */
export interface LicenseFeatures {
  // Projects and workspace
  maxProjects: number; // -1 = unlimited
  maxUsers: number; // -1 = unlimited
  maxStorage: number; // GB, -1 = unlimited
  cloudStorage: number; // GB, -1 = unlimited

  // Core features
  hasAdvancedFEA: boolean;
  hasSheetMetal: boolean;
  hasPipeRouting: boolean;
  has3DVisualization: boolean;

  // Plugin ecosystem
  hasPlugins: boolean;
  hasCustomPlugins: boolean;
  maxCustomPlugins: number;

  // API and integration
  hasAPI: boolean;
  hasRESTAPI: boolean;
  hasWebhooks: boolean;
  hasSSO: boolean;

  // Support level
  supportLevel: 'community' | 'email' | 'priority' | '24/7';
  supportResponseTime: number; // hours

  // Collaborative features
  hasCollaboration: boolean;
  maxTeamMembers: number;
  hasVersionControl: boolean;

  // Advanced features
  hasAdvancedSimulation: boolean; // FEA with nonlinear analysis
  hasManufacturingOptimization: boolean;
  hasAIAssistant: boolean;

  // Compliance
  hasAuditLog: boolean;
  hasCompliance: boolean; // ISO, FDA, etc.
  hasDataResidency: boolean;
}

/**
 * Pricing configuration
 */
export const LICENSE_PRICING: Record<LicenseType, {
  monthlyPrice: number;
  annualPrice: number;
  discountPercent: number;
}> = {
  [LicenseType.FREE]: {
    monthlyPrice: 0,
    annualPrice: 0,
    discountPercent: 0,
  },
  [LicenseType.STARTER]: {
    monthlyPrice: 29,
    annualPrice: 290,
    discountPercent: 17, // Save 2 months per year
  },
  [LicenseType.PROFESSIONAL]: {
    monthlyPrice: 99,
    annualPrice: 990,
    discountPercent: 17,
  },
  [LicenseType.ENTERPRISE]: {
    monthlyPrice: 499,
    annualPrice: 4990,
    discountPercent: 17,
  },
};

/**
 * Feature matrix by license type
 */
export const LICENSE_FEATURES: Record<LicenseType, LicenseFeatures> = {
  [LicenseType.FREE]: {
    maxProjects: 5,
    maxUsers: 1,
    maxStorage: 1, // 1 GB
    cloudStorage: 1,
    hasAdvancedFEA: false,
    hasSheetMetal: false,
    hasPipeRouting: false,
    has3DVisualization: true,
    hasPlugins: false,
    hasCustomPlugins: false,
    maxCustomPlugins: 0,
    hasAPI: false,
    hasRESTAPI: false,
    hasWebhooks: false,
    hasSSO: false,
    supportLevel: 'community',
    supportResponseTime: 48,
    hasCollaboration: false,
    maxTeamMembers: 1,
    hasVersionControl: false,
    hasAdvancedSimulation: false,
    hasManufacturingOptimization: false,
    hasAIAssistant: false,
    hasAuditLog: false,
    hasCompliance: false,
    hasDataResidency: false,
  },

  [LicenseType.STARTER]: {
    maxProjects: 50,
    maxUsers: 3,
    maxStorage: 50,
    cloudStorage: 50,
    hasAdvancedFEA: true,
    hasSheetMetal: true,
    hasPipeRouting: true,
    has3DVisualization: true,
    hasPlugins: true,
    hasCustomPlugins: false,
    maxCustomPlugins: 0,
    hasAPI: false,
    hasRESTAPI: false,
    hasWebhooks: false,
    hasSSO: false,
    supportLevel: 'email',
    supportResponseTime: 24,
    hasCollaboration: true,
    maxTeamMembers: 3,
    hasVersionControl: true,
    hasAdvancedSimulation: false,
    hasManufacturingOptimization: false,
    hasAIAssistant: false,
    hasAuditLog: false,
    hasCompliance: false,
    hasDataResidency: false,
  },

  [LicenseType.PROFESSIONAL]: {
    maxProjects: -1,
    maxUsers: 10,
    maxStorage: 500,
    cloudStorage: 500,
    hasAdvancedFEA: true,
    hasSheetMetal: true,
    hasPipeRouting: true,
    has3DVisualization: true,
    hasPlugins: true,
    hasCustomPlugins: true,
    maxCustomPlugins: 5,
    hasAPI: true,
    hasRESTAPI: true,
    hasWebhooks: true,
    hasSSO: false,
    supportLevel: 'priority',
    supportResponseTime: 8,
    hasCollaboration: true,
    maxTeamMembers: 10,
    hasVersionControl: true,
    hasAdvancedSimulation: true,
    hasManufacturingOptimization: true,
    hasAIAssistant: false,
    hasAuditLog: true,
    hasCompliance: false,
    hasDataResidency: false,
  },

  [LicenseType.ENTERPRISE]: {
    maxProjects: -1,
    maxUsers: -1,
    maxStorage: -1,
    cloudStorage: -1,
    hasAdvancedFEA: true,
    hasSheetMetal: true,
    hasPipeRouting: true,
    has3DVisualization: true,
    hasPlugins: true,
    hasCustomPlugins: true,
    maxCustomPlugins: -1,
    hasAPI: true,
    hasRESTAPI: true,
    hasWebhooks: true,
    hasSSO: true,
    supportLevel: '24/7',
    supportResponseTime: 1,
    hasCollaboration: true,
    maxTeamMembers: -1,
    hasVersionControl: true,
    hasAdvancedSimulation: true,
    hasManufacturingOptimization: true,
    hasAIAssistant: true,
    hasAuditLog: true,
    hasCompliance: true,
    hasDataResidency: true,
  },
};

/**
 * User license information
 */
export interface UserLicense {
  userId: string;
  licenseType: LicenseType;
  expiresAt: Date;
  isValid: boolean;
  autoRenew: boolean;
  billingCycle: 'monthly' | 'annual';
  paymentMethodId?: string;
  nextChargeDate?: Date;
}

/**
 * License validation result
 */
export interface LicenseValidation {
  valid: boolean;
  reason?: string;
  license: UserLicense;
  features: LicenseFeatures;
}

/**
 * License manager
 */
export class LicenseManager {
  /**
   * Get features for a license type
   */
  static getFeatures(licenseType: LicenseType): LicenseFeatures {
    return LICENSE_FEATURES[licenseType];
  }

  /**
   * Check if license is valid (not expired)
   */
  static isLicenseValid(license: UserLicense): boolean {
    if (!license.isValid) {
      return false;
    }
    return new Date() < license.expiresAt;
  }

  /**
   * Validate license and return features
   */
  static validateLicense(license: UserLicense): LicenseValidation {
    const valid = this.isLicenseValid(license);
    return {
      valid,
      reason: valid ? undefined : 'License expired',
      license,
      features: this.getFeatures(license.licenseType),
    };
  }

  /**
   * Check if user has access to a specific feature
   */
  static hasFeature(features: LicenseFeatures, feature: keyof LicenseFeatures): boolean {
    const value = features[feature];
    if (typeof value === 'boolean') {
      return value;
    }
    // For numeric features, any positive or -1 (unlimited) means available
    if (typeof value === 'number') {
      return value !== 0;
    }
    return false;
  }

  /**
   * Check quota usage
   */
  static checkQuota(
    features: LicenseFeatures,
    quotaName: keyof LicenseFeatures,
    currentUsage: number
  ): { valid: boolean; remaining: number } {
    const limit = features[quotaName] as number | undefined;

    if (!limit) {
      return { valid: false, remaining: 0 };
    }

    if (limit === -1) {
      // Unlimited
      return { valid: true, remaining: -1 };
    }

    const remaining = limit - currentUsage;
    return {
      valid: remaining > 0,
      remaining: Math.max(0, remaining),
    };
  }

  /**
   * Get license tier recommendations based on usage
   */
  static recommendLicense(usage: {
    projectCount: number;
    userCount: number;
    storageGb: number;
    needsAPI: boolean;
    needsCustomPlugins: boolean;
  }): LicenseType {
    // Free tier
    if (
      usage.projectCount <= 5 &&
      usage.userCount === 1 &&
      usage.storageGb <= 1 &&
      !usage.needsAPI &&
      !usage.needsCustomPlugins
    ) {
      return LicenseType.FREE;
    }

    // Starter tier
    if (
      usage.projectCount <= 50 &&
      usage.userCount <= 3 &&
      usage.storageGb <= 50 &&
      !usage.needsAPI &&
      !usage.needsCustomPlugins
    ) {
      return LicenseType.STARTER;
    }

    // Professional tier
    if (
      usage.projectCount <= 500 &&
      usage.userCount <= 10 &&
      usage.storageGb <= 500 &&
      !usage.needsCustomPlugins
    ) {
      return LicenseType.PROFESSIONAL;
    }

    // Enterprise
    return LicenseType.ENTERPRISE;
  }

  /**
   * Calculate effective price with discount
   */
  static calculatePrice(
    licenseType: LicenseType,
    billingCycle: 'monthly' | 'annual'
  ): { basePrice: number; discount: number; finalPrice: number } {
    const pricing = LICENSE_PRICING[licenseType];

    const basePrice = billingCycle === 'monthly' ? pricing.monthlyPrice : pricing.annualPrice;
    const discount = (basePrice * pricing.discountPercent) / 100;
    const finalPrice = basePrice - discount;

    return {
      basePrice,
      discount,
      finalPrice,
    };
  }

  /**
   * Generate license key (for demo/development)
   */
  static generateLicenseKey(
    licenseType: LicenseType,
    userId: string,
    expiresAt: Date
  ): string {
    // Simple format: TYPE-USERID-EXPIRY-CHECKSUM
    const timestamp = expiresAt.getTime();
    const data = `${licenseType}-${userId}-${timestamp}`;
    const checksum = Array.from(data).reduce((a, b) => a + b.charCodeAt(0), 0);
    return `BC-${licenseType.toUpperCase()}-${Buffer.from(data).toString('base64').substring(0, 8)}-${checksum}`;
  }
}

/**
 * Feature usage tracker
 */
export class FeatureUsageTracker {
  private usage: Map<string, Record<string, number>> = new Map();

  /**
   * Record feature usage
   */
  trackUsage(userId: string, feature: string, amount: number = 1): void {
    if (!this.usage.has(userId)) {
      this.usage.set(userId, {});
    }
    const userUsage = this.usage.get(userId)!;
    userUsage[feature] = (userUsage[feature] || 0) + amount;
  }

  /**
   * Get usage for user
   */
  getUsage(userId: string, feature?: string): Record<string, number> | number {
    if (!this.usage.has(userId)) {
      return feature ? 0 : {};
    }

    if (feature) {
      return this.usage.get(userId)![feature] || 0;
    }

    return this.usage.get(userId) || {};
  }

  /**
   * Reset usage (monthly/annual reset)
   */
  resetUsage(userId: string): void {
    this.usage.delete(userId);
  }
}
