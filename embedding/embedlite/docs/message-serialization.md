# EmbedLite message serialization contract

## Child-to-chrome delivery

- Each view owns one chrome-side `ChromeMessageSender` whose parent is the
  global message manager.
- A child send enters that hierarchy once. Per-view listeners run first, then
  global listeners, and the EmbedLite JSON callback runs last.
- Each registered listener and the EmbedLite callback receive a message at
  most once. Registering the same message-manager listener twice keeps Gecko's
  existing deduplication behavior.
- The structured-clone payload is deserialized once. All message-manager
  listeners and the EmbedLite adapter observe that same JS value.

## Asynchronous delivery

- An asynchronous send returns before any same-process chrome or EmbedLite
  listener is called.
- Asynchronous messages use Gecko's same-process queue and preserve FIFO
  order. An asynchronous message sent by a listener is queued after work that
  was already pending and does not re-enter the active delivery.
- A synchronous send is an explicit reentrancy point. It flushes older queued
  same-process messages before delivering the synchronous message, matching
  Gecko's in-process message-manager behavior.

## Synchronous replies

- Reply order is per-view message-manager listeners, global message-manager
  listeners, then EmbedLite replies in the order returned by the embedder.
- Each EmbedLite reply string is parsed as JSON and written into a new
  non-transferable `StructuredCloneData` holder.
- An invalid JSON reply is omitted without changing replies that were already
  produced.

## JSON compatibility

- Gecko message-manager listeners always receive the original structured-clone
  value, including values that JSON cannot represent.
- The EmbedLite callback remains a JSON API and uses the legacy
  `JSON.stringify` projection for non-transferable values.
- If stringification fails or does not produce JSON, the EmbedLite callback is
  skipped. Gecko delivery still succeeds.
- If the clone contains an actual transferable, the EmbedLite callback is
  skipped. Gecko retains and consumes the transferable through the normal
  single message-manager receive; the value is never cloned or rewritten for
  JSON conversion.
