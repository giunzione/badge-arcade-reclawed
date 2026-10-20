#!/usr/bin/env python3
"""Stands in for GitHub releases, to test the installer's online patch check.

Lets the installer's online check run in an emulator (Azahar) without
publishing anything.

It answers the two URLs the app asks for, like GitHub does:
    /releases/latest/download/patches.txt  -> 302 to the "current" release
    /releases/download/<tag>/<file>        -> the file
serving the real patches from build/patches/ (run tools/make_manifest.py
first), changed according to the scenario.

Build the app for it, then run one scenario at a time:
    make -C installer clean
    make -C installer FAKE_GAME=1 ONLINE_BASE=http://127.0.0.1:8080
    python3 installer/tests/fake_release.py online

Scenarios (what the app should show, "+ built-in": then it uses its own patch):
    online         a release v0.4.1 with the same patch    -> "Patch from v0.4.1 (online)"
    newer          v0.4.2 with a different patch           -> online; SD patch "will be replaced"
    no-entry       this game version is not listed         -> "Not in the latest release"
                                                              + built-in
    missing-file   listed, but the file download is a 404  -> "Online patch download failed"
                                                              + built-in
    bad-hash       the file fails its SHA-256              -> "failed its security check"
                                                              + built-in
    bad-manifest   patches.txt is an HTML page             -> "bad patch list" + built-in
    future-format  patches.txt in format 2                 -> "bad patch list" + built-in
    too-big        patches.txt larger than the app accepts -> "file too big" + built-in
    not-found      everything is a 404 (GitHub before a release)
                                                           -> "not found on GitHub" + built-in
    slow           like online, 6 s per request            -> "Checking online" stays visible
    https          like online over HTTPS with a self-signed certificate (port 8443;
                   build with ONLINE_BASE=https://127.0.0.1:8443) -> "check date and time"
No server running at all -> "no internet".

Python 3 standard library only (plus the openssl command for "https").
"""

import argparse
import contextlib
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
    """Reads the real patches from build/patches/.

    Returns:
        A tuple (entries, files): the manifest lines split into their 6 fields,
        and file name -> bytes for every patch they list.

    Exits the program with a message if the manifest hasn't been built.
    """
    path = os.path.join(PATCHES, 'patches.txt')
    if not os.path.isfile(path):
        sys.exit(f'{path} not found: run tools/build_ips.py and tools/make_manifest.py first')
    entries = []
    with open(path, encoding='ascii') as f:
        for line in f:
            fields = line.split()
            if len(fields) == 6 and not line.startswith('#'):
                entries.append(fields)
    files = {}
    for e in entries:
        with open(os.path.join(PATCHES, e[3]), 'rb') as f:
            files[e[3]] = f.read()
    return entries, files


def changed(ips):
    """Returns a different but still well-formed IPS.

    Args:
        ips: An IPS file.

    Returns:
        `ips` with its last data byte (the one before "EOF") inverted.
    """
    b = bytearray(ips)
    b[-4] ^= 0xFF
    return bytes(b)


def build_release(scenario):
    """Builds the fake release a scenario serves.

    Args:
        scenario: One of SCENARIOS.

    Returns:
        A tuple (tag, files, manifest): the release tag, file name -> bytes
        (None: answer 404), and the patches.txt text (None: answer 404).
    """
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
    """Makes the HTTP request handler for a fake release.

    Args:
        scenario: One of SCENARIOS ("slow" delays every answer).
        tag: The release tag.
        served: File name -> bytes, or None for a 404, from build_release().
        manifest: The patches.txt text, or None for a 404.

    Returns:
        A BaseHTTPRequestHandler subclass answering like GitHub releases.
    """

    class Handler(http.server.BaseHTTPRequestHandler):
        """Answers the two URLs the app asks for, like GitHub does."""

        def send(self, code, body=b'', headers=()):
            """Sends a whole response: status, headers, Content-Length and body."""
            self.send_response(code)
            for k, v in headers:
                self.send_header(k, v)
            self.send_header('Content-Length', str(len(body)))
            self.end_headers()
            self.wfile.write(body)

        def do_GET(self):  # noqa: N802 - name required by BaseHTTPRequestHandler
            """Serves patches.txt (via the "latest" redirect) and the patch files."""
            if scenario == 'slow':
                time.sleep(6)
            if self.path == '/releases/latest/download/patches.txt' and manifest is not None:
                # GitHub redirects "latest" to the release's own URL.
                location = f'/releases/download/{tag}/patches.txt'
                return self.send(302, headers=[('Location', location)])
            if self.path == f'/releases/download/{tag}/patches.txt' and manifest is not None:
                return self.send(200, manifest.encode(), [('Content-Type', 'text/plain')])
            prefix = f'/releases/download/{tag}/'
            if self.path.startswith(prefix) and served.get(self.path[len(prefix):]) is not None:
                return self.send(200, served[self.path[len(prefix):]],
                                 [('Content-Type', 'application/octet-stream')])
            return self.send(404, b'Not Found')

        def log_message(self, fmt, *args):
            """Prints one line per request: method, path and status."""
            print(f'{self.command} {self.path} -> {args[1] if len(args) > 1 else "?"}', flush=True)
    return Handler


def self_signed_context():
    """Makes a TLS context with a throw-away self-signed certificate.

    The app must reject it: that's the "https" scenario.

    Returns:
        A server-side ssl.SSLContext.

    Exits the program with a message if openssl fails.
    """
    tmp = tempfile.mkdtemp(prefix='fake-release-')
    key, crt, cnf = (os.path.join(tmp, n) for n in ('key.pem', 'cert.pem', 'openssl.cnf'))
    # An explicit minimal config: some openssl builds (e.g. conda's) can't find their own.
    with open(cnf, 'w') as f:
        f.write('[req]\ndistinguished_name = dn\nprompt = no\n[dn]\nCN = 127.0.0.1\n')
    r = subprocess.run(['openssl', 'req', '-x509', '-newkey', 'rsa:2048', '-nodes', '-days', '2',
                        '-config', cnf, '-keyout', key, '-out', crt],
                       capture_output=True, text=True, check=False)
    if r.returncode != 0:
        sys.exit(f'openssl failed:\n{r.stderr}')
    ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    ctx.load_cert_chain(crt, key)
    return ctx


def main():
    """Parses the command line and serves the scenario until Ctrl+C."""
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('scenario', choices=SCENARIOS)
    ap.add_argument('--host', default='127.0.0.1')
    a = ap.parse_args()

    tag, served, manifest = build_release(a.scenario)
    port = 8443 if a.scenario == 'https' else 8080
    handler = make_handler(a.scenario, tag, served, manifest)
    server = http.server.ThreadingHTTPServer((a.host, port), handler)
    if a.scenario == 'https':
        server.socket = self_signed_context().wrap_socket(server.socket, server_side=True)
    scheme = 'https' if a.scenario == 'https' else 'http'
    print(f'scenario "{a.scenario}": serving release {tag} at {scheme}://{a.host}:{port} '
          '(Ctrl+C to stop)', flush=True)
    with contextlib.suppress(KeyboardInterrupt):
        server.serve_forever()


if __name__ == '__main__':
    main()
