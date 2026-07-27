# JMAP Support for Thunderbird

This directory contains the JMAP (JSON Meta Application Protocol) protocol
implementation for Mozilla Thunderbird, with a focus on compatibility with
[Stalwart](https://stalw.art) Mail Server.

## Overview

JMAP is a modern, JSON-based email protocol defined in:
- **RFC 8620** — [JSON Meta Application Protocol (JMAP)](https://datatracker.ietf.org/doc/html/rfc8620)
- **RFC 8621** — [JMAP Mail](https://datatracker.ietf.org/doc/html/rfc8621)

JMAP offers significant advantages over IMAP/POP:
- **JSON-based**: No text-based protocol parsing needed
- **Delta sync**: Efficient state-based synchronization (less bandwidth)
- **Batching**: Multiple operations in a single HTTP request
- **Push notifications**: Real-time updates via EventSource
- **Modern auth**: Built for OAuth2 Bearer tokens
- **No port scanning**: Uses standard HTTPS (443)

## Architecture

The implementation follows Thunderbird's existing protocol architecture,
modeled after the Exchange/EWS protocol implementation:

```
mailnews/protocols/jmap/
├── src/
│   ├── IJmapClient.idl            # JMAP client interface (RFC 8620/8621 ops)
│   ├── IJmapFolder.idl            # JMAP folder extensions
│   ├── IJmapIncomingServer.idl    # JMAP server configuration
│   ├── JmapClient.h/.cpp           # HTTP client for JMAP API
│   ├── JmapFolder.h/.cpp           # Folder (mailbox) implementation
│   ├── JmapIncomingServer.h/.cpp   # Server implementation
│   ├── JmapProtocolInfo.h/.cpp     # Protocol registration
│   ├── JmapProtocolHandler.h/.cpp  # x-moz-jmap URL scheme
│   ├── JmapService.h/.cpp          # Message service (display, fetch, etc.)
│   ├── JmapListeners.h/.cpp        # Async operation callbacks
│   ├── JmapUtils.h/.cpp             # Constants, helpers
│   ├── components.conf             # XPCOM component registration
│   └── moz.build                   # Build configuration
├── public/
│   ├── IJmapOutgoingServer.idl     # Outgoing (EmailSubmission) interface
│   └── moz.build
├── test/
│   ├── browser/head.js
│   ├── unit/head.js
│   └── moz.build
└── moz.build
```

## XPCOM Components

Registered in `components.conf`:

| Contract ID                    | Class               | Purpose                     |
|-------------------------------|---------------------|-----------------------------|
| `@mozilla.org/messenger/server;1?type=jmap` | JmapIncomingServer | Server configuration       |
| `@mozilla.org/messenger/protocol/info;1?type=jmap` | JmapProtocolInfo | Protocol metadata |
| `@mozilla.org/mail/folder-factory;1?name=jmap` | JmapFolder | Folder creation            |
| `@mozilla.org/messenger/messageservice;1?type=jmap` | JmapService | Message access            |
| `@mozilla.org/network/protocol;1?name=x-moz-jmap` | JmapProtocolHandler | URL scheme handler         |
| `@mozilla.org/messenger/jmap-client;1` | JmapClient | JMAP HTTP client            |

## JMAP Protocol Flow

### Session Discovery (RFC 8620 §2)
```
Client                              Server
  |                                    |
  |  GET /.well-known/jmap             |
  |----------------------------------->|
  |                                    |
  |  200 OK (session JSON)            |
  |<-----------------------------------|
  |                                    |
  // Response includes:
  // - accounts (list of account objects)
  // - primaryAccounts (primary for each capability)
  // - urls.apiUrl (endpoint for API requests)
  // - urls.downloadUrl (template for blob downloads)
  // - urls.uploadUrl (template for uploads)
  // - capabilities (what the server supports)
```

### Mailbox Sync (Mailbox/changes + Mailbox/get)
```
POST /jmap
[
  ["Mailbox/changes", {
    "accountId": "...",
    "sinceState": "abc123"
  }, "c1"],
  ["Mailbox/get", {
    "accountId": "...",
    "#ids": { "resultOf": "c1", "path": "/changed", "from": 0, "length": 100 },
    "properties": ["id", "name", "parentId", "role", "sortOrder"]
  }, "c2"]
]
```

### Message Sync (Email/changes + Email/get)
```
POST /jmap
[
  ["Email/changes", {
    "accountId": "...",
    "sinceState": "xyz789"
  }, "c1"],
  ["Email/get", {
    "accountId": "...",
    "#ids": { "resultOf": "c1", "path": "/created", "from": 0, "length": 50 },
    "properties": ["id", "threadId", "mailboxIds", "from", "subject",
                    "receivedAt", "size", "preview", "keywords"]
  }, "c2"]
]
```

## Stalwart-Specific Details

[Stalwart](https://stalw.art) is a modern mail server that natively supports JMAP:

### Session URL
- Default: `https://mail.example.com/.well-known/jmap/session`
- The session endpoint also accepts direct API requests at `https://mail.example.com/jmap`

### Authentication
- OAuth2 (preferred): Bearer token in Authorization header
- HTTP Basic: Username/password (server must enable)
- Stalwart uses its own built-in OAuth2 server

### Capabilities
Stalwart supports the full JMAP specification:
- `urn:ietf:params:jmap:mail` (RFC 8621)
- `urn:ietf:params:jmap:submission`
- `urn:ietf:params:jmap:contacts` (CardDAV bridge)
- `urn:ietf:params:jmap:push` (EventSource)
- Sieve filtering via JMAP extension

### Default Ports
- JMAP HTTPS: 443 (default)
- SMTP Submission: 587 (STARTTLS)

## Implementation Status

### Phase 1: Foundation (Current)
- [x] Protocol registration (components, moz.build)
- [x] IDL interfaces (IJmapClient, IJmapFolder, IJmapIncomingServer)
- [x] JmapIncomingServer skeleton
- [x] JmapFolder skeleton
- [x] JmapClient HTTP skeleton
- [x] JmapProtocolInfo (protocol metadata)
- [x] JmapProtocolHandler (URL scheme)
- [x] JmapService (message service skeleton)
- [x] Default preferences
- [x] Async listener framework
- [x] JMAP constants and utilities
- [x] MOTS entry

### Phase 2: Core Protocol
- [ ] Async HTTP session discovery
- [ ] Full JSON parsing for session response
- [ ] Bearer token authentication
- [ ] Mailbox/changes + Mailbox/get implementation
- [ ] Email/changes + Email/get implementation
- [ ] Role-to-folder-flag mapping
- [ ] State token persistence

### Phase 3: Message Operations
- [ ] Full message body download (blob download)
- [ ] Message display (JmapService)
- [ ] Message compose (Email/import)
- [ ] Message submission (EmailSubmission/set)
- [ ] Read/unread flag sync ($seen keyword)
- [ ] Starred flag sync ($flagged keyword)
- [ ] Delete/Move/Copy operations

### Phase 4: Advanced Features
- [ ] JMAP Push (EventSource notifications)
- [ ] Email/query (server-side search)
- [ ] Thread collapsing
- [ ] Biff (new message notification)
- [ ] Offline support (Email/get with body)
- [ ] Attachment upload/download
- [ ] Sieve filter management

### Phase 5: UI Integration
- [ ] Account setup wizard integration
- [ ] JMAP server settings panel
- [ ] Push notification status indicator
- [ ] Error handling and UI feedback

## Building

After integrating into the Thunderbird tree, build with:
```bash
./mach build
./mach run
```

## Testing

Unit tests and browser tests are in `test/`. To run:
```bash
# Unit tests
./mach test mailnews/protocols/jmap/test/unit/

# Browser tests
./mach test mailnews/protocols/jmap/test/browser/
```

## References

- [RFC 8620 — JMAP Core](https://datatracker.ietf.org/doc/html/rfc8620)
- [RFC 8621 — JMAP Mail](https://datatracker.ietf.org/doc/html/rfc8621)
- [RFC 8622 — JMAP CalDAV/CARD](https://datatracker.ietf.org/doc/html/rfc8622)
- [RFC 8623 — JMAP Contacts](https://datatracker.ietf.org/doc/html/rfc8623)
- [Stalwart Mail Server](https://stalw.art)
- [JMAP vs IMAP comparison](https://jmap.io/spec.html)
