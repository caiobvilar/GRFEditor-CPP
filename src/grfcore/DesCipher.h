#pragma once

#include "GrfUtil.h"

namespace grf {

// Fixed-key permutation cipher used by GRF v0x100 (and per-file MIXCRYPT/DES
// bodies). Ported bit-for-bit from GRF Editor's Utilities/DesDecryption.cs.
// It is NOT standard DES; it is a fixed-table permutation network.
namespace descipher {

// Decrypt/encode a file name buffer (v0x100). Buffer is processed in 8-byte
// blocks; mutate in place.
void decodeFileName(byte* buf, std::size_t len);
void encodeFileName(byte* buf, std::size_t len);

// The per-file-body cipher. type=false == MIXCRYPT, type=true == DES(0x14).
void decryptFileData(byte* data, std::size_t len, bool type, int cycle);

void desDecodeBlock(byte block[8]);

} // namespace descipher

} // namespace grf