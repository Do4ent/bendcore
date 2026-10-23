/**
 * Template Creator Wizard Component
 * Q4 Task 2: JSON Template System
 * Step-by-step wizard для создания новых JSON шаблонов
 */

import React, { useState } from 'react';
import './TemplateCreator.css';

// ============ TYPES ============

type CreatorStep = 'basic' | 'constraints' | 'manufacturing' | 'metadata' | 'preview' | 'publish';

interface TemplateFormData {
  // Basic
  id: string;
  name: string;
  description: string;

  // Constraints
  outerDiameterMin: number;
  outerDiameterMax: number;
  wallThicknessMin: number;
  wallThicknessMax: number;
  bendRadiusRatioMin: number;
  bendRadiusRatioMax: number;
  maxBendAngle: number;
  maxConsecutiveBends: number;
  minBendSpacing: number;

  // Manufacturing
  materials: string[];
  method: string;
  setupTimeMin: number;
  costPerUnit: number;
  minBatchSize: number;

  // Metadata
  author: string;
  category: string;
  tags: string;
  popularity: number;

  // CAD Formats
  compatibleFormats: string[];
  warrantyInfo: string;
  notes: string;
}

// ============ COMPONENT ============

export const TemplateCreator: React.FC = () => {
  // === STATE ===
  const [currentStep, setCurrentStep] = useState<CreatorStep>('basic');
  const [formData, setFormData] = useState<TemplateFormData>({
    id: '',
    name: '',
    description: '',
    outerDiameterMin: 6,
    outerDiameterMax: 100,
    wallThicknessMin: 0.5,
    wallThicknessMax: 5,
    bendRadiusRatioMin: 1.5,
    bendRadiusRatioMax: 3.0,
    maxBendAngle: 180,
    maxConsecutiveBends: 2,
    minBendSpacing: 10,
    materials: [],
    method: '',
    setupTimeMin: 15,
    costPerUnit: 50,
    minBatchSize: 10,
    author: 'engineering-team',
    category: 'standard',
    tags: '',
    popularity: 0,
    compatibleFormats: ['STEP', 'IGES'],
    warrantyInfo: '12 months standard',
    notes: '',
  });
  const [errors, setErrors] = useState<Record<string, string>>({});
  const [jsonMode, setJsonMode] = useState(false);
  const [jsonContent, setJsonContent] = useState('');
  const [publishMessage, setPublishMessage] = useState('');

  // === HANDLERS ===
  const handleInputChange = (
    e: React.ChangeEvent<HTMLInputElement | HTMLTextAreaElement | HTMLSelectElement>,
    field: keyof TemplateFormData
  ) => {
    const value = e.target.value;
    setFormData((prev) => ({
      ...prev,
      [field]: field.includes('Min') || field.includes('Max') || field === 'popularity'
        ? parseFloat(value) || 0
        : field === 'maxBendAngle' || field === 'maxConsecutiveBends'
        ? parseInt(value, 10) || 0
        : value,
    }));
  };

  const handleMaterialChange = (e: React.ChangeEvent<HTMLInputElement>) => {
    const material = e.target.value;
    setFormData((prev) => ({
      ...prev,
      materials: e.target.checked
        ? [...prev.materials, material]
        : prev.materials.filter((m) => m !== material),
    }));
  };

  const handleFormatChange = (e: React.ChangeEvent<HTMLInputElement>) => {
    const format = e.target.value;
    setFormData((prev) => ({
      ...prev,
      compatibleFormats: e.target.checked
        ? [...prev.compatibleFormats, format]
        : prev.compatibleFormats.filter((f) => f !== format),
    }));
  };

  const validateStep = (step: CreatorStep): boolean => {
    const newErrors: Record<string, string> = {};

    if (step === 'basic') {
      if (!formData.id.match(/^[a-z0-9\-]+$/)) newErrors.id = 'Invalid ID format';
      if (formData.name.length < 3) newErrors.name = 'Name too short';
      if (formData.description.length < 10) newErrors.description = 'Description too short';
    }

    if (step === 'constraints') {
      if (formData.outerDiameterMin >= formData.outerDiameterMax)
        newErrors.outerDiameter = 'Invalid range';
      if (formData.wallThicknessMin >= formData.wallThicknessMax)
        newErrors.wallThickness = 'Invalid range';
      if (formData.bendRadiusRatioMin >= formData.bendRadiusRatioMax)
        newErrors.bendRadiusRatio = 'Invalid range';
      if (formData.maxBendAngle <= 0 || formData.maxBendAngle > 360)
        newErrors.maxBendAngle = 'Invalid angle';
    }

    if (step === 'manufacturing') {
      if (formData.materials.length === 0) newErrors.materials = 'Select at least one material';
      if (!formData.method) newErrors.method = 'Select a method';
    }

    setErrors(newErrors);
    return Object.keys(newErrors).length === 0;
  };

  const goToStep = (step: CreatorStep) => {
    if (validateStep(currentStep)) {
      setCurrentStep(step);
    }
  };

  const buildJSON = () => {
    return {
      version: '1.0',
      id: formData.id,
      name: formData.name,
      description: formData.description,
      metadata: {
        author: formData.author,
        created_at: new Date().toISOString(),
        updated_at: new Date().toISOString(),
        category: formData.category,
        popularity: formData.popularity,
        tags: formData.tags.split(',').map((t) => t.trim()),
      },
      constraints: {
        outer_diameter: {
          min: formData.outerDiameterMin,
          max: formData.outerDiameterMax,
        },
        wall_thickness: {
          min: formData.wallThicknessMin,
          max: formData.wallThicknessMax,
        },
        bend_radius_ratio: {
          min: formData.bendRadiusRatioMin,
          max: formData.bendRadiusRatioMax,
        },
        max_bend_angle: formData.maxBendAngle,
        max_consecutive_bends: formData.maxConsecutiveBends,
        min_bend_spacing: formData.minBendSpacing,
      },
      manufacturing: {
        materials: formData.materials,
        method: formData.method,
        setup_time_min: formData.setupTimeMin,
        cost_per_unit: formData.costPerUnit,
        min_batch_size: formData.minBatchSize,
      },
      compatible_cad_formats: formData.compatibleFormats,
      warranty_info: formData.warrantyInfo,
      notes: [formData.notes].filter((n) => n),
    };
  };

  const handlePublish = async () => {
    if (!validateStep(currentStep)) return;

    const templateJson = buildJSON();

    try {
      const response = await fetch('/api/marketplace/templates', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(templateJson),
      });

      if (response.ok) {
        setPublishMessage('Template published successfully! 🎉');
        setTimeout(() => {
          // Reset form
          setCurrentStep('basic');
          setPublishMessage('');
        }, 2000);
      } else {
        setPublishMessage('Publication failed. Please try again.');
      }
    } catch (err) {
      setPublishMessage('Error publishing template.');
    }
  };

  const handleSaveDraft = () => {
    const templateJson = buildJSON();
    const dataStr = JSON.stringify(templateJson, null, 2);
    const dataBlob = new Blob([dataStr], { type: 'application/json' });
    const url = URL.createObjectURL(dataBlob);
    const link = document.createElement('a');
    link.href = url;
    link.download = `${formData.id}-draft.json`;
    link.click();
  };

  // === RENDER STEPS ===

  const renderBasicStep = () => (
    <div className="step-content">
      <h3>Basic Information</h3>

      <div className="form-group">
        <label>Template ID *</label>
        <input
          type="text"
          placeholder="e.g., std-90-deg-v2"
          value={formData.id}
          onChange={(e) => handleInputChange(e, 'id')}
          className={errors.id ? 'error' : ''}
        />
        {errors.id && <span className="error-text">{errors.id}</span>}
        <small>Unique identifier, lowercase with hyphens</small>
      </div>

      <div className="form-group">
        <label>Template Name *</label>
        <input
          type="text"
          placeholder="e.g., Standard 90° Bend v2"
          value={formData.name}
          onChange={(e) => handleInputChange(e, 'name')}
          className={errors.name ? 'error' : ''}
        />
        {errors.name && <span className="error-text">{errors.name}</span>}
      </div>

      <div className="form-group">
        <label>Description *</label>
        <textarea
          placeholder="Detailed description of this template..."
          value={formData.description}
          onChange={(e) => handleInputChange(e, 'description')}
          className={errors.description ? 'error' : ''}
          rows={4}
        />
        {errors.description && <span className="error-text">{errors.description}</span>}
      </div>

      <div className="button-group">
        <button className="btn btn-primary" onClick={() => goToStep('constraints')}>
          Next: Constraints →
        </button>
      </div>
    </div>
  );

  const renderConstraintsStep = () => (
    <div className="step-content">
      <h3>Design Constraints</h3>

      <div className="form-row">
        <div className="form-group">
          <label>Outer Diameter Min (mm)</label>
          <input
            type="number"
            value={formData.outerDiameterMin}
            onChange={(e) => handleInputChange(e, 'outerDiameterMin')}
            step="0.5"
          />
        </div>
        <div className="form-group">
          <label>Outer Diameter Max (mm)</label>
          <input
            type="number"
            value={formData.outerDiameterMax}
            onChange={(e) => handleInputChange(e, 'outerDiameterMax')}
            step="0.5"
          />
        </div>
      </div>

      <div className="form-row">
        <div className="form-group">
          <label>Wall Thickness Min (mm)</label>
          <input
            type="number"
            value={formData.wallThicknessMin}
            onChange={(e) => handleInputChange(e, 'wallThicknessMin')}
            step="0.1"
          />
        </div>
        <div className="form-group">
          <label>Wall Thickness Max (mm)</label>
          <input
            type="number"
            value={formData.wallThicknessMax}
            onChange={(e) => handleInputChange(e, 'wallThicknessMax')}
            step="0.1"
          />
        </div>
      </div>

      <div className="form-row">
        <div className="form-group">
          <label>Bend Radius Ratio Min</label>
          <input
            type="number"
            value={formData.bendRadiusRatioMin}
            onChange={(e) => handleInputChange(e, 'bendRadiusRatioMin')}
            step="0.1"
          />
        </div>
        <div className="form-group">
          <label>Bend Radius Ratio Max</label>
          <input
            type="number"
            value={formData.bendRadiusRatioMax}
            onChange={(e) => handleInputChange(e, 'bendRadiusRatioMax')}
            step="0.1"
          />
        </div>
      </div>

      <div className="form-row">
        <div className="form-group">
          <label>Max Bend Angle (°)</label>
          <input
            type="number"
            value={formData.maxBendAngle}
            onChange={(e) => handleInputChange(e, 'maxBendAngle')}
            min="0"
            max="360"
          />
        </div>
        <div className="form-group">
          <label>Max Consecutive Bends</label>
          <input
            type="number"
            value={formData.maxConsecutiveBends}
            onChange={(e) => handleInputChange(e, 'maxConsecutiveBends')}
            min="1"
          />
        </div>
      </div>

      <div className="form-group">
        <label>Min Bend Spacing (mm)</label>
        <input
          type="number"
          value={formData.minBendSpacing}
          onChange={(e) => handleInputChange(e, 'minBendSpacing')}
          step="1"
        />
      </div>

      <div className="button-group">
        <button className="btn btn-secondary" onClick={() => goToStep('basic')}>
          ← Back
        </button>
        <button className="btn btn-primary" onClick={() => goToStep('manufacturing')}>
          Next: Manufacturing →
        </button>
      </div>
    </div>
  );

  const renderManufacturingStep = () => (
    <div className="step-content">
      <h3>Manufacturing Information</h3>

      <div className="form-group">
        <label>Materials *</label>
        <div className="checkbox-group">
          {['Steel', 'Aluminum', 'Copper', 'Titanium', 'Stainless Steel'].map((material) => (
            <label key={material} className="checkbox-label">
              <input
                type="checkbox"
                checked={formData.materials.includes(material)}
                onChange={(e) => handleMaterialChange(e as any)}
                value={material}
              />
              {material}
            </label>
          ))}
        </div>
        {errors.materials && <span className="error-text">{errors.materials}</span>}
      </div>

      <div className="form-group">
        <label>Manufacturing Method *</label>
        <select
          value={formData.method}
          onChange={(e) => handleInputChange(e, 'method')}
          className={errors.method ? 'error' : ''}
        >
          <option value="">Select a method...</option>
          <option value="3D CNC bending">3D CNC Bending</option>
          <option value="Hydraulic">Hydraulic Press</option>
          <option value="CNC 5-axis">CNC 5-axis</option>
          <option value="Manual bending">Manual Bending</option>
        </select>
        {errors.method && <span className="error-text">{errors.method}</span>}
      </div>

      <div className="form-row">
        <div className="form-group">
          <label>Setup Time (minutes)</label>
          <input
            type="number"
            value={formData.setupTimeMin}
            onChange={(e) => handleInputChange(e, 'setupTimeMin')}
            min="0"
            step="5"
          />
        </div>
        <div className="form-group">
          <label>Cost Per Unit ($)</label>
          <input
            type="number"
            value={formData.costPerUnit}
            onChange={(e) => handleInputChange(e, 'costPerUnit')}
            min="0"
            step="5"
          />
        </div>
      </div>

      <div className="form-group">
        <label>Min Batch Size</label>
        <input
          type="number"
          value={formData.minBatchSize}
          onChange={(e) => handleInputChange(e, 'minBatchSize')}
          min="1"
        />
      </div>

      <div className="button-group">
        <button className="btn btn-secondary" onClick={() => goToStep('constraints')}>
          ← Back
        </button>
        <button className="btn btn-primary" onClick={() => goToStep('metadata')}>
          Next: Metadata →
        </button>
      </div>
    </div>
  );

  const renderMetadataStep = () => (
    <div className="step-content">
      <h3>Metadata & Publishing</h3>

      <div className="form-row">
        <div className="form-group">
          <label>Author</label>
          <input
            type="text"
            value={formData.author}
            onChange={(e) => handleInputChange(e, 'author')}
          />
        </div>
        <div className="form-group">
          <label>Category</label>
          <select value={formData.category} onChange={(e) => handleInputChange(e, 'category')}>
            <option value="standard">Standard</option>
            <option value="reinforced">Reinforced</option>
            <option value="custom">Custom</option>
            <option value="large-assembly">Large Assembly</option>
          </select>
        </div>
      </div>

      <div className="form-group">
        <label>Tags (comma-separated)</label>
        <input
          type="text"
          placeholder="e.g., 90-degree, standard, popular"
          value={formData.tags}
          onChange={(e) => handleInputChange(e, 'tags')}
        />
      </div>

      <div className="form-group">
        <label>CAD Formats</label>
        <div className="checkbox-group">
          {['STEP', 'IGES', 'DXF', 'ACIS'].map((format) => (
            <label key={format} className="checkbox-label">
              <input
                type="checkbox"
                checked={formData.compatibleFormats.includes(format)}
                onChange={(e) => handleFormatChange(e as any)}
                value={format}
              />
              {format}
            </label>
          ))}
        </div>
      </div>

      <div className="form-group">
        <label>Warranty Info</label>
        <input
          type="text"
          value={formData.warrantyInfo}
          onChange={(e) => handleInputChange(e, 'warrantyInfo')}
        />
      </div>

      <div className="form-group">
        <label>Additional Notes</label>
        <textarea
          value={formData.notes}
          onChange={(e) => handleInputChange(e, 'notes')}
          rows={3}
        />
      </div>

      <div className="button-group">
        <button className="btn btn-secondary" onClick={() => goToStep('manufacturing')}>
          ← Back
        </button>
        <button className="btn btn-primary" onClick={() => goToStep('preview')}>
          Next: Preview →
        </button>
      </div>
    </div>
  );

  const renderPreviewStep = () => {
    const templateJson = buildJSON();

    return (
      <div className="step-content">
        <h3>Preview</h3>

        <div className="preview-container">
          {!jsonMode ? (
            <div className="preview-form">
              <h4>{templateJson.name}</h4>
              <p>{templateJson.description}</p>

              <div className="preview-section">
                <h5>Constraints</h5>
                <p>
                  Outer Diameter: {templateJson.constraints.outer_diameter.min} -{' '}
                  {templateJson.constraints.outer_diameter.max} mm
                </p>
                <p>
                  Wall Thickness: {templateJson.constraints.wall_thickness.min} -{' '}
                  {templateJson.constraints.wall_thickness.max} mm
                </p>
              </div>

              <div className="preview-section">
                <h5>Manufacturing</h5>
                <p>Materials: {templateJson.manufacturing.materials.join(', ')}</p>
                <p>Method: {templateJson.manufacturing.method}</p>
              </div>
            </div>
          ) : (
            <pre className="json-preview">{JSON.stringify(templateJson, null, 2)}</pre>
          )}
        </div>

        <div className="button-group">
          <button
            className={`btn btn-secondary ${jsonMode ? 'active' : ''}`}
            onClick={() => setJsonMode(!jsonMode)}
          >
            {jsonMode ? 'Form View' : 'JSON View'}
          </button>
        </div>

        <div className="button-group">
          <button className="btn btn-secondary" onClick={() => goToStep('metadata')}>
            ← Back
          </button>
          <button className="btn btn-success" onClick={handlePublish}>
            Publish Template
          </button>
          <button className="btn btn-outline" onClick={handleSaveDraft}>
            Save as Draft
          </button>
        </div>

        {publishMessage && <div className="message">{publishMessage}</div>}
      </div>
    );
  };

  return (
    <div className="template-creator dark-mode">
      <div className="creator-header">
        <h1>Create Template</h1>
        <p>Create a new BendCore template step by step</p>
      </div>

      <div className="creator-container">
        {/* PROGRESS BAR */}
        <div className="progress-bar">
          {(['basic', 'constraints', 'manufacturing', 'metadata', 'preview'] as const).map(
            (step, idx) => (
              <div
                key={step}
                className={`progress-step ${currentStep === step ? 'active' : ''} ${
                  idx < (['basic', 'constraints', 'manufacturing', 'metadata', 'preview'].indexOf(
                    currentStep
                  ) as any)
                    ? 'completed'
                    : ''
                }`}
              >
                <span className="step-number">{idx + 1}</span>
                <span className="step-name">
                  {step.charAt(0).toUpperCase() + step.slice(1)}
                </span>
              </div>
            )
          )}
        </div>

        {/* STEP CONTENT */}
        <div className="step-wrapper">
          {currentStep === 'basic' && renderBasicStep()}
          {currentStep === 'constraints' && renderConstraintsStep()}
          {currentStep === 'manufacturing' && renderManufacturingStep()}
          {currentStep === 'metadata' && renderMetadataStep()}
          {currentStep === 'preview' && renderPreviewStep()}
        </div>
      </div>
    </div>
  );
};

export default TemplateCreator;
