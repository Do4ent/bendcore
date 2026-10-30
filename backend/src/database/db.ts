/**
 * BendCore Database Abstraction
 * Simple SQLite database layer for projects and persistence
 */

import sqlite3 from 'sqlite3';
import path from 'path';
import { Project, Tube } from '../models/types';
import { v4 as uuidv4 } from 'uuid';

interface ProjectRow {
  id: string;
  name: string;
  description: string | null;
  created_at: string;
  updated_at: string;
  metadata: string | null;
}

interface TubeRow {
  id: string;
  name: string;
  specification: string;
  sketch: string | null;
  geometry: string | null;
  properties: string | null;
  created_at: string;
  updated_at: string;
}

/** Результат INSERT/UPDATE/DELETE: sqlite3 передаёт его через `this` колбэка, а не аргументом. */
interface RunResult {
  changes: number;
  lastID: number;
}

function tubeFromRow(t: TubeRow): Tube {
  return {
    id: t.id,
    name: t.name,
    specification: JSON.parse(t.specification),
    sketch: t.sketch ? JSON.parse(t.sketch) : undefined,
    geometry: t.geometry ? JSON.parse(t.geometry) : undefined,
    properties: t.properties ? JSON.parse(t.properties) : undefined,
    created_at: t.created_at,
    updated_at: t.updated_at,
  };
}

export class Database {
  private db: sqlite3.Database | null = null;
  private dbPath: string;

  constructor(dbPath: string = 'bendcore.db') {
    this.dbPath = dbPath;
  }

  private connection(): sqlite3.Database {
    if (!this.db) throw new Error('Database not initialized');
    return this.db;
  }

  private run(sql: string, params: unknown[] = []): Promise<RunResult> {
    const db = this.connection();
    return new Promise((resolve, reject) => {
      db.run(sql, params, function (this: sqlite3.RunResult, err: Error | null) {
        if (err) reject(err);
        else resolve({ changes: this.changes, lastID: this.lastID });
      });
    });
  }

  private get<T>(sql: string, params: unknown[] = []): Promise<T | undefined> {
    const db = this.connection();
    return new Promise((resolve, reject) => {
      db.get(sql, params, (err: Error | null, row: T | undefined) => (err ? reject(err) : resolve(row)));
    });
  }

  private all<T>(sql: string, params: unknown[] = []): Promise<T[]> {
    const db = this.connection();
    return new Promise((resolve, reject) => {
      db.all(sql, params, (err: Error | null, rows: T[]) => (err ? reject(err) : resolve(rows)));
    });
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

    await this.run(`
      CREATE TABLE IF NOT EXISTS projects (
        id TEXT PRIMARY KEY,
        name TEXT NOT NULL,
        description TEXT,
        created_at TEXT NOT NULL,
        updated_at TEXT NOT NULL,
        metadata TEXT
      )
    `);

    await this.run(`
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

    await this.run(`
      CREATE TABLE IF NOT EXISTS users (
        id TEXT PRIMARY KEY,
        email TEXT UNIQUE NOT NULL,
        name TEXT,
        created_at TEXT NOT NULL,
        updated_at TEXT NOT NULL
      )
    `);

    await this.run(`
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

    await this.run(
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

    const project = await this.get<ProjectRow>('SELECT * FROM projects WHERE id = ?', [id]);

    if (!project) {
      return null;
    }

    const tubes = await this.all<TubeRow>('SELECT * FROM tubes WHERE project_id = ?', [id]);

    return {
      id: project.id,
      name: project.name,
      description: project.description ?? undefined,
      created_at: project.created_at,
      updated_at: project.updated_at,
      tubes: tubes.map(tubeFromRow),
      metadata: project.metadata ? JSON.parse(project.metadata) : undefined
    };
  }

  /**
   * List all projects
   */
  async listProjects(): Promise<Project[]> {
    if (!this.db) throw new Error('Database not initialized');

    const rows = await this.all<{ id: string }>('SELECT id FROM projects ORDER BY updated_at DESC');
    const projects = await Promise.all(rows.map((r) => this.getProject(r.id)));
    return projects.filter((p): p is Project => p !== null);
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
    const now = new Date().toISOString();

    await this.run(
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

    await this.run('DELETE FROM tubes WHERE project_id = ?', [id]);
    await this.run('DELETE FROM project_shares WHERE project_id = ?', [id]);
    const result = await this.run('DELETE FROM projects WHERE id = ?', [id]);

    return result.changes > 0;
  }

  /**
   * Create a new tube in a project
   */
  async createTube(projectId: string, name: string, spec: unknown): Promise<Tube> {
    if (!this.db) throw new Error('Database not initialized');

    const id = uuidv4();
    const now = new Date().toISOString();

    await this.run(
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

    const tube = await this.get<TubeRow>('SELECT * FROM tubes WHERE id = ?', [id]);

    if (!tube) {
      return null;
    }

    return tubeFromRow(tube);
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
    const now = new Date().toISOString();

    await this.run(
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
    const result = await this.run('DELETE FROM tubes WHERE id = ?', [id]);

    return result.changes > 0;
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
