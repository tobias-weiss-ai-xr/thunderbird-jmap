# Thunderbird JMAP Support

A JMAP (JSON Meta Application Protocol) implementation for Mozilla Thunderbird,
with special focus on [Stalwart](https://stalw.art) Mail Server compatibility.

## What is JMAP?

JMAP (RFC 8620/8621) is a modern, JSON-based email protocol that replaces IMAP
with a more efficient, developer-friendly approach:

| Feature | IMAP | JMAP |
|---------|------|------|
| Protocol | Text-based | JSON over HTTPS |
| Auth | LOGIN/CRAM/SASL | OAuth2 Bearer tokens |
| Sync | Full folder scan | Delta sync with state tokens |
| Batching | Sequential commands | Multiple ops in single request |
| Push | IDLE (TCP keepalive) | EventSource (SSE) |
| Search | Server-side, limited | Powerful filter conditions |
| Ports | 143/993 | 443 (standard HTTPS) |

## Repository Structure

```
├── mailnews/                    # Thunderbird mail/news codebase
│   ├── protocols/
│   │   ├── exchange/            # Exchange/EWS/Graph (reference)
│   │   ├── jmap/               # ★ JMAP implementation (this project)
│   │   │   ├── src/            # C++ implementation
│   │   │   ├── public/         # Public IDL interfaces
│   │   │   ├── test/           # Tests
│   │   │   └── README.md       # Detailed implementation docs
│   │   └── common/
│   └── mailnews.js             # Default preferences
├── mots.yaml                    # Module ownership (added JMAP entry)
└── README.md                    # This file
```

## Quick Start (Development)

### Prerequisites

To build Thunderbird from source:
```bash
# Install dependencies (Debian/Ubuntu)
sudo apt install mercurial git autoconf2.13 libgtk-3-dev \
  libdbus-glib-1-dev libpulse-dev libasound2-dev \
  python3 python3-pip clang llvm

# Clone Thunderbird (already done)
cd /path/to/thunderbird-jmap

# Bootstrap (creates mach and client.mk)
./mach bootstrap
```

### Building

```bash
# Configure and build
./mach configure
./mach build

# Run Thunderbird with JMAP support
./mach run
```

### Adding a JMAP Account

Currently the UI integration is pending, but you can manually add a JMAP
account by editing your `prefs.js`:

```javascript
// In your Thunderbird profile's prefs.js:
pref("mail.server.server1.type", "jmap");
pref("mail.server.server1.hostname", "mail.stalwart.example.com");
pref("mail.server.server1.port", 443);
pref("mail.server.server1.socketType", 3);  // SSL
pref("mail.server.server1.jmap_url", "https://mail.stalwart.example.com");
pref("mail.server.server1.authMethod", 10);  // OAuth2
pref("mail.account.account1.server", "server1");
```

## Implementation Roadmap

### ✅ Phase 1: Foundation
Protocol architecture, interfaces, registration, and skeleton code.

### 🔨 Phase 2: Core Protocol (Next)
- Async HTTP session discovery
- JSON parsing of JMAP responses
- Mailbox hierarchy sync (Mailbox/changes + Mailbox/get)
- Message list sync (Email/changes + Email/get)
- State token management

### 📋 Phase 3: Message Operations
- Full message download and display
- Compose and send (Email/import + EmailSubmission/set)
- Read/flag/delete operations

### 📋 Phase 4: Advanced Features
- JMAP Push (EventSource)
- Server-side search (Email/query)
- Offline support

### 📋 Phase 5: UI Integration
- Account setup wizard
- Settings panel
- Error handling

## Stalwart Server Setup

For testing with Stalwart:

```yaml
# docker-compose.yml
services:
  stalwart:
    image: stalwartlabs/stalwart:latest
    ports:
      - "443:443"
      - "80:80"
      - "587:587"
    volumes:
      - ./stalwart-data:/opt/stalwart-mail/data
    environment:
      - STALWART_RECOVERY_ADMIN=admin:your-secure-password
```

```bash
docker compose up -d
# Admin UI: https://localhost/
```

Stalwart enables JMAP by default at:
- Session: `https://mail.example.com/.well-known/jmap/session`
- API: `https://mail.example.com/jmap`

## Contributing

1. Make changes in `mailnews/protocols/jmap/`
2. Update `mailnews/mailnews.js` for new preferences
3. Add tests in `mailnews/protocols/jmap/test/`
4. Run `./mach test` to verify

## References

- [RFC 8620 — JMAP Core](https://datatracker.ietf.org/doc/html/rfc8620)
- [RFC 8621 — JMAP Mail](https://datatracker.ietf.org/doc/html/rfc8621)
- [Stalwart Documentation](https://stalw.art/docs/)
- [Thunderbird Source](https://hg.mozilla.org/comm-central/)
- [Thunderbird MXR](https://searchfox.org/comm-central/)
