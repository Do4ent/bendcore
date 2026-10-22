import React, { useState, useEffect } from 'react';
import { useReporting } from '../../hooks/useReporting';

interface ReportRow {
  reportId: string;
  status: 'draft' | 'approved' | 'released';
  timestamp: number;
  approvedBy: string;
  bendCardCount: number;
}

interface ReportsPanelProps {
  onExport?: (reportId: string) => void;
}

/**
 * Production Reports Panel
 * Table view with filters and actions
 */
export const ReportsPanel: React.FC<ReportsPanelProps> = ({ onExport }) => {
  const [reports, setReports] = useState<ReportRow[]>([]);
  const [filter, setFilter] = useState('');
  const [isLoading, setIsLoading] = useState(true);
  const { getProductionReports, deleteReport } = useReporting();

  // Load reports on mount
  useEffect(() => {
    loadReports();
  }, []);

  const loadReports = async () => {
    setIsLoading(true);
    try {
      const data = await getProductionReports(filter);
      setReports(data);
    } catch (error) {
      console.error('Failed to load reports:', error);
    } finally {
      setIsLoading(false);
    }
  };

  const handleFilterChange = (e: React.ChangeEvent<HTMLInputElement>) => {
    const value = e.target.value;
    setFilter(value);
    // Debounce would go here in a real implementation
  };

  const handleSearch = () => {
    loadReports();
  };

  const handleDelete = async (reportId: string) => {
    if (!window.confirm(`Delete report ${reportId}?`)) {
      return;
    }

    try {
      await deleteReport(reportId);
      loadReports();
    } catch (error) {
      alert('Delete failed: ' + (error as Error).message);
    }
  };

  const getStatusColor = (status: string) => {
    switch (status) {
      case 'released':
        return '#4CAF50'; // Green
      case 'approved':
        return '#FF9800'; // Orange
      case 'draft':
      default:
        return '#999'; // Gray
    }
  };

  const getStatusLabel = (status: string) => {
    switch (status) {
      case 'released':
        return '✓ Released';
      case 'approved':
        return '◐ Approved';
      case 'draft':
      default:
        return '◯ Draft';
    }
  };

  return (
    <div className="reports-panel">
      <div className="panel-header">
        <h2>Production Reports</h2>
      </div>

      <div className="filter-section">
        <input
          type="text"
          placeholder="Filter by Report ID or Status..."
          value={filter}
          onChange={handleFilterChange}
          onKeyPress={(e) => e.key === 'Enter' && handleSearch()}
          className="filter-input"
        />
        <button onClick={handleSearch} className="btn-search">
          Search
        </button>
      </div>

      <div className="table-container">
        {isLoading ? (
          <div className="loading">Loading reports...</div>
        ) : reports.length === 0 ? (
          <div className="empty">No reports found</div>
        ) : (
          <table className="reports-table">
            <thead>
              <tr>
                <th>Report ID</th>
                <th>Status</th>
                <th>Date</th>
                <th>Approved By</th>
                <th>Cards</th>
                <th>Actions</th>
              </tr>
            </thead>
            <tbody>
              {reports.map((report) => (
                <tr key={report.reportId}>
                  <td className="cell-id">{report.reportId}</td>
                  <td className="cell-status">
                    <span
                      className="status-badge"
                      style={{ color: getStatusColor(report.status) }}
                    >
                      {getStatusLabel(report.status)}
                    </span>
                  </td>
                  <td className="cell-date">
                    {new Date(report.timestamp * 1000).toLocaleDateString()}
                  </td>
                  <td className="cell-approver">{report.approvedBy || '-'}</td>
                  <td className="cell-count">{report.bendCardCount}</td>
                  <td className="cell-actions">
                    {onExport && (
                      <button
                        className="btn-action"
                        onClick={() => onExport(report.reportId)}
                        title="Export report"
                      >
                        ↓
                      </button>
                    )}
                    <button
                      className="btn-action btn-danger"
                      onClick={() => handleDelete(report.reportId)}
                      title="Delete report"
                    >
                      ✕
                    </button>
                  </td>
                </tr>
              ))}
            </tbody>
          </table>
        )}
      </div>

      <style jsx>{`
        .reports-panel {
          display: flex;
          flex-direction: column;
          height: 100%;
          background: white;
        }

        .panel-header {
          padding: 16px 20px;
          border-bottom: 1px solid #e0e0e0;
        }

        .panel-header h2 {
          margin: 0;
          font-size: 18px;
          font-weight: 600;
        }

        .filter-section {
          display: flex;
          gap: 8px;
          padding: 12px 20px;
          border-bottom: 1px solid #f0f0f0;
        }

        .filter-input {
          flex: 1;
          padding: 8px 12px;
          border: 1px solid #ddd;
          border-radius: 4px;
          font-size: 14px;
        }

        .filter-input:focus {
          outline: none;
          border-color: #2196F3;
          box-shadow: 0 0 0 2px rgba(33, 150, 243, 0.1);
        }

        .btn-search {
          padding: 8px 16px;
          background: #2196F3;
          color: white;
          border: none;
          border-radius: 4px;
          cursor: pointer;
          font-weight: 500;
        }

        .btn-search:hover {
          background: #1976D2;
        }

        .table-container {
          flex: 1;
          overflow-y: auto;
          padding: 0 20px;
        }

        .loading,
        .empty {
          padding: 40px 20px;
          text-align: center;
          color: #999;
        }

        .reports-table {
          width: 100%;
          border-collapse: collapse;
          margin-top: 12px;
        }

        .reports-table thead {
          position: sticky;
          top: 0;
          background: #f9f9f9;
        }

        .reports-table th {
          padding: 12px 8px;
          text-align: left;
          font-size: 13px;
          font-weight: 600;
          color: #666;
          border-bottom: 1px solid #ddd;
        }

        .reports-table td {
          padding: 12px 8px;
          font-size: 14px;
          border-bottom: 1px solid #f0f0f0;
        }

        .reports-table tbody tr:hover {
          background: #f9f9f9;
        }

        .cell-id {
          font-family: monospace;
          font-size: 13px;
        }

        .status-badge {
          font-weight: 500;
        }

        .cell-date {
          font-size: 13px;
          color: #666;
        }

        .cell-count {
          text-align: center;
          color: #666;
        }

        .cell-actions {
          display: flex;
          gap: 4px;
        }

        .btn-action {
          width: 28px;
          height: 28px;
          padding: 0;
          background: #f0f0f0;
          border: none;
          border-radius: 3px;
          cursor: pointer;
          font-size: 14px;
          transition: all 0.2s;
        }

        .btn-action:hover {
          background: #e0e0e0;
        }

        .btn-action.btn-danger:hover {
          background: #ffcdd2;
          color: #d32f2f;
        }
      `}</style>
    </div>
  );
};
