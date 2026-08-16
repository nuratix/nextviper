export interface CheckDiagnostic {
  code: string;
  message: string;
  file: string;
  line: number;
  column: number;
  doc_url: string;
}

export interface CheckResult {
  success: boolean;
  diagnostics: CheckDiagnostic[];
  raw?: string;
  error?: string;
}

export interface SpawnResult {
  status: number | null;
  stdout: string;
  stderr: string;
}

export declare const version: string;
export declare function getBinaryPath(): string | null;
export declare function run(filePath: string, args?: string[], options?: Record<string, any>): SpawnResult;
export declare function check(filePath: string, options?: Record<string, any>): CheckResult;
export declare function eval(code: string, options?: Record<string, any>): SpawnResult;
