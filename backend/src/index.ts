/**
 * BendCore Web Backend
 * Main entry point - Express.js server setup
 */

import express, { Express, Request, Response, NextFunction } from 'express';
import cors from 'cors';
import dotenv from 'dotenv';
import path from 'path';
import { ProfileManager } from './core/profile-manager';
import { Database } from './database/db';

// Import routes
import profileRoutes from './api/routes/profiles';
import tubeRoutes from './api/routes/tubes';
import sketchRoutes from './api/routes/sketches';
import importRoutes from './api/routes/import';
import projectRoutes, { setDatabase } from './api/routes/projects';

// Load environment variables
dotenv.config();

// Create Express app
const app: Express = express();
const port = process.env.PORT || 3001;
const env = process.env.NODE_ENV || 'development';

// Middleware
app.use(
  cors({
    origin: process.env.CORS_ORIGIN || '*',
    credentials: true
  })
);

app.use(express.json({ limit: '10mb' }));
app.use(express.urlencoded({ limit: '10mb', extended: true }));

// Request logging middleware
app.use((req: Request, res: Response, next: NextFunction): void => {
  console.log(`${req.method} ${req.path}`);
  next();
});

// Error handling middleware
app.use((err: Error, req: Request, res: Response, next: NextFunction): void => {
  console.error('Error:', err.message);
  res.status(500).json({
    error: true,
    message: err.message || 'Internal server error',
    ...(env === 'development' && { stack: err.stack })
  });
});

// Health check endpoint
app.get('/api/health', (req: Request, res: Response): void => {
  res.json({
    status: 'ok',
    timestamp: new Date().toISOString(),
    version: '1.0.0',
    environment: env
  });
});

// API Routes
app.use('/api/profiles', profileRoutes);
app.use('/api/tubes', tubeRoutes);
app.use('/api/sketches', sketchRoutes);
app.use('/api/import', importRoutes);
app.use('/api/projects', projectRoutes);

// 404 handler
app.use((req: Request, res: Response): void => {
  res.status(404).json({
    error: true,
    message: 'Not found',
    path: req.path
  });
});

// Initialize and start server
async function startServer(): Promise<void> {
  try {
    // Initialize database
    const dbPath = process.env.DATABASE_URL || 'bendcore.db';
    const db = new Database(dbPath);
    await db.initialize();
    console.log(`Database initialized at ${dbPath}`);

    // Set database for project routes
    setDatabase(db);

    // Load bending profiles
    const profileManager = ProfileManager.getInstance();
    const profilesPath = path.join(process.cwd(), '..', 'data');
    const loaded = await profileManager.loadProfiles(profilesPath);

    if (!loaded) {
      console.warn('Warning: Failed to load bending profiles from', profilesPath);
    } else {
      console.log(
        `Bending profiles loaded successfully (${profileManager.listProfileIds().length} profiles)`
      );
    }

    // Start listening
    const server = app.listen(port, (): void => {
      console.log(`BendCore Backend v1.0.0 listening on port ${port}`);
      console.log(`Environment: ${env}`);
      console.log(`Database: ${dbPath}`);
    });

    // Graceful shutdown
    process.on('SIGTERM', async () => {
      console.log('SIGTERM received, shutting down gracefully...');
      server.close(async () => {
        await db.close();
        process.exit(0);
      });
    });

    process.on('SIGINT', async () => {
      console.log('SIGINT received, shutting down gracefully...');
      server.close(async () => {
        await db.close();
        process.exit(0);
      });
    });
  } catch (err) {
    console.error('Failed to start server:', err);
    process.exit(1);
  }
}

startServer();

export default app;
