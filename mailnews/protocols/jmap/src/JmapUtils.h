/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef COMM_MAILNEWS_PROTOCOLS_JMAP_SRC_JMAPUTILS_H_
#define COMM_MAILNEWS_PROTOCOLS_JMAP_SRC_JMAPUTILS_H_

#include "nsString.h"
#include "nsTArray.h"
#include "nsMsgFolderFlags.h"

/**
 * JMAP protocol constants and utilities.
 *
 * References:
 * - RFC 8620: JSON Meta Application Protocol (JMAP)
 * - RFC 8621: JMAP Mail
 */

namespace jmap {

// JMAP protocol constants
constexpr auto kJmapMimeType = "application/json";
constexpr auto kJmapAcceptHeader = "application/json";
constexpr auto kContentTypeJmap = "application/json; charset=utf-8";

// JMAP method names (RFC 8620 Section 3.2)
namespace methods {
constexpr auto kSessionGet = "Session/get";
constexpr auto kMailboxGet = "Mailbox/get";
constexpr auto kMailboxSet = "Mailbox/set";
constexpr auto kMailboxChanges = "Mailbox/changes";
constexpr auto kMailboxQuery = "Mailbox/query";
constexpr auto kEmailGet = "Email/get";
constexpr auto kEmailSet = "Email/set";
constexpr auto kEmailChanges = "Email/changes";
constexpr auto kEmailQuery = "Email/query";
constexpr auto kEmailCopy = "Email/copy";
constexpr auto kEmailImport = "Email/import";
constexpr auto kEmailParse = "Email/parse";
constexpr auto kEmailSubmissionSet = "EmailSubmission/set";
constexpr auto kIdentityGet = "Identity/get";
}  // namespace methods

// JMAP capabilities URNs (RFC 8620 Section 2)
namespace capabilities {
constexpr auto kMail = "urn:ietf:params:jmap:mail";
constexpr auto kSubmission = "urn:ietf:params:jmap:submission";
constexpr auto kCalendar = "urn:ietf:params:jmap:calendar";
constexpr auto kContacts = "urn:ietf:params:jmap:contacts";
constexpr auto kPush = "urn:ietf:params:jmap:push";
}  // namespace capabilities

// JMAP well-known paths (RFC 8620 Section 2)
constexpr auto kWellKnownJmapPath = "/.well-known/jmap";
constexpr auto kSessionEndpoint = "/session";

// JMAP special keywords (RFC 8621 Section 4.4)
namespace keywords {
constexpr auto kSeen = "$seen";
constexpr auto kFlagged = "$flagged";
constexpr auto kAnswered = "$answered";
constexpr auto kDraft = "$draft";
constexpr auto kForwarded = "$forwarded";
constexpr auto kRecent = "$recent";
constexpr auto kImportant = "$important";
constexpr auto kJunk = "$junk";
constexpr auto kNotJunk = "$notjunk";
constexpr auto kPhishing = "$phishing";
}  // namespace keywords

// JMAP error types (RFC 8620 Section 3.6.1)
namespace errors {
constexpr auto kAccountNotFound = "accountNotFound";
constexpr auto kAccountNotSupportedByMethod = "accountNotSupportedByMethod";
constexpr auto kBlobNotFound = "blobNotFound";
constexpr auto kCannotCreateRecipients = "cannotCreateRecipients";
constexpr auto kForbidden = "forbidden";
constexpr auto kFromNotInAccount = "fromNotInAccount";
constexpr auto kInvalidArguments = "invalidArguments";
constexpr auto kInvalidPatch = "invalidPatch";
constexpr auto kNotFound = "notFound";
constexpr auto kProxyAllowed = "proxyAllowed";
constexpr auto kRequestTooLarge = "requestTooLarge";
constexpr auto kServerUnavailable = "serverUnavailable";
constexpr auto kStateMismatch = "stateMismatch";
constexpr auto kTooManyChanges = "tooManyChanges";
constexpr auto kUnknownCapability = "unknownCapability";
constexpr auto kUnknownMethod = "unknownMethod";
constexpr auto kAlreadyExists = "alreadyExists";
}  // namespace errors

// JMAP mailbox roles (RFC 8621 Section 2.1)
namespace roles {
constexpr auto kInbox = "inbox";
constexpr auto kArchive = "archive";
constexpr auto kDrafts = "drafts";
constexpr auto kJunk = "junk";
constexpr auto kSent = "sent";
constexpr auto kTrash = "trash";
}  // namespace roles

}  // namespace jmap

/**
 * Generates a JMAP request ID (used as the 3rd element in each method call).
 */
nsCString JmapGenerateCallId();

/**
 * Parses a JMAP state token from a JSON object field.
 */
nsresult JmapParseStateToken(const nsACString& aJson,
                             const nsACString& aField, nsACString& _retval);

/**
 * Builds a JMAP filter condition JSON for a specific mailbox.
 *
 * @param aMailboxId - The mailbox ID to filter on.
 * @param aAdditionalFilter - Optional additional filter JSON to AND with.
 * @return JSON string for the "inMailbox" filter.
 */
nsCString JmapBuildMailboxFilter(const nsACString& aMailboxId,
                                  const nsACString& aAdditionalFilter = ""_ns);

#endif  // COMM_MAILNEWS_PROTOCOLS_JMAP_SRC_JMAPUTILS_H_
