import { useState, useEffect, useCallback, useRef } from 'react';
import './SpotlightSearch.css';

interface SpotlightSearchProps {
  isOpen: boolean;
  onClose: () => void;
  obus: any[];
  onSelectResult: (obuIndex: number) => void;
}

interface SearchResult {
  obuIndex: number;
  obu: any;
  matchType: 'type' | 'layer' | 'offset' | 'index' | 'field';
  matchReason: string;
  matchedFields?: FieldMatch[]; // For field matches, show where the field was found with values
}

interface FieldMatch {
  path: string;
  value: any;
}

export function SpotlightSearch({ isOpen, onClose, obus, onSelectResult }: SpotlightSearchProps) {
  const [query, setQuery] = useState('');
  const [results, setResults] = useState<SearchResult[]>([]);
  const [selectedIndex, setSelectedIndex] = useState(0);
  const inputRef = useRef<HTMLInputElement>(null);

  // Focus input when opened
  useEffect(() => {
    if (isOpen && inputRef.current) {
      inputRef.current.focus();
      setQuery('');
      setSelectedIndex(0);
    }
  }, [isOpen]);

  // Search logic
  useEffect(() => {
    if (!query.trim()) {
      setResults([]);
      setSelectedIndex(0);
      return;
    }

    const searchResults: SearchResult[] = [];
    const lowerQuery = query.toLowerCase().trim();

    obus.forEach((obu, index) => {
      // Check for index search: #5
      if (lowerQuery.startsWith('#')) {
        const targetIndex = parseInt(lowerQuery.substring(1));
        if (!isNaN(targetIndex) && index === targetIndex) {
          searchResults.push({
            obuIndex: index,
            obu,
            matchType: 'index',
            matchReason: `OBU index #${index}`,
          });
        }
        return;
      }

      // Check for offset search: @1024
      if (lowerQuery.startsWith('@')) {
        const targetOffset = parseInt(lowerQuery.substring(1));
        if (!isNaN(targetOffset) && obu.position.file_offset === targetOffset) {
          searchResults.push({
            obuIndex: index,
            obu,
            matchType: 'offset',
            matchReason: `File offset @${obu.position.file_offset}`,
          });
        }
        return;
      }

      // Check for layer filters: t:0, m:0, x:0
      const layerMatch = lowerQuery.match(/^([tmx]):(\d+)$/);
      if (layerMatch) {
        const [, layerType, layerValue] = layerMatch;
        const value = parseInt(layerValue);
        let matches = false;

        if (layerType === 't' && obu.header.tlayer_id === value) matches = true;
        if (layerType === 'm' && obu.header.mlayer_id === value) matches = true;
        if (layerType === 'x' && obu.header.xlayer_id === value) matches = true;

        if (matches) {
          searchResults.push({
            obuIndex: index,
            obu,
            matchType: 'layer',
            matchReason: `${layerType.toUpperCase()}layer ${value}`,
          });
        }
        return;
      }

      // Check for type filter: type:SEQUENCE_HEADER
      if (lowerQuery.startsWith('type:')) {
        const typeQuery = lowerQuery.substring(5);
        if (obu.type_name.toLowerCase().includes(typeQuery)) {
          searchResults.push({
            obuIndex: index,
            obu,
            matchType: 'type',
            matchReason: `Type: ${obu.type_name}`,
          });
        }
        return;
      }

      // Check for field name search: field:BitDepth or just BitDepth
      const isFieldSearch = lowerQuery.startsWith('field:');
      const fieldQuery = isFieldSearch ? lowerQuery.substring(6) : lowerQuery;

      // Search in OBU type name first (if not using field: prefix)
      if (!isFieldSearch && obu.type_name.toLowerCase().includes(lowerQuery)) {
        searchResults.push({
          obuIndex: index,
          obu,
          matchType: 'type',
          matchReason: `Type: ${obu.type_name}`,
        });
        return;
      }

      // Deep search in OBU fields
      const matchedFields = searchFieldsInObject(obu, fieldQuery);
      if (matchedFields.length > 0) {
        searchResults.push({
          obuIndex: index,
          obu,
          matchType: 'field',
          matchReason: `${matchedFields.length} field${matchedFields.length > 1 ? 's' : ''} matched`,
          matchedFields: matchedFields.slice(0, 3), // Show max 3
        });
      }
    });

    setResults(searchResults);
    setSelectedIndex(0);
  }, [query, obus]);

  // Keyboard navigation
  const handleKeyDown = useCallback(
    (e: React.KeyboardEvent) => {
      if (e.key === 'Escape') {
        onClose();
      } else if (e.key === 'ArrowDown') {
        e.preventDefault();
        setSelectedIndex((prev) => Math.min(prev + 1, results.length - 1));
      } else if (e.key === 'ArrowUp') {
        e.preventDefault();
        setSelectedIndex((prev) => Math.max(prev - 1, 0));
      } else if (e.key === 'Enter' && results.length > 0) {
        e.preventDefault();
        handleSelectResult(results[selectedIndex]);
      }
    },
    [results, selectedIndex, onClose]
  );

  const handleSelectResult = (result: SearchResult) => {
    onSelectResult(result.obuIndex);
    onClose();
  };

  if (!isOpen) return null;

  return (
    <div className="spotlight-overlay" onClick={onClose}>
      <div className="spotlight-modal" onClick={(e) => e.stopPropagation()}>
        <div className="spotlight-search-box">
          <span className="spotlight-icon">🔍</span>
          <input
            ref={inputRef}
            type="text"
            className="spotlight-input"
            placeholder="Search OBUs... (try: BitDepth, type:METADATA, t:0, #5)"
            value={query}
            onChange={(e) => setQuery(e.target.value)}
            onKeyDown={handleKeyDown}
          />
          {query && (
            <button className="spotlight-clear" onClick={() => setQuery('')}>
              ×
            </button>
          )}
        </div>

        {query && (
          <div className="spotlight-results">
            <div className="spotlight-results-header">
              {results.length > 0 ? (
                <>
                  {results.length} {results.length === 1 ? 'result' : 'results'}
                </>
              ) : (
                'No results found'
              )}
            </div>

            <div className="spotlight-results-list">
              {results.map((result, index) => (
                <div
                  key={result.obuIndex}
                  className={`spotlight-result-item ${index === selectedIndex ? 'selected' : ''}`}
                  onClick={() => handleSelectResult(result)}
                  onMouseEnter={() => setSelectedIndex(index)}
                >
                  <div className="spotlight-result-main">
                    <span className="spotlight-result-index">#{result.obuIndex}</span>
                    <span className={`spotlight-result-type type-${result.obu.type_name.toLowerCase()}`}>
                      {result.obu.type_name}
                    </span>
                    <span className="spotlight-result-offset">@{result.obu.position.file_offset}</span>
                  </div>
                  <div className="spotlight-result-meta">
                    <span className="spotlight-result-reason">{result.matchReason}</span>
                    {result.obu.header.extension_flag === 1 && (
                      <span className="spotlight-result-layers">
                        T{result.obu.header.tlayer_id}/M{result.obu.header.mlayer_id}/X
                        {result.obu.header.xlayer_id}
                      </span>
                    )}
                  </div>
                  {result.matchedFields && result.matchedFields.length > 0 && (
                    <div className="spotlight-result-paths">
                      {result.matchedFields.map((field, i) => (
                        <div key={i} className="spotlight-result-path">
                          <span className="path-arrow">→</span>
                          <code>{field.path}</code>
                          <span className="path-value">= {formatFieldValue(field.value)}</span>
                        </div>
                      ))}
                      {result.matchedFields.length < (result.matchReason.match(/\d+/)?.[0] ? parseInt(result.matchReason.match(/\d+/)![0]) : 0) && (
                        <div className="spotlight-result-path-more">
                          +{parseInt(result.matchReason.match(/\d+/)![0]) - result.matchedFields.length} more...
                        </div>
                      )}
                    </div>
                  )}
                </div>
              ))}
            </div>
          </div>
        )}

        {!query && (
          <div className="spotlight-tips">
            <div className="spotlight-tips-header">Search tips:</div>
            <div className="spotlight-tips-grid">
              <div className="spotlight-tip">
                <code>SEQUENCE</code>
                <span>Find OBU type</span>
              </div>
              <div className="spotlight-tip">
                <code>type:METADATA</code>
                <span>Exact type filter</span>
              </div>
              <div className="spotlight-tip">
                <code>t:0</code>
                <span>obu_tlayer_id 0</span>
              </div>
              <div className="spotlight-tip">
                <code>m:0</code>
                <span>obu_mlayer_id 0</span>
              </div>
              <div className="spotlight-tip">
                <code>x:0</code>
                <span>obu_xlayer_id 0</span>
              </div>
              <div className="spotlight-tip">
                <code>#5</code>
                <span>Jump to OBU 5</span>
              </div>
              <div className="spotlight-tip">
                <code>@1024</code>
                <span>OBU at offset 1024</span>
              </div>
              <div className="spotlight-tip">
                <code>BitDepth</code>
                <span>Find field name</span>
              </div>
              <div className="spotlight-tip">
                <code>field:hash</code>
                <span>Search in fields</span>
              </div>
            </div>
            <div className="spotlight-tips-footer">
              Press <kbd>↑</kbd> <kbd>↓</kbd> to navigate • <kbd>↵</kbd> to select • <kbd>ESC</kbd> to
              close
            </div>
          </div>
        )}
      </div>
    </div>
  );
}

/**
 * Format a field value for display in search results
 */
function formatFieldValue(value: any): string {
  if (value === null || value === undefined) return 'null';
  if (typeof value === 'boolean') return value ? 'true' : 'false';
  if (typeof value === 'number') return String(value);
  if (typeof value === 'string') return `"${value}"`;
  if (Array.isArray(value)) {
    if (value.length <= 4) return `[${value.join(', ')}]`;
    return `[${value.slice(0, 3).join(', ')}, ... (${value.length})]`;
  }
  if (typeof value === 'object') return '{...}';
  return String(value);
}

/**
 * Recursively search for field names in an object
 * Returns array of {path, value} pairs where the field was found
 */
function searchFieldsInObject(
  obj: any,
  searchTerm: string,
  currentPath: string = '',
  maxResults: number = 10
): FieldMatch[] {
  const results: FieldMatch[] = [];
  const lowerSearch = searchTerm.toLowerCase();

  // Skip searching in these metadata fields
  if (currentPath === 'type_name' || currentPath === 'position' || currentPath === 'header') {
    return results;
  }

  if (obj === null || obj === undefined) {
    return results;
  }

  // If it's a primitive, check if we're at a matching field name
  if (typeof obj !== 'object') {
    return results;
  }

  // Search in object keys
  if (typeof obj === 'object' && !Array.isArray(obj)) {
    for (const [key, value] of Object.entries(obj)) {
      // Skip metadata fields
      if (key === 'type_name' || key === 'position' || key === 'header') {
        continue;
      }

      const newPath = currentPath ? `${currentPath}.${key}` : key;

      // Check if this key matches
      if (key.toLowerCase().includes(lowerSearch)) {
        results.push({ path: newPath, value });
        if (results.length >= maxResults) return results;
      }

      // Recursively search nested objects
      if (typeof value === 'object' && value !== null) {
        const nestedResults = searchFieldsInObject(value, searchTerm, newPath, maxResults - results.length);
        results.push(...nestedResults);
        if (results.length >= maxResults) return results;
      }
    }
  }

  // Search in arrays
  if (Array.isArray(obj)) {
    for (let i = 0; i < obj.length; i++) {
      const item = obj[i];
      const newPath = `${currentPath}[${i}]`;

      if (typeof item === 'object' && item !== null) {
        const nestedResults = searchFieldsInObject(item, searchTerm, newPath, maxResults - results.length);
        results.push(...nestedResults);
        if (results.length >= maxResults) return results;
      }
    }
  }

  return results;
}
