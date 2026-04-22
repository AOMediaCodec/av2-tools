/**
 * Frame dependency graph computation from parsed AV2 OBU data.
 * Implements order hint unwrapping and reference buffer simulation
 * per AV2 spec sections 5.9.2 and 7.6.
 */

export interface FrameNode {
  decodeIndex: number;
  displayOrder: number;
  orderHintLsbs: number;
  obuType: string;
  frameType: string;
  isOutput: boolean;
  isSEF: boolean;
  isTIP: boolean;
  refreshFlags: number;
  references: number[];   // decodeIndex of frames this depends on
  sefTargetDisplayOrder: number;  // display order of referenced frame (-1 if not SEF)
  xlayerId: number;
  tlayerId: number;         // from OBU header (0-3)
  hierarchyLevel: number;   // derived from min reference distance
  size: number;
}

interface RefSlot {
  valid: boolean;
  orderHint: number;
  decodeIndex: number;
  isOutput: boolean;
}

function getFrameHeader(obu: any): any | null {
  return obu.tile_group?.frame_header || obu.frame_header || null;
}

function getXlayerId(obu: any): number {
  if (obu.header.extension_flag === 1) return obu.header.xlayer_id;
  return (obu.type_name === 'MSDO' || obu.type_name === 'TEMPORAL_DELIMITER') ? 31 : 0;
}

function getObuSize(obu: any): number {
  return obu.position.size_field_bytes + obu.position.header_size + obu.position.payload_size;
}

/**
 * Unwrap modular order_hint to absolute display order.
 * Spec: get_disp_order_hint() in section 5.9.2
 */
function getDispOrderHint(
  orderHintLsbs: number,
  obuType: string,
  isSEF: boolean,
  frameType: string,
  restrictedPredictionSwitch: number,
  refSlots: RefSlot[],
  orderHintBits: number,
  gopBase: number
): number {
  // CLK: return raw LSBs + GOP base (fresh start)
  if (obuType === 'CLK') {
    return gopBase + orderHintLsbs;
  }

  // Restricted SWITCH: return raw LSBs + GOP base
  if (!isSEF && frameType === 'SWITCH_FRAME' && restrictedPredictionSwitch) {
    return gopBase + orderHintLsbs;
  }

  // Normal path: unwrap using max of valid showable refs
  let maxDisp = gopBase;
  for (const slot of refSlots) {
    if (slot.valid && slot.orderHint >= 0 && slot.isOutput) {
      maxDisp = Math.max(maxDisp, slot.orderHint);
    }
  }

  let dispOrderHint = orderHintLsbs;
  if (orderHintBits > 0) {
    const halfRange = 1 << (orderHintBits - 1);
    const offset = maxDisp - halfRange - orderHintLsbs;
    if (offset >= 0) {
      dispOrderHint += (((offset >> orderHintBits) + 1) << orderHintBits);
    }
  }
  return dispOrderHint;
}

/**
 * Compute frame dependency graphs from parsed OBU data.
 * Returns one FrameNode[] per xlayer (each xlayer is independent).
 */
export function computeFrameGraphs(obus: any[]): Map<number, FrameNode[]> {
  const FRAME_TYPES = new Set([
    'CLK', 'OLK', 'REGULAR_TILE_GROUP', 'LEADING_TILE_GROUP',
    'SWITCH', 'RAS_FRAME', 'BRIDGE_FRAME',
    'REGULAR_SEF', 'LEADING_SEF',
    'REGULAR_TIP', 'LEADING_TIP',
  ]);

  // Collect sequence headers per xlayer
  const seqHeaders = new Map<number, { OrderHintBits: number; NumRefFrames: number }>();
  for (const obu of obus) {
    if (obu.type_name === 'SEQUENCE_HEADER') {
      const xl = obu.header.extension_flag === 1 ? obu.header.xlayer_id : 0;
      const ic = obu.sequence_header?.inter_config;
      if (ic) {
        seqHeaders.set(xl, {
          OrderHintBits: ic.OrderHintBits || 1,
          NumRefFrames: ic.NumRefFrames || 8,
        });
      }
    }
  }

  // Group frame OBUs by xlayer
  const obusByXlayer = new Map<number, any[]>();
  for (const obu of obus) {
    if (!FRAME_TYPES.has(obu.type_name)) continue;
    const xl = getXlayerId(obu);
    if (xl === 31) continue; // skip global OBUs (TD, etc.)
    if (!obusByXlayer.has(xl)) obusByXlayer.set(xl, []);
    obusByXlayer.get(xl)!.push(obu);
  }

  // Compute graph for each xlayer independently
  const result = new Map<number, FrameNode[]>();
  for (const [xl, xlObus] of obusByXlayer) {
    const sh = seqHeaders.get(xl) || seqHeaders.values().next().value || { OrderHintBits: 1, NumRefFrames: 8 };
    result.set(xl, computeSingleLayerGraph(xlObus, sh.OrderHintBits, sh.NumRefFrames, xl));
  }

  return result;
}

/**
 * Compute frame graph for a single xlayer.
 */
function computeSingleLayerGraph(
  obus: any[],
  orderHintBits: number,
  numRefFrames: number,
  xlayerId: number
): FrameNode[] {
  const refSlots: RefSlot[] = Array.from({ length: numRefFrames }, () => ({
    valid: false, orderHint: -1, decodeIndex: -1, isOutput: false,
  }));

  const frames: FrameNode[] = [];
  let decodeIndex = 0;
  let gopBase = 0;
  let prevGopMaxHint = 0;

  for (const obu of obus) {

    const fh = getFrameHeader(obu);
    const isSEF = obu.type_name === 'REGULAR_SEF' || obu.type_name === 'LEADING_SEF';
    const isTIP = obu.type_name === 'REGULAR_TIP' || obu.type_name === 'LEADING_TIP';

    let orderHintLsbs = 0;
    let frameType = 'INTER_FRAME';
    let isOutput = true;
    let refreshFlags = 0;
    let restrictedPredictionSwitch = 0;
    let refFrameIdx: number[] = [];
    let sefShowIdx = -1;

    if (fh) {
      orderHintLsbs = fh.order_hint ?? 0;
      frameType = fh.FrameType ?? 'INTER_FRAME';
      isOutput = !!(fh.immediate_output_frame || fh.implicit_output_frame);
      refreshFlags = fh.refresh_frame_flags ?? 0;
      restrictedPredictionSwitch = fh.restricted_prediction_switch ?? 0;
      refFrameIdx = fh.ref_frame_idx ?? [];

      if (isSEF) {
        isOutput = true;
        sefShowIdx = fh.frame_to_show_map_idx ?? -1;
        if (!fh.derive_sef_order_hint) {
          orderHintLsbs = fh.sef_order_hint ?? fh.order_hint ?? 0;
        }
      }
    }

    // CLK resets GOP base
    if (obu.type_name === 'CLK') {
      if (decodeIndex > 0) {
        gopBase = prevGopMaxHint + 1;
      }
      // Reset reference buffer
      for (let i = 0; i < numRefFrames; i++) {
        refSlots[i].valid = false;
      }
    }

    // Compute display order
    let displayOrder: number;
    if (isSEF && fh?.derive_sef_order_hint && sefShowIdx >= 0 && refSlots[sefShowIdx]?.valid) {
      displayOrder = refSlots[sefShowIdx].orderHint;
    } else {
      displayOrder = getDispOrderHint(
        orderHintLsbs, obu.type_name, isSEF, frameType,
        restrictedPredictionSwitch, refSlots, orderHintBits, gopBase
      );
    }

    // Track max for GOP base computation
    prevGopMaxHint = Math.max(prevGopMaxHint, displayOrder);

    // Determine references
    const references: number[] = [];
    if (frameType === 'KEY_FRAME' || frameType === 'INTRA_ONLY_FRAME') {
      // No references
    } else if (isSEF && sefShowIdx >= 0) {
      if (refSlots[sefShowIdx]?.valid && refSlots[sefShowIdx].decodeIndex >= 0) {
        references.push(refSlots[sefShowIdx].decodeIndex);
      }
    } else if (refFrameIdx.length > 0) {
      // Explicit ref frame map
      for (const idx of refFrameIdx) {
        if (idx < numRefFrames && refSlots[idx]?.valid && refSlots[idx].decodeIndex >= 0) {
          if (!references.includes(refSlots[idx].decodeIndex)) {
            references.push(refSlots[idx].decodeIndex);
          }
        }
      }
    } else {
      // Implicit ref frame map: all valid slots are potential references
      for (let i = 0; i < numRefFrames; i++) {
        if (refSlots[i]?.valid && refSlots[i].decodeIndex >= 0) {
          if (!references.includes(refSlots[i].decodeIndex)) {
            references.push(refSlots[i].decodeIndex);
          }
        }
      }
    }

    // Compute min display-order distance to nearest available reference
    let minRefDist = 0;
    if (frameType !== 'KEY_FRAME' && frameType !== 'INTRA_ONLY_FRAME') {
      minRefDist = Infinity;
      for (let i = 0; i < numRefFrames; i++) {
        if (refSlots[i].valid && refSlots[i].orderHint >= 0) {
          const dist = Math.abs(displayOrder - refSlots[i].orderHint);
          if (dist > 0 && dist < minRefDist) minRefDist = dist;
        }
      }
      if (!isFinite(minRefDist)) minRefDist = 1;
    }

    // Resolve SEF target display order
    const sefTargetDisplayOrder = (isSEF && sefShowIdx >= 0 && refSlots[sefShowIdx]?.valid)
      ? refSlots[sefShowIdx].orderHint : -1;

    frames.push({
      decodeIndex,
      displayOrder,
      orderHintLsbs,
      obuType: obu.type_name,
      frameType,
      isOutput,
      isSEF,
      isTIP,
      refreshFlags,
      references,
      sefTargetDisplayOrder,
      xlayerId,
      tlayerId: obu.header.tlayer_id ?? 0,
      hierarchyLevel: -1, // computed in second pass below
      _minRefDist: minRefDist, // temp field for hierarchy computation
      size: getObuSize(obu),
    } as any);

    // Update reference buffer
    for (let i = 0; i < numRefFrames; i++) {
      if ((refreshFlags >> i) & 1) {
        refSlots[i] = { valid: true, orderHint: displayOrder, decodeIndex, isOutput };
      }
    }

    decodeIndex++;
  }

  // Second pass: compute hierarchy levels from min ref distances
  // maxDepth = floor(log2(maxDist)) auto-scales with GOP size
  const maxDist = Math.max(1, ...frames.map((f: any) => f._minRefDist || 0).filter(isFinite));
  const maxDepth = Math.max(1, Math.floor(Math.log2(maxDist)));

  for (const f of frames as any[]) {
    if (f.frameType === 'KEY_FRAME' || f.frameType === 'INTRA_ONLY_FRAME' || f._minRefDist === 0) {
      f.hierarchyLevel = 0;
    } else {
      const dist = f._minRefDist || 1;
      f.hierarchyLevel = Math.max(0, maxDepth - Math.floor(Math.log2(dist)));
    }
    delete f._minRefDist;
  }

  return frames;
}
