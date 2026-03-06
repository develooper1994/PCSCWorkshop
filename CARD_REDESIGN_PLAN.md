# Card Project Redesign - From Scratch

## Current Problem
- **Complexity**: Too many intertwined responsibilities
- **Hierarchy**: Unclear abstraction levels
- **Dependencies**: Circular or unclear dependencies
- **Maintainability**: Hard to reason about the codebase

---

## New Design Strategy: **Topology-First**

### Step 1: Extract Card Model (Topology)
Instead of mixing **business logic** with **card layout**, separate them:

```
CardTopology (Pure Model)
├── CardLayout (1K, 4K structure)
├── Sector (16 or 40)
├── Block (physical layout)
└── AccessControl (pure data)
```

### Step 2: Build Minimal Interfaces
```
Reader (interface only, methods only)
Card (abstract, minimal API)
├── MifareClassic1K
├── MifareClassic4K
└── (future: DESFire, Ultralight)
```

### Step 3: Separate Concerns
```
├── CardModel/       (pure data + layout)
├── CardProtocol/    (auth, access control logic)
├── CardInterface/   (reader, driver)
├── Testing/         (simulator, mocks)
└── Examples/        (usage)
```

---

## New Directory Structure

```
Card/
├── CardModel/
│   ├── CardDataTypes.h         (minimal types)
│   ├── CardTopology.h/cpp      (layout, structure)
│   ├── SectorDefinition.h      (sector = 4/16 blocks)
│   └── BlockDefinition.h       (physical block)
│
├── CardProtocol/
│   ├── AccessControl.h         (permissions, auth)
│   ├── KeyManagement.h         (keys, slots)
│   ├── AccessBits.h            (C1 C2 C3 encoding/decoding)
│   └── AuthenticationState.h   (cache, session)
│
├── CardInterface/
│   ├── Reader.h                (abstract interface)
│   ├── Card.h                  (abstract base)
│   ├── MifareClassic.h         (concrete impl)
│   └── (future: DESFire, Ultralight)
│
├── Testing/
│   ├── MifareCardSimulator.h   (virtual card)
│   └── SimulatedReader.h       (mock reader)
│
└── Examples/
    └── Usage examples
```

---

## Design Principles

### 1. **Single Responsibility**
Each class does ONE thing:
- `CardTopology` = Layout only
- `AccessControl` = Permissions only  
- `MifareClassic` = Orchestration only

### 2. **Dependency Injection**
No hidden dependencies:
```cpp
class MifareClassic {
    MifareClassic(Reader& r, CardTopology topology, AccessControl ac)
        : reader_(r), topology_(topology), access_(ac) {}
};
```

### 3. **Composition Over Inheritance**
Instead of deep hierarchies:
```cpp
class Card {
    CardTopology topology_;
    AccessControl access_;
    KeyManagement keys_;
};
```

### 4. **Testability First**
Every component should be testable in isolation:
```cpp
// Test access bits without reader
AccessBits bits = AccessBits::decode(rawBytes);
assert(bits.isValid());

// Test topology without card
CardTopology topo(false);  // 1K
assert(topo.sectorCount() == 16);

// Test simulator without hardware
MifareCardSimulator sim(false);
sim.readBlock(0);
```

---

## Implementation Plan

### Phase A: Card Model (Pure Data)
1. **CardDataTypes.h** - Minimal types (BYTE, BLOCK, etc.)
2. **CardTopology.h/cpp** - Layout information (1K vs 4K)
3. **SectorDefinition.h** - Sector = {4 or 16 blocks}
4. **BlockDefinition.h** - Block metadata
5. **AccessBits.h/cpp** - Encode/decode C1 C2 C3

### Phase B: Protocol Layer
6. **AccessControl.h** - Permissions matrix
7. **KeyManagement.h** - Key storage, slots, types
8. **AuthenticationState.h** - Auth cache, sessions

### Phase C: Interface Layer
9. **Reader.h** - Abstract interface (methods only)
10. **Card.h** - Abstract base
11. **MifareClassic.h/cpp** - Concrete 1K/4K

### Phase D: Testing & Examples
12. **MifareCardSimulator** - Virtual card
13. **SimulatedReader** - Mock reader
14. **Examples & Tests**

---

## Key Differences from Current Design

| Aspect | Current | New |
|--------|---------|-----|
| **Data Model** | Mixed with logic | Separate, pure |
| **Topology** | Scattered | Centralized |
| **Dependencies** | Circular | Linear, clear |
| **Testability** | Hard (needs hardware) | Easy (unit tests) |
| **Complexity** | High (many files) | Low (focused layers) |
| **Extensibility** | Difficult | Easy (new card types) |

---

## Example: New API

```cpp
// 1. Create minimal topology
CardTopology topology(false);  // 1K card

// 2. Setup access control
AccessControl access;
access.setKeyA(KeyType::A, {0xFF, ...});
access.setKeyB(KeyType::B, {0x00, ...});
access.setPermission(0, KeyType::A, Permission::Read);
access.setPermission(0, KeyType::B, Permission::ReadWrite);

// 3. Create card with dependencies
SimulatedReader reader;
MifareClassic card(reader, topology, access);

// 4. Use it
card.authenticate(0, KeyType::A);
auto data = card.readBlock(1);
```

---

## Benefits

✅ **Clarity** - Each class has one job  
✅ **Testability** - Mock/inject dependencies  
✅ **Maintainability** - Easy to reason about  
✅ **Extensibility** - Add new card types easily  
✅ **Performance** - No unnecessary complexity  
✅ **Documentation** - Self-documenting code  

---

## Migration Strategy

1. **Keep current code** as reference
2. **Build new model** in parallel
3. **Test thoroughly** with simulator
4. **Replace incrementally**
5. **Remove old code** when confident

---

## Timeline

- **Phase A (Model)**: ~2-3 commits
- **Phase B (Protocol)**: ~2-3 commits  
- **Phase C (Interface)**: ~2-3 commits
- **Phase D (Testing)**: ~2-3 commits

Total: ~8-12 focused commits (vs current chaos)

---

## Questions to Answer

1. Keep old code or delete?
2. Start with 1K or both 1K/4K?
3. DESFire support from day 1?
4. Simulation first, then hardware?

---

## Next: Start Phase A (Card Model)

Ready to rebuild the Card project from scratch with proper topology extraction?

