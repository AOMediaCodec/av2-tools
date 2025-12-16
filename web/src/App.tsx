import { useState, useCallback, useEffect } from 'react';
import { FileUpload } from './components/FileUpload';
import { OBUBrowser } from './components/OBUBrowser';
import { initWasm, parseAV2Bitstream, isWasmSupported } from './wasm/av2-parser';
import './App.css';

type AppState =
  | { status: 'init' }
  | { status: 'loading' }
  | { status: 'ready' }
  | { status: 'parsing'; filename: string }
  | { status: 'parsed'; result: any }
  | { status: 'error'; message: string };

function App() {
  const [state, setState] = useState<AppState>({ status: 'init' });

  useEffect(() => {
    // Check WASM support
    if (!isWasmSupported()) {
      setState({
        status: 'error',
        message: 'WebAssembly is not supported in this browser.',
      });
      return;
    }

    // Initialize WASM module
    setState({ status: 'loading' });
    initWasm()
      .then(() => {
        setState({ status: 'ready' });
      })
      .catch((error) => {
        setState({
          status: 'error',
          message: `Failed to load WASM module: ${error.message}`,
        });
      });
  }, []);

  const handleFileLoaded = useCallback(async (data: Uint8Array, filename: string) => {
    setState({ status: 'parsing', filename });

    try {
      const result = await parseAV2Bitstream(data);
      // Replace the temp filename with the actual filename
      result.file = filename;
      setState({ status: 'parsed', result });
    } catch (error) {
      setState({
        status: 'error',
        message: error instanceof Error ? error.message : 'Unknown parsing error',
      });
    }
  }, []);

  const handleReset = useCallback(() => {
    setState({ status: 'ready' });
  }, []);

  return (
    <div className="app">
      <header className="app-header">
        <h1>AV2 OBU Analyzer</h1>
        <p className="app-subtitle">Browser-based AV2 bitstream analysis tool</p>
      </header>

      <main className="app-main">
        {state.status === 'init' && (
          <div className="status-message">
            <div className="spinner" />
            <p>Checking browser compatibility...</p>
          </div>
        )}

        {state.status === 'loading' && (
          <div className="status-message">
            <div className="spinner" />
            <p>Loading WASM module...</p>
          </div>
        )}

        {state.status === 'error' && (
          <div className="status-message error">
            <div className="error-icon">⚠️</div>
            <h3>Error</h3>
            <p>{state.message}</p>
          </div>
        )}

        {state.status === 'ready' && (
          <div className="upload-container">
            <FileUpload onFileLoaded={handleFileLoaded} />
            <div className="info-box">
              <h3>About</h3>
              <p>
                This tool parses AV2 elementary streams and displays the OBU structure.
              </p>
              <ul>
                <li>View all OBUs with detailed syntax information</li>
                <li>Inspect headers, positions, and parsed payloads</li>
                <li>All processing happens locally in your browser</li>
              </ul>
            </div>
          </div>
        )}

        {state.status === 'parsing' && (
          <div className="status-message">
            <div className="spinner" />
            <p>Parsing {state.filename}...</p>
          </div>
        )}

        {state.status === 'parsed' && (
          <div className="results-container">
            <div className="results-header">
              <div>
                <h3>{state.result.file}</h3>
                <p className="file-info">{state.result.obu_count} OBUs parsed</p>
              </div>
              <button onClick={handleReset} className="btn-primary">
                Load Another File
              </button>
            </div>
            <OBUBrowser obus={state.result.obus} />
          </div>
        )}
      </main>

      <footer className="app-footer">
        <p>
          Built with WebAssembly •{' '}
          <a href="https://github.com/AOMediaCodec/av2-tools" target="_blank" rel="noopener noreferrer">
            GitHub
          </a>
        </p>
      </footer>
    </div>
  );
}

export default App;
