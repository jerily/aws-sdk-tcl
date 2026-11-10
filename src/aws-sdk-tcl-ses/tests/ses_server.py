#!/usr/bin/env python3
"""Test the real SDK's SES Query requests using a local HTTP fixture."""
import argparse
import base64
import os
from pathlib import Path
import subprocess
import threading
from collections import Counter
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import parse_qs


class Handler(BaseHTTPRequestHandler):
    def log_message(self, *_):
        pass

    def reply(self, code, body):
        body = body.encode('utf-8')
        self.send_response(code)
        self.send_header('Content-Type', 'text/xml; charset=utf-8')
        self.send_header('Content-Length', str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_POST(self):
        fields = parse_qs(self.rfile.read(int(self.headers['Content-Length'])).decode(), keep_blank_values=True)
        q = {key: values[0] for key, values in fields.items()}
        self.server.requests.append(q)
        try:
            assert q['Version'] == '2010-12-01'
            assert '/ses/aws4_request' in self.headers['Authorization']
            action = q['Action']
            if q.get('Source') == 'rejected@example.com':
                self.reply(400, '<ErrorResponse><Error><Type>Sender</Type><Code>MessageRejected</Code>'
                           '<Message>injected rejection</Message></Error><RequestId>test</RequestId></ErrorResponse>')
                return
            body = self.response(action, q)
            self.reply(200, f'<{action}Response xmlns="http://ses.amazonaws.com/doc/2010-12-01/">'
                       f'<{action}Result>{body}</{action}Result>'
                       f'<ResponseMetadata><RequestId>test</RequestId></ResponseMetadata></{action}Response>')
        except Exception as error:
            self.server.failures.append(f'{q}: {error!r}')
            self.reply(400, '<ErrorResponse><Error><Code>InvalidParameterValue</Code>'
                       '<Message>fixture assertion failed</Message></Error></ErrorResponse>')

    def response(self, action, q):
        if action in ('SendEmail', 'SendTemplatedEmail'):
            if q['Source'] == 'minimal@example.com':
                assert action == 'SendEmail'
                assert q['Destination.CcAddresses.member.1'] == 'first@example.com'
                assert not any('ToAddresses' in key for key in q)
                assert q['Message.Subject.Data'] == 'Hello'
                bodies = [key for key in q if key.startswith('Message.Body.') and key.endswith('.Data')]
                assert len(bodies) == 1 and q[bodies[0]] == 'body'
                assert 'ReplyToAddresses.member.1' not in q
                return '<MessageId>minimal-123</MessageId>'
            assert q['Source'] == 'sender@example.com'
            assert q['Destination.ToAddresses.member.1'] == 'first@example.com'
            assert q['Destination.ToAddresses.member.2'] == 'second@example.com'
            assert q['Destination.CcAddresses.member.1'] == 'cc@example.com'
            assert q['Destination.BccAddresses.member.1'] == 'bcc@example.com'
            assert q['ReplyToAddresses.member.1'] == 'reply@example.com'
            assert q['ReturnPath'] == 'bounce@example.com'
            assert q['SourceArn'] == 'source-arn'
            assert q['ReturnPathArn'] == 'return-arn'
            assert q['ConfigurationSetName'] == 'events'
            assert q['Tags.member.1.Name'] == 'purpose'
            assert q['Tags.member.1.Value'] == 'test'
            if action == 'SendEmail':
                assert q['Message.Subject.Data'] == 'Hello café 😀 & +'
                assert q['Message.Body.Text.Data'] == 'plain café 😀\nsecond line'
                assert q['Message.Body.Html.Data'] == '<b>café 😀</b>'
                for part in ('Subject', 'Body.Text', 'Body.Html'):
                    assert q[f'Message.{part}.Charset'] == 'UTF-8'
            else:
                assert q['Template'] == 'welcome'
                assert q['TemplateData'] == '{"name":"café 😀"}'
            return '<MessageId>message-123</MessageId>'
        if action == 'SendRawEmail':
            assert base64.b64decode(q['RawMessage.Data']) == b'From: sender@example.com\r\n\r\n\x00\xff\x80\r\n'
            if 'Source' in q:
                assert q['Source'] == 'sender@example.com'
                assert q['Destinations.member.1'] == 'first@example.com'
                assert q['FromArn'] == 'from-arn'
                assert q['SourceArn'] == 'source-arn'
                assert q['ReturnPathArn'] == 'return-arn'
                assert q['ConfigurationSetName'] == 'events'
                assert q['Tags.member.1.Value'] == 'raw'
            return '<MessageId>raw-123</MessageId>'
        if action in ('CreateTemplate', 'UpdateTemplate'):
            assert q['Template.TemplateName'] == 'welcome'
            assert q['Template.SubjectPart'] == 'Hello {{name}}'
            assert q['Template.TextPart'] == 'plain {{name}}'
            assert q['Template.HtmlPart'] == '<b>{{name}}</b>'
            return ''
        if action == 'GetTemplate':
            assert q['TemplateName'] == 'welcome'
            return '<Template><TemplateName>welcome</TemplateName><SubjectPart>Hello café 😀</SubjectPart><TextPart>plain</TextPart><HtmlPart>&lt;b&gt;html&lt;/b&gt;</HtmlPart></Template>'
        if action == 'DeleteTemplate':
            assert q['TemplateName'] == 'welcome'
            return ''
        if action == 'TestRenderTemplate':
            assert q['TemplateName'] == 'welcome'
            assert q['TemplateData'] == '{"name":"café 😀"}'
            return '<RenderedTemplate>Hello café 😀</RenderedTemplate>'
        if action in ('ListIdentities', 'ListTemplates'):
            if 'NextToken' in q:
                assert q['NextToken'] == 'next+/?=&'
                assert q['MaxItems'] == '2'
                if action == 'ListIdentities':
                    assert q['IdentityType'] == 'Domain'
                    return '<Identities/>'
                return '<TemplatesMetadata/>'
            token = '<NextToken>next+/?=&amp;</NextToken>'
            if action == 'ListIdentities':
                return '<Identities><member>example.com</member><member>sender@example.com</member></Identities>' + token
            return '<TemplatesMetadata><member><Name>welcome</Name><CreatedTimestamp>2026-11-10T12:00:00Z</CreatedTimestamp></member></TemplatesMetadata>' + token
        if action == 'VerifyEmailIdentity':
            assert q['EmailAddress'] == 'sender@example.com'
            return ''
        if action in ('VerifyDomainIdentity', 'VerifyDomainDkim'):
            assert q['Domain'] == 'example.com'
            if action == 'VerifyDomainIdentity':
                return '<VerificationToken>verify-token</VerificationToken>'
            return '<DkimTokens><member>one</member><member>two</member><member>three</member></DkimTokens>'
        if action in ('GetIdentityVerificationAttributes', 'GetIdentityDkimAttributes'):
            assert q['Identities.member.1'] == 'example.com'
            assert q['Identities.member.2'] == 'sender@example.com'
            if action == 'GetIdentityVerificationAttributes':
                return '<VerificationAttributes><entry><key>example.com</key><value><VerificationStatus>Success</VerificationStatus><VerificationToken>verify-token</VerificationToken></value></entry></VerificationAttributes>'
            return '<DkimAttributes><entry><key>example.com</key><value><DkimEnabled>true</DkimEnabled><DkimVerificationStatus>Success</DkimVerificationStatus><DkimTokens><member>one</member></DkimTokens></value></entry></DkimAttributes>'
        if action == 'DeleteIdentity':
            assert q['Identity'] == 'example.com'
            return ''
        if action == 'GetSendQuota':
            return '<Max24HourSend>200.5</Max24HourSend><MaxSendRate>1.5</MaxSendRate><SentLast24Hours>3</SentLast24Hours>'
        if action == 'GetSendStatistics':
            return '<SendDataPoints><member><Timestamp>2026-11-10T12:00:00Z</Timestamp><DeliveryAttempts>4294967296</DeliveryAttempts><Bounces>2</Bounces><Complaints>1</Complaints><Rejects>4</Rejects></member></SendDataPoints>'
        if action == 'GetAccountSendingEnabled':
            return '<Enabled>true</Enabled>'
        raise AssertionError(f'unexpected action {action}')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--library', required=True, type=Path)
    parser.add_argument('--tclsh', required=True)
    args = parser.parse_args()
    env = dict(os.environ, AWS_EC2_METADATA_DISABLED='true')
    with ThreadingHTTPServer(('127.0.0.1', 0), Handler) as server:
        server.requests, server.failures = [], []
        worker = threading.Thread(target=server.serve_forever, daemon=True)
        worker.start()
        try:
            result = subprocess.run([args.tclsh, str(Path(__file__).with_name('ses.test')),
                                     str(args.library.resolve()), f'http://127.0.0.1:{server.server_port}'],
                                    env=env, timeout=100)
            if result.returncode:
                return result.returncode
            assert not server.failures, '\n'.join(server.failures)
            counts = Counter(q['Action'] for q in server.requests)
            expected = {name: 2 for name in (
                'SendTemplatedEmail', 'CreateTemplate', 'UpdateTemplate', 'GetTemplate',
                'DeleteTemplate', 'TestRenderTemplate', 'VerifyEmailIdentity', 'VerifyDomainIdentity',
                'VerifyDomainDkim', 'GetIdentityVerificationAttributes', 'GetIdentityDkimAttributes',
                'DeleteIdentity', 'GetSendQuota', 'GetSendStatistics', 'GetAccountSendingEnabled')}
            expected.update(SendEmail=8, SendRawEmail=4, ListTemplates=4, ListIdentities=4)
            assert counts == expected, f'unexpected requests (invalid inputs must stay local): {counts}'
            print('SES HTTP assertions passed: all 19 operations, UTF-8, MIME bytes, pagination, errors, and local validation')
            return 0
        finally:
            server.shutdown()
            worker.join()


if __name__ == '__main__':
    raise SystemExit(main())
