// Smoke test of the WebAssembly parser in Node.
//   node tests/wasm/wasm_smoke.mjs build-wasm/src/wasm/av2-parser.js [stream.obu]
// Without a stream argument a tiny built-in stream is parsed (Temporal Delimiter + Padding OBU).
import { createRequire } from 'node:module';
import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';

const [modulePath, streamPath] = process.argv.slice(2);
if (!modulePath) {
  console.error('usage: node wasm_smoke.mjs <av2-parser.js> [stream.obu]');
  process.exit(2);
}
const require = createRequire(import.meta.url);
const createAV2Module = require(resolve(modulePath));
const m = await createAV2Module();
const bytes = streamPath ? new Uint8Array(readFileSync(streamPath)) : Uint8Array.from([0x01, 0x08, 0x02, 0x64, 0x80]);

const ptr = m._malloc(bytes.length);
m.HEAPU8.set(bytes, ptr);
const out = m.ccall('av2_parse_to_json', 'number', ['number', 'number'], [ptr, bytes.length]);
m._free(ptr);
if (!out) {
  console.error('parse failed:', m.ccall('av2_get_last_error', 'string', [], []));
  process.exit(1);
}
let end = out;
while (m.HEAPU8[end] !== 0) end++;
const json = JSON.parse(new TextDecoder().decode(m.HEAPU8.subarray(out, end)));
m.ccall('av2_free_json', null, ['number'], [out]);

const failures = [];
if (json.schema_version !== 1) failures.push(`schema_version is ${json.schema_version}, expected 1`);
const td = json.obus.find((o) => o.type_name === 'TEMPORAL_DELIMITER');
if (!td) failures.push('no Temporal Delimiter in output');
else if (td.header.xlayer_id !== 31) failures.push(`TD xlayer_id is ${td.header.xlayer_id}, expected 31 (global)`);
console.log(`obus: ${json.obu_count}, schema_version: ${json.schema_version}`);
if (failures.length) {
  console.error(failures.join('\n'));
  process.exit(1);
}
console.log('OK');
