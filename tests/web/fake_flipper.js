// Fake Flipper Zero CLI behind navigator.serial, for end-to-end tests.
window.__fs = {}; window.__log = [];
const FW = window.__FW;
class FakePort {
  constructor() {
    this.inbuf = []; this.mode = 'cmd'; this.line = ''; this.need = 0; this.path = '';
    const self = this;
    this.readable = new ReadableStream({ start(c) { self.ctrl = c; } });
    this.writable = new WritableStream({ write(chunk) { self.feed(chunk); } });
    this.listeners = {};
  }
  addEventListener(n, f) { this.listeners[n] = f; }
  async open() {}
  async close() {}
  out(s) { this.ctrl.enqueue(new TextEncoder().encode(s)); }
  feed(bytes) {
    for (const b of bytes) {
      if (this.mode === 'data') {
        window.__fs[this.path].push(b);
        if (--this.need === 0) { this.mode = 'cmd'; this.out('\r\n>: '); }
        continue;
      }
      const ch = String.fromCharCode(b);
      if (ch === '\r') { const l = this.line.trim(); this.line = ''; this.out('\r\n'); this.exec(l); }
      else { this.line += ch; this.out(ch); }
    }
  }
  exec(l) {
    window.__log.push(l);
    const a = l.split(' ');
    if (l === '') return this.out('>: ');
    if (l === 'device_info') {
      const kv = {hardware_name: 'Fakey', firmware_commit: 'd3f89dfe', firmware_branch: FW.branch,
        firmware_version: FW.version, firmware_api_major: FW.major, firmware_api_minor: FW.minor,
        firmware_origin_fork: FW.fork, firmware_origin_git: 'https://github.com/x'};
      return this.out(Object.entries(kv).map(([k, v]) => k.padEnd(30) + ': ' + v + '\r\n').join('') + '>: ');
    }
    if (a[0] === 'storage' && a[1] === 'remove') { delete window.__fs[a[2]]; return this.out('>: '); }
    if (a[0] === 'storage' && a[1] === 'mkdir') return this.out('>: ');
    if (a[0] === 'storage' && a[1] === 'write_chunk') {
      this.path = a[2]; this.need = +a[3]; window.__fs[this.path] = window.__fs[this.path] || [];
      this.mode = 'data'; return this.out('Ready\r\n');
    }
    if (a[0] === 'loader') return this.out('>: ');
    this.out('command not found\r\n>: ');
  }
}
Object.defineProperty(navigator, 'serial', { value: { requestPort: async () => new FakePort() } });
