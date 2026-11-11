#!/usr/bin/env python3
"""Check DynamoDB request serialization using the real Tcl extension."""
import argparse
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
import os
from pathlib import Path
import subprocess
import threading


class Handler(BaseHTTPRequestHandler):
    def log_message(self, *_args):
        pass

    def do_POST(self):
        operation = self.headers['X-Amz-Target'].split('.')[-1]
        body = json.loads(self.rfile.read(int(self.headers['Content-Length'])))
        self.server.calls += 1
        status, response = 200, {}
        try:
            assert body['TableName'] == 'Sessions', body
            if operation == 'Query':
                assert body['ConsistentRead'] is True, body
                assert 'IndexName' not in body and 'ProjectionExpression' not in body, body
                assert body['KeyConditionExpression'] == 'tenant = :tenant', body
                if body['ExpressionAttributeValues'][':tenant']['S'] == 'empty':
                    assert 'Limit' not in body, body
                elif 'ExclusiveStartKey' not in body:
                    assert body['Limit'] == 3, body
                    response = {'Items': [{'id': {'S': str(i)}, 'tenant': {'S': 'tenant'}} for i in range(2)],
                                'LastEvaluatedKey': {'id': {'S': '1'}, 'tenant': {'S': 'tenant'}}}
                else:
                    assert body['Limit'] == 1, body
                    assert body['ExclusiveStartKey']['id']['S'] == '1', body
                    response = {'Items': [{'id': {'S': '2'}}]}
            else:
                item = body['Item'] if operation == 'PutItem' else body['Key']
                identifier = item['id']['S']
                if identifier == 'legacy':
                    assert 'ConditionExpression' not in body and 'ConsistentRead' not in body, body
                elif identifier in ('duplicate', 'missing'):
                    assert body['ExpressionAttributeNames'] == {'#id': 'id'}, body
                    assert body['ConditionExpression'] == ('attribute_not_exists(#id)' if identifier == 'duplicate' else 'attribute_exists(#id)'), body
                    status, response = 400, {'__type': 'com.amazonaws.dynamodb.v20120810#ConditionalCheckFailedException', 'message': 'condition failed'}
                elif identifier == 'cas':
                    assert body['ConditionExpression'] == '#r = :r AND #e > :now AND #s = :s AND #b = :b', body
                    assert body['ExpressionAttributeNames'] == {'#r': 'revision', '#e': 'expires', '#s': 'subject', '#b': 'enabled'}, body
                    assert body['ExpressionAttributeValues'] == {':r': {'N': '2'}, ':now': {'N': '10'}, ':s': {'S': 'alice'}, ':b': {'BOOL': False}}, body
                elif identifier == 'consistent':
                    assert body['ConsistentRead'] is True, body
                    response = {'Item': item}
                else:
                    raise AssertionError(body)
        except Exception as error:
            self.server.failures.append(repr(error))
            status, response = 400, {'__type': 'ValidationException', 'message': str(error)}
        encoded = json.dumps(response).encode()
        self.send_response(status)
        self.send_header('Content-Type', 'application/x-amz-json-1.0')
        self.send_header('Content-Length', str(len(encoded)))
        self.end_headers()
        self.wfile.write(encoded)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--library', required=True)
    parser.add_argument('--tclsh', default='tclsh')
    args = parser.parse_args()
    server = ThreadingHTTPServer(('127.0.0.1', 0), Handler)
    server.failures, server.calls = [], 0
    worker = threading.Thread(target=server.serve_forever, daemon=True)
    worker.start()
    try:
        result = subprocess.run([args.tclsh, str(Path(__file__).with_name('requests.test')), args.library,
                                 f'http://127.0.0.1:{server.server_port}'],
                                env=dict(os.environ, AWS_EC2_METADATA_DISABLED='true'), timeout=60)
        assert result.returncode == 0, 'Tcl tests failed'
        assert server.calls == 20, server.calls
        assert not server.failures, server.failures
    finally:
        server.shutdown()
        server.server_close()
        worker.join()


if __name__ == '__main__':
    main()
