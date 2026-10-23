import React, { ReactNode } from "react";
import { useFeatureFlag } from "../hooks/useFeatureFlag";

interface FeatureFlagProps {
  name: string;
  children: ReactNode;
  fallback?: ReactNode;
  loadingComponent?: ReactNode;
}

/**
 * FeatureFlag Component
 * Conditionally renders content based on feature flag status
 *
 * @example
 * <FeatureFlag name="parametric_dimensions">
 *   <ParametricDimensionsUI />
 * </FeatureFlag>
 */
export const FeatureFlag: React.FC<FeatureFlagProps> = ({
  name,
  children,
  fallback,
  loadingComponent,
}) => {
  const { enabled, isLoading } = useFeatureFlag(name);

  // Show loading component while feature flag status is being determined
  if (isLoading && loadingComponent) {
    return <>{loadingComponent}</>;
  }

  // Show children if feature is enabled
  if (enabled) {
    return <>{children}</>;
  }

  // Show fallback if feature is disabled
  if (fallback) {
    return <>{fallback}</>;
  }

  // By default, render nothing if feature is disabled and no fallback
  return null;
};

/**
 * FeatureFlagSection Component
 * Renders a section with conditional visibility based on feature flag
 */
interface FeatureFlagSectionProps extends FeatureFlagProps {
  title?: string;
  className?: string;
}

export const FeatureFlagSection: React.FC<FeatureFlagSectionProps> = ({
  name,
  title,
  className = "",
  children,
  fallback,
  loadingComponent,
}) => {
  return (
    <FeatureFlag
      name={name}
      fallback={fallback}
      loadingComponent={loadingComponent}
    >
      <section className={className}>
        {title && <h3>{title}</h3>}
        {children}
      </section>
    </FeatureFlag>
  );
};

/**
 * ConditionalRender Component
 * Renders different content based on feature flag status
 */
interface ConditionalRenderProps {
  flag: string;
  enabled: ReactNode;
  disabled: ReactNode;
  loading?: ReactNode;
}

export const ConditionalRender: React.FC<ConditionalRenderProps> = ({
  flag,
  enabled,
  disabled,
  loading,
}) => {
  const { enabled: isEnabled, isLoading } = useFeatureFlag(flag);

  if (isLoading && loading) {
    return <>{loading}</>;
  }

  return <>{isEnabled ? enabled : disabled}</>;
};

export default FeatureFlag;
