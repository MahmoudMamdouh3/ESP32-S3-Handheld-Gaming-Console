// Ambient type declarations shared by browser simulator and Node.js harness checks.

interface Window {
  BMO_PLAYTEST_WASM_B64?: string;
  BMO_ENCLOSURE_MESHES?: Record<string, any>;
}

interface Navigator {
  serial?: {
    requestPort(): Promise<any>;
  };
}

declare const process: {
  argv: string[];
  env: Record<string, string | undefined>;
  versions?: { node?: string };
  exitCode?: number;
  exit(code?: number): never;
  memoryUsage(): { rss: number; heapTotal: number; heapUsed: number; external: number };
  stdout: { write(chunk: string): boolean };
};

declare module "node:fs" {
  const fsMod: {
    readFileSync(path: string, encoding?: null): Uint8Array<ArrayBuffer>;
    readFileSync(path: string, encoding: string): string;
    statSync(path: string): { size: number };
    existsSync(path: string): boolean;
  };
  export default fsMod;
}

declare module "node:path" {
  const pathMod: {
    dirname(p: string): string;
    join(...paths: string[]): string;
    resolve(...paths: string[]): string;
  };
  export const dirname: typeof pathMod.dirname;
  export const join: typeof pathMod.join;
  export const resolve: typeof pathMod.resolve;
  export default pathMod;
}

declare module "node:module" {
  export function createRequire(filename: string | URL): (id: string) => any;
}

declare module "node:perf_hooks" {
  export const performance: { now(): number };
}

declare module "node:url" {
  export function fileURLToPath(url: string | URL): string;
  export function pathToFileURL(p: string): URL;
}
