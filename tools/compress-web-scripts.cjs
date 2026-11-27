// Pack the checked-in reference scripts for the device. The host uses the same original bytes.
const { readFileSync, writeFileSync } = require('node:fs');
const path = require('node:path');
const { deflateRawSync, inflateRawSync } = require('node:zlib');
const assert = require('node:assert/strict');
const dir = path.resolve(__dirname, '../firmware/components/learning_web/generated');
const reference = readFileSync(path.join(dir, 'web_bundle.cpp'), 'utf8');
const scripts = [...reference.matchAll(/static const unsigned char script(\d+)\[\]=\{([^}]+)\};/g)];
const rows = [...reference.matchAll(/\{"([^"]+)",reinterpret_cast<const char\*>\(script(\d+)\),sizeof\(script\d+\)-1\}/g)];
assert(scripts.length && rows.length === scripts.length);
let out = '#include <cstddef>\nnamespace wordcard {\nstruct WebScript {const char *name;const char *data;size_t length;size_t packed;};\n';
const table = [];
for (let i = 0; i < scripts.length; i++) {
    assert.equal(Number(scripts[i][1]), i);
    assert.equal(Number(rows[i][2]), i);
    const raw = Buffer.from(scripts[i][2].trim().split(',').map(Number));
    assert.equal(raw.at(-1), 0);
    const src = raw.subarray(0, -1);
    const packed = deflateRawSync(src, { level: 9 });
    assert.deepEqual(inflateRawSync(packed), src);
    const lines = [];
    for (let j = 0; j < packed.length; j += 32) lines.push(packed.subarray(j, j + 32).join(','));
    out += 'static const unsigned char script'+i+'[]={\n'+lines.join(',\n')+'};\n';
    table.push('{'+JSON.stringify(rows[i][1])+',reinterpret_cast<const char*>(script'+i+'),'+src.length+',sizeof(script'+i+')}');
}
out += 'extern const WebScript web_scripts[]={\n'+table.join(',\n')+'};\nextern const size_t web_script_count=sizeof(web_scripts)/sizeof(web_scripts[0]);\n}\n';
writeFileSync(path.join(dir, 'web_bundle_deflate.cpp'), out);

