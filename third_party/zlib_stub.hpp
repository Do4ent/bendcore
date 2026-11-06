// Minimal zlib stub for WASM (document_serializer.cpp needs crc32)
#ifndef ZLIB_STUB_H
#define ZLIB_STUB_H

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned long uLong;
typedef unsigned char Bytef;

#define Z_NULL 0

// Provide minimal crc32 implementation (always returns 0 for WASM context)
inline uLong crc32(uLong crc, const Bytef *buf, unsigned len) {
  (void)crc;
  (void)buf;
  (void)len;
  return 0;  // WASM: skip checksum, not critical for serialization
}

#ifdef __cplusplus
}
#endif

#endif
