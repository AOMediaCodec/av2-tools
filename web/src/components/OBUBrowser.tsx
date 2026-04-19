import { useState, useEffect, useRef, useMemo } from 'react';
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

interface TUGroup {
  label: string;
  isConfig: boolean;
  keyframeType: string | null;  // 'CLK', 'OLK', or null
  obus: { obu: any; globalIndex: number }[];
  totalSize: number;
}

function groupByTemporalUnit(obus: any[]): TUGroup[] {
  const groups: TUGroup[] = [];
  let current: { obu: any; globalIndex: number }[] = [];
  let tuIndex = 0;
  let seenTD = false;

  const computeGroup = (entries: { obu: any; globalIndex: number }[], label: string, isConfig: boolean): TUGroup => {
    const clkObu = entries.find(e => e.obu.type_name === 'CLK');
    const olkObu = entries.find(e => e.obu.type_name === 'OLK');
    const keyframeType = clkObu ? 'CLK' : olkObu ? 'OLK' : null;
    const totalSize = entries.reduce(
      (sum, e) => sum + e.obu.position.size_field_bytes + e.obu.position.header_size + e.obu.position.payload_size,
      0
    );
    return { label, isConfig, keyframeType, obus: entries, totalSize };
  };

  for (let i = 0; i < obus.length; i++) {
    if (obus[i].type_name === 'TEMPORAL_DELIMITER') {
      if (current.length > 0) {
        if (!seenTD) {
          groups.push(computeGroup(current, 'Config', true));
        } else {
          groups.push(computeGroup(current, `TU ${tuIndex}`, false));
          tuIndex++;
        }
        current = [];
      }
      seenTD = true;
      current.push({ obu: obus[i], globalIndex: i });
    } else {
      current.push({ obu: obus[i], globalIndex: i });
    }
  }

  if (current.length > 0) {
    if (!seenTD) {
      groups.push(computeGroup(current, 'Config', true));
    } else {
      groups.push(computeGroup(current, `TU ${tuIndex}`, false));
    }
  }

  return groups;
}

export function OBUBrowser({ obus }: OBUBrowserProps) {
  const [expandedIndices, setExpandedIndices] = useState<Set<number>>(new Set());
  const [expandedTUs, setExpandedTUs] = useState<Set<number>>(new Set());
  const [isSpotlightOpen, setIsSpotlightOpen] = useState(false);
  const [xlayerFilter, setXlayerFilter] = useState<number | null>(null);
  const obuRefs = useRef<Map<number, HTMLDivElement>>(new Map());

  const tuGroups = useMemo(() => groupByTemporalUnit(obus), [obus]);

  // Expand all TU groups by default when data changes
  useEffect(() => {
    setExpandedTUs(new Set(tuGroups.map((_, i) => i)));
  }, [tuGroups]);

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
    // Expand the TU group containing this OBU
    for (let gi = 0; gi < tuGroups.length; gi++) {
      if (tuGroups[gi].obus.some(e => e.globalIndex === obuIndex)) {
        setExpandedTUs(prev => new Set(prev).add(gi));
        break;
      }
    }

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

  const toggleTU = (tuIndex: number) => {
    setExpandedTUs((prev) => {
      const next = new Set(prev);
      if (next.has(tuIndex)) {
        next.delete(tuIndex);
      } else {
        next.add(tuIndex);
      }
      return next;
    });
  };

  const expandAll = () => {
    setExpandedTUs(new Set(tuGroups.map((_, i) => i)));
    const expandable = new Set<number>();
    obus.forEach((o, i) => { if (o.position.payload_size > 0) expandable.add(i); });
    setExpandedIndices(expandable);
  };

  const collapseAll = () => {
    setExpandedTUs(new Set());
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

  const tuCount = tuGroups.filter(g => !g.isConfig).length;

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
            <strong>{tuCount}</strong> TUs
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

      {xlayerFilter !== null && (
        <div className="xlayer-filter-bar">
          <span>Filtering:</span>
          <span className={`xlayer-badge xlayer-${xlayerFilter === 31 ? 'global' : Math.min(xlayerFilter, 4)}`}>
            {xlayerFilter === 31 ? 'GL' : `X${xlayerFilter}`}
          </span>
          <span>(global OBUs always shown)</span>
          <button className="filter-clear" onClick={() => setXlayerFilter(null)}>Clear filter</button>
        </div>
      )}

      <div className="obu-list">
        {tuGroups.map((group, groupIndex) => {
          const isTUExpanded = expandedTUs.has(groupIndex);

          return (
            <div key={groupIndex} className={`tu-group ${group.isConfig ? 'tu-config' : ''} ${group.keyframeType ? 'tu-keyframe' : ''}`}>
              <div className="tu-group-header" onClick={() => toggleTU(groupIndex)}>
                <span className="expand-icon">{isTUExpanded ? '▼' : '▶'}</span>
                <span className="tu-label">{group.label}</span>
                <span className="tu-stats">
                  {group.obus.length} OBUs
                </span>
                <span className="tu-size">
                  {group.totalSize} B
                </span>
                {group.keyframeType && <span className="tu-keyframe-badge">{group.keyframeType}</span>}
              </div>

              {isTUExpanded && (
                <div className="tu-group-obus">
                  {group.obus.map(({ obu, globalIndex }) => {
                    const isExpanded = expandedIndices.has(globalIndex);
                    const obuTotalSize =
                      obu.position.size_field_bytes + obu.position.header_size + obu.position.payload_size;
                    const hasExtension = obu.header.extension_flag === 1;
                    // Derive xlayer_id per spec: when no extension, MSDO/TD are global, others are 0
                    const typeName = obu.type_name;
                    const xlayerId = hasExtension
                      ? obu.header.xlayer_id
                      : (typeName === 'MSDO' || typeName === 'TEMPORAL_DELIMITER') ? 31 : 0;
                    const mlayerId = hasExtension ? obu.header.mlayer_id : 0;
                    const tlayerId = obu.header.tlayer_id;  // always in byte 1
                    const isGlobal = xlayerId === 31;
                    const isFilteredOut = xlayerFilter !== null && xlayerId !== xlayerFilter && !isGlobal;
                    const xlayerBorderClass = `xlayer-border-${isGlobal ? 'global' : Math.min(xlayerId, 4)}`;

                    const hasPayload = obu.position.payload_size > 0;

                    return (
                      <div
                        key={globalIndex}
                        ref={(el) => {
                          if (el) obuRefs.current.set(globalIndex, el);
                        }}
                        className={`obu-item ${isExpanded ? 'expanded' : ''} ${xlayerBorderClass} ${isFilteredOut ? 'xlayer-filtered-out' : ''}`}
                      >
                        <div className={`obu-header ${hasPayload ? '' : 'no-payload'}`} onClick={() => hasPayload && toggleExpand(globalIndex)}>
                          <span className="expand-icon">{hasPayload ? (isExpanded ? '▼' : '▶') : ' '}</span>
                          <span className="obu-index">#{globalIndex}</span>
                          <span className={`obu-type type-${obu.type_name.toLowerCase()}`}>
                            {obu.type_name} ({obu.header.obu_type})
                          </span>
                          <span className="obu-offset">@{obu.position.file_offset}</span>
                          <span className="obu-size">{obuTotalSize} B</span>
                          <span className="obu-layers">
                            {hasExtension && <span className="ext-flag" title="obu_header_extension_flag = 1">E</span>}
                            <span className="tm-info">
                              T{tlayerId} M{mlayerId}
                            </span>
                              <span
                                className={`xlayer-badge xlayer-${isGlobal ? 'global' : Math.min(xlayerId, 4)} ${xlayerFilter === xlayerId ? 'active' : ''}`}
                                onClick={(e) => {
                                  e.stopPropagation();
                                  setXlayerFilter(xlayerFilter === xlayerId ? null : xlayerId);
                                }}
                                title={`Click to ${xlayerFilter === xlayerId ? 'clear' : 'filter to'} xlayer ${isGlobal ? 'GL (global)' : xlayerId}`}
                              >
                                {isGlobal ? 'GL' : `X${xlayerId}`}
                              </span>
                            </span>
                        </div>

                        {isExpanded && (
                          <div className="obu-details">
                            {obu.position.payload_size > 0 && (
                            <div className="detail-section">
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
                            )}
                          </div>
                        )}
                      </div>
                    );
                  })}
                </div>
              )}
            </div>
          );
        })}
      </div>
    </div>
  );
}
