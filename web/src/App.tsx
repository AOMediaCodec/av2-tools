import { useState, useCallback, useEffect } from 'react';
import { FileUpload } from './components/FileUpload';
import { OBUBrowser } from './components/OBUBrowser';
import { BitstreamStats } from './components/BitstreamStats';
import { initWasm, parseAV2Bitstream, isWasmSupported } from './wasm/av2-parser';
import { BUILD_VERSION } from './generated/version';
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
  const [activeTab, setActiveTab] = useState<'browser' | 'statistics'>('browser');

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
        <h1>AV2 OBU Analyzer <span className="app-version">v{BUILD_VERSION}</span></h1>
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
              <h3>{state.result.file}</h3>
              <p className="file-info">
                {(() => {
                  const obus = state.result.obus;
                  const totalBytes = obus.reduce((s: number, o: any) =>
                    s + o.position.size_field_bytes + o.position.header_size + o.position.payload_size, 0);
                  const tuCount = obus.filter((o: any) => o.type_name === 'TEMPORAL_DELIMITER').length;
                  const xlayers = new Set(obus.map((o: any) => {
                    if (o.header.extension_flag === 1) return o.header.xlayer_id;
                    return (o.type_name === 'MSDO' || o.type_name === 'TEMPORAL_DELIMITER') ? 31 : 0;
                  }));
                  const sizeStr = totalBytes >= 1024 * 1024
                    ? `${(totalBytes / (1024 * 1024)).toFixed(2)} MB`
                    : `${(totalBytes / 1024).toFixed(2)} KB`;
                  const parts = [sizeStr, `${obus.length} OBUs`, `${tuCount} TUs`];
                  if (xlayers.size > 1) {
                    const nonGlobal = [...xlayers].filter(x => x !== 31).length;
                    parts.push(`${nonGlobal} xlayer${nonGlobal !== 1 ? 's' : ''}`);
                  }
                  return parts.join(' • ');
                })()}
              </p>
              <div className="results-tabs-row">
                <div className="results-tabs">
                  <button
                    className={`tab-btn ${activeTab === 'browser' ? 'active' : ''}`}
                    onClick={() => setActiveTab('browser')}
                  >
                    Browser
                  </button>
                  <button
                    className={`tab-btn ${activeTab === 'statistics' ? 'active' : ''}`}
                    onClick={() => setActiveTab('statistics')}
                  >
                    Statistics
                  </button>
                </div>
                <div className="results-actions">
                  <button onClick={() => {
                    const jsonStr = JSON.stringify(state.result, null, 2);
                    const blob = new Blob([jsonStr], { type: 'application/json' });
                    const url = URL.createObjectURL(blob);
                    const a = document.createElement('a');
                    a.href = url;
                    const baseName = state.result.file.replace(/\.[^.]+$/, '');
                    a.download = `${baseName}.json`;
                    a.click();
                    URL.revokeObjectURL(url);
                  }} className="btn-secondary">
                    Download JSON
                  </button>
                  <button onClick={handleReset} className="btn-secondary">
                    Load Another File
                  </button>
                </div>
              </div>
            </div>
            {activeTab === 'browser' && (
              <OBUBrowser obus={state.result.obus} />
            )}
            {activeTab === 'statistics' && (
              <BitstreamStats obus={state.result.obus} />
            )}
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
