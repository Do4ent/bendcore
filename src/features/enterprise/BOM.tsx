/**
 * Bill of Materials (BOM) Module - Ведомость материалов
 */

import React, { useState } from 'react';
import { BillOfMaterials, BOMItem } from './types';

interface BOMProps {
  onGenerate: (bom: BillOfMaterials) => void;
  onCancel: () => void;
}

export const BOM: React.FC<BOMProps> = ({ onGenerate, onCancel }) => {
  const [projectName, setProjectName] = useState<string>('Новый проект');
  const [bom, setBOM] = useState<BillOfMaterials | null>(null);
  const [isGenerating, setIsGenerating] = useState<boolean>(false);

  const generateMockBOM = () => {
    setIsGenerating(true);

    setTimeout(() => {
      const mockItems: BOMItem[] = [
        {
          id: 'item-1',
          partNumber: 'TB-001',
          description: 'Трубка 25.4мм OD x 2.0мм стенка',
          quantity: 2,
          unit: 'шт',
          material: 'Сталь',
          weight: 2.5,
          unitCost: 45,
          totalCost: 90,
          supplier: 'AcmeTube Inc.',
          leadTime: 5,
        },
        {
          id: 'item-2',
          partNumber: 'EL-001',
          description: 'Изгиб 90° 25.4мм',
          quantity: 4,
          unit: 'шт',
          material: 'Сталь',
          weight: 0.3,
          unitCost: 12,
          totalCost: 48,
          supplier: 'FittingsPlus',
          leadTime: 3,
        },
        {
          id: 'item-3',
          partNumber: 'COU-001',
          description: 'Муфта 25.4мм',
          quantity: 6,
          unit: 'шт',
          material: 'Сталь',
          weight: 0.15,
          unitCost: 8,
          totalCost: 48,
          supplier: 'FittingsPlus',
          leadTime: 3,
        },
        {
          id: 'item-4',
          partNumber: 'TEE-001',
          description: 'Тройник 25.4мм x 25.4мм',
          quantity: 2,
          unit: 'шт',
          material: 'Сталь',
          weight: 0.35,
          unitCost: 18,
          totalCost: 36,
          supplier: 'FittingsPlus',
          leadTime: 3,
        },
        {
          id: 'item-5',
          partNumber: 'RED-001',
          description: 'Переходник 25.4мм на 19.05мм',
          quantity: 2,
          unit: 'шт',
          material: 'Сталь',
          weight: 0.2,
          unitCost: 10,
          totalCost: 20,
          supplier: 'FittingsPlus',
          leadTime: 3,
        },
        {
          id: 'item-6',
          partNumber: 'CAP-001',
          description: 'Заглушка 25.4мм',
          quantity: 2,
          unit: 'шт',
          material: 'Сталь',
          weight: 0.1,
          unitCost: 5,
          totalCost: 10,
          supplier: 'FittingsPlus',
          leadTime: 2,
        },
      ];

      const totalCost = mockItems.reduce((sum, item) => sum + item.totalCost, 0);
      const totalWeight = mockItems.reduce((sum, item) => sum + item.weight * item.quantity, 0);

      const mockBOM: BillOfMaterials = {
        id: `bom-${Date.now()}`,
        projectName,
        items: mockItems,
        totalCost,
        totalWeight,
        createdAt: new Date(),
      };

      setBOM(mockBOM);
      setIsGenerating(false);
      onGenerate(mockBOM);
    }, 1000);
  };

  const exportToCSV = () => {
    if (!bom) return;

    let csv = 'Ведомость материалов (BOM)\n';
    csv += `Проект: ${bom.projectName}\n`;
    csv += `Дата: ${bom.createdAt.toLocaleDateString('ru-RU')}\n\n`;

    csv += 'Номер детали,Описание,Количество,Ед.изм,Материал,Вес,Стоим. за ед.,Сумма,Поставщик,Сроки поставки\n';

    bom.items.forEach((item) => {
      csv += `${item.partNumber},"${item.description}",${item.quantity},${item.unit},${item.material},${item.weight},${item.unitCost},${item.totalCost},${item.supplier},${item.leadTime}\n`;
    });

    csv += `\nИТОГО,,,,,${bom.totalWeight}кг,,${bom.totalCost}USD\n`;

    const blob = new Blob([csv], { type: 'text/csv;charset=utf-8;' });
    const link = document.createElement('a');
    const url = URL.createObjectURL(blob);
    link.setAttribute('href', url);
    link.setAttribute('download', `BOM_${bom.projectName}_${Date.now()}.csv`);
    link.style.visibility = 'hidden';
    document.body.appendChild(link);
    link.click();
    document.body.removeChild(link);
  };

  return (
    <div className="bom-dialog">
      <h3>Ведомость материалов (BOM)</h3>

      <div className="bom-form">
        <div className="form-group">
          <label htmlFor="projectName">Название проекта:</label>
          <input
            id="projectName"
            type="text"
            value={projectName}
            onChange={(e) => setProjectName(e.target.value)}
            disabled={isGenerating || bom !== null}
            placeholder="Введите название проекта"
          />
        </div>

        {!bom && (
          <button
            className="generate-bom-button"
            onClick={generateMockBOM}
            disabled={isGenerating}
          >
            {isGenerating ? (
              <>
                <span className="spinner" />
                Генерирование BOM...
              </>
            ) : (
              '📋 Генерировать BOM'
            )}
          </button>
        )}
      </div>

      {bom && (
        <div className="bom-results">
          <div className="bom-summary">
            <div className="summary-card">
              <span className="label">Итого материалов</span>
              <span className="value">{bom.items.length}</span>
              <span className="unit">позиций</span>
            </div>

            <div className="summary-card">
              <span className="label">Общий вес</span>
              <span className="value">{bom.totalWeight.toFixed(2)}</span>
              <span className="unit">кг</span>
            </div>

            <div className="summary-card">
              <span className="label">Сметная стоимость</span>
              <span className="value">${bom.totalCost.toFixed(2)}</span>
              <span className="unit">USD</span>
            </div>
          </div>

          <div className="bom-table-container">
            <table className="bom-table">
              <thead>
                <tr>
                  <th>Номер</th>
                  <th>Описание</th>
                  <th>Кол-во</th>
                  <th>Материал</th>
                  <th>Вес</th>
                  <th>Стоим./шт</th>
                  <th>Сумма</th>
                </tr>
              </thead>
              <tbody>
                {bom.items.map((item, idx) => (
                  <tr key={item.id}>
                    <td className="part-number">{item.partNumber}</td>
                    <td className="description">{item.description}</td>
                    <td className="quantity">{item.quantity}</td>
                    <td className="material">{item.material}</td>
                    <td className="weight">{item.weight}кг</td>
                    <td className="cost">${item.unitCost}</td>
                    <td className="total-cost">${item.totalCost}</td>
                  </tr>
                ))}
              </tbody>
              <tfoot>
                <tr>
                  <td colSpan={4} className="total-label">
                    <strong>ИТОГО</strong>
                  </td>
                  <td className="total-weight">
                    <strong>{bom.totalWeight.toFixed(2)}кг</strong>
                  </td>
                  <td></td>
                  <td className="total-cost-value">
                    <strong>${bom.totalCost.toFixed(2)}</strong>
                  </td>
                </tr>
              </tfoot>
            </table>
          </div>

          <button className="export-button" onClick={exportToCSV}>
            💾 Экспортировать в CSV
          </button>
        </div>
      )}

      <div className="dialog-buttons">
        <button className="btn-secondary" onClick={onCancel}>
          Закрыть
        </button>
      </div>

      <style>{`
        .bom-dialog {
          padding: 16px;
          background: white;
          border-radius: 8px;
          border: 1px solid #ddd;
          max-width: 900px;
        }

        .bom-form {
          margin: 16px 0;
          padding: 12px;
          background: #fafafa;
          border-radius: 4px;
        }

        .form-group {
          margin-bottom: 12px;
        }

        .form-group label {
          display: block;
          font-weight: 500;
          margin-bottom: 4px;
          font-size: 12px;
        }

        .form-group input {
          width: 100%;
          padding: 6px;
          border: 1px solid #ddd;
          border-radius: 3px;
          font-size: 12px;
        }

        .generate-bom-button {
          width: 100%;
          padding: 10px;
          background: #4caf50;
          color: white;
          border: none;
          border-radius: 4px;
          cursor: pointer;
          font-weight: 500;
          font-size: 13px;
          display: flex;
          align-items: center;
          justify-content: center;
          gap: 8px;
          transition: background 0.2s;
        }

        .generate-bom-button:hover:not(:disabled) {
          background: #45a049;
        }

        .generate-bom-button:disabled {
          opacity: 0.7;
          cursor: not-allowed;
        }

        .spinner {
          display: inline-block;
          width: 12px;
          height: 12px;
          border: 2px solid rgba(255, 255, 255, 0.3);
          border-radius: 50%;
          border-top-color: white;
          animation: spin 0.8s linear infinite;
        }

        @keyframes spin {
          to { transform: rotate(360deg); }
        }

        .bom-results {
          margin: 16px 0;
        }

        .bom-summary {
          display: grid;
          grid-template-columns: repeat(3, 1fr);
          gap: 12px;
          margin-bottom: 16px;
        }

        .summary-card {
          background: #f0f7ff;
          border: 1px solid #b3e5fc;
          border-radius: 4px;
          padding: 12px;
          text-align: center;
        }

        .summary-card .label {
          display: block;
          font-size: 11px;
          color: #666;
          margin-bottom: 4px;
        }

        .summary-card .value {
          display: block;
          font-size: 24px;
          font-weight: bold;
          color: #1976d2;
          margin-bottom: 2px;
        }

        .summary-card .unit {
          display: block;
          font-size: 10px;
          color: #999;
        }

        .bom-table-container {
          overflow-x: auto;
          margin-bottom: 12px;
          border: 1px solid #ddd;
          border-radius: 4px;
        }

        .bom-table {
          width: 100%;
          border-collapse: collapse;
          font-size: 11px;
          background: white;
        }

        .bom-table thead {
          background: #f5f5f5;
          border-bottom: 2px solid #ddd;
        }

        .bom-table th {
          padding: 8px;
          text-align: left;
          font-weight: 600;
          color: #333;
        }

        .bom-table td {
          padding: 8px;
          border-bottom: 1px solid #f0f0f0;
        }

        .bom-table tbody tr:hover {
          background: #fafafa;
        }

        .bom-table tfoot {
          background: #f9f9f9;
          border-top: 2px solid #ddd;
          font-weight: 600;
        }

        .bom-table tfoot td {
          padding: 10px 8px;
          border-bottom: none;
        }

        .total-label {
          text-align: left;
          color: #333;
        }

        .total-weight,
        .total-cost-value {
          color: #1976d2;
        }

        .part-number {
          color: #1976d2;
          font-weight: 600;
        }

        .quantity,
        .cost,
        .total-cost {
          text-align: right;
        }

        .export-button {
          width: 100%;
          padding: 10px;
          background: #2196f3;
          color: white;
          border: none;
          border-radius: 4px;
          cursor: pointer;
          font-weight: 500;
          font-size: 13px;
          margin-bottom: 12px;
        }

        .export-button:hover {
          background: #1976d2;
        }

        .dialog-buttons {
          display: flex;
          gap: 8px;
        }

        .btn-secondary {
          flex: 1;
          padding: 8px;
          background: #f5f5f5;
          border: 1px solid #ddd;
          border-radius: 4px;
          cursor: pointer;
          font-weight: 500;
        }

        .btn-secondary:hover {
          background: #eeeeee;
        }
      `}</style>
    </div>
  );
};
