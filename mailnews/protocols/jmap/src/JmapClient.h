/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef COMM_MAILNEWS_PROTOCOLS_JMAP_SRC_JMAPCLIENT_H_
#define COMM_MAILNEWS_PROTOCOLS_JMAP_SRC_JMAPCLIENT_H_

#include "IJmapClient.h"
#include "nsIIOService.h"
#include "nsIStreamListener.h"
#include "nsIChannel.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "mozilla/Logging.h"
#include "mozilla/RefPtr.h"

extern "C" {
// The Rust JMAP client is created via the XPCOM components.conf registration.
// The actual protocol logic lives in rust/jmap_xpcom/.
// This C++ class is a thin wrapper that delegates all calls to the Rust
// implementation via the IJmapClient interface.
}

extern mozilla::LazyLogModule gJmapLog;

/**
 * JmapSession holds the results of a JMAP Session/get response.
 *
 * JMAP servers advertise their capabilities and API endpoints via a session
 * resource, which is discovered at the well-known path or via configuration.
 *
 * @see https://datatracker.ietf.org/doc/html/rfc8620#section-2
 */
struct JmapSession {
  nsCString accountId;
  nsCString apiUrl;           // URL for all API requests (apiUrl)
  nsCString downloadUrl;      // URL template for downloading blobs
  nsCString uploadUrl;        // URL template for uploading blobs
  nsCString eventSourceUrl;   // URL for EventSource push (optional)
  uint32_t maxConcurrentRequests = 0;
  uint64_t maxSizeUpload = 0;
  uint64_t maxSizeObjectInRequest = 0;
  bool serverSupportsPush = false;
  bool serverSupportsEmailQuery = false;
  bool serverSupportsEmailSubmission = false;
};

/**
 * JmapRequest represents a single JMAP API method call within a request.
 *
 * JMAP batches multiple method calls into a single HTTP request.
 * Each call has: methodName, id (client-generated), and arguments.
 *
 * @see https://datatracker.ietf.org/doc/html/rfc8620#section-3.2
 */
class JmapRequest {
 public:
  nsCString methodName;  // e.g., "Mailbox/get", "Email/changes"
  nsCString callId;      // Client-generated reference ID
  nsCString arguments;    // JSON-encoded arguments object

  JmapRequest() = default;
  JmapRequest(const nsACString& aMethodName, const nsACString& aCallId,
              const nsACString& aArguments)
      : methodName(aMethodName), callId(aCallId), arguments(aArguments) {}
};

/**
 * JmapClient implements IJmapClient to communicate with a JMAP server.
 *
 * All actual protocol logic is implemented in Rust (rust/jmap_xpcom/).
 * This C++ class serves as the XPCOM entry point and thin wrapper.
 *
 * The Rust implementation handles:
 * - Session discovery via .well-known/jmap (RFC 8620 §2)
 * - JSON request batching and response parsing
 * - All JMAP methods (Mailbox/*, Email/*, EmailSubmission/*)
 * - Delta sync with state tokens
 * - Blob upload/download
 * - OAuth2 Bearer token authentication
 *
 * @see https://datatracker.ietf.org/doc/html/rfc8620
 */
class JmapClient final : public IJmapClient {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_IJMAPCLIENT

  JmapClient();

  /// Returns the current session info.
  const JmapSession& GetSession() const { return mSession; }

  /// Returns the primary account ID.
  nsresult GetAccountId(nsACString& aAccountId) const;

  /// Check if the session has been initialized.
  bool HasSession() const { return mHasSession; }

 private:
  ~JmapClient();

  // Session state
  JmapSession mSession;
  bool mHasSession = false;
  bool mShuttingDown = false;

  // The configured endpoint URL (before session discovery)
  nsCString mConfiguredUrl;

  // The incoming server for authentication
  nsCOMPtr<nsIMsgIncomingServer> mServer;
  nsCOMPtr<nsIIOService> mIoService;
};

/// Helper to generate a JMAP method call ID.
nsCString GenerateJmapCallId();

#endif  // COMM_MAILNEWS_PROTOCOLS_JMAP_SRC_JMAPCLIENT_H_
