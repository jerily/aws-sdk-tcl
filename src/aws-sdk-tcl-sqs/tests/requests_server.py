#!/usr/bin/env python3
"""Exercise real SQS SDK JSON requests without AWS credentials or resources."""
import argparse
from collections import Counter
import hashlib
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
import os
from pathlib import Path
import subprocess
import threading


class Handler(BaseHTTPRequestHandler):
    def log_message(self, *_):
        pass

    def reply(self, code, body):
        data = json.dumps(body, ensure_ascii=False).encode()
        self.send_response(code)
        self.send_header('Content-Type', 'application/x-amz-json-1.0')
        self.send_header('Content-Length', str(len(data)))
        self.end_headers()
        self.wfile.write(data)

    def do_POST(self):
        try:
            request = json.loads(self.rfile.read(int(self.headers['Content-Length'])))
            action = self.headers['X-Amz-Target'].split('.')[-1]
            self.server.calls[action] += 1
            assert '/sqs/aws4_request' in self.headers['Authorization']
            if request.get('QueueName') == 'missing':
                self.reply(400, {'__type': 'QueueDoesNotExist', 'message': 'missing queue'})
                return
            if request.get('MessageBody') == 'reject' or request.get('ReceiptHandle') == 'invalid':
                self.reply(400, {'__type': 'InvalidParameterValue', 'message': 'injected rejection'})
                return
            queue = f'http://127.0.0.1:{self.server.server_port}/123456789012/user-emails'
            if action == 'GetQueueUrl':
                assert request == {'QueueName': 'user-emails'}
                result = {'QueueUrl': queue}
            else:
                assert request['QueueUrl'] == queue
                body = 'Hello café 😀\nline two'
                md5 = hashlib.md5(body.encode()).hexdigest()
                if action == 'SendMessage':
                    assert request['MessageBody'] == body
                    result = {'MessageId': 'message-id', 'MD5OfMessageBody': md5}
                elif action == 'ReceiveMessage':
                    assert request['MaxNumberOfMessages'] == 10
                    result = {'Messages': [{'MessageId': 'message-id', 'Body': body,
                                           'ReceiptHandle': 'receipt', 'MD5OfBody': md5}]}
                elif action == 'DeleteMessage':
                    assert request['ReceiptHandle'] == 'receipt'
                    result = {}
                else:
                    raise AssertionError(action)
            self.reply(200, result)
        except Exception as error:
            self.server.failures.append(repr(error))
            self.reply(400, {'__type': 'InvalidParameterValue', 'message': 'fixture assertion failed'})


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--library', required=True, type=Path)
    parser.add_argument('--tclsh', required=True)
    args = parser.parse_args()
    with ThreadingHTTPServer(('127.0.0.1', 0), Handler) as server:
        server.calls, server.failures = Counter(), []
        worker = threading.Thread(target=server.serve_forever, daemon=True)
        worker.start()
        try:
            result = subprocess.run([args.tclsh, str(Path(__file__).with_name('requests.test')),
                                     str(args.library.resolve()), f'http://127.0.0.1:{server.server_port}'],
                                    env=dict(os.environ, AWS_EC2_METADATA_DISABLED='true'), timeout=75)
            assert result.returncode == 0
            assert not server.failures, server.failures
            assert server.calls == Counter(GetQueueUrl=4, SendMessage=4, ReceiveMessage=2, DeleteMessage=4), server.calls
            print('SQS HTTP assertions passed')
        finally:
            server.shutdown()
            worker.join()


if __name__ == '__main__':
    main()
