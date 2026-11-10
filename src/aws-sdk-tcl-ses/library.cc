/* SPDX-License-Identifier: MIT */
#include "library.h"
#include "../common/common.h"
#include <aws/email/SESClient.h>
#include <aws/email/model/SendEmailRequest.h>
#include <aws/email/model/SendRawEmailRequest.h>
#include <aws/email/model/SendTemplatedEmailRequest.h>
#include <aws/email/model/CreateTemplateRequest.h>
#include <aws/email/model/UpdateTemplateRequest.h>
#include <aws/email/model/GetTemplateRequest.h>
#include <aws/email/model/DeleteTemplateRequest.h>
#include <aws/email/model/ListTemplatesRequest.h>
#include <aws/email/model/TestRenderTemplateRequest.h>
#include <aws/email/model/VerifyEmailIdentityRequest.h>
#include <aws/email/model/VerifyDomainIdentityRequest.h>
#include <aws/email/model/VerifyDomainDkimRequest.h>
#include <aws/email/model/ListIdentitiesRequest.h>
#include <aws/email/model/GetIdentityVerificationAttributesRequest.h>
#include <aws/email/model/GetIdentityDkimAttributesRequest.h>
#include <aws/email/model/DeleteIdentityRequest.h>
#include <aws/email/model/GetSendQuotaRequest.h>
#include <aws/email/model/GetSendStatisticsRequest.h>
#include <aws/email/model/GetAccountSendingEnabledRequest.h>
#include <cstring>
#include <exception>
#include <initializer_list>
#include <map>
#include <memory>
#include <string>

#ifndef TCL_SIZE_MAX
typedef int Tcl_Size;
#endif
#ifndef VERSION
#define VERSION 1.0.16
#endif
#define SES_STRINGIFY_(s) #s
#define SES_STRINGIFY(s) SES_STRINGIFY_(s)

namespace {
using namespace Aws::SES::Model;
struct TclError {};

// AWS expects standard UTF-8; Tcl 8.6's internal encoding uses surrogate pairs.
Aws::String String(Tcl_Obj *obj) {
    Tcl_Size length;
    const char *value = Tcl_GetStringFromObj(obj, &length);
    Tcl_DString buffer;
    Tcl_Encoding encoding = Tcl_GetEncoding(nullptr, "utf-8");
    Tcl_UtfToExternalDString(encoding, value, length, &buffer);
    Aws::String result(Tcl_DStringValue(&buffer), Tcl_DStringLength(&buffer));
    Tcl_DStringFree(&buffer);
    Tcl_FreeEncoding(encoding);
    return result;
}

Tcl_Obj *StringObj(const Aws::String &value) {
    Tcl_DString buffer;
    Tcl_Encoding encoding = Tcl_GetEncoding(nullptr, "utf-8");
    Tcl_ExternalToUtfDString(encoding, value.data(), value.size(), &buffer);
    Tcl_Obj *result = Tcl_NewStringObj(Tcl_DStringValue(&buffer), Tcl_DStringLength(&buffer));
    Tcl_DStringFree(&buffer);
    Tcl_FreeEncoding(encoding);
    return result;
}

[[noreturn]] void Fail(Tcl_Interp *interp, const std::string &message) {
    Tcl_SetObjResult(interp, Tcl_NewStringObj(message.c_str(), -1));
    throw TclError();
}

// Own temporary results even if an exception interrupts their construction.
struct Obj {
    Tcl_Obj *value;
    explicit Obj(Tcl_Obj *obj) : value(obj) { Tcl_IncrRefCount(value); }
    ~Obj() { Tcl_DecrRefCount(value); }
    Obj(const Obj &) = delete;
    Obj &operator=(const Obj &) = delete;
    operator Tcl_Obj *() const { return value; }
};

void Put(Tcl_Obj *dict, const char *key, Tcl_Obj *value) {
    Tcl_DictObjPut(nullptr, dict, Tcl_NewStringObj(key, -1), value);
}

Tcl_Obj *StringList(const Aws::Vector<Aws::String> &values) {
    Tcl_Obj *list = Tcl_NewListObj(0, nullptr);
    for (const auto &value : values) Tcl_ListObjAppendElement(nullptr, list, StringObj(value));
    return list;
}

Aws::Vector<Aws::String> List(Tcl_Interp *interp, Tcl_Obj *obj) {
    Aws::Vector<Aws::String> result;
    if (!obj) return result;
    Tcl_Size count;
    Tcl_Obj **items;
    if (Tcl_ListObjGetElements(interp, obj, &count, &items) != TCL_OK) throw TclError();
    for (Tcl_Size i = 0; i < count; ++i) {
        auto value = String(items[i]);
        if (value.empty()) Fail(interp, "list entries must not be empty");
        result.push_back(std::move(value));
    }
    return result;
}

struct Dict {
    Tcl_Interp *interp;
    std::map<std::string, Tcl_Obj *> fields;
    Dict(Tcl_Interp *i, Tcl_Obj *obj, std::initializer_list<const char *> allowed) : interp(i) {
        if (!obj) return;
        Tcl_DictSearch search;
        Tcl_Obj *key, *value;
        int done;
        if (Tcl_DictObjFirst(interp, obj, &search, &key, &value, &done) != TCL_OK) throw TclError();
        while (!done) {
            std::string name(Tcl_GetString(key));
            bool valid = false;
            for (const char *item : allowed) if (name == item) valid = true;
            if (!valid) {
                Tcl_DictObjDone(&search);
                Fail(interp, "unknown option: " + name);
            }
            fields[name] = value;
            Tcl_DictObjNext(&search, &key, &value, &done);
        }
        Tcl_DictObjDone(&search);
    }
    Tcl_Obj *Get(const char *key) const {
        auto it = fields.find(key);
        return it == fields.end() ? nullptr : it->second;
    }
    Aws::String Required(const char *key) const {
        Tcl_Obj *obj = Get(key);
        if (!obj || String(obj).empty()) Fail(interp, std::string("missing or empty option: ") + key);
        return String(obj);
    }
};

template<class Request> void CommonSend(const Dict &dict, Request &request) {
    if (dict.Get("from")) request.SetSource(dict.Required("from"));
    if (dict.Get("source_arn")) request.SetSourceArn(String(dict.Get("source_arn")));
    if (dict.Get("return_path_arn")) request.SetReturnPathArn(String(dict.Get("return_path_arn")));
    if (dict.Get("configuration_set")) request.SetConfigurationSetName(String(dict.Get("configuration_set")));
    if (dict.Get("tags")) {
        Tcl_DictSearch search;
        Tcl_Obj *key, *value;
        int done;
        if (Tcl_DictObjFirst(dict.interp, dict.Get("tags"), &search, &key, &value, &done) != TCL_OK) throw TclError();
        while (!done) {
            MessageTag tag;
            tag.SetName(String(key));
            tag.SetValue(String(value));
            request.AddTags(std::move(tag));
            Tcl_DictObjNext(&search, &key, &value, &done);
        }
        Tcl_DictObjDone(&search);
    }
}

template<class Request> void Envelope(const Dict &dict, Request &request) {
    request.SetSource(dict.Required("from"));
    Destination destination;
    auto to = List(dict.interp, dict.Get("to"));
    auto cc = List(dict.interp, dict.Get("cc"));
    auto bcc = List(dict.interp, dict.Get("bcc"));
    size_t count = to.size() + cc.size() + bcc.size();
    if (count < 1 || count > 50) Fail(dict.interp, "provide between 1 and 50 recipients across to, cc, and bcc");
    if (dict.Get("to")) destination.SetToAddresses(to);
    if (dict.Get("cc")) destination.SetCcAddresses(cc);
    if (dict.Get("bcc")) destination.SetBccAddresses(bcc);
    request.SetDestination(destination);
    if (dict.Get("reply_to")) request.SetReplyToAddresses(List(dict.interp, dict.Get("reply_to")));
    if (dict.Get("return_path")) request.SetReturnPath(String(dict.Get("return_path")));
    CommonSend(dict, request);
}

Content TextContent(Tcl_Obj *obj) {
    Content content;
    content.SetCharset("UTF-8");
    content.SetData(String(obj));
    return content;
}

template<class Request> void Pagination(const Dict &dict, Request &request, int maximum) {
    if (dict.Get("next_token")) request.SetNextToken(String(dict.Get("next_token")));
    if (dict.Get("max_items")) {
        int count;
        if (Tcl_GetIntFromObj(dict.interp, dict.Get("max_items"), &count) != TCL_OK) throw TclError();
        if (count < 1 || count > maximum) Fail(dict.interp, "max_items must be between 1 and " + std::to_string(maximum));
        request.SetMaxItems(count);
    }
}

template<class Outcome> void Check(Tcl_Interp *interp, const Outcome &outcome) {
    if (outcome.IsSuccess()) return;
    const auto &error = outcome.GetError();
    Tcl_SetObjResult(interp, StringObj(error.GetMessage()));
    Tcl_SetErrorCode(interp, "AWS", "SES", error.GetExceptionName().c_str(), nullptr);
    throw TclError();
}

struct Runtime {
    Aws::SDKOptions options;
    unsigned long serial = 0;
    Runtime() { Aws::InitAPI(options); }
    ~Runtime() { Aws::ShutdownAPI(options); }
};
struct Client {
    // Keep the SDK alive until AFTER the client is destroyed, including on interp deletion.
    std::shared_ptr<Runtime> runtime;
    std::unique_ptr<Aws::SES::SESClient> sdk;
    Tcl_Command command = nullptr;
};

struct Method { const char *name; int minArgs; int maxArgs; const char *usage; };
const Method methods[] = {
    {"destroy", 0, 0, ""},
    {"send_email", 1, 1, "message_dict"},
    {"send_raw_email", 1, 2, "mime_bytes ?options_dict?"},
    {"send_templated_email", 1, 1, "message_dict"},
    {"create_template", 1, 1, "template_dict"},
    {"update_template", 1, 1, "template_dict"},
    {"get_template", 1, 1, "name"},
    {"delete_template", 1, 1, "name"},
    {"list_templates", 0, 1, "?options_dict?"},
    {"test_render_template", 2, 2, "name template_data_json"},
    {"verify_email_identity", 1, 1, "email"},
    {"verify_domain_identity", 1, 1, "domain"},
    {"verify_domain_dkim", 1, 1, "domain"},
    {"list_identities", 0, 1, "?options_dict?"},
    {"get_identity_verification_attributes", 1, 1, "identities"},
    {"get_identity_dkim_attributes", 1, 1, "identities"},
    {"delete_identity", 1, 1, "identity"},
    {"get_send_quota", 0, 0, ""},
    {"get_send_statistics", 0, 0, ""},
    {"get_account_sending_enabled", 0, 0, ""},
    {nullptr, 0, 0, nullptr}
};

int Invoke(Client *client, Tcl_Interp *interp, const Method &method, int objc,
           Tcl_Obj *const objv[], int start) {
    const int count = objc - start;
    if (count < method.minArgs || count > method.maxArgs) {
        Tcl_WrongNumArgs(interp, start, objv, method.usage);
        return TCL_ERROR;
    }
    Tcl_Obj *arg = count ? objv[start] : nullptr;
    const std::string name(method.name);
    auto &sdk = *client->sdk;
    if (name == "destroy") {
        Tcl_DeleteCommandFromToken(interp, client->command);
    } else if (name == "send_email" || name == "send_templated_email") {
        // Keep option sets separate so misspelled or inapplicable fields fail locally.
        const bool templated = name == "send_templated_email";
        Dict dict(interp, arg, templated
            ? std::initializer_list<const char *>{"from", "to", "cc", "bcc", "reply_to", "return_path", "source_arn", "return_path_arn", "configuration_set", "tags", "template", "template_data"}
            : std::initializer_list<const char *>{"from", "to", "cc", "bcc", "reply_to", "return_path", "source_arn", "return_path_arn", "configuration_set", "tags", "subject", "text", "html"});
        if (templated) {
            SendTemplatedEmailRequest request;
            Envelope(dict, request);
            request.SetTemplate(dict.Required("template"));
            request.SetTemplateData(dict.Required("template_data"));
            auto outcome = sdk.SendTemplatedEmail(request);
            Check(interp, outcome);
            Tcl_SetObjResult(interp, StringObj(outcome.GetResult().GetMessageId()));
        } else {
            SendEmailRequest request;
            Envelope(dict, request);
            if (!dict.Get("subject")) Fail(interp, "missing option: subject");
            if (!dict.Get("text") && !dict.Get("html")) Fail(interp, "provide text or html (or both)");
            Body body;
            if (dict.Get("text")) body.SetText(TextContent(dict.Get("text")));
            if (dict.Get("html")) body.SetHtml(TextContent(dict.Get("html")));
            Message message;
            message.SetSubject(TextContent(dict.Get("subject")));
            message.SetBody(body);
            request.SetMessage(message);
            auto outcome = sdk.SendEmail(request);
            Check(interp, outcome);
            Tcl_SetObjResult(interp, StringObj(outcome.GetResult().GetMessageId()));
        }
    } else if (name == "send_raw_email") {
        Dict dict(interp, count == 2 ? objv[start + 1] : nullptr,
            {"from", "destinations", "source_arn", "from_arn", "return_path_arn", "configuration_set", "tags"});
        SendRawEmailRequest request;
        CommonSend(dict, request);
        if (dict.Get("from_arn")) request.SetFromArn(String(dict.Get("from_arn")));
        if (dict.Get("destinations")) {
            auto recipients = List(interp, dict.Get("destinations"));
            if (recipients.empty() || recipients.size() > 50) Fail(interp, "destinations must contain between 1 and 50 recipients");
            request.SetDestinations(recipients);
        }
        Tcl_Size length;
        const unsigned char *bytes = Tcl_GetByteArrayFromObj(arg, &length);
        if (!length) Fail(interp, "mime_bytes must not be empty");
        RawMessage message;
        message.SetData(Aws::Utils::ByteBuffer(bytes, length));
        request.SetRawMessage(message);
        auto outcome = sdk.SendRawEmail(request);
        Check(interp, outcome);
        Tcl_SetObjResult(interp, StringObj(outcome.GetResult().GetMessageId()));
    } else if (name == "create_template" || name == "update_template") {
        Dict dict(interp, arg, {"name", "subject", "text", "html"});
        Template value;
        value.SetTemplateName(dict.Required("name"));
        if (!dict.Get("subject")) Fail(interp, "missing option: subject");
        value.SetSubjectPart(String(dict.Get("subject")));
        if (!dict.Get("text") && !dict.Get("html")) Fail(interp, "provide text or html (or both)");
        if (dict.Get("text")) value.SetTextPart(String(dict.Get("text")));
        if (dict.Get("html")) value.SetHtmlPart(String(dict.Get("html")));
        if (name == "create_template") {
            CreateTemplateRequest request;
            request.SetTemplate(value);
            Check(interp, sdk.CreateTemplate(request));
        } else {
            UpdateTemplateRequest request;
            request.SetTemplate(value);
            Check(interp, sdk.UpdateTemplate(request));
        }
    } else if (name == "get_template") {
        GetTemplateRequest request;
        request.SetTemplateName(String(arg));
        auto outcome = sdk.GetTemplate(request);
        Check(interp, outcome);
        const auto &value = outcome.GetResult().GetTemplate();
        Obj result(Tcl_NewDictObj());
        Put(result, "name", StringObj(value.GetTemplateName()));
        Put(result, "subject", StringObj(value.GetSubjectPart()));
        Put(result, "text", StringObj(value.GetTextPart()));
        Put(result, "html", StringObj(value.GetHtmlPart()));
        Tcl_SetObjResult(interp, result);
    } else if (name == "delete_template") {
        DeleteTemplateRequest request;
        request.SetTemplateName(String(arg));
        Check(interp, sdk.DeleteTemplate(request));
    } else if (name == "list_templates") {
        Dict dict(interp, arg, {"max_items", "next_token"});
        ListTemplatesRequest request;
        Pagination(dict, request, 100);
        auto outcome = sdk.ListTemplates(request);
        Check(interp, outcome);
        Obj result(Tcl_NewDictObj()), entries(Tcl_NewListObj(0, nullptr));
        for (const auto &value : outcome.GetResult().GetTemplatesMetadata()) {
            Obj entry(Tcl_NewDictObj());
            Put(entry, "name", StringObj(value.GetName()));
            Put(entry, "created_timestamp", StringObj(value.GetCreatedTimestamp().ToGmtString(Aws::Utils::DateFormat::ISO_8601)));
            Tcl_ListObjAppendElement(interp, entries, entry);
        }
        Put(result, "templates", entries);
        Put(result, "next_token", StringObj(outcome.GetResult().GetNextToken()));
        Tcl_SetObjResult(interp, result);
    } else if (name == "test_render_template") {
        TestRenderTemplateRequest request;
        request.SetTemplateName(String(arg));
        request.SetTemplateData(String(objv[start + 1]));
        auto outcome = sdk.TestRenderTemplate(request);
        Check(interp, outcome);
        Tcl_SetObjResult(interp, StringObj(outcome.GetResult().GetRenderedTemplate()));
    } else if (name == "verify_email_identity") {
        VerifyEmailIdentityRequest request;
        request.SetEmailAddress(String(arg));
        Check(interp, sdk.VerifyEmailIdentity(request));
    } else if (name == "verify_domain_identity") {
        VerifyDomainIdentityRequest request;
        request.SetDomain(String(arg));
        auto outcome = sdk.VerifyDomainIdentity(request);
        Check(interp, outcome);
        Tcl_SetObjResult(interp, StringObj(outcome.GetResult().GetVerificationToken()));
    } else if (name == "verify_domain_dkim") {
        VerifyDomainDkimRequest request;
        request.SetDomain(String(arg));
        auto outcome = sdk.VerifyDomainDkim(request);
        Check(interp, outcome);
        Tcl_SetObjResult(interp, StringList(outcome.GetResult().GetDkimTokens()));
    } else if (name == "list_identities") {
        Dict dict(interp, arg, {"identity_type", "max_items", "next_token"});
        ListIdentitiesRequest request;
        Pagination(dict, request, 1000);
        if (dict.Get("identity_type")) {
            auto type = dict.Required("identity_type");
            if (type != "EmailAddress" && type != "Domain") Fail(interp, "identity_type must be EmailAddress or Domain");
            request.SetIdentityType(IdentityTypeMapper::GetIdentityTypeForName(type));
        }
        auto outcome = sdk.ListIdentities(request);
        Check(interp, outcome);
        Obj result(Tcl_NewDictObj());
        Put(result, "identities", StringList(outcome.GetResult().GetIdentities()));
        Put(result, "next_token", StringObj(outcome.GetResult().GetNextToken()));
        Tcl_SetObjResult(interp, result);
    } else if (name == "get_identity_verification_attributes" || name == "get_identity_dkim_attributes") {
        auto identities = List(interp, arg);
        if (identities.empty() || identities.size() > 100) Fail(interp, "identities must contain between 1 and 100 entries");
        Obj result(Tcl_NewDictObj());
        if (name == "get_identity_verification_attributes") {
            GetIdentityVerificationAttributesRequest request;
            request.SetIdentities(identities);
            auto outcome = sdk.GetIdentityVerificationAttributes(request);
            Check(interp, outcome);
            for (const auto &item : outcome.GetResult().GetVerificationAttributes()) {
                Obj entry(Tcl_NewDictObj());
                Put(entry, "verification_status", StringObj(VerificationStatusMapper::GetNameForVerificationStatus(item.second.GetVerificationStatus())));
                Put(entry, "verification_token", StringObj(item.second.GetVerificationToken()));
                Tcl_DictObjPut(interp, result, StringObj(item.first), entry);
            }
        } else {
            GetIdentityDkimAttributesRequest request;
            request.SetIdentities(identities);
            auto outcome = sdk.GetIdentityDkimAttributes(request);
            Check(interp, outcome);
            for (const auto &item : outcome.GetResult().GetDkimAttributes()) {
                Obj entry(Tcl_NewDictObj());
                Put(entry, "dkim_enabled", Tcl_NewBooleanObj(item.second.GetDkimEnabled()));
                Put(entry, "dkim_verification_status", StringObj(VerificationStatusMapper::GetNameForVerificationStatus(item.second.GetDkimVerificationStatus())));
                Put(entry, "dkim_tokens", StringList(item.second.GetDkimTokens()));
                Tcl_DictObjPut(interp, result, StringObj(item.first), entry);
            }
        }
        Tcl_SetObjResult(interp, result);
    } else if (name == "delete_identity") {
        DeleteIdentityRequest request;
        request.SetIdentity(String(arg));
        Check(interp, sdk.DeleteIdentity(request));
    } else if (name == "get_send_quota") {
        auto outcome = sdk.GetSendQuota(GetSendQuotaRequest());
        Check(interp, outcome);
        Obj result(Tcl_NewDictObj());
        Put(result, "max_24_hour_send", Tcl_NewDoubleObj(outcome.GetResult().GetMax24HourSend()));
        Put(result, "max_send_rate", Tcl_NewDoubleObj(outcome.GetResult().GetMaxSendRate()));
        Put(result, "sent_last_24_hours", Tcl_NewDoubleObj(outcome.GetResult().GetSentLast24Hours()));
        Tcl_SetObjResult(interp, result);
    } else if (name == "get_send_statistics") {
        auto outcome = sdk.GetSendStatistics(GetSendStatisticsRequest());
        Check(interp, outcome);
        Obj result(Tcl_NewListObj(0, nullptr));
        for (const auto &point : outcome.GetResult().GetSendDataPoints()) {
            Obj entry(Tcl_NewDictObj());
            Put(entry, "timestamp", StringObj(point.GetTimestamp().ToGmtString(Aws::Utils::DateFormat::ISO_8601)));
            Put(entry, "delivery_attempts", Tcl_NewWideIntObj(point.GetDeliveryAttempts()));
            Put(entry, "bounces", Tcl_NewWideIntObj(point.GetBounces()));
            Put(entry, "complaints", Tcl_NewWideIntObj(point.GetComplaints()));
            Put(entry, "rejects", Tcl_NewWideIntObj(point.GetRejects()));
            Tcl_ListObjAppendElement(interp, result, entry);
        }
        Tcl_SetObjResult(interp, result);
    } else if (name == "get_account_sending_enabled") {
        auto outcome = sdk.GetAccountSendingEnabled(GetAccountSendingEnabledRequest());
        Check(interp, outcome);
        Tcl_SetObjResult(interp, Tcl_NewBooleanObj(outcome.GetResult().GetEnabled()));
    }
    return TCL_OK;
}

// No C++ exception may cross a Tcl callback boundary.
template<class Function> int Boundary(Tcl_Interp *interp, Function function) {
    try { return function(); }
    catch (const TclError &) { return TCL_ERROR; }
    catch (const std::exception &error) {
        Tcl_SetObjResult(interp, Tcl_NewStringObj(error.what(), -1));
        return TCL_ERROR;
    }
    catch (...) {
        Tcl_SetObjResult(interp, Tcl_NewStringObj("unexpected SES client error", -1));
        return TCL_ERROR;
    }
}

int ClientCommand(ClientData data, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[]) {
    return Boundary(interp, [&]() {
        if (objc < 2) { Tcl_WrongNumArgs(interp, 1, objv, "method ?arg ...?"); return TCL_ERROR; }
        int index;
        if (Tcl_GetIndexFromObjStruct(interp, objv[1], methods, sizeof(Method), "method", TCL_EXACT, &index) != TCL_OK) return TCL_ERROR;
        return Invoke(static_cast<Client *>(data), interp, methods[index], objc, objv, 2);
    });
}

int NamespaceCommand(ClientData data, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[]) {
    return Boundary(interp, [&]() {
        const auto &method = *static_cast<const Method *>(data);
        if (objc < 2) {
            std::string usage = std::string("handle ") + method.usage;
            Tcl_WrongNumArgs(interp, 1, objv, usage.c_str());
            return TCL_ERROR;
        }
        Tcl_Command token = Tcl_GetCommandFromObj(interp, objv[1]);
        Tcl_CmdInfo info;
        if (!token || !Tcl_GetCommandInfoFromToken(token, &info) || info.objProc != ClientCommand)
            Fail(interp, "SES client handle not found in this interpreter");
        return Invoke(static_cast<Client *>(info.objClientData), interp, method, objc, objv, 2);
    });
}

void DeleteClient(ClientData data) { delete static_cast<Client *>(data); }
void DeleteRuntime(ClientData data, Tcl_Interp *) { delete static_cast<std::shared_ptr<Runtime> *>(data); }

int CreateCommand(ClientData data, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[]) {
    return Boundary(interp, [&]() {
        if (objc != 2) { Tcl_WrongNumArgs(interp, 1, objv, "config_dict"); return TCL_ERROR; }
        Dict config(interp, objv[1], {"region", "endpoint", "aws_access_key_id", "aws_secret_access_key", "aws_session_token"});
        if (!!config.Get("aws_access_key_id") != !!config.Get("aws_secret_access_key"))
            Fail(interp, "aws_access_key_id and aws_secret_access_key must be supplied together");
        auto settings = get_client_config_and_credentials_provider(interp, objv[1]);
        if (std::get<0>(settings) != TCL_OK) throw TclError();
        auto &runtime = *static_cast<std::shared_ptr<Runtime> *>(data);
        std::unique_ptr<Client> client(new Client());
        client->runtime = runtime;
        auto provider = std::get<2>(settings);
        client->sdk.reset(provider ? new Aws::SES::SESClient(provider, std::get<1>(settings))
                                   : new Aws::SES::SESClient(std::get<1>(settings)));
        std::string handle;
        do { handle = "::aws::ses::client" + std::to_string(++runtime->serial); }
        while (Tcl_FindCommand(interp, handle.c_str(), nullptr, TCL_GLOBAL_ONLY));
        client->command = Tcl_CreateObjCommand(interp, handle.c_str(), ClientCommand, client.get(), DeleteClient);
        if (!client->command) return TCL_ERROR;
        client.release();
        Tcl_SetObjResult(interp, Tcl_NewStringObj(handle.c_str(), -1));
        return TCL_OK;
    });
}
} // namespace

int Aws_sdk_tcl_ses_Init(Tcl_Interp *interp) {
#if TCL_MAJOR_VERSION > 8
    const char *minimum = "9.0";
#else
    const char *minimum = "8.6";
#endif
    if (!Tcl_InitStubs(interp, minimum, 0)) return TCL_ERROR;
    return Boundary(interp, [&]() {
        if (!Tcl_FindNamespace(interp, "::aws::ses", nullptr, TCL_GLOBAL_ONLY) &&
            !Tcl_CreateNamespace(interp, "::aws::ses", nullptr, nullptr)) return TCL_ERROR;
        auto *runtime = static_cast<std::shared_ptr<Runtime> *>(Tcl_GetAssocData(interp, "aws-sdk-tcl-ses", nullptr));
        if (!runtime) {
            runtime = new std::shared_ptr<Runtime>(std::make_shared<Runtime>());
            Tcl_SetAssocData(interp, "aws-sdk-tcl-ses", DeleteRuntime, runtime);
        }
        Tcl_CreateObjCommand(interp, "::aws::ses::create", CreateCommand, runtime, nullptr);
        for (const Method *method = methods; method->name; ++method) {
            std::string name = std::string("::aws::ses::") + method->name;
            Tcl_CreateObjCommand(interp, name.c_str(), NamespaceCommand, const_cast<Method *>(method), nullptr);
        }
        return Tcl_PkgProvide(interp, "awsses", SES_STRINGIFY(VERSION));
    });
}

#ifdef USE_NAVISERVER
int Ns_ModuleVersion = 1;
int Ns_ModuleInit(const char *server, const char *module) {
    Ns_TclRegisterTrace(server, (Ns_TclTraceProc *) Aws_sdk_tcl_ses_Init, server, NS_TCL_TRACE_CREATE);
    return NS_OK;
}
#endif
