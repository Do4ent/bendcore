/**
 * BendCore Database Abstraction
 * Simple SQLite database layer for projects and persistence
 */

import sqlite3 from 'sqlite3';
import { promisify } from 'util';
import path from 'path';
import { Project, Tube } from '../models/types';
import { v4 as uuidv4 } from 'uuid';

export class Database {
  private db: sqlite3.Database | null = null;
  private dbPath: string;

  constructor(dbPath: string = 'bendcore.db') {
    this.dbPath = dbPath;
  }

  /**
   * Initialize database and create tables
   */
  async initialize(): Promise<void> {
    return new Promise((resolve, reject) => {
      this.db = new sqlite3.Database(this.dbPath, async (err) => {
        if (err) {
          reject(err);
          return;
        }

        try {
          await this.createTables();
          resolve();
        } catch (e) {
          reject(e);
        }
      });
    });
  }

  /**
   * Create database tables
   */
  private async createTables(): Promise<void> {
    if (!this.db) throw new Error('Database not initialized');

    const run = promisify(this.db.run.bind(this.db));

    await run(`
      CREATE TABLE IF NOT EXISTS projects (
        id TEXT PRIMARY KEY,
        name TEXT NOT NULL,
        description TEXT,
        created_at TEXT NOT NULL,
        updated_at TEXT NOT NULL,
        metadata TEXT
      )
    `);

    await run(`
      CREATE TABLE IF NOT EXISTS tubes (
        id TEXT PRIMARY KEY,
        project_id TEXT NOT NULL,
        name TEXT NOT NULL,
        specification TEXT NOT NULL,
        sketch TEXT,
        geometry TEXT,
        properties TEXT,
        created_at TEXT NOT NULL,
        updated_at TEXT NOT NULL,
        FOREIGN KEY (project_id) REFERENCES projects (id)
      )
    `);

    await run(`
      CREATE TABLE IF NOT EXISTS users (
        id TEXT PRIMARY KEY,
        email TEXT UNIQUE NOT NULL,
        name TEXT,
        created_at TEXT NOT NULL,
        updated_at TEXT NOT NULL
      )
    `);

    await run(`
      CREATE TABLE IF NOT EXISTS project_shares (
        id TEXT PRIMARY KEY,
        project_id TEXT NOT NULL,
        user_id TEXT NOT NULL,
        permission TEXT NOT NULL,
        created_at TEXT NOT NULL,
        FOREIGN KEY (project_id) REFERENCES projects (id),
        FOREIGN KEY (user_id) REFERENCES users (id)
      )
    `);
  }

  /**
   * Create a new project
   */
  async createProject(name: string, description?: string): Promise<Project> {
    if (!this.db) throw new Error('Database not initialized');

    const id = uuidv4();
    const now = new Date().toISOString();

    const run = promisify(this.db.run.bind(this.db));

    await run(
      `INSERT INTO projects (id, name, description, created_at, updated_at) VALUES (?, ?, ?, ?, ?)`,
      [id, name, description || null, now, now]
    );

    return {
      id,
      name,
      description,
      created_at: now,
      updated_at: now,
      tubes: []
    };
  }

  /**
   * Get project by ID
   */
  async getProject(id: string): Promise<Project | null> {
    if (!this.db) throw new Error('Database not initialized');

    const get = promisify(this.db.get.bind(this.db));
    const all = promisify(this.db.all.bind(this.db));

    const project = await get('SELECT * FROM projects WHERE id = ?', [id]);

    if (!project) {
      return null;
    }

    const tubes = await all('SELECT * FROM tubes WHERE project_id = ?', [id]);

    return {
      id: project.id,
      name: project.name,
      description: project.description,
      created_at: project.created_at,
      updated_at: project.updated_at,
      tubes: tubes.map((t: Record<string, unknown>) => ({
        id: t.id,
        name: t.name,
        specification: JSON.parse(String(t.specification)),
        sketch: t.sketch ? JSON.parse(String(t.sketch)) : undefined,
        geometry: t.geometry ? JSON.parse(String(t.geometry)) : undefined,
        properties: t.properties ? JSON.parse(String(t.properties)) : undefined,
        created_at: t.created_at,
        updated_at: t.updated_at
      })),
      metadata: project.metadata ? JSON.parse(project.metadata) : undefined
    };
  }

  /**
   * List all projects
   */
  async listProjects(): Promise<Project[]> {
    if (!this.db) throw new Error('Database not initialized');

    const all = promisify(this.db.all.bind(this.db));

    const projects = await all('SELECT * FROM projects ORDER BY updated_at DESC');

    return Promise.all(
      projects.map((p: Record<string, unknown>) =>
        this.getProject(String(p.id))
      ).filter((p): p is Promise<Project> => p !== null)
    );
  }

  /**
   * Update project
   */
  async updateProject(
    id: string,
    name?: string,
    description?: string
  ): Promise<Project | null> {
    if (!this.db) throw new Error('Database not initialized');

    const run = promisify(this.db.run.bind(this.db));
    const now = new Date().toISOString();

    await run(
      `UPDATE projects SET name = COALESCE(?, name), description = COALESCE(?, description), updated_at = ? WHERE id = ?`,
      [name || null, description || null, now, id]
    );

    return this.getProject(id);
  }

  /**
   * Delete project
   */
  async deleteProject(id: string): Promise<boolean> {
    if (!this.db) throw new Error('Database not initialized');

    const run = promisify(this.db.run.bind(this.db));

    await run('DELETE FROM tubes WHERE project_id = ?', [id]);
    await run('DELETE FROM project_shares WHERE project_id = ?', [id]);
    const result = await run('DELETE FROM projects WHERE id = ?', [id]);

    return (result as { changes: number }).changes > 0;
  }

  /**
   * Create a new tube in a project
   */
  async createTube(projectId: string, name: string, spec: unknown): Promise<Tube> {
    if (!this.db) throw new Error('Database not initialized');

    const id = uuidv4();
    const now = new Date().toISOString();
    const run = promisify(this.db.run.bind(this.db));

    await run(
      `INSERT INTO tubes (id, project_id, name, specification, created_at, updated_at) VALUES (?, ?, ?, ?, ?, ?)`,
      [id, projectId, name, JSON.stringify(spec), now, now]
    );

    return {
      id,
      name,
      specification: spec as any,
      created_at: now,
      updated_at: now
    };
  }

  /**
   * Get tube
   */
  async getTube(id: string): Promise<Tube | null> {
    if (!this.db) throw new Error('Database not initialized');

    const get = promisify(this.db.get.bind(this.db));

    const tube = await get('SELECT * FROM tubes WHERE id = ?', [id]);

    if (!tube) {
      return null;
    }

    return {
      id: tube.id,
      name: tube.name,
      specification: JSON.parse(String(tube.specification)),
      sketch: tube.sketch ? JSON.parse(String(tube.sketch)) : undefined,
      geometry: tube.geometry ? JSON.parse(String(tube.geometry)) : undefined,
      properties: tube.properties ? JSON.parse(String(tube.properties)) : undefined,
      created_at: tube.created_at,
      updated_at: tube.updated_at
    };
  }

  /**
   * Update tube geometry and properties
   */
  async updateTubeGeometry(
    id: string,
    geometry: unknown,
    properties: unknown
  ): Promise<Tube | null> {
    if (!this.db) throw new Error('Database not initialized');

    const run = promisify(this.db.run.bind(this.db));
    const now = new Date().toISOString();

    await run(
      `UPDATE tubes SET geometry = ?, properties = ?, updated_at = ? WHERE id = ?`,
      [JSON.stringify(geometry), JSON.stringify(properties), now, id]
    );

    return this.getTube(id);
  }

  /**
   * Delete tube
   */
  async deleteTube(id: string): Promise<boolean> {
    if (!this.db) throw new Error('Database not initialized');

    const run = promisify(this.db.run.bind(this.db));
    const result = await run('DELETE FROM tubes WHERE id = ?', [id]);

    return (result as { changes: number }).changes > 0;
  }

  /**
   * Close database
   */
  close(): Promise<void> {
    return new Promise((resolve, reject) => {
      if (!this.db) {
        resolve();
        return;
      }
      this.db.close((err) => {
        if (err) reject(err);
        else resolve();
      });
    });
  }
}
