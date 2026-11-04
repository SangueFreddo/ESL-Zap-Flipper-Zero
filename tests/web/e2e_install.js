// ESL Zap installer end-to-end test with a simulated Flipper.
// Usage: node tests/web/e2e_install.js site dist   (after tools/make_site.py dist web site)
const { chromium } = require('playwright');
const fs = require('fs');
const cases = [
  {fork:'Momentum', version:'mntm-dev', branch:'dev', major:87, minor:1, expect:'momentum-dev'},
  {fork:'Momentum', version:'mntm-012', branch:'release', major:87, minor:1, expect:'momentum-release'},
  {fork:'Official', version:'1.5.1', branch:'release', major:88, minor:2, expect:'official-release'},
  {fork:'Unleashed', version:'unlshd-094', branch:'release', major:88, minor:19, expect:'unleashed-release'},
  {fork:'RogueMaster', version:'RM1030', branch:'420', major:88, minor:23, expect:'roguemaster'},
];
(async () => {
  const b = await chromium.launch();
  const fake = fs.readFileSync(require('path').join(__dirname, 'fake_flipper.js'), 'utf8');
  for (const fw of cases) {
    const p = await b.newPage();
    const errs = []; p.on('pageerror', e => errs.push(e.message));
    await p.addInitScript('window.__FW=' + JSON.stringify(fw) + ';' + fake);
    await p.goto('file://' + require('path').resolve(process.argv[2] || 'site') + '/index.html');   // opened from disk, like the user
    await p.waitForTimeout(300);
    await p.click('#install');
    await p.waitForFunction(() => /ok|err/.test(document.querySelector('#status').className), null, {timeout: 60000});
    const status = await p.textContent('#status'), dev = await p.textContent('#device');
    const written = await p.evaluate(() => window.__fs['/ext/apps/Infrared/esl_zap.fap'] || []);
    const want = fs.readFileSync(require('path').resolve(process.argv[3] || 'dist') + '/esl_zap-' + fw.expect + '.fap');
    const same = written.length === want.length && want.every((v, i) => v === written[i]);
    const log = await p.evaluate(() => window.__log.filter(l => !l.startsWith('storage write_chunk')));
    console.log((fw.fork + ' ' + fw.version + ' API ' + fw.major + '.' + fw.minor).padEnd(30), '|', dev.replace(/\s+/g,' ').trim());
    console.log('   status:', status.trim(), '| file identical to', fw.expect + ':', same, '(' + written.length + ' bytes)', errs.length ? 'ERRORS ' + errs : '');
    if (fw === cases[0]) console.log('   commands:', log.join(' ; '));
    await p.close();
  }
  await b.close();
})();
