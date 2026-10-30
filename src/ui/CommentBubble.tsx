/**
 * CommentBubble компонент (4.6): маленький кружок на SVG слое в позиции комментария.
 * Цвет по статусу (открыт=красный, разрешен=зелёный, нужна правка=оранжевый).
 * Клик открывает детали, hover показывает текст.
 */
import React, { useState } from 'react';

export interface Comment {
  id: string;
  sheet_number: number;
  position: [number, number];
  text: string;
  author: string;
  created_at: string;
  status: 'open' | 'resolved' | 'needs_rework' | 'archived';
  replies: any[];
}

export interface CommentBubbleProps {
  comment: Comment;
  scale?: number;
  origin?: [number, number];
  onClick?: (comment: Comment) => void;
  onHover?: (comment: Comment | null) => void;
}

const STATUS_COLOR: Record<string, string> = {
  open: '#ef4444',
  resolved: '#22c55e',
  needs_rework: '#f97316',
  archived: '#6b7280',
};

const BUBBLE_RADIUS = 8;

export const CommentBubble: React.FC<CommentBubbleProps> = ({
  comment,
  scale = 1,
  origin = [0, 0],
  onClick,
  onHover,
}) => {
  const [showTooltip, setShowTooltip] = useState(false);

  // Convert drawing coordinates to SVG coordinates
  const x = origin[0] + comment.position[0] * scale;
  const y = origin[1] + comment.position[1] * scale;

  const bubbleColor = STATUS_COLOR[comment.status];
  const replyCount = comment.replies?.length ?? 0;

  const handleMouseEnter = () => {
    setShowTooltip(true);
    onHover?.(comment);
  };

  const handleMouseLeave = () => {
    setShowTooltip(false);
    onHover?.(null);
  };

  return (
    <g key={comment.id} className="comment-bubble">
      {/* Main bubble circle */}
      <circle
        cx={x}
        cy={y}
        r={BUBBLE_RADIUS}
        fill={bubbleColor}
        opacity={0.85}
        stroke="white"
        strokeWidth={2}
        style={{ cursor: 'pointer' }}
        onClick={() => onClick?.(comment)}
        onMouseEnter={handleMouseEnter}
        onMouseLeave={handleMouseLeave}
      />

      {/* Reply count badge */}
      {replyCount > 0 && (
        <text
          x={x}
          y={y}
          textAnchor="middle"
          dominantBaseline="middle"
          fill="white"
          fontSize="10"
          fontWeight="bold"
          pointerEvents="none"
        >
          {replyCount}
        </text>
      )}

      {/* Tooltip on hover */}
      {showTooltip && (
        <g className="comment-tooltip">
          {/* Tooltip background */}
          <rect
            x={x + BUBBLE_RADIUS + 8}
            y={y - 30}
            width={180}
            height={60}
            rx={4}
            fill="white"
            stroke={bubbleColor}
            strokeWidth="1"
            opacity="0.95"
            style={{ filter: 'drop-shadow(0 2px 4px rgba(0,0,0,0.1))' }}
          />

          {/* Tooltip text background (for better text rendering) */}
          <g pointerEvents="none">
            {/* Author and date */}
            <text
              x={x + BUBBLE_RADIUS + 12}
              y={y - 22}
              fontSize="10"
              fill="#666"
              fontWeight="500"
            >
              {comment.author}
            </text>

            {/* Comment text (truncated) */}
            <text
              x={x + BUBBLE_RADIUS + 12}
              y={y - 8}
              fontSize="11"
              fill="#333"
            >
              {comment.text.substring(0, 30)}
            </text>

            {comment.text.length > 30 && (
              <text x={x + BUBBLE_RADIUS + 12} y={y + 6} fontSize="10" fill="#999">
                ...
              </text>
            )}
          </g>

          {/* Status badge in tooltip */}
          <rect
            x={x + BUBBLE_RADIUS + 12}
            y={y + 12}
            width="32"
            height="14"
            rx="2"
            fill={bubbleColor}
            opacity="0.1"
          />
          <text
            x={x + BUBBLE_RADIUS + 28}
            y={y + 22}
            fontSize="9"
            fill={bubbleColor}
            textAnchor="middle"
            fontWeight="500"
          >
            {comment.status === 'open'
              ? 'Открыт'
              : comment.status === 'resolved'
                ? 'ОК'
                : comment.status === 'needs_rework'
                  ? 'Правка'
                  : 'В архиве'}
          </text>
        </g>
      )}
    </g>
  );
};

export default CommentBubble;
