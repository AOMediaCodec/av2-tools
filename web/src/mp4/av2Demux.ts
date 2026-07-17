// Demux an AV2-in-ISOBMFF (.mp4) file into an AV2 elementary bitstream (.obu)
// in the browser, so the existing OBU parser can process it unchanged.
//
// Mirrors the C++ av2_demux (av2-tools/apps/av2_demux/av2_demuxer.cpp):
//   - read the av2C configOBUs from the sample entry,
//   - for every sample emit a Temporal Delimiter, splice the configOBUs after
//     the first sample's TD, then the sample's frame OBUs.
//
// mp4box.js handles ISOBMFF parsing + sample extraction. It does not know AV2,
// and mp4box 2.4.1 dropped the classic createBoxCtor registration API, so the
// one box it can't surface - av2C - is read here with a small manual box walk.

import { createFile } from 'mp4box';

// av2C payload layout (per av2-isobmff): a 9-byte prefix followed by configOBUs.
const AV2C_PREFIX_LEN = 9;
// VisualSampleEntry fixed fields preceding the child boxes (matches the C++
// kVisualSampleEntryFixedSize): 8-byte box header + 78 bytes.
const SAMPLE_ENTRY_HEADER_LEN = 8 + 78;
// Annex-B framed Temporal Delimiter OBU (leb128 size = 1, obu_type = 2).
const TD_BYTES = Uint8Array.of(0x01, 0x08);

function u32(d: Uint8Array, o: number): number {
  return (d[o] * 0x1000000 + (d[o + 1] << 16) + (d[o + 2] << 8) + d[o + 3]) >>> 0;
}

function fourCC(d: Uint8Array, o: number): string {
  return String.fromCharCode(d[o], d[o + 1], d[o + 2], d[o + 3]);
}

// First child box of `type` within [start, end). Returns [boxStart, boxEnd] or null.
function findChild(
  d: Uint8Array,
  start: number,
  end: number,
  type: string,
): [number, number] | null {
  let o = start;
  while (o + 8 <= end) {
    const size = u32(d, o);
    const boxEnd = size === 0 ? end : o + size;
    if (size !== 0 && (size < 8 || boxEnd > end)) break;
    if (fourCC(d, o + 4) === type) return [o, boxEnd];
    o = boxEnd;
  }
  return null;
}

// Walk one trak's mdia > minf > stbl > stsd > av02 > av2C; return the
// configOBUs (av2C payload minus the 9-byte prefix), or null if not an AV2 track.
function configObusFromTrak(d: Uint8Array, start: number, end: number): Uint8Array | null {
  let node = findChild(d, start, end, 'mdia');
  if (!node) return null;
  node = findChild(d, node[0] + 8, node[1], 'minf');
  if (!node) return null;
  node = findChild(d, node[0] + 8, node[1], 'stbl');
  if (!node) return null;
  const stsd = findChild(d, node[0] + 8, node[1], 'stsd');
  if (!stsd) return null;
  // stsd is a FullBox: 8-byte header + 4 (version/flags) + 4 (entry_count).
  const seStart = stsd[0] + 16;
  if (seStart + 8 > stsd[1] || fourCC(d, seStart + 4) !== 'av02') return null;
  const seEnd = Math.min(seStart + u32(d, seStart), stsd[1]);
  const av2c = findChild(d, seStart + SAMPLE_ENTRY_HEADER_LEN, seEnd, 'av2C');
  if (!av2c) return null;
  const payload = d.subarray(av2c[0] + 8, av2c[1]);
  return payload.length >= AV2C_PREFIX_LEN ? payload.subarray(AV2C_PREFIX_LEN) : null;
}

// Find the av2C configOBUs of the first AV2 (av02) track in the file.
function extractConfigObus(d: Uint8Array): Uint8Array | null {
  const moov = findChild(d, 0, d.length, 'moov');
  if (!moov) return null;
  let o = moov[0] + 8;
  while (o + 8 <= moov[1]) {
    const size = u32(d, o);
    const boxEnd = size === 0 ? moov[1] : o + size;
    if (size < 8 || boxEnd > moov[1]) break;
    if (fourCC(d, o + 4) === 'trak') {
      const cfg = configObusFromTrak(d, o + 8, boxEnd);
      if (cfg) return cfg;
    }
    o = boxEnd;
  }
  return null;
}

// If `p` starts with an Annex-B framed Temporal Delimiter OBU, return its total
// length in bytes; otherwise 0. Mirrors leading_td_length() in the C++ demuxer.
function leadingTdLength(p: Uint8Array): number {
  if (p.length < 2) return 0;
  let i = 0;
  let obuSize = 0;
  let shift = 0;
  while (i < p.length && i < 8) {
    const b = p[i++];
    obuSize |= (b & 0x7f) << shift;
    if (!(b & 0x80)) break;
    shift += 7;
  }
  if (obuSize === 0 || i >= p.length || i + obuSize > p.length) return 0;
  const obuType = (p[i] >> 2) & 0x1f; // header byte: ext(1) type(5) tlayer(2)
  if (obuType !== 2) return 0; // not a TEMPORAL_DELIMITER
  return i + obuSize;
}

// Reconstruct the elementary stream from configOBUs + per-sample frame data.
function reconstruct(samples: Uint8Array[], configObus: Uint8Array): Uint8Array {
  const parts: Uint8Array[] = [];
  samples.forEach((sample, i) => {
    const tdLen = leadingTdLength(sample);
    if (tdLen > 0) {
      parts.push(sample.subarray(0, tdLen)); // TD already in the sample
      if (i === 0) parts.push(configObus);
      parts.push(sample.subarray(tdLen));
    } else {
      parts.push(TD_BYTES); // synthesize a TD at the sample boundary
      if (i === 0) parts.push(configObus);
      parts.push(sample);
    }
  });
  const total = parts.reduce((n, p) => n + p.length, 0);
  const out = new Uint8Array(total);
  let off = 0;
  for (const p of parts) {
    out.set(p, off);
    off += p.length;
  }
  return out;
}

// True if the bytes look like an ISOBMFF file (`....ftyp`) or the name is .mp4.
export function looksLikeMp4(data: Uint8Array, filename: string): boolean {
  if (/\.(mp4|m4s|m4v|mov)$/i.test(filename)) return true;
  return (
    data.length >= 8 &&
    data[4] === 0x66 && // 'f'
    data[5] === 0x74 && // 't'
    data[6] === 0x79 && // 'y'
    data[7] === 0x70 //    'p'
  );
}

// Demux an AV2 .mp4 into an .obu elementary stream.
export function demuxMp4ToObu(data: Uint8Array): Promise<Uint8Array> {
  const configObus = extractConfigObus(data);
  if (!configObus) {
    return Promise.reject(
      new Error("no AV2 sample entry ('av02' with 'av2C') found in this file"),
    );
  }

  const mp4: any = createFile();

  return new Promise<Uint8Array>((resolve, reject) => {
    const collected: Uint8Array[] = [];
    let expected = -1;
    let settled = false;

    const fail = (msg: string) => {
      if (settled) return;
      settled = true;
      reject(new Error(msg));
    };

    mp4.onError = (e: string) => fail(`mp4box error: ${e}`);

    mp4.onReady = (info: any) => {
      const track = (info.tracks || []).find(
        (t: any) => typeof t.codec === 'string' && t.codec.startsWith('av02'),
      );
      if (!track) {
        fail("no 'av02' (AV2) video track found in this file");
        return;
      }
      expected = track.nb_samples;
      if (expected === 0) {
        if (!settled) {
          settled = true;
          resolve(reconstruct([], configObus));
        }
        return;
      }
      mp4.setExtractionOptions(track.id, null, { nbSamples: expected });
      mp4.start();
    };

    mp4.onSamples = (_id: number, _user: unknown, samples: any[]) => {
      for (const s of samples) collected.push(new Uint8Array(s.data));
      if (!settled && expected >= 0 && collected.length >= expected) {
        settled = true;
        resolve(reconstruct(collected, configObus));
      }
    };

    // mp4box wants an ArrayBuffer tagged with its offset in the file.
    const ab = data.slice().buffer as ArrayBuffer & { fileStart?: number };
    ab.fileStart = 0;
    mp4.appendBuffer(ab);
    mp4.flush();
  });
}
