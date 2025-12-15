import { useState } from 'react';
import './JsonHybridViewer.css';

interface JsonHybridViewerProps {
  data: any;
  defaultExpanded?: boolean;
}

export function JsonHybridViewer({ data, defaultExpanded = false }: JsonHybridViewerProps) {
  if (!data || typeof data !== 'object') {
    return null;
  }

  const entries = Object.entries(data);

  // Separate primitive fields from nested objects
  const primitiveEntries = entries.filter(
    ([_, v]) =>
      v === null ||
      typeof v === 'boolean' ||
      typeof v === 'number' ||
      typeof v === 'string' ||
      (Array.isArray(v) &&
        v.length > 0 &&
        v.every((item) => item === null || ['boolean', 'number', 'string'].includes(typeof item)))
  );

  const nestedEntries = entries.filter(
    ([key, _]) => !primitiveEntries.some(([k]) => k === key)
  );

  return (
    <div className="json-hybrid-viewer">
      {/* Show top-level primitives in a table */}
      {primitiveEntries.length > 0 && (
        <table className="json-table json-table-root">
          <tbody>
            {primitiveEntries.map(([key, val]) => (
              <tr key={key}>
                <td className="json-table-key">{key}:</td>
                <td className="json-table-value">{formatValue(val)}</td>
              </tr>
            ))}
          </tbody>
        </table>
      )}

      {/* Show nested objects/arrays as expandable sections */}
      {nestedEntries.map(([key, value]) => (
        <JsonSection key={key} name={key} value={value} defaultExpanded={defaultExpanded} />
      ))}
    </div>
  );
}

interface JsonSectionProps {
  name: string;
  value: any;
  defaultExpanded?: boolean;
  depth?: number;
}

function JsonSection({ name, value, defaultExpanded = false, depth = 0 }: JsonSectionProps) {
  const [isExpanded, setIsExpanded] = useState(defaultExpanded);

  // Handle primitives
  if (value === null || typeof value === 'boolean' || typeof value === 'number' || typeof value === 'string') {
    return null; // Primitives are shown in parent's table
  }

  // Handle arrays
  if (Array.isArray(value)) {
    if (value.length === 0) {
      return null; // Empty arrays shown in parent's table
    }

    // Check if array of primitives
    const allPrimitives = value.every(
      (item) => item === null || ['boolean', 'number', 'string'].includes(typeof item)
    );

    if (allPrimitives) {
      return null; // Primitive arrays shown in parent's table
    }

    // Complex array - use expandable with preview
    return (
      <div className={`json-section depth-${depth}`}>
        <div className="json-section-header" onClick={() => setIsExpanded(!isExpanded)}>
          <span className="json-toggle">{isExpanded ? '▼' : '▶'}</span>
          <span className="json-section-name">{name}</span>
          <span className="json-section-badge">{value.length} items</span>
        </div>
        {isExpanded && (
          <div className="json-section-content">
            {value.map((item, index) => {
              if (typeof item === 'object' && item !== null && !Array.isArray(item)) {
                return <JsonSection key={index} name={`[${index}]`} value={item} depth={depth + 1} />;
              }
              return (
                <div key={index} className="json-array-item">
                  <span className="json-array-index">[{index}]</span>
                  <span className="json-array-value">{formatValue(item)}</span>
                </div>
              );
            })}
          </div>
        )}
      </div>
    );
  }

  // Handle objects
  if (typeof value === 'object' && value !== null) {
    const entries = Object.entries(value);
    if (entries.length === 0) {
      return null; // Empty objects shown in parent's table
    }

    // Separate primitive fields from nested objects
    const primitiveEntries = entries.filter(
      ([_, v]) =>
        v === null ||
        typeof v === 'boolean' ||
        typeof v === 'number' ||
        typeof v === 'string' ||
        (Array.isArray(v) && v.every((item) => ['boolean', 'number', 'string'].includes(typeof item)))
    );

    const nestedEntries = entries.filter(
      ([_, v]) => typeof v === 'object' && v !== null && !primitiveEntries.some(([k]) => k === _)
    );

    const hasContent = primitiveEntries.length > 0 || nestedEntries.length > 0;

    if (!hasContent) {
      return null;
    }

    return (
      <div className={`json-section depth-${depth}`}>
        <div className="json-section-header" onClick={() => setIsExpanded(!isExpanded)}>
          <span className="json-toggle">{isExpanded ? '▼' : '▶'}</span>
          <span className="json-section-name">{name}</span>
          <span className="json-section-badge">
            {primitiveEntries.length + nestedEntries.length} properties
          </span>
        </div>

        {isExpanded && (
          <div className="json-section-content">
            {/* Show primitives in a table */}
            {primitiveEntries.length > 0 && (
              <table className="json-table">
                <tbody>
                  {primitiveEntries.map(([key, val]) => (
                    <tr key={key}>
                      <td className="json-table-key">{key}:</td>
                      <td className="json-table-value">{formatValue(val)}</td>
                    </tr>
                  ))}
                </tbody>
              </table>
            )}

            {/* Show nested objects as expandable sections */}
            {nestedEntries.map(([key, val]) => (
              <JsonSection key={key} name={key} value={val} depth={depth + 1} />
            ))}
          </div>
        )}
      </div>
    );
  }

  return null;
}

function formatValue(value: any): React.ReactNode {
  if (value === null) {
    return <span className="json-value-null">null</span>;
  }

  if (typeof value === 'boolean') {
    return <span className="json-value-boolean">{value.toString()}</span>;
  }

  if (typeof value === 'number') {
    return <span className="json-value-number">{value}</span>;
  }

  if (typeof value === 'string') {
    return <span className="json-value-string">{value}</span>;
  }

  if (Array.isArray(value)) {
    // Array of primitives - show inline
    if (value.length === 0) {
      return <span className="json-value-array">[]</span>;
    }
    const allPrimitives = value.every(
      (item) => item === null || ['boolean', 'number', 'string'].includes(typeof item)
    );
    if (allPrimitives && value.length <= 5) {
      return (
        <span className="json-value-array">
          [{value.map((v, i) => (
            <span key={i}>
              {formatValue(v)}
              {i < value.length - 1 ? ', ' : ''}
            </span>
          ))}]
        </span>
      );
    }
    return <span className="json-value-array">[{value.length} items]</span>;
  }

  if (typeof value === 'object') {
    return <span className="json-value-object">{'{...}'}</span>;
  }

  return <span>{String(value)}</span>;
}
