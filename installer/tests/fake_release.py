#!/usr/bin/env python3
"""
fake_release.py - a stand-in for GitHub releases, to test the installer's
online patch check in an emulator (Azahar) without publishing anything.

It answers the two URLs the app asks for, like GitHub does:
    /releases/latest/download/patches.txt  -> 302 to the "current" release
    /releases/download/<tag>/<file>        -> the file
serving the real patches from build/patches/ (run tools/make_manifest.py
first), changed according to the scenario.

Build the app for it, then run one scenario at a time:
    make -C installer clean
    make -C installer FAKE_GAME=1 ONLINE_BASE=http://127.0.0.1:8080
    python3 installer/tests/fake_release.py online

Scenarios:
    online         a release v0.4.1 with the same patch       -> "Patch from v0.4.1 (online)"
    newer          v0.4.2 with a different patch              -> online; SD patch "will be replaced"
    no-entry       the list doesn't have this game version    -> "Not in the latest release", built-in
    missing-file   listed, but the file download is a 404     -> "Online patch download failed", built-in
    bad-hash       the file doesn't match its SHA-256         -> "failed its security check", built-in
    bad-manifest   patches.txt is an HTML page                -> "bad patch list", built-in
    future-format  patches.txt in format 2                    -> "bad patch list", built-in
    too-big        patches.txt larger than the app accepts    -> "file too big", built-in
    not-found      everything is a 404 (GitHub before a release) -> "not found on GitHub", built-in
    slow           like online, 6 s per request                -> "Checking online" stays visible
    https          like online over HTTPS with a self-signed certificate (port 8443;
                   build with ONLINE_BASE=https://127.0.0.1:8443) -> "check date and time"
No server running at all -> "no internet".

Python 3 standard library only (plus the openssl command for "https").
"""
import argparse
import hashlib
import http.server
import os
import ssl
import subprocess
import sys
import tempfile
import time

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
PATCHES = os.path.join(ROOT, 'build', 'patches')
SCENARIOS = ['online', 'newer', 'no-entry', 'missing-file', 'bad-hash', 'bad-manifest',
             'future-format', 'too-big', 'not-found', 'slow', 'https']


def load_builtin():
    """The real manifest lines and files from build/patches/."""
    path = os.path.join(PATCHES, 'patches.txt')
    if not os.path.isfile(path):
        sys.exit(f'{path} not found: run tools/build_ips.py and tools/make_manifest.py first')
    entries = []
    for line in open(path, encoding='ascii'):
        f = line.split()
        if len(f) == 6 and not line.startswith('#'):
            entries.append(f)
    files = {e[3]: open(os.path.join(PATCHES, e[3]), 'rb').read() for e in entries}
    return entries, files


def changed(ips):
    """A different but still well-formed IPS: flip the last data byte before "EOF"."""
    b = bytearray(ips)
    b[-4] ^= 0xFF
    return bytes(b)


def build_release(scenario):
    """Return (tag, {file name: bytes or None for 404}, manifest text or None for 404)."""
    entries, files = load_builtin()
    tag = 'v0.4.2' if scenario == 'newer' else 'v0.4.1'
    served = dict(files)
    lines = []
    for tid, code, region, name, size, sha in entries:
        data = files[name]
        if scenario == 'newer':
            data = changed(data)
            served[name] = data
            size, sha = str(len(data)), hashlib.sha256(data).hexdigest()
        lines.append(' '.join([tid, code, region, name, size, sha]))
    if scenario == 'missing-file':
        served = {n: None for n in served}
    if scenario == 'bad-hash':
        served = {n: changed(d) for n, d in served.items()}

    header = f'reclawed-patches 1 {tag}'
    if scenario == 'no-entry':
        lines = []
    manifest = '\n'.join(['# fake release for testing', header] + lines) + '\n'
    if scenario == 'bad-manifest':
        manifest = '<!DOCTYPE html><html><body>Sign in to GitHub</body></html>\n'
    if scenario == 'future-format':
        manifest = manifest.replace('reclawed-patches 1', 'reclawed-patches 2')
    if scenario == 'too-big':
        manifest += '# ' + 'x' * (100 * 1024) + '\n'
    if scenario == 'not-found':
        manifest, served = None, {}
    return tag, served, manifest


def make_handler(scenario, tag, served, manifest):
    class Handler(http.server.BaseHTTPRequestHandler):
        def send(self, code, body=b'', headers=()):
            self.send_response(code)
            for k, v in headers:
                self.send_header(k, v)
            self.send_header('Content-Length', str(len(body)))
            self.end_headers()
            self.wfile.write(body)

        def do_GET(self):
            if scenario == 'slow':
                time.sleep(6)
            if self.path == '/releases/latest/download/patches.txt' and manifest is not None:
                # GitHub redirects "latest" to the release's own URL.
                return self.send(302, headers=[('Location', f'/releases/download/{tag}/patches.txt')])
            if self.path == f'/releases/download/{tag}/patches.txt' and manifest is not None:
                return self.send(200, manifest.encode(), [('Content-Type', 'text/plain')])
            prefix = f'/releases/download/{tag}/'
            if self.path.startswith(prefix) and served.get(self.path[len(prefix):]) is not None:
                return self.send(200, served[self.path[len(prefix):]],
                                 [('Content-Type', 'application/octet-stream')])
            return self.send(404, b'Not Found')

        def log_message(self, fmt, *args):
            print(f'{self.command} {self.path} -> {args[1] if len(args) > 1 else "?"}', flush=True)
    return Handler


def self_signed_context():
    """TLS with a throw-away self-signed certificate (the app must reject it)."""
    tmp = tempfile.mkdtemp(prefix='fake-release-')
    key, crt, cnf = (os.path.join(tmp, n) for n in ('key.pem', 'cert.pem', 'openssl.cnf'))
    # An explicit minimal config: some openssl builds (e.g. conda's) can't find their own.
    with open(cnf, 'w') as f:
        f.write('[req]\ndistinguished_name = dn\nprompt = no\n[dn]\nCN = 127.0.0.1\n')
    r = subprocess.run(['openssl', 'req', '-x509', '-newkey', 'rsa:2048', '-nodes', '-days', '2',
                        '-config', cnf, '-keyout', key, '-out', crt], capture_output=True, text=True)
    if r.returncode != 0:
        sys.exit(f'openssl failed:\n{r.stderr}')
    ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    ctx.load_cert_chain(crt, key)
    return ctx


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('scenario', choices=SCENARIOS)
    ap.add_argument('--host', default='127.0.0.1')
    a = ap.parse_args()

    tag, served, manifest = build_release(a.scenario)
    port = 8443 if a.scenario == 'https' else 8080
    server = http.server.ThreadingHTTPServer((a.host, port), make_handler(a.scenario, tag, served, manifest))
    if a.scenario == 'https':
        server.socket = self_signed_context().wrap_socket(server.socket, server_side=True)
    scheme = 'https' if a.scenario == 'https' else 'http'
    print(f'scenario "{a.scenario}": serving release {tag} at {scheme}://{a.host}:{port} (Ctrl+C to stop)',
          flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass


if __name__ == '__main__':
    main()
