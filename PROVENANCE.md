# Provenance

`bbb-sacn` is an independent, self-contained implementation of sACN / ANSI E1.31 packet encoding and parsing.

It does not include source code from:

- ETCLabs/sACN
- Open Lighting Architecture (OLA)
- other third-party sACN implementations

Protocol field names, packet offsets, constants, and interoperability behavior are used only to implement the protocol. This repository must not redistribute copyrighted standard text or third-party implementation source unless the relevant license obligations are explicitly added.

Do not call this a formal clean-room implementation. The accurate claim is: independent implementation with no third-party sACN source included.
