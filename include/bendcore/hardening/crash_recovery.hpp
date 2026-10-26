#pragma once

#include <string>
#include <vector>
#include <map>
#include <cstdint>

namespace bendcore::hardening {

// ==============================================================================
// CRASH RECOVERY - GAP-089
// ==============================================================================
// Crash dump analysis, state recovery, automatic restart, data preservation

enum class CrashSeverity {
    UNRECOVERABLE,
    CRITICAL,
    SEVERE,
    MODERATE
};

struct CrashDump {
    std::string dumpId;
    std::string timestamp;
    std::string crashReason;
    CrashSeverity severity;
    std::string stackTrace;
    std::string registers;
    std::string memory;
    std::string threadInfo;
    std::map<std::string, std::string> metadata;
};

struct SavePoint {
    std::string savePointId;
    int64_t timestamp;
    std::string applicationState;
    std::string lastStableState;
    int32_t sequenceNumber;
    bool isValid;
};

class CrashRecoverySystem {
public:
    CrashRecoverySystem();
    ~CrashRecoverySystem();

    // Crash detection and handling
    bool registerCrashHandler();
    bool handleCrash(const std::string& crashReason);
    bool generateCrashDump(CrashDump& outDump);
    bool saveCrashDump(const CrashDump& dump);

    // Crash dump analysis
    std::vector<CrashDump> getCrashDumps();
    CrashDump getCrashDump(const std::string& dumpId);
    std::string analyzeCrashDump(const CrashDump& dump);
    CrashSeverity determineCrashSeverity(const CrashDump& dump);

    // State management
    bool createSavePoint();
    bool restoreFromSavePoint(const std::string& savePointId);
    std::vector<SavePoint> getAvailableSavePoints();
    SavePoint getLatestSavePoint();

    // Automatic recovery
    bool enableAutoRecovery(bool enable);
    bool isAutoRecoveryEnabled() const;
    bool attemptAutoRecovery();
    bool checkAndRecover();

    // Data preservation
    bool preserveApplicationData();
    bool preserveUserData();
    bool preserveTransactionState();
    std::string getPreservedData();

    // Restart handling
    bool scheduleRestart(int64_t delayMs);
    bool initiateGracefulRestart();
    bool forcedRestart();
    bool isRestartScheduled() const;

    // Recovery verification
    bool verifyRecoverySuccess();
    bool validateRestoredState();
    bool checkDataIntegrity();

    // Recovery statistics
    int32_t getTotalCrashCount() const;
    int32_t getRecoverySuccessCount() const;
    float getRecoverySuccessRate() const;
    std::map<std::string, int32_t> getCrashFrequencyByReason();

    // Watchdog timer
    bool enableWatchdog(int32_t timeoutMs);
    bool kickWatchdog();
    bool disableWatchdog();
    bool isWatchdogActive() const;

    // Health check
    bool performHealthCheck();
    bool isSystemRecovering() const;
    std::string getRecoveryStatus();

private:
    std::vector<CrashDump> crashDumps;
    std::vector<SavePoint> savePoints;
    bool autoRecoveryEnabled;
    bool watchdogEnabled;
    int32_t watchdogTimeoutMs;
};

}  // namespace bendcore::hardening
