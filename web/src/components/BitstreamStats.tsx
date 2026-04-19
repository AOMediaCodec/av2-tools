import { useMemo } from 'react';
import {
  BarChart, Bar, XAxis, YAxis, CartesianGrid, Tooltip, Legend,
  PieChart, Pie, Cell, ResponsiveContainer,
} from 'recharts';
import { FrameDependencyGraph } from './FrameDependencyGraph';
import './BitstreamStats.css';

interface BitstreamStatsProps {
  obus: any[];
}

// Xlayer colors matching OBUBrowser
const XLAYER_COLORS: Record<number, string> = {
  0: '#1976d2',
  1: '#43a047',
  2: '#8e24aa',
  3: '#ef6c00',
  4: '#00897b',
  31: '#9e9e9e',
};

// OBU type colors matching CSS
const OBU_TYPE_COLORS: Record<string, string> = {
  SEQUENCE_HEADER: '#1565c0',
  CLK: '#6a1b9a',
  OLK: '#6a1b9a',
  REGULAR_TILE_GROUP: '#2e7d32',
  LEADING_TILE_GROUP: '#2e7d32',
  TEMPORAL_DELIMITER: '#c2185b',
  METADATA: '#f57c00',
  METADATA_GROUP: '#e65100',
  CONTENT_INTERPRETATION: '#558b2f',
  REGULAR_TIP: '#0277bd',
  LEADING_TIP: '#0277bd',
  REGULAR_SEF: '#00838f',
  LEADING_SEF: '#00838f',
  LAYER_CONFIGURATION_RECORD: '#4527a0',
  OPERATING_POINT_SET: '#283593',
  MSDO: '#1565c0',
  SWITCH: '#6a1b9a',
  RAS_FRAME: '#6a1b9a',
  BRIDGE_FRAME: '#4e342e',
  BUFFER_REMOVAL_TIMING: '#546e7a',
  MULTI_FRAME_HEADER: '#37474f',
  QM: '#607d8b',
  FGM: '#607d8b',
  ATLAS_SEGMENT: '#455a64',
  PADDING: '#bdbdbd',
};

function getObuSize(obu: any): number {
  return obu.position.size_field_bytes + obu.position.header_size + obu.position.payload_size;
}

function getXlayerId(obu: any): number {
  if (obu.header.extension_flag === 1) return obu.header.xlayer_id;
  return (obu.type_name === 'MSDO' || obu.type_name === 'TEMPORAL_DELIMITER') ? 31 : 0;
}

function getXlayerColor(xlId: number): string {
  return XLAYER_COLORS[Math.min(xlId, 4)] || XLAYER_COLORS[31];
}

interface TUData {
  index: number;
  label: string;
  totalSize: number;
  keyframeType: string | null;
  xlayerSizes: Record<number, number>;
  obuTypeSizes: Record<string, number>;
}

function computeTUData(obus: any[]): TUData[] {
  const tus: TUData[] = [];
  let currentTU: TUData | null = null;
  let tuIndex = 0;

  for (const obu of obus) {
    if (obu.type_name === 'TEMPORAL_DELIMITER') {
      if (currentTU) tus.push(currentTU);
      currentTU = {
        index: tuIndex++,
        label: `TU ${tuIndex - 1}`,
        totalSize: 0,
        keyframeType: null,
        xlayerSizes: {},
        obuTypeSizes: {},
      };
      continue;
    }

    if (!currentTU) {
      currentTU = {
        index: tuIndex++,
        label: 'Config',
        totalSize: 0,
        keyframeType: null,
        xlayerSizes: {},
        obuTypeSizes: {},
      };
    }

    const size = getObuSize(obu);
    const xlId = getXlayerId(obu);
    currentTU.totalSize += size;
    currentTU.xlayerSizes[xlId] = (currentTU.xlayerSizes[xlId] || 0) + size;
    currentTU.obuTypeSizes[obu.type_name] = (currentTU.obuTypeSizes[obu.type_name] || 0) + size;

    if (obu.type_name === 'CLK') currentTU.keyframeType = 'CLK';
    else if (obu.type_name === 'OLK' && !currentTU.keyframeType) currentTU.keyframeType = 'OLK';
  }

  if (currentTU) tus.push(currentTU);
  return tus;
}

function formatSize(bytes: number): string {
  if (bytes >= 1024 * 1024) return `${(bytes / (1024 * 1024)).toFixed(2)} MB`;
  if (bytes >= 1024) return `${(bytes / 1024).toFixed(1)} KB`;
  return `${bytes} B`;
}

export function BitstreamStats({ obus }: BitstreamStatsProps) {
  const tuData = useMemo(() => computeTUData(obus), [obus]);

  // Detect xlayers
  const xlayerIds = useMemo(() => {
    const ids = new Set<number>();
    obus.forEach(o => ids.add(getXlayerId(o)));
    return [...ids].sort((a, b) => a - b);
  }, [obus]);
  const isMultiLayer = xlayerIds.filter(x => x !== 31).length > 1;

  // OBU type breakdown
  const obuTypeData = useMemo(() => {
    const typeSizes: Record<string, number> = {};
    obus.forEach(o => {
      const size = getObuSize(o);
      typeSizes[o.type_name] = (typeSizes[o.type_name] || 0) + size;
    });
    return Object.entries(typeSizes)
      .map(([name, value]) => ({ name, value }))
      .sort((a, b) => b.value - a.value);
  }, [obus]);

  // TU size bar data — for multi-layer, add per-xlayer columns
  const tuBarData = useMemo(() => {
    return tuData.filter(tu => tu.label !== 'Config').map(tu => {
      const entry: any = {
        name: tu.label,
        totalSize: tu.totalSize,
        keyframe: tu.keyframeType,
      };
      if (isMultiLayer) {
        xlayerIds.forEach(xl => {
          const key = xl === 31 ? 'GL' : `X${xl}`;
          entry[key] = tu.xlayerSizes[xl] || 0;
        });
      }
      return entry;
    });
  }, [tuData, xlayerIds, isMultiLayer]);

  // Layer breakdown data (for multi-layer)
  const layerData = useMemo(() => {
    if (!isMultiLayer) return [];
    const layerSizes: Record<number, number> = {};
    obus.forEach(o => {
      const xl = getXlayerId(o);
      layerSizes[xl] = (layerSizes[xl] || 0) + getObuSize(o);
    });
    return Object.entries(layerSizes)
      .map(([id, value]) => ({
        name: Number(id) === 31 ? 'Global' : `X${id}`,
        value,
        xlId: Number(id),
      }))
      .sort((a, b) => a.xlId - b.xlId);
  }, [obus, isMultiLayer, xlayerIds]);

  const CustomTooltip = ({ active, payload, label }: any) => {
    if (!active || !payload?.length) return null;
    return (
      <div className="chart-tooltip">
        <p className="chart-tooltip-label">{label}</p>
        {payload.map((p: any, i: number) => (
          <p key={i} style={{ color: p.color }}>
            {p.name}: {formatSize(p.value)}
          </p>
        ))}
      </div>
    );
  };

  return (
    <div className="bitstream-stats">
      {/* Frame dependency graph — full width */}
      <FrameDependencyGraph obus={obus} />

      <div className="stats-grid">
        {/* TU Size Chart */}
        <div className="chart-card">
          <h3>TU Size (Decode Order)</h3>
          <ResponsiveContainer width="100%" height={300}>
            {isMultiLayer ? (
              <BarChart data={tuBarData}>
                <CartesianGrid strokeDasharray="3 3" />
                <XAxis dataKey="name" fontSize={11} />
                <YAxis tickFormatter={(v) => formatSize(v)} fontSize={11} />
                <Tooltip content={<CustomTooltip />} />
                <Legend />
                {xlayerIds.map(xl => {
                  const key = xl === 31 ? 'GL' : `X${xl}`;
                  return (
                    <Bar key={key} dataKey={key} stackId="a"
                      fill={xl === 31 ? XLAYER_COLORS[31] : getXlayerColor(xl)} />
                  );
                })}
              </BarChart>
            ) : (
              <BarChart data={tuBarData}>
                <CartesianGrid strokeDasharray="3 3" />
                <XAxis dataKey="name" fontSize={11} />
                <YAxis tickFormatter={(v) => formatSize(v)} fontSize={11} />
                <Tooltip content={<CustomTooltip />} />
                <Bar dataKey="totalSize" name="Size">
                  {tuBarData.map((entry, index) => (
                    <Cell key={index}
                      fill={entry.keyframe === 'CLK' ? '#6a1b9a' :
                            entry.keyframe === 'OLK' ? '#9c27b0' : '#1976d2'} />
                  ))}
                </Bar>
              </BarChart>
            )}
          </ResponsiveContainer>
        </div>

        {/* OBU Type Distribution */}
        <div className="chart-card">
          <h3>OBU Type Distribution</h3>
          <ResponsiveContainer width="100%" height={Math.max(200, obuTypeData.length * 32)}>
            <BarChart data={obuTypeData} layout="vertical" margin={{ left: 10, right: 30 }}>
              <CartesianGrid strokeDasharray="3 3" horizontal={false} />
              <XAxis type="number" tickFormatter={(v) => formatSize(v)} fontSize={11} />
              <YAxis type="category" dataKey="name" width={200} fontSize={11}
                tick={{ fontFamily: 'Monaco, Courier New, monospace' }} />
              <Tooltip content={<CustomTooltip />} />
              <Bar dataKey="value" name="Size">
                {obuTypeData.map((entry, index) => (
                  <Cell key={index} fill={OBU_TYPE_COLORS[entry.name] || '#9e9e9e'} />
                ))}
              </Bar>
            </BarChart>
          </ResponsiveContainer>
        </div>

        {/* Layer Distribution (multi-layer only) */}
        {isMultiLayer && (
          <div className="chart-card">
            <h3>Layer Distribution</h3>
            <ResponsiveContainer width="100%" height={300}>
              <PieChart>
                <Pie
                  data={layerData}
                  dataKey="value"
                  nameKey="name"
                  cx="50%"
                  cy="50%"
                  outerRadius={100}
                  innerRadius={40}
                  label={({ name, percent }: any) => `${name} (${((percent || 0) * 100).toFixed(0)}%)`}
                >
                  {layerData.map((entry) => (
                    <Cell key={entry.xlId}
                      fill={entry.xlId === 31 ? XLAYER_COLORS[31] : getXlayerColor(entry.xlId)} />
                  ))}
                </Pie>
                <Tooltip formatter={(value: any) => formatSize(Number(value))} />
              </PieChart>
            </ResponsiveContainer>
          </div>
        )}
      </div>
    </div>
  );
}
