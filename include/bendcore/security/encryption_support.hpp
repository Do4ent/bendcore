#pragma once

#include <string>
#include <vector>
#include <memory>
#include <cstdint>

namespace bendcore::security {

// ==============================================================================
// ENCRYPTION SUPPORT - GAP-084
// ==============================================================================
// AES encryption, RSA key management, TLS/HTTPS support, key derivation

enum class EncryptionAlgorithm {
    AES_128,
    AES_192,
    AES_256,
    RSA_2048,
    RSA_4096,
    CHACHA20
};

enum class KeyDerivationFunction {
    PBKDF2,
    SCRYPT,
    ARGON2
};

struct EncryptionKey {
    std::string keyId;
    EncryptionAlgorithm algorithm;
    std::vector<uint8_t> keyData;
    int64_t createdAt;
    int64_t expiresAt;
    bool isActive;
};

struct EncryptedData {
    std::vector<uint8_t> ciphertext;
    std::vector<uint8_t> iv;
    std::vector<uint8_t> salt;
    std::string keyId;
    EncryptionAlgorithm algorithm;
    int64_t encryptedAt;
};

struct TLSConfiguration {
    std::string certificatePath;
    std::string privateKeyPath;
    std::string caCertificatePath;
    bool verifyServerCertificate;
    bool verifyClientCertificate;
    std::vector<std::string> allowedCipherSuites;
};

class EncryptionManager {
public:
    EncryptionManager();
    ~EncryptionManager();

    // Key management
    bool generateEncryptionKey(EncryptionAlgorithm algorithm, std::string& outKeyId);
    bool storeEncryptionKey(const EncryptionKey& key);
    EncryptionKey retrieveEncryptionKey(const std::string& keyId);
    bool deleteEncryptionKey(const std::string& keyId);
    std::vector<EncryptionKey> getAllActiveKeys();

    // Key derivation
    std::vector<uint8_t> deriveKey(
        const std::string& password,
        const std::vector<uint8_t>& salt,
        KeyDerivationFunction kdf,
        int32_t iterations
    );

    // AES encryption/decryption
    bool encryptAES(
        const std::string& plaintext,
        const EncryptionKey& key,
        EncryptedData& outEncrypted
    );
    bool decryptAES(
        const EncryptedData& encrypted,
        std::string& outPlaintext
    );

    // RSA encryption/decryption
    bool encryptRSA(
        const std::string& plaintext,
        const EncryptionKey& publicKey,
        std::vector<uint8_t>& outCiphertext
    );
    bool decryptRSA(
        const std::vector<uint8_t>& ciphertext,
        const EncryptionKey& privateKey,
        std::string& outPlaintext
    );

    // Digital signatures
    bool signData(
        const std::string& data,
        const EncryptionKey& privateKey,
        std::vector<uint8_t>& outSignature
    );
    bool verifySignature(
        const std::string& data,
        const std::vector<uint8_t>& signature,
        const EncryptionKey& publicKey
    );

    // TLS/HTTPS support
    bool initializeTLS(const TLSConfiguration& config);
    bool isTLSEnabled() const;
    TLSConfiguration getTLSConfiguration() const;
    bool validateTLSCertificate(const std::string& certificatePath);

    // Hash functions
    std::string hashSHA256(const std::string& data);
    std::string hashSHA512(const std::string& data);
    std::vector<uint8_t> hashBinary(const std::vector<uint8_t>& data);

    // Random number generation
    std::vector<uint8_t> generateRandomBytes(size_t length);
    std::string generateRandomString(size_t length);

    // Encryption utilities
    bool encryptFile(const std::string& inputPath, const std::string& outputPath, const EncryptionKey& key);
    bool decryptFile(const std::string& inputPath, const std::string& outputPath, const EncryptionKey& key);

    // Encryption key rotation
    bool rotateEncryptionKey(const std::string& keyId);
    std::vector<std::string> getExpiredKeyIds();
    bool reEncryptWithNewKey(const std::string& oldKeyId, const std::string& newKeyId);

    // Secure password management
    bool hashPassword(const std::string& password, std::string& outHash);
    bool verifyPassword(const std::string& password, const std::string& hash);

private:
    std::map<std::string, EncryptionKey> keyStore;
    TLSConfiguration tlsConfig;
    bool tlsEnabled;
};

}  // namespace bendcore::security
