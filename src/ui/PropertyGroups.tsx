/**
 * Property Group Builders
 * Factory functions to create property groups for different object types
 */

import { PropertyGroup, PropertySchema } from './PropertiesPanel';

export type PropertyValue = string | number | boolean | null;

/**
 * Document properties (when nothing is selected)
 */
export function createDocumentPropertyGroup(): PropertyGroup {
  return {
    id: 'document-props',
    title: 'Document Properties',
    properties: [
      {
        name: 'documentName',
        label: 'Name',
        type: 'text',
        value: 'Untitled Document',
      },
      {
        name: 'standard',
        label: 'Standard',
        type: 'select',
        value: 'ISO',
        options: [
          { label: 'ISO (International)', value: 'ISO' },
          { label: 'GOST (Russian)', value: 'GOST' },
          { label: 'DIN (German)', value: 'DIN' },
          { label: 'ANSI (American)', value: 'ANSI' },
        ],
      },
      {
        name: 'description',
        label: 'Description',
        type: 'textarea',
        value: '',
      },
      {
        name: 'author',
        label: 'Author',
        type: 'text',
        value: 'User',
      },
      {
        name: 'created',
        label: 'Created',
        type: 'readonly',
        value: new Date().toLocaleString(),
      },
    ],
  };
}

/**
 * Tube object properties
 */
export function createTubePropertyGroup(tubeData?: any): PropertyGroup[] {
  return [
    {
      id: 'tube-basic',
      title: 'Basic Properties',
      properties: [
        {
          name: 'tubeName',
          label: 'Name',
          type: 'text',
          value: tubeData?.name || 'Tube_1',
        },
        {
          name: 'material',
          label: 'Material',
          type: 'select',
          value: tubeData?.material || 'Steel',
          options: [
            { label: 'Steel', value: 'Steel' },
            { label: 'Aluminum', value: 'Aluminum' },
            { label: 'Copper', value: 'Copper' },
            { label: 'Titanium', value: 'Titanium' },
            { label: 'Stainless Steel', value: 'StainlessSteel' },
          ],
        },
      ],
    },
    {
      id: 'tube-dimensions',
      title: 'Dimensions',
      properties: [
        {
          name: 'outerDiameter',
          label: 'Outer Diameter',
          type: 'number',
          value: tubeData?.od || 12.7,
          unit: 'mm',
          min: 0.5,
          max: 100,
          step: 0.1,
        },
        {
          name: 'wallThickness',
          label: 'Wall Thickness',
          type: 'number',
          value: tubeData?.wt || 1.6,
          unit: 'mm',
          min: 0.1,
          max: 10,
          step: 0.1,
        },
        {
          name: 'length',
          label: 'Length',
          type: 'number',
          value: tubeData?.length || 100,
          unit: 'mm',
          min: 0,
          max: 10000,
          step: 1,
        },
      ],
    },
    {
      id: 'tube-bending',
      title: 'Bending Properties',
      properties: [
        {
          name: 'bendRadius',
          label: 'Bend Radius',
          type: 'number',
          value: tubeData?.bendRadius || 20,
          unit: 'mm',
          min: 5,
          max: 500,
          step: 0.5,
        },
        {
          name: 'bender',
          label: 'Bender',
          type: 'text',
          value: tubeData?.bender || 'Default',
        },
        {
          name: 'bendCount',
          label: 'Number of Bends',
          type: 'readonly',
          value: tubeData?.bendCount || 0,
        },
      ],
    },
    {
      id: 'tube-state',
      title: 'State',
      properties: [
        {
          name: 'state',
          label: 'State',
          type: 'select',
          value: tubeData?.state || 'adaptive',
          options: [
            { label: 'Frozen', value: 'frozen' },
            { label: 'Adaptive', value: 'adaptive' },
          ],
        },
        {
          name: 'visible',
          label: 'Visible',
          type: 'toggle',
          value: tubeData?.visible !== false,
        },
        {
          name: 'color',
          label: 'Color',
          type: 'color',
          value: tubeData?.color || '#4a90e2',
        },
      ],
    },
  ];
}

/**
 * Sketch properties
 */
export function createSketchPropertyGroup(sketchData?: any): PropertyGroup[] {
  return [
    {
      id: 'sketch-basic',
      title: 'Basic Properties',
      properties: [
        {
          name: 'sketchName',
          label: 'Name',
          type: 'text',
          value: sketchData?.name || 'Sketch_1',
        },
        {
          name: 'plane',
          label: 'Plane',
          type: 'readonly',
          value: sketchData?.plane || 'XY Plane',
        },
      ],
    },
    {
      id: 'sketch-constraints',
      title: 'Constraints',
      properties: [
        {
          name: 'fullyConstrained',
          label: 'Fully Constrained',
          type: 'readonly',
          value: sketchData?.fullyConstrained ? 'Yes' : 'No',
        },
        {
          name: 'dimensionCount',
          label: 'Number of Dimensions',
          type: 'readonly',
          value: sketchData?.dimensionCount || 0,
        },
        {
          name: 'constraintCount',
          label: 'Number of Constraints',
          type: 'readonly',
          value: sketchData?.constraintCount || 0,
        },
      ],
    },
  ];
}

/**
 * Port properties
 */
export function createPortPropertyGroup(portData?: any): PropertyGroup {
  return {
    id: 'port-props',
    title: 'Port Properties',
    properties: [
      {
        name: 'portName',
        label: 'Name',
        type: 'text',
        value: portData?.name || 'Port_1',
      },
      {
        name: 'portType',
        label: 'Port Type',
        type: 'select',
        value: portData?.type || 'connector',
        options: [
          { label: 'Connector', value: 'connector' },
          { label: 'Flange', value: 'flange' },
          { label: 'Threaded', value: 'threaded' },
          { label: 'Welded', value: 'welded' },
        ],
      },
      {
        name: 'visible',
        label: 'Visible',
        type: 'toggle',
        value: portData?.visible !== false,
      },
    ],
  };
}

/**
 * Command parameters (temporary, during command execution)
 */
export function createCommandPropertyGroup(
  commandType: string,
  params?: any
): PropertyGroup {
  const baseProperties: PropertySchema[] = [];

  switch (commandType) {
    case 'new-tube':
      baseProperties.push(
        {
          name: 'diameter',
          label: 'Outer Diameter',
          type: 'number',
          value: params?.diameter || 12.7,
          unit: 'mm',
          min: 0.5,
          max: 100,
          step: 0.1,
        },
        {
          name: 'thickness',
          label: 'Wall Thickness',
          type: 'number',
          value: params?.thickness || 1.6,
          unit: 'mm',
          min: 0.1,
          max: 10,
          step: 0.1,
        }
      );
      break;

    case 'new-sketch':
      baseProperties.push({
        name: 'plane',
        label: 'Select Plane',
        type: 'select',
        value: params?.plane || 'XY',
        options: [
          { label: 'XY Plane', value: 'XY' },
          { label: 'YZ Plane', value: 'YZ' },
          { label: 'ZX Plane', value: 'ZX' },
          { label: 'Custom', value: 'custom' },
        ],
      });
      break;

    case 'bend':
      baseProperties.push(
        {
          name: 'angle',
          label: 'Bend Angle',
          type: 'number',
          value: params?.angle || 90,
          unit: '°',
          min: 0,
          max: 180,
          step: 1,
        },
        {
          name: 'radius',
          label: 'Bend Radius',
          type: 'number',
          value: params?.radius || 20,
          unit: 'mm',
          min: 5,
          max: 500,
          step: 0.5,
        }
      );
      break;

    default:
      baseProperties.push({
        name: 'info',
        label: 'Command',
        type: 'readonly',
        value: commandType,
      });
  }

  return {
    id: 'command-params',
    title: `${commandType} Parameters`,
    properties: baseProperties,
  };
}

export default {
  createDocumentPropertyGroup,
  createTubePropertyGroup,
  createSketchPropertyGroup,
  createPortPropertyGroup,
  createCommandPropertyGroup,
};
