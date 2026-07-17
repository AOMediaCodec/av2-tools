// WebAssembly loader and API wrapper for AV2 parser

interface AV2Module extends EmscriptenModule {
  ccall: (
    ident: string,
    returnType: string | null,
    argTypes: string[],
    args: unknown[]
  ) => unknown;
  cwrap: (
    ident: string,
    returnType: string | null,
    argTypes: string[]
  ) => (...args: unknown[]) => unknown;
  _malloc: (size: number) => number;
  _free: (ptr: number) => void;
  HEAPU8: Uint8Array;
}

interface EmscriptenModule {
  // Add minimal Emscripten module interface
}

let wasmModule: AV2Module | null = null;

/**
 * Initialize the WebAssembly module
 * Must be called before parseAV2Bitstream()
 */
export async function initWasm(): Promise<void> {
  if (wasmModule) {
    return; // Already initialized
  }

  // Load Emscripten-generated JS via script tag (Vite doesn't allow imports from /public)
  const createModule = await loadEmscriptenModule();
  wasmModule = await createModule({
    locateFile: (path: string) => {
      // Resolve against the page base so it works under any deploy path
      // (root, GitHub project page /av2-tools/, or local dev).
      if (path.endsWith('.wasm')) {
        return new URL(`wasm/${path}`, document.baseURI).href;
      }
      return path;
    },
  });
  console.log('[WASM] AV2 parser module loaded');
}

/**
 * Load Emscripten module factory via script tag
 */
function loadEmscriptenModule(): Promise<(config?: any) => Promise<AV2Module>> {
  return new Promise((resolve, reject) => {
    // Check if already loaded
    if ((window as any).createAV2Module) {
      resolve((window as any).createAV2Module);
      return;
    }

    const script = document.createElement('script');
    script.src = new URL('wasm/av2-parser.js', document.baseURI).href;
    script.async = true;

    script.onload = () => {
      if ((window as any).createAV2Module) {
        resolve((window as any).createAV2Module);
      } else {
        reject(new Error('Module factory not found after loading script'));
      }
    };

    script.onerror = () => {
      reject(new Error('Failed to load av2-parser.js'));
    };

    document.head.appendChild(script);
  });
}

/**
 * Parse AV2 bitstream and return JSON structure
 * @param data - AV2 bitstream as Uint8Array
 * @returns Parsed OBU structure
 */
export async function parseAV2Bitstream(data: Uint8Array): Promise<any> {
  if (!wasmModule) {
    throw new Error('WASM module not initialized. Call initWasm() first.');
  }

  let dataPtr = 0;
  let resultPtr = 0;

  try {
    // Allocate memory for input data
    dataPtr = wasmModule._malloc(data.length);
    if (!dataPtr) {
      throw new Error('Failed to allocate WASM memory');
    }

    // Copy JS array to WASM heap
    wasmModule.HEAPU8.set(data, dataPtr);

    // Call C function: av2_parse_to_json(const uint8_t* data, size_t size)
    resultPtr = wasmModule.ccall(
      'av2_parse_to_json',
      'number',
      ['number', 'number'],
      [dataPtr, data.length]
    ) as number;

    if (!resultPtr) {
      // Get error message
      const errorPtr = wasmModule.ccall(
        'av2_get_last_error',
        'number',
        [],
        []
      ) as number;

      const errorMsg = readCString(wasmModule.HEAPU8, errorPtr);
      throw new Error(`Parse failed: ${errorMsg}`);
    }

    // Read JSON string from WASM heap
    const jsonString = readCString(wasmModule.HEAPU8, resultPtr);

    // Parse JSON
    const result = JSON.parse(jsonString);

    return result;
  } finally {
    // Clean up memory
    if (dataPtr) {
      wasmModule._free(dataPtr);
    }
    if (resultPtr) {
      wasmModule.ccall('av2_free_json', null, ['number'], [resultPtr]);
    }
  }
}

/**
 * Read null-terminated C string from WASM heap
 */
function readCString(heap: Uint8Array, ptr: number): string {
  const bytes: number[] = [];
  let offset = ptr;

  while (heap[offset] !== 0) {
    bytes.push(heap[offset]);
    offset++;
  }

  return new TextDecoder().decode(new Uint8Array(bytes));
}

/**
 * Check if WASM is supported in this browser
 */
export function isWasmSupported(): boolean {
  try {
    if (typeof WebAssembly === 'object' && typeof WebAssembly.instantiate === 'function') {
      // Test with minimal module
      const module = new WebAssembly.Module(
        Uint8Array.of(0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00)
      );
      return module instanceof WebAssembly.Module;
    }
  } catch {
    return false;
  }
  return false;
}
