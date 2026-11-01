/**
 * Viewport3D - 3D Mesh Visualization Component
 * Displays tube geometry using Three.js
 */

import React, { useEffect, useRef, useState } from 'react';

export interface ViewportMesh {
  id: string;
  name: string;
  mesh: {
    vertices: number[];
    indices: number[];
  };
  color?: number;
}

export interface Viewport3DProps {
  meshes: ViewportMesh[];
  width?: number;
  height?: number;
}

/**
 * 3D Viewport for visualizing tube meshes
 * Currently uses canvas with basic rendering
 * Can be enhanced with Three.js for advanced features
 */
export const Viewport3D: React.FC<Viewport3DProps> = ({
  meshes,
  width = 1400,
  height = 900
}) => {
  const canvasRef = useRef<HTMLCanvasElement>(null);
  const [isLoading, setIsLoading] = useState(false);

  useEffect(() => {
    if (!canvasRef.current || meshes.length === 0) return;

    setIsLoading(true);

    try {
      const canvas = canvasRef.current;
      const ctx = canvas.getContext('2d');
      if (!ctx) return;

      // Set canvas size
      canvas.width = width;
      canvas.height = height;

      // Clear canvas
      ctx.fillStyle = '#f5f5f5';
      ctx.fillRect(0, 0, width, height);

      // Draw 3D representation
      const mesh = meshes[0]; // Use first mesh
      if (!mesh || !mesh.mesh) return;

      const vertices = mesh.mesh.vertices;
      const indices = mesh.mesh.indices;

      // Calculate mesh bounds for centering
      if (vertices.length < 3) {
        ctx.font = '16px Arial';
        ctx.fillStyle = '#999';
        ctx.textAlign = 'center';
        ctx.fillText('No mesh data available', width / 2, height / 2);
        return;
      }

      // Find bounds (for 3 components per vertex: x, y, z)
      let minX = Infinity, maxX = -Infinity;
      let minY = Infinity, maxY = -Infinity;
      let minZ = Infinity, maxZ = -Infinity;

      for (let i = 0; i < vertices.length; i += 3) {
        minX = Math.min(minX, vertices[i]);
        maxX = Math.max(maxX, vertices[i]);
        minY = Math.min(minY, vertices[i + 1]);
        maxY = Math.max(maxY, vertices[i + 1]);
        minZ = Math.min(minZ, vertices[i + 2]);
        maxZ = Math.max(maxZ, vertices[i + 2]);
      }

      const centerX = (minX + maxX) / 2;
      const centerY = (minY + maxY) / 2;
      const centerZ = (minZ + maxZ) / 2;

      const scaleX = (maxX - minX) || 1;
      const scaleY = (maxY - minY) || 1;
      const scaleZ = (maxZ - minZ) || 1;

      // Project 3D to 2D with simple isometric projection
      const project = (x: number, y: number, z: number) => {
        // Normalize to [-1, 1]
        const nx = (x - centerX) / (scaleX / 2);
        const ny = (y - centerY) / (scaleY / 2);
        const nz = (z - centerZ) / (scaleZ / 2);

        // Isometric projection
        const screenX = width / 2 + (nx - ny) * 50;
        const screenY = height / 2 + (nx + ny + nz) * 30;

        return { x: screenX, y: screenY, z: nz };
      };

      // Draw wireframe
      ctx.strokeStyle = mesh.color ? '#' + mesh.color.toString(16).padStart(6, '0') : '#1f6fc5';
      ctx.lineWidth = 1;
      ctx.globalAlpha = 0.7;

      // Draw triangles as wireframe
      for (let i = 0; i < indices.length; i += 3) {
        const i0 = indices[i] * 3;
        const i1 = indices[i + 1] * 3;
        const i2 = indices[i + 2] * 3;

        const p0 = project(vertices[i0], vertices[i0 + 1], vertices[i0 + 2]);
        const p1 = project(vertices[i1], vertices[i1 + 1], vertices[i1 + 2]);
        const p2 = project(vertices[i2], vertices[i2 + 1], vertices[i2 + 2]);

        // Draw triangle edges
        ctx.beginPath();
        ctx.moveTo(p0.x, p0.y);
        ctx.lineTo(p1.x, p1.y);
        ctx.lineTo(p2.x, p2.y);
        ctx.closePath();
        ctx.stroke();
      }

      ctx.globalAlpha = 1;

      // Draw info text
      ctx.font = '12px monospace';
      ctx.fillStyle = '#666';
      ctx.textAlign = 'left';
      ctx.fillText(`Vertices: ${vertices.length / 3}`, 10, 20);
      ctx.fillText(`Triangles: ${indices.length / 3}`, 10, 40);
      ctx.fillText(`Name: ${mesh.name}`, 10, 60);

      // Draw center point
      ctx.fillStyle = '#ff6b6b';
      ctx.fillRect(width / 2 - 3, height / 2 - 3, 6, 6);

      // Draw axes
      ctx.strokeStyle = '#ccc';
      ctx.lineWidth = 0.5;
      ctx.beginPath();
      ctx.moveTo(width / 2 - 50, height / 2);
      ctx.lineTo(width / 2 + 50, height / 2);
      ctx.stroke();
      ctx.beginPath();
      ctx.moveTo(width / 2, height / 2 - 50);
      ctx.lineTo(width / 2, height / 2 + 50);
      ctx.stroke();

    } catch (error) {
      console.error('Error rendering viewport:', error);
      if (canvasRef.current) {
        const ctx = canvasRef.current.getContext('2d');
        if (ctx) {
          ctx.fillStyle = '#f5f5f5';
          ctx.fillRect(0, 0, width, height);
          ctx.font = '14px Arial';
          ctx.fillStyle = '#e74c3c';
          ctx.textAlign = 'center';
          ctx.fillText('Error rendering mesh', width / 2, height / 2);
        }
      }
    } finally {
      setIsLoading(false);
    }
  }, [meshes, width, height]);

  return (
    <div style={{ width: '100%', height: '100%', display: 'flex', flexDirection: 'column' }}>
      {isLoading && (
        <div style={{
          position: 'absolute',
          top: '50%',
          left: '50%',
          transform: 'translate(-50%, -50%)',
          backgroundColor: 'rgba(0, 0, 0, 0.7)',
          color: 'white',
          padding: '20px 40px',
          borderRadius: '8px',
          zIndex: 100
        }}>
          Loading mesh...
        </div>
      )}

      {meshes.length === 0 ? (
        <div style={{
          width: '100%',
          height: '100%',
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'center',
          backgroundColor: '#f5f5f5',
          color: '#999',
          fontSize: '16px'
        }}>
          <div style={{ textAlign: 'center' }}>
            <p>📦 Import a STEP/IGES file to view 3D mesh</p>
            <p style={{ fontSize: '12px', marginTop: '10px' }}>
              Use the Import button above to get started
            </p>
          </div>
        </div>
      ) : (
        <canvas
          ref={canvasRef}
          style={{
            width: '100%',
            height: '100%',
            display: 'block',
            backgroundColor: '#f5f5f5',
            border: '1px solid #e0e0e0'
          }}
        />
      )}
    </div>
  );
};

export default Viewport3D;
