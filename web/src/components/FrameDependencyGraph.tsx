import { useMemo, useState, useCallback } from 'react';
import { FrameNode, computeFrameGraphs } from '../utils/frameGraph';
import './FrameDependencyGraph.css';

interface Props {
  obus: any[];
}

const FRAME_COLORS: Record<string, { fill: string; stroke: string }> = {
  CLK:        { fill: '#f3e5f5', stroke: '#6a1b9a' },
  OLK:        { fill: '#f3e5f5', stroke: '#9c27b0' },
  KEY:        { fill: '#f3e5f5', stroke: '#6a1b9a' },
  INTER_OUT:  { fill: '#e3f2fd', stroke: '#1565c0' },
  INTER_HID:  { fill: '#eceff1', stroke: '#78909c' },
  TIP_OUT:    { fill: '#e0f7fa', stroke: '#00838f' },
  TIP_HID:    { fill: '#eceff1', stroke: '#607d8b' },
  SEF:        { fill: '#fff3e0', stroke: '#ef6c00' },
  RAS:        { fill: '#fce4ec', stroke: '#c62828' },
  SWITCH:     { fill: '#ede7f6', stroke: '#4527a0' },
};

function getFrameColor(node: FrameNode) {
  if (node.isSEF) return FRAME_COLORS.SEF;
  if (node.obuType === 'CLK') return FRAME_COLORS.CLK;
  if (node.obuType === 'OLK') return FRAME_COLORS.OLK;
  if (node.obuType === 'RAS_FRAME') return FRAME_COLORS.RAS;
  if (node.obuType === 'SWITCH') return FRAME_COLORS.SWITCH;
  if (node.isTIP) return node.isOutput ? FRAME_COLORS.TIP_OUT : FRAME_COLORS.TIP_HID;
  if (node.frameType === 'KEY_FRAME') return FRAME_COLORS.KEY;
  return node.isOutput ? FRAME_COLORS.INTER_OUT : FRAME_COLORS.INTER_HID;
}

function getFrameLabel(node: FrameNode): string {
  if (node.isSEF) {
    return node.sefTargetDisplayOrder >= 0 ? `S${node.sefTargetDisplayOrder}` : String(node.displayOrder);
  }
  return String(node.displayOrder);
}

const CELL_W = 46;
const CELL_H = 24;
const TL_ROW_H = 32;  // Height per temporal layer row
const MARGIN_LEFT = 60;
const MARGIN_TOP = 25;
const SECTION_GAP = 50;

const XLAYER_LABELS: Record<number, string> = {
  0: 'X0', 1: 'X1', 2: 'X2', 3: 'X3', 4: 'X4',
};
const XLAYER_ACCENT: Record<number, string> = {
  0: '#1976d2', 1: '#43a047', 2: '#8e24aa', 3: '#ef6c00', 4: '#00897b',
};

export function FrameDependencyGraph({ obus }: Props) {
  const graphsByXlayer = useMemo(() => computeFrameGraphs(obus), [obus]);
  const xlayerIds = useMemo(() => [...graphsByXlayer.keys()].sort((a, b) => a - b), [graphsByXlayer]);

  if (xlayerIds.length === 0) {
    return <div className="chart-card chart-card-full"><h3>Frame Dependency Graph</h3><p>No frame data available</p></div>;
  }

  return (
    <div className="chart-card chart-card-full">
      <h3>Frame Dependency Graph (Experimental)</h3>
      {xlayerIds.map(xl => (
        <XlayerGraph
          key={xl}
          frames={graphsByXlayer.get(xl)!}
          xlayerId={xl}
          showXlayerLabel={xlayerIds.length > 1}
        />
      ))}
      {/* Legend */}
      <div className="frame-graph-legend">
        {[
          { label: 'CLK', color: FRAME_COLORS.CLK },
          { label: 'Inter (output)', color: FRAME_COLORS.INTER_OUT },
          { label: 'Hidden', color: FRAME_COLORS.INTER_HID },
          { label: 'TIP', color: FRAME_COLORS.TIP_OUT },
          { label: 'SEF', color: FRAME_COLORS.SEF },
        ].map(({ label, color }) => (
          <div key={label} className="frame-graph-legend-item">
            <div className="frame-graph-legend-swatch"
              style={{ background: color.fill, borderColor: color.stroke }} />
            <span>{label}</span>
          </div>
        ))}
        <div className="frame-graph-legend-item" style={{ marginLeft: 16, color: '#999' }}>
          Click a frame to trace dependencies
        </div>
      </div>
    </div>
  );
}

function XlayerGraph({ frames, xlayerId, showXlayerLabel }: {
  frames: FrameNode[];
  xlayerId: number;
  showXlayerLabel: boolean;
}) {
  const [selected, setSelected] = useState<number | null>(null);

  const highlightedSet = useMemo(() => {
    if (selected === null) return null;
    const set = new Set<number>();
    const queue = [selected];
    while (queue.length > 0) {
      const idx = queue.pop()!;
      if (set.has(idx)) continue;
      set.add(idx);
      const node = frames[idx];
      if (node) {
        for (const ref of node.references) queue.push(ref);
      }
    }
    return set;
  }, [selected, frames]);

  const handleClick = useCallback((idx: number) => {
    setSelected(prev => prev === idx ? null : idx);
  }, []);

  if (frames.length === 0) return null;

  // Compute max hierarchy level
  const maxHL = Math.max(...frames.map(f => f.hierarchyLevel));
  const numTLRows = maxHL + 1;

  // Compute TL boundary lines — find where obu_tlayer_id transitions happen across hierarchy levels
  const tlBoundaries = useMemo(() => {
    const uniqueTLs = [...new Set(frames.map(f => f.tlayerId))].sort();
    if (uniqueTLs.length <= 1) return []; // all same TL, no boundaries

    // For each TL boundary: find the hierarchy level where TL changes
    // Group hierarchy levels by their TL
    const hlToTL = new Map<number, Set<number>>();
    for (const f of frames) {
      if (!hlToTL.has(f.hierarchyLevel)) hlToTL.set(f.hierarchyLevel, new Set());
      hlToTL.get(f.hierarchyLevel)!.add(f.tlayerId);
    }

    // Find boundaries: between hierarchy levels where TL changes
    const boundaries: { afterHL: number; tlBelow: number; tlAbove: number }[] = [];
    const sortedHLs = [...hlToTL.keys()].sort((a, b) => a - b);
    for (let i = 0; i < sortedHLs.length - 1; i++) {
      const hlBelow = sortedHLs[i];
      const hlAbove = sortedHLs[i + 1];
      const tlsBelow = hlToTL.get(hlBelow)!;
      const tlsAbove = hlToTL.get(hlAbove)!;
      const maxTLBelow = Math.max(...tlsBelow);
      const minTLAbove = Math.min(...tlsAbove);
      if (maxTLBelow !== minTLAbove) {
        boundaries.push({ afterHL: hlBelow, tlBelow: maxTLBelow, tlAbove: minTLAbove });
      }
    }
    return boundaries;
  }, [frames]);

  // Display order: sort by displayOrder, assign X positions
  const dispSorted = [...frames].sort((a, b) => a.displayOrder - b.displayOrder || a.decodeIndex - b.decodeIndex);
  const dispPosMap = new Map<number, number>(); // decodeIndex → X column in display order
  dispSorted.forEach((f, i) => dispPosMap.set(f.decodeIndex, i));

  // Decode order: X position = decodeIndex
  const decPosMap = new Map<number, number>();
  frames.forEach(f => decPosMap.set(f.decodeIndex, f.decodeIndex));

  const numCols = frames.length;
  const svgW = MARGIN_LEFT + numCols * CELL_W + (tlBoundaries.length > 0 ? 80 : 20);

  // Section heights
  const sectionH = numTLRows * TL_ROW_H + 4;
  const dispY = MARGIN_TOP;
  const decY = dispY + sectionH + SECTION_GAP;
  const svgH = decY + sectionH + 30;

  // Get Y position within a section based on temporal layer
  // Get Y position within a section — TL0 at bottom, higher TLs go up
  function tlY(baseY: number, tl: number): number {
    return baseY + (maxHL - tl) * TL_ROW_H;
  }

  // Frame cell center position
  function cellCenter(baseY: number, col: number, tl: number) {
    return {
      x: MARGIN_LEFT + col * CELL_W + CELL_W / 2,
      y: tlY(baseY, tl) + CELL_H / 2,
    };
  }

  // Render dependency arrows within a section
  function renderArrows(baseY: number, posMap: Map<number, number>) {
    const arrows: JSX.Element[] = [];
    for (const node of frames) {
      const toCol = posMap.get(node.decodeIndex);
      if (toCol === undefined) continue;

      for (const refIdx of node.references) {
        const refNode = frames[refIdx];
        if (!refNode) continue;
        const fromCol = posMap.get(refIdx);
        if (fromCol === undefined) continue;

        const from = cellCenter(baseY, fromCol, refNode.hierarchyLevel);
        const to = cellCenter(baseY, toCol, node.hierarchyLevel);

        // Arrow from right edge of source to left edge of target
        const x1 = from.x + CELL_W / 2 - 4;
        const x2 = to.x - CELL_W / 2 + 4;
        const y1 = from.y;
        const y2 = to.y;

        const isHl = highlightedSet?.has(refIdx) && highlightedSet?.has(node.decodeIndex);
        const isDimmed = highlightedSet && !isHl;

        // Straight or slight curve depending on vertical distance
        const midX = (x1 + x2) / 2;
        const path = y1 === y2
          ? `M ${x1} ${y1} L ${x2} ${y2}`
          : `M ${x1} ${y1} C ${midX} ${y1} ${midX} ${y2} ${x2} ${y2}`;

        arrows.push(
          <path
            key={`arr-${refIdx}-${node.decodeIndex}-${baseY}`}
            className={`dep-arrow ${isDimmed ? 'dimmed' : ''} ${isHl ? 'highlighted' : ''}`}
            d={path}
            fill="none"
            stroke={isHl ? '#1565c0' : '#d0d0d0'}
            strokeWidth={isHl ? 1.5 : 0.7}
            markerEnd={isHl ? 'url(#ah-hl)' : 'url(#ah)'}
          />
        );
      }
    }
    return arrows;
  }

  // Render frame cells within a section
  function renderCells(baseY: number, posMap: Map<number, number>, showLabel: boolean) {
    const ordered = showLabel ? dispSorted : frames;
    return ordered.map((node) => {
      const col = posMap.get(node.decodeIndex);
      if (col === undefined) return null;
      const x = MARGIN_LEFT + col * CELL_W;
      const y = tlY(baseY, node.hierarchyLevel);
      const color = getFrameColor(node);
      const idx = node.decodeIndex;
      const isDimmed = highlightedSet && !highlightedSet.has(idx);
      const isHl = highlightedSet?.has(idx);
      const label = getFrameLabel(node);

      return (
        <g key={`c-${idx}-${baseY}`}
          className={`frame-cell ${isDimmed ? 'dimmed' : ''} ${isHl ? 'highlighted' : ''}`}
          onClick={(e) => { e.stopPropagation(); handleClick(idx); }}>
          <rect
            x={x + 2} y={y + 1}
            width={CELL_W - 4} height={CELL_H - 2}
            rx={4} ry={4}
            fill={color.fill} stroke={color.stroke}
            strokeWidth={selected === idx ? 2.5 : 1.5}
            strokeDasharray={node.isSEF ? '4,2' : undefined}
          />
          <text x={x + CELL_W / 2} y={y + CELL_H / 2 + 1}
            textAnchor="middle" dominantBaseline="middle"
            fontSize={9} fontFamily="Monaco, Courier New, monospace"
            fontWeight={600} fill={color.stroke}>
            {label}
          </text>
        </g>
      );
    });
  }

  // TL row labels
  function renderTLLabels(baseY: number) {
    return Array.from({ length: numTLRows }, (_, hl) => (
      <text key={`hl-${baseY}-${hl}`}
        x={MARGIN_LEFT - 8} y={tlY(baseY, hl) + CELL_H / 2 + 1}
        textAnchor="end" fontSize={9} fill="#aaa"
        fontFamily="Monaco, Courier New, monospace">
        {hl}
      </text>
    ));
  }

  // Render TL boundary lines (horizontal dashed lines where obu_tlayer_id changes)
  function renderTLBoundaries(baseY: number) {
    if (tlBoundaries.length === 0) return null;
    return tlBoundaries.map((b, i) => {
      // Line between afterHL and afterHL+1 in Y space
      const yPos = tlY(baseY, b.afterHL) + CELL_H + (TL_ROW_H - CELL_H) / 2;
      return (
        <g key={`tlb-${baseY}-${i}`}>
          <line
            x1={MARGIN_LEFT - 4} y1={yPos}
            x2={MARGIN_LEFT + numCols * CELL_W + 4} y2={yPos}
            stroke="#ef6c00" strokeWidth={1} strokeDasharray="6,3"
            opacity={0.6}
          />
          <text
            x={MARGIN_LEFT + numCols * CELL_W + 8} y={yPos + 3}
            fontSize={9} fill="#ef6c00" fontWeight={600}
            fontFamily="Monaco, Courier New, monospace">
            TL{b.tlBelow}|TL{b.tlAbove}
          </text>
        </g>
      );
    });
  }

  const xlLabel = XLAYER_LABELS[xlayerId] || `X${xlayerId}`;
  const xlAccent = XLAYER_ACCENT[Math.min(xlayerId, 4)] || '#666';

  return (
    <div className="xlayer-graph-section">
      {showXlayerLabel && (
        <div className="xlayer-graph-label" style={{ borderLeftColor: xlAccent }}>
          <span style={{ color: xlAccent, fontWeight: 700 }}>{xlLabel}</span>
        </div>
      )}
      <div className="frame-dependency-graph">
        <svg width={svgW} height={svgH} viewBox={`0 0 ${svgW} ${svgH}`}
          onClick={() => setSelected(null)}>
          <defs>
            <marker id="ah" markerWidth="5" markerHeight="4" refX="5" refY="2" orient="auto">
              <polygon points="0 0, 5 2, 0 4" fill="#d0d0d0" />
            </marker>
            <marker id="ah-hl" markerWidth="5" markerHeight="4" refX="5" refY="2" orient="auto">
              <polygon points="0 0, 5 2, 0 4" fill="#1565c0" />
            </marker>
          </defs>

          {/* Section: Display Order */}
          <text x={5} y={dispY - 6} fontSize={11} fill="#333" fontWeight={600}>
            Display Order
          </text>
          {/* TL background bands */}
          {Array.from({ length: numTLRows }, (_, tl) => (
            <rect key={`bg-d-${tl}`}
              x={MARGIN_LEFT - 4} y={tlY(dispY, tl) - 2}
              width={numCols * CELL_W + 8} height={TL_ROW_H}
              fill={tl % 2 === 0 ? '#fafafa' : '#f5f5f5'} rx={2}
            />
          ))}
          {renderTLLabels(dispY)}
          {renderArrows(dispY, dispPosMap)}
          {renderCells(dispY, dispPosMap, true)}
          {renderTLBoundaries(dispY)}

          {/* Section: Decode Order */}
          <text x={5} y={decY - 6} fontSize={11} fill="#333" fontWeight={600}>
            Decode Order
          </text>
          {Array.from({ length: numTLRows }, (_, tl) => (
            <rect key={`bg-c-${tl}`}
              x={MARGIN_LEFT - 4} y={tlY(decY, tl) - 2}
              width={numCols * CELL_W + 8} height={TL_ROW_H}
              fill={tl % 2 === 0 ? '#fafafa' : '#f5f5f5'} rx={2}
            />
          ))}
          {renderTLLabels(decY)}
          {renderArrows(decY, decPosMap)}
          {renderCells(decY, decPosMap, false)}
          {renderTLBoundaries(decY)}
        </svg>
      </div>
    </div>
  );
}
