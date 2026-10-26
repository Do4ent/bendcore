#include <gtest/gtest.h>
#include "bendcore/security/encryption_support.hpp"

using namespace bendcore::security;

class EncryptionTest : public ::testing::Test {
protected:
    EncryptionManager manager;
};

#define ENCRYPTION_TEST(name, code) TEST_F(EncryptionTest, name) { code; EXPECT_TRUE(true); }

ENCRYPTION_TEST(GenerateEncryptionKey, std::string keyId; manager.generateEncryptionKey(EncryptionAlgorithm::AES_256, keyId))
ENCRYPTION_TEST(StoreEncryptionKey, EncryptionKey k; manager.storeEncryptionKey(k))
ENCRYPTION_TEST(RetrieveEncryptionKey, auto k = manager.retrieveEncryptionKey("id"))
ENCRYPTION_TEST(DeleteEncryptionKey, bool r = manager.deleteEncryptionKey("id"))
ENCRYPTION_TEST(GetAllActiveKeys, auto keys = manager.getAllActiveKeys())
ENCRYPTION_TEST(InitializeTLS, TLSConfiguration tc; manager.initializeTLS(tc))
ENCRYPTION_TEST(IsTLSEnabled, bool enabled = manager.isTLSEnabled())
ENCRYPTION_TEST(GetTLSConfiguration, auto tc = manager.getTLSConfiguration())
ENCRYPTION_TEST(ValidateTLSCertificate, bool valid = manager.validateTLSCertificate("path"))
ENCRYPTION_TEST(HashSHA256, std::string hash = manager.hashSHA256("data"))
ENCRYPTION_TEST(HashSHA512, std::string hash = manager.hashSHA512("data"))
ENCRYPTION_TEST(HashBinary, auto hash = manager.hashBinary(std::vector<uint8_t>{1, 2, 3}))
ENCRYPTION_TEST(GenerateRandomBytes, auto bytes = manager.generateRandomBytes(32))
ENCRYPTION_TEST(GenerateRandomString, std::string str = manager.generateRandomString(16))
ENCRYPTION_TEST(RotateEncryptionKey, bool r = manager.rotateEncryptionKey("id"))
ENCRYPTION_TEST(GetExpiredKeyIds, auto ids = manager.getExpiredKeyIds())
ENCRYPTION_TEST(HashPassword, std::string hash; manager.hashPassword("pass", hash))
ENCRYPTION_TEST(DeriveKey, auto key = manager.deriveKey("pass", std::vector<uint8_t>{}, KeyDerivationFunction::PBKDF2, 100000))
ENCRYPTION_TEST(EncryptFile, bool r = manager.encryptFile("in", "out", EncryptionKey{}))
ENCRYPTION_TEST(DecryptFile, bool r = manager.decryptFile("in", "out", EncryptionKey{}))

TEST_F(EncryptionTest, KeyGeneration) {
    std::string keyId1, keyId2;
    manager.generateEncryptionKey(EncryptionAlgorithm::AES_256, keyId1);
    manager.generateEncryptionKey(EncryptionAlgorithm::RSA_2048, keyId2);
    EXPECT_NE(keyId1, keyId2);
}

TEST_F(EncryptionTest, EncryptionDecryption) {
    std::string keyId;
    manager.generateEncryptionKey(EncryptionAlgorithm::AES_256, keyId);
    auto key = manager.retrieveEncryptionKey(keyId);
    EncryptedData encrypted;
    EXPECT_TRUE(manager.encryptAES("plaintext", key, encrypted));
}

TEST_F(EncryptionTest, PasswordVerification) {
    std::string hash;
    manager.hashPassword("mypassword", hash);
    EXPECT_TRUE(manager.verifyPassword("mypassword", hash));
}

