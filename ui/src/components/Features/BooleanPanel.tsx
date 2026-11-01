import React, { useState } from 'react';

interface BooleanPanelProps {
  onUnion?: () => void;
  onSubtract?: () => void;
  onIntersect?: () => void;
}

export const BooleanPanel: React.FC<BooleanPanelProps> = ({
  onUnion,
  onSubtract,
  onIntersect
}) => {
  const [selectedOp, setSelectedOp] = useState<'union' | 'subtract' | 'intersect'>('union');

  return (
    <div className="boolean-panel">
      <h3>Boolean Operations</h3>
      
      <div className="operation-buttons">
        <button
          className={`op-btn ${selectedOp === 'union' ? 'active' : ''}`}
          onClick={() => { setSelectedOp('union'); onUnion?.(); }}
        >
          Union (Add)
        </button>
        
        <button
          className={`op-btn ${selectedOp === 'subtract' ? 'active' : ''}`}
          onClick={() => { setSelectedOp('subtract'); onSubtract?.(); }}
        >
          Subtract
        </button>
        
        <button
          className={`op-btn ${selectedOp === 'intersect' ? 'active' : ''}`}
          onClick={() => { setSelectedOp('intersect'); onIntersect?.(); }}
        >
          Intersect
        </button>
      </div>

      <div className="operation-info">
        {selectedOp === 'union' && <p>Combine two solids into one</p>}
        {selectedOp === 'subtract' && <p>Remove one solid from another</p>}
        {selectedOp === 'intersect' && <p>Find the common volume</p>}
      </div>
    </div>
  );
};

export default BooleanPanel;
