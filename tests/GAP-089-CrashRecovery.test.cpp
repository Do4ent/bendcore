// GAP-089: CrashRecovery (35 tests) - Checkpointing, graceful shutdown
#include "bendcore/hardening/crash_recovery.hpp"
#include <cassert>
#include <cstdio>
#include <thread>

using namespace bendcore::production;

int main() {
    int passed = 0, failed = 0;

    #define TEST(name, cond) { \
        if (cond) { passed++; } else { \
            printf("FAIL: %s\n", name); failed++; \
        } \
    }

    // Test 1-5: Checkpoint creation
    {
        CheckpointManager cm;
        uint64_t id1 = cm.createCheckpoint(CheckpointType::FULL, "checkpoint data 1", "test1");

        TEST("T1: Checkpoint created", id1 > 0);
        TEST("T2: Checkpoint count", cm.getCheckpointCount() == 1);

        Checkpoint* cp = cm.getCheckpoint(id1);
        TEST("T3: Checkpoint retrieved", cp != nullptr);
        TEST("T4: Data preserved", cp->data == "checkpoint data 1");
        TEST("T5: Label preserved", cp->label == "test1");
    }

    // Test 6-10: Multiple checkpoints
    {
        CheckpointManager cm;
        uint64_t id1 = cm.createCheckpoint(CheckpointType::FULL, "data1");
        uint64_t id2 = cm.createCheckpoint(CheckpointType::INCREMENTAL, "data2");
        uint64_t id3 = cm.createCheckpoint(CheckpointType::SNAPSHOT, "data3");

        TEST("T6: Three checkpoints", cm.getCheckpointCount() == 3);
        TEST("T7: IDs are unique", id1 != id2 && id2 != id3);

        auto* latest = cm.getLatestCheckpoint();
        TEST("T8: Latest checkpoint correct", latest->checkpoint_id == id3);
        TEST("T9: Latest type correct", latest->type == CheckpointType::SNAPSHOT);

        TEST("T10: All retrievable", cm.getCheckpoint(id1) != nullptr &&
             cm.getCheckpoint(id2) != nullptr && cm.getCheckpoint(id3) != nullptr);
    }

    // Test 11-15: Checkpoint validation
    {
        CheckpointManager cm;
        uint64_t id = cm.createCheckpoint(CheckpointType::FULL, "valid data");

        TEST("T11: Valid checkpoint", cm.validateCheckpoint(id));
        TEST("T12: Invalid ID fails", !cm.validateCheckpoint(999));

        Checkpoint* cp = cm.getCheckpoint(id);
        TEST("T13: Checkpoint is_valid flag", cp->is_valid);
        TEST("T14: Checksum calculated", cp->checksum != 0);

        TEST("T15: Validation works", cm.validateCheckpoint(id));
    }

    // Test 16-20: Checkpoint deletion
    {
        CheckpointManager cm;
        uint64_t id1 = cm.createCheckpoint(CheckpointType::FULL, "data1");
        uint64_t id2 = cm.createCheckpoint(CheckpointType::FULL, "data2");

        TEST("T16: Two checkpoints", cm.getCheckpointCount() == 2);
        bool deleted = cm.deleteCheckpoint(id1);
        TEST("T17: Delete successful", deleted);
        TEST("T18: Count decreased", cm.getCheckpointCount() == 1);
        TEST("T19: Deleted checkpoint gone", cm.getCheckpoint(id1) == nullptr);
        TEST("T20: Other checkpoint remains", cm.getCheckpoint(id2) != nullptr);
    }

    // Test 21-25: Graceful shutdown
    {
        CrashRecoveryManager crm;
        TEST("T21: Not shutting down initially", !crm.isShuttingDown());

        crm.initializeGracefulShutdown();
        TEST("T22: Shutdown initiated", crm.isShuttingDown());

        crm.requestShutdown();
        TEST("T23: Shutdown requested", crm.isShuttingDown());

        crm.waitForShutdown(100);
        TEST("T24: Shutdown wait completes", true);

        TEST("T25: Still shutting down", crm.isShuttingDown());
    }

    // Test 26-30: Recovery attempts
    {
        CrashRecoveryManager crm;
        TEST("T26: Initial recovery attempts 0", crm.getRecoveryAttempts() == 0);

        // Create checkpoint first
        crm.getCheckpointManager().createCheckpoint(CheckpointType::FULL, "recovery data");

        RecoveryLevel level = crm.attemptRecovery();
        TEST("T27: Recovery attempted", crm.getRecoveryAttempts() > 0);
        TEST("T28: Recovery level valid", level != RecoveryLevel::NONE);
        TEST("T29: Last crash time recorded", crm.getLastCrashTime() > 0);

        RecoveryMetadata* metadata = crm.getRecoveryMetadata();
        TEST("T30: Recovery metadata available", metadata != nullptr);
    }

    // Test 31-35: Snapshot management
    {
        SnapshotManager sm;
        TEST("T31: Initial snapshots 0", sm.getSnapshotCount() == 0);

        bool created = sm.createSnapshot("snap1");
        TEST("T32: Snapshot created", created);
        TEST("T33: Snapshot count 1", sm.getSnapshotCount() == 1);

        sm.createSnapshot("snap2");
        sm.createSnapshot("snap3");
        TEST("T34: Three snapshots", sm.getSnapshotCount() == 3);

        sm.clearOldSnapshots(2);
        TEST("T35: Snapshots pruned", sm.getSnapshotCount() == 2);
    }

    printf("\n=== GAP-089: CrashRecovery ===\n");
    printf("Passed: %d/35\n", passed);
    printf("Failed: %d/35\n", failed);
    return (failed == 0) ? 0 : 1;
}
