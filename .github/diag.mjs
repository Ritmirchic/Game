import { readFileSync } from 'node:fs';

const line = (s) => String(s).replace(/\u001b\[[0-9;]*m/g, '').replace(/[\r\n]+/g, ' ').slice(0, 900);

try {
  const out = readFileSync('/tmp/vitest-out.txt', 'utf8').split('\n');
  const interesting = out.filter((l) => /(FAIL|×|Error|error|failed|expected|Exit|ELIFECYCLE|Segmentation)/i.test(l));
  for (const l of interesting.slice(-12)) console.log(`::error title=vitest::${line(l)}`);
  for (const l of out.slice(-6)) console.log(`::error title=tail::${line(l)}`);
} catch (err) {
  console.log(`::error title=diag::${line(err)}`);
}

try {
  const report = JSON.parse(readFileSync('/tmp/vitest-report.json', 'utf8'));
  const files = report.testResults ?? [];
  const fails = files.flatMap((f) =>
    (f.assertionResults ?? [])
      .filter((a) => a.status === 'failed')
      .map((a) => `${f.name.split('/').pop()} :: ${a.fullName} :: ${(a.failureMessages ?? []).join(' ')}`),
  );
  if (!fails.length) console.log('::error title=fails::none reported in json report');
  for (const f of fails.slice(0, 8)) console.log(`::error title=fail::${line(f)}`);
} catch (err) {
  console.log(`::error title=report::${line(err)}`);
}
