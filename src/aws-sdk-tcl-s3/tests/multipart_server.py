#!/usr/bin/env python3
"""Exercise the real Tcl extension against a local, deterministic S3 HTTP server."""

import argparse
import hashlib
import os
from pathlib import Path
import subprocess
import tempfile
import threading
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import parse_qs, unquote, urlsplit
from xml.etree import ElementTree as ET
from xml.sax.saxutils import escape


class S3Handler(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def log_message(self, *_args):
        pass

    def reply(self, status, body=b"", **headers):
        if isinstance(body, str):
            body = body.encode()
        self.send_response(status)
        self.send_header("Content-Length", str(len(body)))
        for key, value in headers.items():
            self.send_header(key, value)
        self.end_headers()
        self.wfile.write(body)

    def error(self, message):
        self.reply(403, f"<Error><Code>AccessDenied</Code><Message>{message}</Message></Error>")

    def request_data(self):
        url = urlsplit(self.path)
        key = unquote(url.path).split("/", 2)[-1]
        query = parse_qs(url.query, keep_blank_values=True)
        body = self.rfile.read(int(self.headers.get("Content-Length", 0)))
        self.server.requests.append((self.command, key, query))
        return key, query, body

    def upload(self, key, query):
        upload = self.server.uploads.get(query.get("uploadId", [None])[0])
        if upload is None or upload["key"] != key:
            self.error("invalid upload ID or key")
            return None
        return upload

    def do_POST(self):
        key, query, body = self.request_data()
        if "uploads" in query:
            if "create-error" in key:
                self.error("injected create failure")
                return
            upload_id = f"upload+/{len(self.server.uploads)}=?&id"
            self.server.uploads[upload_id] = {
                "key": key, "parts": {}, "aborted": False,
                "content_type": self.headers.get("Content-Type"),
            }
            if "read-error" in key:
                (self.server.directory / "read-error.bin").write_bytes(b"")
            self.reply(200, "<InitiateMultipartUploadResult>"
                       f"<UploadId>{escape(upload_id)}</UploadId>"
                       "</InitiateMultipartUploadResult>")
            return
        upload = self.upload(key, query)
        if upload is None:
            return
        if "complete-error" in key:
            # S3 can report a completion error inside an HTTP 200 response.
            self.reply(200, "<Error><Code>AccessDenied</Code>"
                       "<Message>injected completion failure</Message></Error>")
            return
        root = ET.fromstring(body)
        pairs = [(int(part.findtext("{*}PartNumber")), part.findtext("{*}ETag"))
                 for part in root.findall("{*}Part")]
        numbers = [number for number, _ in pairs]
        if not pairs or numbers != sorted(set(numbers)):
            self.error("parts must be in ascending order")
            return
        data = bytearray()
        for index, (number, etag) in enumerate(pairs):
            expected = upload["parts"].get(number)
            if expected is None or expected[0] != etag:
                self.error("incorrect ETag")
                return
            if index < len(pairs) - 1 and len(expected[1]) < 5 * 1024 * 1024:
                self.error("part is too small")
                return
            data.extend(expected[1])
        self.server.objects[key] = bytes(data)
        self.reply(200, '<CompleteMultipartUploadResult><ETag>"complete-etag"</ETag>'
                   '</CompleteMultipartUploadResult>')

    def do_PUT(self):
        key, query, body = self.request_data()
        upload = self.upload(key, query)
        if upload is None:
            return
        if "part-error" in key or "abort-error" in key:
            self.error("injected part failure")
            return
        number = int(query["partNumber"][0])
        # Include an XML-special character to check ETag round trips.
        etag = f'"{number}&{hashlib.md5(body).hexdigest()}"'
        upload["parts"][number] = (etag, body)
        self.reply(200, ETag=etag)

    def do_DELETE(self):
        key, query, _ = self.request_data()
        upload = self.upload(key, query)
        if upload is None:
            return
        if "abort-error" in key:
            self.error("injected abort failure")
            return
        upload["aborted"] = True
        self.reply(204)

    def do_GET(self):
        key, _, _ = self.request_data()
        if key not in self.server.objects:
            self.error("object not found")
            return
        self.reply(200, self.server.objects[key])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--library", required=True, type=Path)
    parser.add_argument("--tclsh", default="tclsh")
    args = parser.parse_args()
    env = dict(os.environ, AWS_EC2_METADATA_DISABLED="true")
    with ThreadingHTTPServer(("127.0.0.1", 0), S3Handler) as server:
        server.requests, server.uploads, server.objects = [], {}, {}
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        try:
            with tempfile.TemporaryDirectory(prefix="s3-multipart-test-") as directory:
                server.directory = Path(directory)
                result = subprocess.run([
                    args.tclsh, str(Path(__file__).with_name("multipart.test")),
                    str(args.library.resolve()), f"http://127.0.0.1:{server.server_port}", directory,
                ], env=env, timeout=120)
            if result.returncode:
                return result.returncode
            for api in ("object", "namespace"):
                for suffix in ("part-error", "complete-error", "manual-abort", "read-error"):
                    upload = next(u for u in server.uploads.values() if u["key"] == f"{api}-{suffix}")
                    assert upload["aborted"], f"upload was not aborted: {api}-{suffix}"
                manual = next(u for u in server.uploads.values() if u["key"] == f"{api}-manual")
                assert manual["content_type"] == "application/octet-stream"
                auto = next(u for u in server.uploads.values() if u["key"] == f"{api}-auto")
                assert [len(part[1]) for part in auto["parts"].values()] == [5242880, 5242880, 7]
                default = next(u for u in server.uploads.values() if u["key"] == f"{api}-default")
                assert [len(part[1]) for part in default["parts"].values()] == [8388608, 2097159]
            assert not any(key == "invalid" for _, key, _ in server.requests), "invalid arguments sent a request"
            print("HTTP assertions passed: byte lengths, ETags, ordering, content type, cleanup, and validation")
            return 0
        finally:
            server.shutdown()
            thread.join()


if __name__ == "__main__":
    raise SystemExit(main())
