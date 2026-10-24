import React, { useState } from 'react';
import { WorkspaceScreen } from './screens/WorkspaceScreen';
import { PathSketchScreen } from './screens/PathSketchScreen';
import { BendCreationScreen } from './screens/BendCreationScreen';
import { DrawingGenerationScreen } from './screens/DrawingGenerationScreen';
import { Icon } from './ui/components';
import './App.css';

type ScreenType = 'workspace' | 'sketch' | 'bends' | 'drawing';

interface ProjectState {
  projectId: string;
  projectName: string;
  tubeId: string;
  currentScreen: ScreenType;
}

export const App: React.FC = () => {
  const [projectState, setProjectState] = useState<ProjectState>({
    projectId: 'proj-001',
    projectName: 'Sample Project',
    tubeId: 'Tube_1',
    currentScreen: 'workspace',
  });

  const [isDarkMode, setIsDarkMode] = useState(false);

  const screens: { id: ScreenType; label: string; icon: string }[] = [
    { id: 'workspace', label: 'Workspace', icon: '⊡' },
    { id: 'sketch', label: 'Sketch', icon: '✏' },
    { id: 'bends', label: 'Bends', icon: '⌒' },
    { id: 'drawing', label: 'Drawing', icon: '📋' },
  ];

  const renderScreen = () => {
    switch (projectState.currentScreen) {
      case 'workspace':
        return <WorkspaceScreen projectId={projectState.projectId} />;
      case 'sketch':
        return <PathSketchScreen tubeId={projectState.tubeId} />;
      case 'bends':
        return <BendCreationScreen tubeId={projectState.tubeId} />;
      case 'drawing':
        return <DrawingGenerationScreen tubeId={projectState.tubeId} />;
      default:
        return <WorkspaceScreen projectId={projectState.projectId} />;
    }
  };

  return (
    <div className={`app ${isDarkMode ? 'dark' : 'light'}`}>
      {/* Top Navigation */}
      <div className="app-header">
        <div className="header-left">
          <h1 className="app-title">
            <Icon name="tube" size="md" />
            BendCore CAD
          </h1>
          <span className="project-name">{projectState.projectName}</span>
        </div>

        <div className="header-center">
          <nav className="screen-nav">
            {screens.map((screen) => (
              <button
                key={screen.id}
                className={`nav-btn ${
                  projectState.currentScreen === screen.id ? 'active' : ''
                }`}
                onClick={() =>
                  setProjectState((prev) => ({
                    ...prev,
                    currentScreen: screen.id,
                  }))
                }
                title={screen.label}
              >
                <span className="nav-icon">{screen.icon}</span>
                <span className="nav-label">{screen.label}</span>
              </button>
            ))}
          </nav>
        </div>

        <div className="header-right">
          <button
            className="theme-toggle"
            onClick={() => setIsDarkMode(!isDarkMode)}
            title={isDarkMode ? 'Light Mode' : 'Dark Mode'}
          >
            {isDarkMode ? '☀️' : '🌙'}
          </button>
        </div>
      </div>

      {/* Screen Content */}
      <div className="app-content">
        {renderScreen()}
      </div>
    </div>
  );
};

export default App;
