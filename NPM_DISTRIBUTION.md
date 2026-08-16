# npm / Node.js Distribution Guide

**Package**: [`nextviper`](https://www.npmjs.com/package/nextviper)  
**Version**: `1.0.0`  
**License**: Apache-2.0  
**Maintainer**: Nuratix LLC ([https://nuratix.com](https://nuratix.com))

---

## 1. Quick Execution with `npx`

Run NextViper scripts, REPL, and tools without global manual compilation:

```bash
# Execute NextViper script
npx nextviper run main.nv

# Static type & syntax check with JSON diagnostics
npx nextviper check main.nv --format=json

# Format code in-place
npx nextviper fmt src/

# Launch the interactive REPL
npx nextviper repl

# Initialize package workspace
npx nextviper init my_service
```

---

## 2. Global Installation

```bash
npm install -g nextviper
```

Verify installation:

```bash
nextviper --version
nextviper info
```

---

## 3. Programmatic Node.js API

```typescript
import { run, check, eval as evaluate, getBinaryPath } from "nextviper";

// Run NextViper code inline
const evalResult = evaluate('print("Hello from NextViper via npm!")');
console.log(evalResult.stdout);

// Static type validation
const checkResult = check("src/main.nv");
console.log(checkResult.diagnostics);
```
