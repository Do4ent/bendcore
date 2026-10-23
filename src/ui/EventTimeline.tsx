import React, { useMemo } from 'react';
import { AnalyticsEvent } from './hooks/useAnalytics';
import './styles/AnalyticsDashboard.css';

interface EventTimelineProps {
  events: AnalyticsEvent[];
  maxEvents?: number;
}

export const EventTimeline: React.FC<EventTimelineProps> = ({ events, maxEvents = 20 }) => {
  const sortedEvents = useMemo(() => {
    return events
      .sort((a, b) => new Date(b.timestamp).getTime() - new Date(a.timestamp).getTime())
      .slice(0, maxEvents);
  }, [events, maxEvents]);

  const formatTime = (timestamp: string): string => {
    const date = new Date(timestamp);
    return date.toLocaleTimeString();
  };

  const getEventIcon = (eventName: string): string => {
    if (eventName.includes('import')) return '📥';
    if (eventName.includes('export')) return '📤';
    if (eventName.includes('template')) return '📋';
    if (eventName.includes('search')) return '🔍';
    if (eventName.includes('formula')) return '🧮';
    if (eventName.includes('tube')) return '📐';
    if (eventName.includes('error')) return '⚠️';
    return '•';
  };

  return (
    <div className="event-timeline" role="region" aria-label="Event timeline">
      <h3>Recent Events (Last {sortedEvents.length})</h3>

      {sortedEvents.length === 0 ? (
        <div className="timeline-empty">No events recorded yet</div>
      ) : (
        <div className="timeline-list">
          {sortedEvents.map((event) => (
            <div key={event.id} className="timeline-item">
              <div className="timeline-icon">{getEventIcon(event.name)}</div>
              <div className="timeline-content">
                <div className="event-name">{event.name}</div>
                <div className="event-meta">
                  <span className="event-user">{event.userId}</span>
                  <span className="event-time">{formatTime(event.timestamp)}</span>
                  {event.variant && (
                    <span className={`event-variant ${event.variant}`}>
                      {event.variant}
                    </span>
                  )}
                </div>
                {Object.keys(event.properties).length > 0 && (
                  <div className="event-properties">
                    {Object.entries(event.properties).map(([key, value]) => (
                      <span key={key} className="property">
                        <code>
                          {key}: {value}
                        </code>
                      </span>
                    ))}
                  </div>
                )}
              </div>
            </div>
          ))}
        </div>
      )}
    </div>
  );
};
