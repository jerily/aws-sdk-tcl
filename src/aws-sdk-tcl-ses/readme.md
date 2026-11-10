# Amazon SES for Tcl

`aws-sdk-tcl-ses` provides `package require awsses 1.0.16` on Tcl 8.6 and 9.x.
It wraps the SES classic API using AWS SDK C++ 1.11.924's `email` library.
The matching registry dependency is named `aws-sdk-cpp-ses`. SESv2 features
such as contact lists and account suppression management are outside this API.

## Install and send an email

```sh
ttrek install aws-sdk-tcl-ses
```

With the extension on Tcl's `auto_path` and AWS credentials available through
the SDK's normal environment, shared credentials file, or IAM role providers:

```tcl
package require awsses 1.0.16
set ses [::aws::ses::create {region us-east-1}]
try {
    set id [$ses send_email [dict create \
        from sender@example.com \
        to {recipient@example.com} \
        subject "Hello from Tcl" \
        text "This is a test email." \
        html "<p>This is a <strong>test</strong> email.</p>"]]
    puts "SES accepted message: $id"
} finally {
    $ses destroy
}
```

The [standalone example](../../examples/ses.tcl) takes `region from to
?subject? ?text?` arguments. The sender identity must be verified in the selected
region. In the SES sandbox, recipients must also be verified or be SES mailbox
simulator addresses. A returned message ID means SES accepted the request;
delivery is asynchronous. See [AWS SendEmail requirements](https://docs.aws.amazon.com/ses/latest/APIReference/API_SendEmail.html).

## Clients and errors

`::aws::ses::create config_dict` returns a client command. Configuration keys:

| Key | Meaning |
| --- | --- |
| `region` | AWS region, such as `us-east-1` |
| `endpoint` | Optional endpoint override, including scheme (useful for local tests) |
| `aws_access_key_id`, `aws_secret_access_key` | Optional explicit credentials; supply both together |
| `aws_session_token` | Optional temporary credential session token |

Omit explicit credentials to use the SDK's default credential provider chain.
Clients belong to their interpreter and are released by `$ses destroy`,
`rename $ses {}`, or interpreter deletion. Unlike some older bindings, `create`
does not accept a variable name; assign its return value using `set`.

Every method below has both forms: `$ses method ...` and
`::aws::ses::method $ses ...`. Names are exact, without abbreviations.
AWS errors raise a Tcl error with the service message and `-errorcode`
`{AWS SES ExceptionName}`, for example `{AWS SES MessageRejected}`.
Malformed dictionaries, unknown options and invalid recipient lists fail locally.
Service errors and request limits are enforced by AWS; calls are synchronous
and use the SDK's standard retry behavior.

## Sending

All three sending methods return the SES message ID.

`send_email message_dict` accepts:

| Key | Value |
| --- | --- |
| `from` | Required sender email address |
| `to`, `cc`, `bcc` | Tcl lists of addresses; 1–50 recipients total |
| `subject` | Required subject text (may be empty) |
| `text`, `html` | Body content; supply at least one, or both for multipart alternative |
| `reply_to` | Optional Tcl list of reply-to addresses |
| `return_path` | Optional address for bounce/complaint forwarding |
| `configuration_set` | Optional existing SES configuration set name |
| `tags` | Optional Tcl dictionary of message tag names and values |
| `source_arn`, `return_path_arn` | Optional sending authorization identity ARNs |

Subject and body strings are encoded as UTF-8. Address syntax and sender
authorization are validated by SES.

`send_templated_email message_dict` accepts the same envelope and optional
fields, replacing `subject`, `text`, and `html` with required `template` (an
existing SES template name) and `template_data` (a JSON string, not a Tcl dict):

```tcl
set id [$ses send_templated_email [dict create \
    from sender@example.com to {recipient@example.com} \
    template welcome template_data {{"name":"Ada"}}]]
```

The outer Tcl braces preserve the inner JSON braces in `template_data`.

`send_raw_email mime_bytes ?options_dict?` accepts a complete MIME message as
a Tcl byte array. Read files in binary mode. The SDK performs transport base64
encoding; do not base64-encode the entire message yourself. MIME construction,
including attachment encodings, is the caller's responsibility.

```tcl
set channel [open message.eml rb]
try {set mime [read $channel]} finally {close $channel}
set id [$ses send_raw_email $mime]
```

Optional keys are `from`, `destinations` (1–50 addresses when supplied),
`configuration_set`, `tags`, `source_arn`, `from_arn`, and `return_path_arn`.
Without explicit `from`/`destinations`, SES uses the MIME headers.
See [AWS SendRawEmail](https://docs.aws.amazon.com/ses/latest/APIReference/API_SendRawEmail.html)
for MIME requirements and attachment restrictions.

## Templates

| Method | Result |
| --- | --- |
| `create_template template_dict` | Empty result |
| `update_template template_dict` | Empty result; replaces the template definition |
| `get_template name` | Dict with `name`, `subject`, `text`, `html` |
| `delete_template name` | Empty result |
| `list_templates ?options_dict?` | Dict with `templates` and `next_token` |
| `test_render_template name template_data_json` | Rendered MIME string |

Template dictionaries require `name`, `subject` and at least one of `text` or
`html`. List options are `max_items` (1–100; AWS defaults to 10) and `next_token`. Each entry in
`templates` contains `name` and `created_timestamp` (UTC ISO 8601).
An empty `next_token` indicates the last page; pagination is explicit.

## Identities and DKIM

| Method | Result |
| --- | --- |
| `verify_email_identity email` | Empty result; SES sends a verification email |
| `verify_domain_identity domain` | Domain verification token for the DNS TXT record |
| `verify_domain_dkim domain` | List of tokens for the DNS DKIM CNAME records |
| `list_identities ?options_dict?` | Dict with `identities` and `next_token` |
| `get_identity_verification_attributes identities` | Dict keyed by identity; values contain `verification_status` and `verification_token` |
| `get_identity_dkim_attributes identities` | Dict keyed by identity; values contain `dkim_enabled`, `dkim_verification_status`, and `dkim_tokens` |
| `delete_identity identity` | Empty result |

Attribute requests take a Tcl list of 1–100 identities. SES may omit unknown
identities from the response. `list_identities` accepts `identity_type`
(`EmailAddress` or `Domain`, omitted for both), `max_items` (1–1000), and
`next_token`. Verification methods request verification; DNS records or email
confirmation must still be completed before SES marks an identity verified.

```tcl
set options [dict create identity_type Domain max_items 100]
while {1} {
    set page [$ses list_identities $options]
    foreach identity [dict get $page identities] {puts $identity}
    set token [dict get $page next_token]
    if {$token eq ""} break
    dict set options next_token $token
}
```

## Account sending information

| Method | Result |
| --- | --- |
| `get_send_quota` | Dict with doubles `max_24_hour_send`, `max_send_rate`, `sent_last_24_hours` |
| `get_send_statistics` | List of dicts with UTC ISO 8601 `timestamp`, and integer `delivery_attempts`, `bounces`, `complaints`, `rejects` |
| `get_account_sending_enabled` | Tcl boolean |

These values apply to the selected region. Configuration sets and event
destinations can be managed separately in AWS; `configuration_set` associates
outgoing email with an existing set.

## Build and test

Install the C++ SDK's `email` component, then from the repository root:

```sh
cmake -S . -B build -DAWS_SDK_CPP_DIR=/path/to/sdk -DAWS_SDK_TCL_SERVICES=ses
cmake --build build
ctest --test-dir build --output-on-failure
```

The request tests require Python 3 and a loopback socket. They check all 19
operations through both Tcl calling styles, UTF-8 and binary request data,
pagination, AWS errors, validation, and interpreter/client cleanup using fake
credentials. They never connect to AWS or send an email. Use separate build
directories for Tcl 8.6 and Tcl 9.x; `-DBUILD_TESTING=OFF` disables the tests.
