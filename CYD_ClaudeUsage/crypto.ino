// ═══════════════════════════════════════════════════════
//  crypto.ino — simple XOR obfuscation for token storage
//  PIN removed: token is stored with light obfuscation
//  using the device MAC as key. Not cryptographically
//  strong, but prevents casual plaintext reads of NVS.
// ═══════════════════════════════════════════════════════

// Obfuscates token bytes using MAC as a repeating key
bool encryptToken(const char* plaintext, EncryptedBlob& blob) {
    size_t ptLen = strlen(plaintext);
    if (ptLen > sizeof(blob.ciphertext)) return false;

    esp_read_mac(blob.salt, ESP_MAC_WIFI_STA);
    blob.len = ptLen;

    for (size_t i = 0; i < ptLen; i++) {
        blob.ciphertext[i] = plaintext[i] ^ blob.salt[i % sizeof(blob.salt)];
    }
    return true;
}

bool decryptToken(const EncryptedBlob& blob, char* plainOut, size_t plainMaxLen) {
    if (blob.len > plainMaxLen - 1) return false;

    for (size_t i = 0; i < blob.len; i++) {
        plainOut[i] = blob.ciphertext[i] ^ blob.salt[i % sizeof(blob.salt)];
    }
    plainOut[blob.len] = '\0';
    return true;
}
