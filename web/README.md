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

## Loading bitstreams via URL

The analyzer can auto-load a remote `.obu` file via a `?url=` query
parameter:

```
https://aomediacodec.github.io/av2-tools/?url=https://podborski-av2-interop-public.s3.us-west-1.amazonaws.com/streams/av2_interop_01_lowdelay.obu
```

For security, only origins listed in `ALLOWED_FETCH_ORIGINS` (in
`web/src/App.tsx`) are accepted -- the app will not act as a generic
proxy. Add new buckets there as needed.

The remote bucket must serve the `.obu` with a CORS policy that allows
`GET` from the analyzer's origin (e.g. `https://aomediacodec.github.io`
and `http://localhost:3000` for development).
