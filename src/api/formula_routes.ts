/**
 * BendCore Formula API Routes - Q3 2027 Task 1 Phase 2
 * Express routes for formula evaluation and validation
 * Integrates C++ formula engine with React frontend
 */

import express, { Router, Request, Response, NextFunction } from 'express';
import { spawn } from 'child_process';
import path from 'path';
import os from 'os';
import fs from 'fs/promises';

const router = Router();

/**
 * Interface for formula evaluation request
 */
interface FormulaRequest {
  formula: string;
  variables?: Record<string, number>;
}

/**
 * Interface for formula evaluation response
 */
interface FormulaResponse {
  success: boolean;
  result?: number;
  message?: string;
  suggestions?: string[];
}

/**
 * Call native C++ formula evaluator
 * Uses IPC or external process if direct binding not available
 */
async function evaluateFormulaNative(
  formula: string,
  variables: Record<string, number>
): Promise<FormulaResponse> {
  return new Promise((resolve, reject) => {
    // For now, implement a JavaScript fallback
    // In production, this would call the compiled C++ module
    try {
      const result = evaluateFormulaFallback(formula, variables);
      resolve({
        success: true,
        result,
      });
    } catch (error) {
      const message = error instanceof Error ? error.message : 'Unknown error';
      resolve({
        success: false,
        message,
        suggestions: generateSuggestions(message, variables),
      });
    }
  });
}

/**
 * JavaScript fallback for formula evaluation
 * Matches C++ parser behavior for basic expressions
 */
function evaluateFormulaFallback(
  formula: string,
  variables: Record<string, number>
): number {
  if (!formula || !formula.trim()) {
    throw new Error('Formula cannot be empty');
  }

  // Replace variables with their values
  let expression = formula;
  for (const [varName, value] of Object.entries(variables)) {
    expression = expression.replace(new RegExp(`\\${varName}`, 'g'), `(${value})`);
  }

  // Validate before evaluation
  if (!/^[\d\s()+\-*/^.]+$/.test(expression.replace(/\^/g, '**'))) {
    throw new Error('Invalid characters in formula');
  }

  // Replace ^ with ** for exponentiation
  expression = expression.replace(/\^/g, '**');

  // Add support for sqrt, abs, min, max
  expression = expression.replace(/sqrt\(/g, 'Math.sqrt(');
  expression = expression.replace(/abs\(/g, 'Math.abs(');
  expression = expression.replace(/min\(/g, 'Math.min(');
  expression = expression.replace(/max\(/g, 'Math.max(');

  try {
    // Use Function constructor for safe evaluation (still risky, but better than eval)
    const result = new Function(`return ${expression}`)();

    if (typeof result !== 'number' || !isFinite(result)) {
      throw new Error('Invalid result');
    }

    return result;
  } catch (error) {
    const message = error instanceof Error ? error.message : 'Evaluation failed';

    if (message.includes('undefined')) {
      throw new Error(
        `Undefined variable: ${extractUndefinedVar(formula, variables)}`
      );
    } else if (message.includes('sqrt')) {
      throw new Error('sqrt of negative number');
    } else if (message.includes('Infinity')) {
      throw new Error('Division by zero');
    } else {
      throw new Error(message);
    }
  }
}

/**
 * Extract undefined variable name for error message
 */
function extractUndefinedVar(
  formula: string,
  variables: Record<string, number>
): string {
  const varPattern = /\$[A-Za-z_][A-Za-z0-9_]*/g;
  const matches = formula.match(varPattern);

  if (!matches) return 'unknown';

  for (const varName of matches) {
    if (!(varName in variables)) {
      return varName;
    }
  }

  return 'unknown';
}

/**
 * Generate helpful suggestions based on error type
 */
function generateSuggestions(
  errorMsg: string,
  variables: Record<string, number>
): string[] {
  const suggestions: string[] = [];

  if (errorMsg.includes('Division by zero')) {
    suggestions.push('Check for division by zero');
    suggestions.push('Ensure denominator is not zero');
  } else if (errorMsg.includes('sqrt of negative')) {
    suggestions.push('sqrt() only works with non-negative numbers');
    suggestions.push('Consider using abs() to ensure positive input');
  } else if (errorMsg.includes('Undefined variable')) {
    suggestions.push('Check variable names (use $OD, $Wall, etc.)');
    const availableVars = Object.keys(variables);
    if (availableVars.length > 0) {
      suggestions.push(`Available variables: ${availableVars.join(', ')}`);
    }
  } else if (errorMsg.includes('Invalid characters')) {
    suggestions.push('Check formula syntax');
    suggestions.push('Use only: +, -, *, /, ^, and parentheses');
  } else {
    suggestions.push('Verify formula syntax');
    suggestions.push('Example: $OD * 2 + 5');
  }

  return suggestions;
}

/**
 * POST /api/bendcore/evaluate-formula
 * Evaluate a parametric formula with variables
 */
router.post('/evaluate-formula', async (req: Request, res: Response) => {
  try {
    const { formula, variables = {} } = req.body as FormulaRequest;

    // Validate input
    if (!formula || typeof formula !== 'string') {
      return res.status(400).json({
        success: false,
        message: 'Formula must be a non-empty string',
      });
    }

    if (formula.length > 1000) {
      return res.status(400).json({
        success: false,
        message: 'Formula too long (max 1000 characters)',
      });
    }

    // Validate variables
    if (typeof variables !== 'object' || variables === null) {
      return res.status(400).json({
        success: false,
        message: 'Variables must be an object',
      });
    }

    // Ensure all variable values are numbers
    const cleanedVars: Record<string, number> = {};
    for (const [key, value] of Object.entries(variables)) {
      if (typeof value === 'number') {
        cleanedVars[key] = value;
      }
    }

    // Evaluate formula
    const response = await evaluateFormulaNative(formula, cleanedVars);
    res.json(response);
  } catch (error) {
    console.error('Formula evaluation error:', error);
    res.status(500).json({
      success: false,
      message: 'Internal server error',
    });
  }
});

/**
 * POST /api/bendcore/validate-formula
 * Validate formula syntax without evaluation
 * Used for real-time syntax checking in UI
 */
router.post('/validate-formula', (req: Request, res: Response) => {
  try {
    const { formula } = req.body as { formula?: string };

    if (!formula || typeof formula !== 'string') {
      return res.status(400).json({
        valid: false,
        message: 'Formula must be a non-empty string',
      });
    }

    if (formula.length > 1000) {
      return res.status(400).json({
        valid: false,
        message: 'Formula too long (max 1000 characters)',
      });
    }

    // Basic syntax validation
    const varPattern = /\$[A-Za-z_][A-Za-z0-9_]*/g;
    const funcPattern = /(sqrt|abs|min|max)\(/g;
    const operatorPattern = /[+\-*/^()]/g;

    // Check for invalid characters
    const cleaned = formula
      .replace(varPattern, 'v')
      .replace(funcPattern, 'f(')
      .replace(operatorPattern, 'o')
      .replace(/[\d.]/g, 'n')
      .replace(/\s/g, '');

    if (!/^[nvfo]+$/.test(cleaned)) {
      return res.json({
        valid: false,
        message: 'Invalid characters in formula',
      });
    }

    // Try to parse with fallback evaluator
    try {
      evaluateFormulaFallback(formula, {});
    } catch (error) {
      const msg = error instanceof Error ? error.message : 'Parse error';
      // Some errors are expected (undefined vars), so we're lenient
      if (msg.includes('Undefined variable')) {
        return res.json({
          valid: true,
          message: 'Formula syntax is valid',
        });
      }
      return res.json({
        valid: false,
        message: msg,
      });
    }

    res.json({
      valid: true,
      message: 'Formula syntax is valid',
    });
  } catch (error) {
    console.error('Formula validation error:', error);
    res.status(500).json({
      valid: false,
      message: 'Internal server error',
    });
  }
});

/**
 * GET /api/bendcore/formula-info
 * Get formula information and extracted variables/functions
 * Useful for formula builder UI
 */
router.post('/formula-info', (req: Request, res: Response) => {
  try {
    const { formula } = req.body as { formula?: string };

    if (!formula || typeof formula !== 'string') {
      return res.status(400).json({
        error: 'Formula must be a non-empty string',
      });
    }

    // Extract variables
    const varPattern = /\$[A-Za-z_][A-Za-z0-9_]*/g;
    const variables = Array.from(new Set(formula.match(varPattern) || []));

    // Extract functions
    const funcPattern = /(sqrt|abs|min|max)/g;
    const functions = Array.from(new Set(formula.match(funcPattern) || []));

    // Count operators
    const operatorCount = (formula.match(/[+\-*/^]/g) || []).length;

    res.json({
      success: true,
      formula,
      length: formula.length,
      variables,
      functions,
      operator_count: operatorCount,
      complexity: estimateComplexity(formula),
    });
  } catch (error) {
    console.error('Formula info error:', error);
    res.status(500).json({
      success: false,
      error: 'Internal server error',
    });
  }
});

/**
 * Estimate formula complexity for UI feedback
 */
function estimateComplexity(formula: string): 'simple' | 'moderate' | 'complex' {
  const operatorCount = (formula.match(/[+\-*/^]/g) || []).length;
  const parenCount = (formula.match(/[()]/g) || []).length;

  if (operatorCount <= 2 && parenCount === 0) {
    return 'simple';
  } else if (operatorCount <= 5 && parenCount <= 4) {
    return 'moderate';
  } else {
    return 'complex';
  }
}

/**
 * Error handling middleware for formula routes
 */
router.use((error: any, req: Request, res: Response, next: NextFunction) => {
  console.error('Formula API error:', error);
  res.status(500).json({
    success: false,
    message: 'Internal server error',
  });
});

export default router;
