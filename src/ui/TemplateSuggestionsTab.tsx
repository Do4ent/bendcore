/**
 * Template Suggestions Tab Component
 * Real-time template matching integrated into TubeDialog
 * Task 2.6: TubeDialog Integration (Subtask 6)
 */

import React, { useState, useEffect } from "react";
import { TemplateMatcherView } from "./TemplateMatcherView";

interface TemplateSuggestionsTabProps {
  outerDiameter: number;
  wallThickness: number;
  length: number;
  onTemplateSelected?: (templateId: string) => void;
}

export const TemplateSuggestionsTab: React.FC<TemplateSuggestionsTabProps> = ({
  outerDiameter,
  wallThickness,
  length,
  onTemplateSelected,
}) => {
  const [bendAngle, setBendAngle] = useState(90);
  const [bendRadius, setBendRadius] = useState(2.0);
  const [bendSpacing, setBendSpacing] = useState(20);

  return (
    <div>
      <h3 className="text-lg font-semibold mb-4 text-gray-800">Template Suggestions</h3>

      {/* Bend Parameters Section */}
      <div className="mb-6 p-4 bg-blue-50 rounded-lg border border-blue-200">
        <h4 className="font-medium text-gray-700 mb-4">Bend Parameters</h4>
        <div className="grid grid-cols-3 gap-4">
          <div>
            <label className="block text-sm font-medium text-gray-700 mb-1">
              Bend Angle (°)
            </label>
            <input
              type="number"
              value={bendAngle}
              onChange={(e) => setBendAngle(Math.max(0, parseFloat(e.target.value) || 0))}
              className="w-full px-3 py-2 border border-gray-300 rounded text-sm"
              min="0"
              max="360"
            />
          </div>
          <div>
            <label className="block text-sm font-medium text-gray-700 mb-1">
              Bend Radius Ratio (R/D)
            </label>
            <input
              type="number"
              value={bendRadius}
              onChange={(e) => setBendRadius(Math.max(0.5, parseFloat(e.target.value) || 0))}
              className="w-full px-3 py-2 border border-gray-300 rounded text-sm"
              min="0.5"
              step="0.1"
            />
          </div>
          <div>
            <label className="block text-sm font-medium text-gray-700 mb-1">
              Min Bend Spacing (mm)
            </label>
            <input
              type="number"
              value={bendSpacing}
              onChange={(e) => setBendSpacing(Math.max(0, parseFloat(e.target.value) || 0))}
              className="w-full px-3 py-2 border border-gray-300 rounded text-sm"
              min="0"
            />
          </div>
        </div>
      </div>

      {/* Template Matching Results */}
      <div className="mb-6">
        <h4 className="font-medium text-gray-700 mb-4">Matching Templates</h4>
        <TemplateMatcherView
          outerDiameter={outerDiameter}
          wallThickness={wallThickness}
          bendAngle={bendAngle}
          bendRadius={bendRadius}
          bendSpacing={bendSpacing}
          onSelectTemplate={(templateId) => {
            console.log("Selected template:", templateId);
            onTemplateSelected?.(templateId);
          }}
        />
      </div>

      {/* Info Section */}
      <div className="p-4 bg-gray-50 rounded-lg border border-gray-200">
        <h4 className="font-medium text-gray-700 mb-2">About Template Matching</h4>
        <p className="text-sm text-gray-600 mb-2">
          The template matcher analyzes your tube parameters and suggests the best templates based on:
        </p>
        <ul className="text-sm text-gray-600 space-y-1 ml-4">
          <li>• Outer Diameter (30% weight)</li>
          <li>• Wall Thickness (20% weight)</li>
          <li>• Bend Angle (25% weight)</li>
          <li>• Bend Radius Ratio (15% weight)</li>
          <li>• Bend Spacing (10% weight)</li>
        </ul>
        <div className="mt-3 text-xs text-gray-500">
          <p>Confidence scores:</p>
          <p>• 95-100%: Perfect match - all constraints satisfied</p>
          <p>• 85-95%: Good match - minor issues</p>
          <p>• 75-85%: Acceptable - some constraints violated</p>
          <p>• &lt;75%: Poor match - not recommended</p>
        </div>
      </div>
    </div>
  );
};
