# CAD-CORE-006: Transaction Manager

**Status:** Specification  
**Depends on:** CAD-CORE-005 (CommandEngine)  
**Target:** Atomicity for batch operations  

---

## Overview

TransactionManager ensures that batch operations are **atomic**: either all commands succeed, or all are rolled back. Prevents partial state corruption.

```cpp
TransactionManager tm(document);

tm.begin_transaction();
tm.execute_command(cmd1);  // CreatePart
tm.execute_command(cmd2);  // CreateInstance
tm.execute_command(cmd3);  // SetParent

bool success = tm.commit();  // All succeed or all fail
```

---

## API

```cpp
class TransactionManager {
public:
    explicit TransactionManager(CadDocument& doc);

    void begin_transaction();
    [[nodiscard]] CommandResult execute_command(const Command& cmd);
    [[nodiscard]] bool commit();
    void rollback();

    bool in_transaction() const;
    int transaction_size() const;

private:
    CadDocument& document_;
    std::vector<Command> transaction_commands_;
    std::vector<CommandResult> transaction_results_;
    DocumentSnapshot snapshot_;
    bool is_active_ = false;
};
```

---

## Implementation Strategy

1. **Snapshot on begin_transaction()** — capture current document state
2. **Queue commands** — store without modifying document
3. **commit()** — execute all or rollback to snapshot
4. **Automatic rollback** — on any command failure

---

## Deliverables

- `include/bendcore/commands/transaction_manager.hpp`
- `src/commands/transaction_manager.cpp`
- `tests/commands/test_transaction_manager.cpp` (5+ test cases)

**Estimated Size:** ~600 lines (header + impl + tests)

---

## Success Criteria

✓ All commands in transaction succeed or all fail  
✓ Document revision unchanged on rollback  
✓ No partial state on failure  
✓ Nested transaction support (optional)
