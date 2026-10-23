/**
 * Панель комментариев (4.5-4.8): замечания и аннотации на чертежах.
 * Комментарии привязаны к позициям на листах, имеют статусы (открыт/разрешён/требует правки).
 * История ответов, фильтрация по статусу, счётчик замечаний.
 */
import React, { useMemo, useState } from 'react';
import { t } from './core/i18n';
import './panels.css';

export interface CommentReply {
  id: string;
  text: string;
  author: string;
  timestamp: string;
}

export interface Comment {
  id: string;
  sheet_number: number;
  position: [number, number];
  text: string;
  author: string;
  created_at: string;
  modified_at: string;
  status: 'open' | 'resolved' | 'needs_rework' | 'archived';
  replies: CommentReply[];
}

export interface CommentsPanelProps {
  comments: Comment[];
  sheetNumber?: number;
  onAddComment?: (sheet: number, x: number, y: number, text: string) => void;
  onReplyToComment?: (commentId: string, text: string) => void;
  onChangeStatus?: (commentId: string, status: string) => void;
  onDeleteComment?: (commentId: string) => void;
}

type StatusFilter = 'all' | 'open' | 'resolved' | 'needs_rework' | 'archived';

const STATUS_LABEL: Record<string, string> = {
  open: 'Открыт',
  resolved: 'Разрешён',
  needs_rework: 'Требует правки',
  archived: 'В архиве',
};

const STATUS_COLOR: Record<string, string> = {
  open: '#ef4444',
  resolved: '#22c55e',
  needs_rework: '#f97316',
  archived: '#6b7280',
};

export const CommentsPanel: React.FC<CommentsPanelProps> = ({
  comments,
  sheetNumber = 1,
  onAddComment,
  onReplyToComment,
  onChangeStatus,
  onDeleteComment,
}) => {
  const [statusFilter, setStatusFilter] = useState<StatusFilter>('all');
  const [expandedComments, setExpandedComments] = useState<Set<string>>(new Set());
  const [replyText, setReplyText] = useState<Record<string, string>>({});
  const [newCommentText, setNewCommentText] = useState('');
  const [showAddComment, setShowAddComment] = useState(false);

  // Load filter state from localStorage
  React.useEffect(() => {
    const saved = localStorage.getItem('comments_filter');
    if (saved && ['all', 'open', 'resolved', 'needs_rework', 'archived'].includes(saved)) {
      setStatusFilter(saved as StatusFilter);
    }
  }, []);

  // Save filter state to localStorage
  const handleFilterChange = (filter: StatusFilter) => {
    setStatusFilter(filter);
    localStorage.setItem('comments_filter', filter);
  };

  // Filter comments by sheet and status
  const sheetComments = useMemo(() => {
    return comments.filter((c) => c.sheet_number === sheetNumber);
  }, [comments, sheetNumber]);

  const filteredComments = useMemo(() => {
    if (statusFilter === 'all') {
      return sheetComments;
    }
    return sheetComments.filter((c) => c.status === statusFilter);
  }, [sheetComments, statusFilter]);

  // Count comments by status
  const counts = useMemo(() => {
    const result: Record<StatusFilter, number> = {
      all: sheetComments.length,
      open: 0,
      resolved: 0,
      needs_rework: 0,
      archived: 0,
    };
    sheetComments.forEach((c) => {
      result[c.status as StatusFilter]++;
    });
    return result;
  }, [sheetComments]);

  const toggleExpand = (commentId: string) => {
    setExpandedComments((prev) => {
      const next = new Set(prev);
      if (next.has(commentId)) {
        next.delete(commentId);
      } else {
        next.add(commentId);
      }
      return next;
    });
  };

  const handleAddReply = (commentId: string) => {
    const text = replyText[commentId]?.trim();
    if (text && onReplyToComment) {
      onReplyToComment(commentId, text);
      setReplyText((prev) => ({ ...prev, [commentId]: '' }));
    }
  };

  const handleAddComment = () => {
    const text = newCommentText.trim();
    if (text && onAddComment) {
      // Default position in center of sheet
      onAddComment(sheetNumber, 100, 100, text);
      setNewCommentText('');
      setShowAddComment(false);
    }
  };

  const handleStatusChange = (commentId: string, newStatus: string) => {
    if (onChangeStatus) {
      onChangeStatus(commentId, newStatus);
    }
  };

  const handleDelete = (commentId: string) => {
    if (onDeleteComment) {
      onDeleteComment(commentId);
    }
  };

  const formatDate = (timestamp: string) => {
    try {
      const date = new Date(timestamp);
      return date.toLocaleString('ru-RU', {
        month: 'short',
        day: 'numeric',
        hour: '2-digit',
        minute: '2-digit',
      });
    } catch {
      return timestamp;
    }
  };

  return (
    <div className="bc-panel" style={{ display: 'flex', flexDirection: 'column', height: '100%' }}>
      <div className="bc-header">
        <h3 style={{ margin: '8px 0', fontSize: '14px', fontWeight: 600 }}>
          Комментарии ({counts[statusFilter]})
        </h3>
      </div>

      {/* Filter buttons */}
      <div style={{ display: 'flex', gap: '4px', flexWrap: 'wrap', padding: '8px', borderBottom: '1px solid #e5e7eb' }}>
        {(['all', 'open', 'resolved', 'needs_rework', 'archived'] as StatusFilter[]).map((filter) => (
          <button
            key={filter}
            className="bc-button"
            onClick={() => handleFilterChange(filter)}
            style={{
              padding: '4px 8px',
              fontSize: '12px',
              backgroundColor: statusFilter === filter ? '#3b82f6' : '#f3f4f6',
              color: statusFilter === filter ? 'white' : '#1f2937',
              border: 'none',
              borderRadius: '4px',
              cursor: 'pointer',
              flexShrink: 0,
            }}
          >
            {filter === 'all' ? 'Все' : STATUS_LABEL[filter]} {counts[filter] > 0 && `(${counts[filter]})`}
          </button>
        ))}
      </div>

      {/* Comments list */}
      <div
        style={{
          flex: 1,
          overflowY: 'auto',
          padding: '8px',
        }}
      >
        {filteredComments.length === 0 ? (
          <div style={{ color: '#9ca3af', fontSize: '12px', padding: '16px', textAlign: 'center' }}>
            {sheetComments.length === 0 ? 'Нет комментариев' : 'Нет комментариев с этим статусом'}
          </div>
        ) : (
          filteredComments.map((comment) => (
            <div
              key={comment.id}
              style={{
                marginBottom: '12px',
                border: '1px solid #e5e7eb',
                borderRadius: '4px',
                backgroundColor: '#f9fafb',
                overflow: 'hidden',
              }}
            >
              {/* Comment header */}
              <div
                onClick={() => toggleExpand(comment.id)}
                style={{
                  padding: '8px',
                  cursor: 'pointer',
                  borderLeft: `3px solid ${STATUS_COLOR[comment.status]}`,
                  display: 'flex',
                  justifyContent: 'space-between',
                  alignItems: 'center',
                  backgroundColor: '#fff',
                }}
              >
                <div style={{ flex: 1, minWidth: 0 }}>
                  <div style={{ fontSize: '12px', fontWeight: 500, color: '#1f2937', wordBreak: 'break-word' }}>
                    {comment.text.substring(0, 50)}
                    {comment.text.length > 50 ? '...' : ''}
                  </div>
                  <div style={{ fontSize: '11px', color: '#6b7280', marginTop: '2px' }}>
                    {comment.author} · {formatDate(comment.created_at)}
                  </div>
                </div>
                <div
                  style={{
                    fontSize: '10px',
                    backgroundColor: STATUS_COLOR[comment.status],
                    color: 'white',
                    padding: '2px 6px',
                    borderRadius: '3px',
                    marginLeft: '8px',
                    whiteSpace: 'nowrap',
                    flexShrink: 0,
                  }}
                >
                  {STATUS_LABEL[comment.status]}
                </div>
                {comment.replies.length > 0 && (
                  <div style={{ fontSize: '11px', color: '#6b7280', marginLeft: '8px', flexShrink: 0 }}>
                    {comment.replies.length}
                  </div>
                )}
              </div>

              {/* Expanded content */}
              {expandedComments.has(comment.id) && (
                <div style={{ borderTop: '1px solid #e5e7eb', padding: '8px', backgroundColor: '#f9fafb' }}>
                  {/* Full comment text */}
                  <div style={{ fontSize: '12px', color: '#1f2937', marginBottom: '8px', whiteSpace: 'pre-wrap' }}>
                    {comment.text}
                  </div>

                  {/* Replies */}
                  {comment.replies.length > 0 && (
                    <div
                      style={{
                        borderTop: '1px solid #e5e7eb',
                        paddingTop: '8px',
                        marginBottom: '8px',
                      }}
                    >
                      {comment.replies.map((reply) => (
                        <div
                          key={reply.id}
                          style={{
                            fontSize: '11px',
                            color: '#4b5563',
                            marginBottom: '6px',
                            paddingLeft: '8px',
                            borderLeft: '2px solid #d1d5db',
                          }}
                        >
                          <div style={{ fontWeight: 500 }}>
                            {reply.author} · {formatDate(reply.timestamp)}
                          </div>
                          <div style={{ marginTop: '2px', whiteSpace: 'pre-wrap' }}>{reply.text}</div>
                        </div>
                      ))}
                    </div>
                  )}

                  {/* Reply input */}
                  {comment.status === 'open' && (
                    <div style={{ display: 'flex', gap: '4px', marginBottom: '8px' }}>
                      <input
                        type="text"
                        placeholder="Ответить..."
                        value={replyText[comment.id] ?? ''}
                        onChange={(e) => setReplyText((prev) => ({ ...prev, [comment.id]: e.target.value }))}
                        onKeyDown={(e) => {
                          if (e.key === 'Enter' && !e.shiftKey) {
                            handleAddReply(comment.id);
                          }
                        }}
                        style={{
                          flex: 1,
                          padding: '4px 6px',
                          fontSize: '11px',
                          border: '1px solid #d1d5db',
                          borderRadius: '3px',
                          minWidth: 0,
                        }}
                      />
                      <button
                        className="bc-button"
                        onClick={() => handleAddReply(comment.id)}
                        style={{
                          padding: '4px 8px',
                          fontSize: '11px',
                          backgroundColor: '#3b82f6',
                          color: 'white',
                          border: 'none',
                          borderRadius: '3px',
                          cursor: 'pointer',
                          whiteSpace: 'nowrap',
                          flexShrink: 0,
                        }}
                      >
                        Отправить
                      </button>
                    </div>
                  )}

                  {/* Status buttons */}
                  <div style={{ display: 'flex', gap: '4px', flexWrap: 'wrap' }}>
                    {comment.status === 'open' && (
                      <>
                        <button
                          className="bc-button"
                          onClick={() => handleStatusChange(comment.id, 'resolved')}
                          style={{
                            padding: '4px 8px',
                            fontSize: '11px',
                            backgroundColor: '#22c55e',
                            color: 'white',
                            border: 'none',
                            borderRadius: '3px',
                            cursor: 'pointer',
                            flex: 1,
                            minWidth: '60px',
                          }}
                        >
                          ✓ Разрешить
                        </button>
                        <button
                          className="bc-button"
                          onClick={() => handleStatusChange(comment.id, 'needs_rework')}
                          style={{
                            padding: '4px 8px',
                            fontSize: '11px',
                            backgroundColor: '#f97316',
                            color: 'white',
                            border: 'none',
                            borderRadius: '3px',
                            cursor: 'pointer',
                            flex: 1,
                            minWidth: '60px',
                          }}
                        >
                          ↻ Правка
                        </button>
                      </>
                    )}
                    {comment.status !== 'open' && (
                      <button
                        className="bc-button"
                        onClick={() => handleStatusChange(comment.id, 'open')}
                        style={{
                          padding: '4px 8px',
                          fontSize: '11px',
                          backgroundColor: '#ef4444',
                          color: 'white',
                          border: 'none',
                          borderRadius: '3px',
                          cursor: 'pointer',
                          flex: 1,
                        }}
                      >
                        ↶ Открыть
                      </button>
                    )}
                    <button
                      className="bc-button"
                      onClick={() => handleDelete(comment.id)}
                      style={{
                        padding: '4px 8px',
                        fontSize: '11px',
                        backgroundColor: '#9ca3af',
                        color: 'white',
                        border: 'none',
                        borderRadius: '3px',
                        cursor: 'pointer',
                      }}
                      title="Удалить"
                    >
                      ✕
                    </button>
                  </div>
                </div>
              )}
            </div>
          ))
        )}
      </div>

      {/* Add comment button */}
      <div style={{ borderTop: '1px solid #e5e7eb', padding: '8px' }}>
        {!showAddComment ? (
          <button
            className="bc-button"
            onClick={() => setShowAddComment(true)}
            style={{
              width: '100%',
              padding: '8px',
              fontSize: '12px',
              backgroundColor: '#3b82f6',
              color: 'white',
              border: 'none',
              borderRadius: '4px',
              cursor: 'pointer',
              fontWeight: 500,
            }}
          >
            + Добавить комментарий
          </button>
        ) : (
          <div style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
            <textarea
              placeholder="Введите текст комментария..."
              value={newCommentText}
              onChange={(e) => setNewCommentText(e.target.value)}
              style={{
                width: '100%',
                padding: '6px',
                fontSize: '11px',
                border: '1px solid #d1d5db',
                borderRadius: '3px',
                fontFamily: 'inherit',
                minHeight: '60px',
                resize: 'none',
              }}
            />
            <div style={{ display: 'flex', gap: '4px' }}>
              <button
                className="bc-button"
                onClick={handleAddComment}
                style={{
                  flex: 1,
                  padding: '6px',
                  fontSize: '11px',
                  backgroundColor: '#3b82f6',
                  color: 'white',
                  border: 'none',
                  borderRadius: '3px',
                  cursor: 'pointer',
                }}
              >
                Отправить
              </button>
              <button
                className="bc-button"
                onClick={() => {
                  setShowAddComment(false);
                  setNewCommentText('');
                }}
                style={{
                  flex: 1,
                  padding: '6px',
                  fontSize: '11px',
                  backgroundColor: '#e5e7eb',
                  color: '#1f2937',
                  border: 'none',
                  borderRadius: '3px',
                  cursor: 'pointer',
                }}
              >
                Отмена
              </button>
            </div>
          </div>
        )}
      </div>
    </div>
  );
};

export default CommentsPanel;
