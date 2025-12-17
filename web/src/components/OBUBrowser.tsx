import { useState, useEffect, useRef } from 'react';
import { JsonHybridViewer } from './JsonHybridViewer';
import { SpotlightSearch } from './SpotlightSearch';
import './OBUBrowser.css';

interface OBUBrowserProps {
  obus: any[];
}

// Metadata unit header fields (fields present even when payload is not parsed)
const METADATA_UNIT_HEADER_FIELDS = new Set([
  'metadata_type',
  'metadata_type_name',
  'muh_header_size',
  'muh_cancel_flag',
  'muh_payload_size',
  'muh_layer_idc',
  'muh_persistence_idc',
  'muh_priority',
  'muh_xlayer_map',
  'muh_mlayer_maps',
  'muh_header_extension_bytes',
  'muh_reserved_zero_2bits'
]);

// Check if metadata unit has parsed payload (has fields beyond header)
function hasMetadataPayload(metadataUnit: any): boolean {
  return Object.keys(metadataUnit).some(k => !METADATA_UNIT_HEADER_FIELDS.has(k));
}

// Check if metadata type is known but not yet implemented
// Skip warning for UNKNOWN/RESERVED types since we'll never parse them
function isKnownButNotImplemented(metadataUnit: any): boolean {
  const typeName = metadataUnit.metadata_type_name;
  const isUnknownOrReserved = typeName.includes('UNKNOWN') || typeName.includes('RESERVED');
  return !isUnknownOrReserved && !hasMetadataPayload(metadataUnit);
}

// Render metadata unit with "not parsed" message if needed
function MetadataUnitViewer({ unit, label }: { unit: any; label?: string }) {
  const [isExpanded, setIsExpanded] = useState(false);
  const hasParsedPayload = hasMetadataPayload(unit);
  const showNotImplementedWarning = isKnownButNotImplemented(unit);

  // Determine badge class based on implementation status
  const badgeClass = hasParsedPayload
    ? 'metadata-type-badge-implemented'
    : 'metadata-type-badge-not-implemented';

  return (
    <div className="metadata-unit-viewer">
      {label && (
        <div className="metadata-unit-header" onClick={() => setIsExpanded(!isExpanded)}>
          <span className="expand-icon">{isExpanded ? '▼' : '▶'}</span>
          <h5>{label}</h5>
          <span className={`metadata-type-badge ${badgeClass}`}>{unit.metadata_type_name}</span>
        </div>
      )}
      {isExpanded && (
        <>
          <div className="syntax-tree">
            <JsonHybridViewer data={unit} defaultExpanded={true} />
          </div>
          {showNotImplementedWarning && (
            <div className="payload-not-parsed metadata-not-parsed">
              <span className="not-parsed-icon">⚠️</span>
              <span className="not-parsed-text">
                Metadata payload parsing not yet implemented for {unit.metadata_type_name}
              </span>
              {unit.muh_payload_size > 0 && (
                <div className="not-parsed-details">
                  Payload size: {unit.muh_payload_size} bytes
                </div>
              )}
            </div>
          )}
        </>
      )}
    </div>
  );
}

export function OBUBrowser({ obus }: OBUBrowserProps) {
  const [expandedIndices, setExpandedIndices] = useState<Set<number>>(new Set());
  const [isSpotlightOpen, setIsSpotlightOpen] = useState(false);
  const obuRefs = useRef<Map<number, HTMLDivElement>>(new Map());

  // Keyboard shortcut for spotlight
  useEffect(() => {
    const handleKeyPress = (e: KeyboardEvent) => {
      // Open spotlight with "/" key (but not in input fields)
      if (e.key === '/' && !['INPUT', 'TEXTAREA'].includes((e.target as HTMLElement).tagName)) {
        e.preventDefault();
        setIsSpotlightOpen(true);
      }
      // Also support Cmd+K / Ctrl+K
      if ((e.metaKey || e.ctrlKey) && e.key === 'k') {
        e.preventDefault();
        setIsSpotlightOpen(true);
      }
    };

    window.addEventListener('keydown', handleKeyPress);
    return () => window.removeEventListener('keydown', handleKeyPress);
  }, []);

  const handleSpotlightSelect = (obuIndex: number) => {
    // Expand the selected OBU
    setExpandedIndices((prev) => {
      const next = new Set(prev);
      next.add(obuIndex);
      return next;
    });

    // Scroll to the OBU
    setTimeout(() => {
      const element = obuRefs.current.get(obuIndex);
      if (element) {
        element.scrollIntoView({ behavior: 'smooth', block: 'center' });
        // Flash highlight
        element.classList.add('highlight-flash');
        setTimeout(() => element.classList.remove('highlight-flash'), 2000);
      }
    }, 100);
  };

  const toggleExpand = (index: number) => {
    setExpandedIndices((prev) => {
      const next = new Set(prev);
      if (next.has(index)) {
        next.delete(index);
      } else {
        next.add(index);
      }
      return next;
    });
  };

  const expandAll = () => {
    setExpandedIndices(new Set(obus.map((_, i) => i)));
  };

  const collapseAll = () => {
    setExpandedIndices(new Set());
  };

  // Calculate OBU type statistics
  const typeStats = obus.reduce((acc, obu) => {
    acc[obu.type_name] = (acc[obu.type_name] || 0) + 1;
    return acc;
  }, {} as Record<string, number>);

  const totalSize = obus.reduce(
    (sum, obu) =>
      sum + obu.position.size_field_bytes + obu.position.header_size + obu.position.payload_size,
    0
  );

  return (
    <div className="obu-browser">
      <SpotlightSearch
        isOpen={isSpotlightOpen}
        onClose={() => setIsSpotlightOpen(false)}
        obus={obus}
        onSelectResult={handleSpotlightSelect}
      />

      <div className="browser-header">
        <h2>OBU Browser</h2>
        <div className="browser-stats">
          <span className="stat">
            <strong>{obus.length}</strong> OBUs
          </span>
          <span className="stat">
            <strong>{(totalSize / 1024).toFixed(2)}</strong> KB
          </span>
          <span className="stat">
            <strong>{Object.keys(typeStats).length}</strong> types
          </span>
        </div>
        <div className="browser-actions">
          <button onClick={() => setIsSpotlightOpen(true)} className="btn-secondary btn-search">
            🔍 Search <kbd>/</kbd>
          </button>
          <button onClick={expandAll} className="btn-secondary">
            Expand All
          </button>
          <button onClick={collapseAll} className="btn-secondary">
            Collapse All
          </button>
        </div>
      </div>

      <div className="obu-list">
        {obus.map((obu, index) => {
          const isExpanded = expandedIndices.has(index);
          const obuTotalSize =
            obu.position.size_field_bytes + obu.position.header_size + obu.position.payload_size;

          return (
            <div
              key={index}
              ref={(el) => {
                if (el) obuRefs.current.set(index, el);
              }}
              className={`obu-item ${isExpanded ? 'expanded' : ''}`}
            >
              <div className="obu-header" onClick={() => toggleExpand(index)}>
                <span className="expand-icon">{isExpanded ? '▼' : '▶'}</span>
                <span className="obu-index">#{index}</span>
                <span className={`obu-type type-${obu.type_name.toLowerCase()}`}>
                  {obu.type_name}
                </span>
                <span className="obu-offset">@{obu.position.file_offset}</span>
                <span className="obu-size">{obuTotalSize} B</span>
                {obu.header.extension_flag ? (
                  <span className="obu-layers">
                    T{obu.header.tlayer_id}/M{obu.header.mlayer_id}/X{obu.header.xlayer_id}
                  </span>
                ) : null}
              </div>

              {isExpanded && (
                <div className="obu-details">
                  <div className="detail-section">
                    <h4>OBU Header</h4>
                    <table className="detail-table">
                      <tbody>
                        <tr>
                          <td>OBU Type:</td>
                          <td>{obu.header.obu_type}</td>
                        </tr>
                        <tr>
                          <td>Extension flag:</td>
                          <td>{obu.header.extension_flag}</td>
                        </tr>
                        {obu.header.extension_flag ? (
                          <>
                            <tr>
                              <td>Temporal layer:</td>
                              <td>{obu.header.tlayer_id}</td>
                            </tr>
                            <tr>
                              <td>Multi layer:</td>
                              <td>{obu.header.mlayer_id}</td>
                            </tr>
                            <tr>
                              <td>Cross layer:</td>
                              <td>{obu.header.xlayer_id}</td>
                            </tr>
                          </>
                        ) : null}
                      </tbody>
                    </table>
                  </div>

                  <div className="detail-section">
                    <h4>OBU Payload</h4>
                    {Object.keys(obu).some(
                      (k) => k !== 'type_name' && k !== 'position' && k !== 'header'
                    ) ? (
                      <div className="syntax-tree">
                        {/* Special handling for metadata OBUs with metadata_unit field */}
                        {obu.metadata_unit ? (
                          <MetadataUnitViewer unit={obu.metadata_unit} label="Metadata Unit" />
                        ) : obu.units ? (
                          /* MetadataGroupOBU with multiple units */
                          <div>
                            <div className="metadata-group-info">
                              <JsonHybridViewer
                                data={Object.fromEntries(
                                  Object.entries(obu).filter(
                                    ([k]) => k !== 'type_name' && k !== 'position' && k !== 'header' && k !== 'units'
                                  )
                                )}
                                defaultExpanded={true}
                              />
                            </div>
                            <div className="metadata-units-list">
                              <h5>Metadata Units ({obu.units.length})</h5>
                              {obu.units.map((unit: any, idx: number) => (
                                <MetadataUnitViewer key={idx} unit={unit} label={`Unit ${idx}`} />
                              ))}
                            </div>
                          </div>
                        ) : (
                          /* Regular OBU payload */
                          <JsonHybridViewer
                            data={Object.fromEntries(
                              Object.entries(obu).filter(
                                ([k]) => k !== 'type_name' && k !== 'position' && k !== 'header'
                              )
                            )}
                            defaultExpanded={true}
                          />
                        )}
                      </div>
                    ) : (
                      <div className="payload-not-parsed">
                        <span className="not-parsed-icon">⚠️</span>
                        <span className="not-parsed-text">
                          Payload parsing not yet implemented for this OBU type
                        </span>
                        <div className="not-parsed-details">
                          Payload size: {obu.position.payload_size} bytes
                        </div>
                      </div>
                    )}
                  </div>
                </div>
              )}
            </div>
          );
        })}
      </div>
    </div>
  );
}
