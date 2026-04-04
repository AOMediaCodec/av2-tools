# WebAssembly Browser Tool

Browser-based AV2 bitstream analyzer built with C++ (compiled to WebAssembly) and React.

## Quick Start

### Prerequisites
- **Emscripten SDK**: https://emscripten.org/docs/getting_started/downloads.html
- **Node.js 18+**: https://nodejs.org/

### Build and Run

```bash
# 1. Activate Emscripten
source /path/to/emsdk/emsdk_env.sh

# 2. Build & Copy WASM module
emcmake cmake -S . -B build-wasm -DBUILD_WASM=ON -DBUILD_TESTS=OFF
cmake --build build-wasm -j
cp build-wasm/src/wasm/av2-parser.* web/public/wasm/

# 3. Run frontend (development mode)
cd web
npm install
npm run dev 
# or `npm run build` for production in web/dist/ 
# or `npm run preview` Preview Production Build
```

Opens at http://localhost:3000

## Architecture

```
src/wasm/
├── av2_wasm_api.cpp       # API wrapper
└── CMakeLists.txt         # WASM build config

web/
├── src/
│   ├── components/       # React UI components
│   ├── wasm/             # WASM loader
│   └── types/            # TypeScript types
├── public/wasm/          # Built WASM files
└── package.json
```

**C++ WASM API** (`src/wasm/`):
- `av2_parse_to_json()` - Parse bitstream -> JSON
- `av2_free_json()` - Free JSON string
- `av2_get_last_error()` - Get error message

**React Frontend** (`web/src/`):
- `OBUBrowser.tsx` - Hierarchical OBU viewer
- `SpotlightSearch.tsx` - Keyboard-driven search (press `/`)
- `JsonHybridViewer.tsx` - Expandable syntax tree with tables
- `av2-parser.ts` - WASM loader wrapper
