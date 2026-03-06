# 🚀 QUICK START — Include Optimization

**Status**: 📋 PLANNED  
**Difficulty**: 🟢 EASY (Phase 1) / 🟡 MEDIUM (Phase 2)  
**Time**: 75 minutes total

---

## TL;DR

**Problem**: CardMemoryLayout.h 5 kez include ediliyor → 3500 lines/header → slow compile

**Solution**: Forward declare + move includes to .cpp

**Benefit**: 20-40% build time faster

---

## DO THIS (Phase 1 — 30 min, SAFE)

### Step 1: CardIO.h
```cpp
// Lines 4-7, REPLACE with:
class CardInterface;
struct TrailerConfig;
class Reader;

#include <vector>
```
Then add to **CardIO.cpp** (top after `#include "CardIO.h"`):
```cpp
#include "CardInterface.h"
#include "CardModel/TrailerConfig.h"
```

### Step 2: AccessControl.h
```cpp
// Lines 4-6, REPLACE with:
#include "../CardModel/CardDataTypes.h"
#include <array>
#include <string>

class CardMemoryLayout;
struct MifareBlock;
```
Then add to **AccessControl.cpp**:
```cpp
#include "../CardModel/CardMemoryLayout.h"
#include "../CardModel/BlockDefinition.h"
#include "../CardModel/TrailerConfig.h"
```

### Step 3: KeyManagement.h
```cpp
// Line 5, REPLACE with:
#include "../CardModel/CardDataTypes.h"
#include <map>
#include <vector>
#include <stdexcept>

class CardMemoryLayout;
```
Then add to **KeyManagement.cpp**:
```cpp
#include "../CardModel/CardMemoryLayout.h"
```

### Step 4: AuthenticationState.h
```cpp
// Line 5, REPLACE with:
#include "../CardModel/CardDataTypes.h"
#include <map>
#include <chrono>
#include <optional>

class CardMemoryLayout;
```
Then add to **AuthenticationState.cpp**:
```cpp
#include "../CardModel/CardMemoryLayout.h"
```

### Test
```bash
run_build  # Should compile clean
Tests.exe 1  # Should show 10/10 PASS
```

**If PASS**: Commit!
```bash
git commit -m "optimize: forward declare CardMemoryLayout in protocol headers"
```

---

## OPTIONAL (Phase 2 — 45 min, MEDIUM RISK)

**Only if Phase 1 passed!**

### CardInterface.h
```cpp
// Lines 4-9, REPLACE with:
#include <memory>

class CardMemoryLayout;
class CardLayoutTopology;
class AccessControl;
class KeyManagement;
class AuthenticationState;
```

Then update **CardInterface.cpp** to have all includes:
```cpp
#include "CardInterface.h"
#include "CardModel/CardMemoryLayout.h"
#include "CardModel/CardTopology.h"
#include "CardProtocol/AccessControl.h"
#include "CardProtocol/KeyManagement.h"
#include "CardProtocol/AuthenticationState.h"
```

### Test
```bash
run_build  # Should compile clean
Tests.exe 1  # Should show 10/10 PASS
```

**If PASS**: Commit!
```bash
git commit -m "optimize: forward declare CardInterface components (pimpl-style)"
```

---

## Files to Edit

```
✏️ CardIO.h + CardIO.cpp
✏️ AccessControl.h + AccessControl.cpp
✏️ KeyManagement.h + KeyManagement.cpp
✏️ AuthenticationState.h + AuthenticationState.cpp

(Optional:)
✏️ CardInterface.h + CardInterface.cpp
```

---

## Verify Success

**Compilation**: Zero errors/warnings
**Tests**: `Tests.exe 1` → 10/10 PASS
**Build time**: (subjective, but cache benefits incremental builds)

---

## Rollback (if needed)
```bash
git checkout Card/
```

---

## Full Details

See `Card/OPTIMIZATION_PLAN.md` for detailed step-by-step guide.

