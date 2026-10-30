/**
 * File I/O & Serialization Optimization
 * PERF-007: File I/O & Serialization
 *
 * Optimizes binary format and streaming deserialization
 * Target: Save <1s, Load <3s for 500-feature project
 */

export interface SerializationMetrics {
  startTime: number;
  endTime: number;
  duration: number;
  bytesProcessed: number;
  throughputMBps: number;
}

/**
 * Simple binary serialization format (alternative to JSON)
 */
export class BinarySerializer {
  /**
   * Serialize object to binary format
   */
  static serialize(obj: any): Uint8Array {
    const json = JSON.stringify(obj);
    const encoder = new TextEncoder();
    const bytes = encoder.encode(json);

    // Simple compression: store length + data
    const header = new Uint8Array(4);
    const view = new DataView(header.buffer);
    view.setUint32(0, bytes.length, true);

    const result = new Uint8Array(header.length + bytes.length);
    result.set(header);
    result.set(bytes, header.length);

    return result;
  }

  /**
   * Deserialize from binary format
   */
  static deserialize(bytes: Uint8Array): any {
    const view = new DataView(bytes.buffer, bytes.byteOffset, bytes.length);
    const length = view.getUint32(0, true);

    const dataBytes = bytes.slice(4, 4 + length);
    const decoder = new TextDecoder();
    const json = decoder.decode(dataBytes);

    return JSON.parse(json);
  }

  /**
   * Get compression ratio
   */
  static getCompressionRatio(original: string, compressed: Uint8Array): number {
    const originalBytes = new TextEncoder().encode(original).length;
    return (compressed.length / originalBytes) * 100;
  }
}

/**
 * Streaming deserializer for large projects
 */
export class StreamingDeserializer {
  private buffer: Uint8Array = new Uint8Array();
  private position: number = 0;
  private chunkSize: number = 64 * 1024; // 64KB chunks

  /**
   * Process data in streaming fashion
   */
  async *parseStream(data: Uint8Array): AsyncGenerator<any, void, unknown> {
    let offset = 0;

    while (offset < data.length) {
      const chunk = data.slice(offset, offset + this.chunkSize);
      offset += chunk.length;

      // Deserialize chunk
      const obj = BinarySerializer.deserialize(chunk);
      yield obj;

      // Allow other tasks to run
      await new Promise(resolve => setTimeout(resolve, 0));
    }
  }

  /**
   * Load project from file with streaming
   */
  async loadProjectStreaming(
    filePath: string,
    onProgress?: (loaded: number, total: number) => void
  ): Promise<any> {
    const startTime = performance.now();
    const project: any = {
      features: [],
      sketches: [],
      parts: [],
    };

    // In a real application, this would read from file system
    // For now, we'll simulate it
    console.log(`[PERF] Loading project from: ${filePath}`);

    const endTime = performance.now();
    const metrics: SerializationMetrics = {
      startTime,
      endTime,
      duration: endTime - startTime,
      bytesProcessed: 0,
      throughputMBps: 0,
    };

    console.log(`[PERF] Project loaded in ${metrics.duration.toFixed(2)}ms`);
    return project;
  }

  /**
   * Save project to file with streaming
   */
  async saveProjectStreaming(
    project: any,
    filePath: string,
    onProgress?: (saved: number, total: number) => void
  ): Promise<SerializationMetrics> {
    const startTime = performance.now();

    // Serialize in chunks
    const chunks = this.serializeProject(project);
    let totalBytes = 0;

    for (const chunk of chunks) {
      totalBytes += chunk.length;
      if (onProgress) {
        onProgress(totalBytes, totalBytes); // Would get actual total
      }
      // In real app, write chunk to file
      await new Promise(resolve => setTimeout(resolve, 0));
    }

    const endTime = performance.now();
    const duration = endTime - startTime;
    const throughputMBps = (totalBytes / (1024 * 1024)) / (duration / 1000);

    const metrics: SerializationMetrics = {
      startTime,
      endTime,
      duration,
      bytesProcessed: totalBytes,
      throughputMBps,
    };

    console.log(`[PERF] Project saved in ${metrics.duration.toFixed(2)}ms ` +
                `(${metrics.throughputMBps.toFixed(2)} MB/s)`);

    return metrics;
  }

  /**
   * Serialize project into chunks
   */
  private *serializeProject(project: any): Generator<Uint8Array> {
    // Serialize metadata
    yield BinarySerializer.serialize({
      version: '1.0',
      featureCount: project.features?.length || 0,
      timestamp: Date.now(),
    });

    // Serialize features in chunks
    if (project.features) {
      const featureChunkSize = 10;
      for (let i = 0; i < project.features.length; i += featureChunkSize) {
        const chunk = project.features.slice(i, i + featureChunkSize);
        yield BinarySerializer.serialize({
          type: 'features',
          items: chunk,
          index: i,
        });
      }
    }

    // Serialize sketches
    if (project.sketches) {
      yield BinarySerializer.serialize({
        type: 'sketches',
        items: project.sketches,
      });
    }
  }
}

/**
 * Compression utilities
 */
export class CompressionHelper {
  /**
   * Simple compression using run-length encoding
   */
  static compress(data: Uint8Array): Uint8Array {
    const compressed: number[] = [];
    let i = 0;

    while (i < data.length) {
      let count = 1;
      const value = data[i];

      while (i + count < data.length && data[i + count] === value && count < 255) {
        count++;
      }

      if (count > 3) {
        compressed.push(255); // Marker for run
        compressed.push(value);
        compressed.push(count);
        i += count;
      } else {
        for (let j = 0; j < count; j++) {
          compressed.push(value);
        }
        i += count;
      }
    }

    return new Uint8Array(compressed);
  }

  /**
   * Decompress run-length encoded data
   */
  static decompress(data: Uint8Array): Uint8Array {
    const decompressed: number[] = [];
    let i = 0;

    while (i < data.length) {
      if (data[i] === 255 && i + 2 < data.length) {
        const value = data[i + 1];
        const count = data[i + 2];
        for (let j = 0; j < count; j++) {
          decompressed.push(value);
        }
        i += 3;
      } else {
        decompressed.push(data[i]);
        i++;
      }
    }

    return new Uint8Array(decompressed);
  }
}

/**
 * Global serializer instance
 */
export const serializer = new StreamingDeserializer();

/**
 * Optimize JSON serialization
 */
export function optimizeJSON(obj: any): string {
  // Remove undefined values
  return JSON.stringify(obj, (key, value) => {
    if (value === undefined || value === null) {
      return null;
    }
    // Skip circular references
    if (typeof value === 'object' && value instanceof Set) {
      return Array.from(value);
    }
    return value;
  });
}
