import { useState } from 'react';
import './JsonTreeViewer.css';

interface JsonTreeViewerProps {
  data: any;
  name?: string;
  defaultExpanded?: boolean;
}

type JsonValue = string | number | boolean | null | JsonObject | JsonArray;
type JsonObject = { [key: string]: JsonValue };
type JsonArray = JsonValue[];

export function JsonTreeViewer({ data, name, defaultExpanded = false }: JsonTreeViewerProps) {
  return (
    <div className="json-tree-viewer">
      <JsonNode data={data} name={name} defaultExpanded={defaultExpanded} />
    </div>
  );
}

interface JsonNodeProps {
  data: JsonValue;
  name?: string;
  defaultExpanded?: boolean;
  depth?: number;
}

function JsonNode({ data, name, defaultExpanded = false, depth = 0 }: JsonNodeProps) {
  const [isExpanded, setIsExpanded] = useState(defaultExpanded);

  if (data === null) {
    return (
      <div className="json-node json-null" style={{ paddingLeft: `${depth * 16}px` }}>
        {name && <span className="json-key">{name}: </span>}
        <span className="json-value-null">null</span>
      </div>
    );
  }

  if (typeof data === 'boolean') {
    return (
      <div className="json-node json-boolean" style={{ paddingLeft: `${depth * 16}px` }}>
        {name && <span className="json-key">{name}: </span>}
        <span className="json-value-boolean">{data.toString()}</span>
      </div>
    );
  }

  if (typeof data === 'number') {
    return (
      <div className="json-node json-number" style={{ paddingLeft: `${depth * 16}px` }}>
        {name && <span className="json-key">{name}: </span>}
        <span className="json-value-number">{data}</span>
      </div>
    );
  }

  if (typeof data === 'string') {
    return (
      <div className="json-node json-string" style={{ paddingLeft: `${depth * 16}px` }}>
        {name && <span className="json-key">{name}: </span>}
        <span className="json-value-string">"{data}"</span>
      </div>
    );
  }

  if (Array.isArray(data)) {
    const isEmpty = data.length === 0;

    if (isEmpty) {
      return (
        <div className="json-node json-array-empty" style={{ paddingLeft: `${depth * 16}px` }}>
          {name && <span className="json-key">{name}: </span>}
          <span className="json-bracket">[]</span>
        </div>
      );
    }

    return (
      <div className="json-node json-array">
        <div
          className="json-expandable"
          style={{ paddingLeft: `${depth * 16}px` }}
          onClick={() => setIsExpanded(!isExpanded)}
        >
          <span className="json-toggle">{isExpanded ? '▼' : '▶'}</span>
          {name && <span className="json-key">{name}: </span>}
          <span className="json-bracket">[</span>
          {!isExpanded && (
            <>
              <span className="json-preview">{data.length} items</span>
              <span className="json-bracket">]</span>
            </>
          )}
        </div>

        {isExpanded && (
          <>
            {data.map((item, index) => (
              <JsonNode key={index} data={item} name={`${index}`} depth={depth + 1} />
            ))}
            <div className="json-bracket-close" style={{ paddingLeft: `${depth * 16}px` }}>
              ]
            </div>
          </>
        )}
      </div>
    );
  }

  if (typeof data === 'object') {
    const keys = Object.keys(data);
    const isEmpty = keys.length === 0;

    if (isEmpty) {
      return (
        <div className="json-node json-object-empty" style={{ paddingLeft: `${depth * 16}px` }}>
          {name && <span className="json-key">{name}: </span>}
          <span className="json-bracket">{'{}'}</span>
        </div>
      );
    }

    return (
      <div className="json-node json-object">
        <div
          className="json-expandable"
          style={{ paddingLeft: `${depth * 16}px` }}
          onClick={() => setIsExpanded(!isExpanded)}
        >
          <span className="json-toggle">{isExpanded ? '▼' : '▶'}</span>
          {name && <span className="json-key">{name}: </span>}
          <span className="json-bracket">{'{'}</span>
          {!isExpanded && (
            <>
              <span className="json-preview">{keys.length} keys</span>
              <span className="json-bracket">{'}'}</span>
            </>
          )}
        </div>

        {isExpanded && (
          <>
            {keys.map((key) => (
              <JsonNode key={key} data={data[key]} name={key} depth={depth + 1} />
            ))}
            <div className="json-bracket-close" style={{ paddingLeft: `${depth * 16}px` }}>
              {'}'}
            </div>
          </>
        )}
      </div>
    );
  }

  return null;
}
