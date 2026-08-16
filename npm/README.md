<p align="center">
  <a href="https://nextviper.nuratix.com">
    <img src="https://nextviper.nuratix.com/logo-black.png" alt="NextViper Logo" width="180" height="auto" />
  </a>
</p>

<p align="center">
  <a href="https://nuratix.com">
    <img src="https://www.nuratix.com/nuratix-logo-light.png" alt="By Nuratix LLC" width="140" height="auto" />
  </a>
</p>

# nextviper

Official npm distribution and CLI runner for the **NextViper** programming language, developed and maintained by **Nuratix LLC** ([https://nuratix.com](https://nuratix.com)).

NextViper is a modern, compiled, high-performance programming language designed for columnar Data, N-dimensional Tensors, AI/ML, and Khronos Vulkan GPU acceleration.

- **Website**: [https://nextviper.nuratix.com](https://nextviper.nuratix.com)
- **Documentation**: [https://nextviper.nuratix.com/docs](https://nextviper.nuratix.com/docs)
- **6-Day Master Course**: [https://nextviper.nuratix.com/learn](https://nextviper.nuratix.com/learn)
- **GitHub**: [https://github.com/nuratix/nextviper](https://github.com/nuratix/nextviper)

---

## 🚀 Quick Usage with `npx`

Execute NextViper programs directly without manual setup:

```bash
# Run a script
npx nextviper run script.nv

# Check syntax & types with JSON output
npx nextviper check script.nv --format=json

# Format code in-place
npx nextviper fmt src/

# Launch the interactive REPL
npx nextviper repl

# Initialize a package workspace
npx nextviper init my_project
```

---

## 📦 Global Installation

```bash
npm install -g nextviper
```

Then run `nextviper` directly:

```bash
nextviper --version
nextviper info
```

---

## 💻 Programmatic Node.js API

```typescript
import { run, check, eval as evaluate } from "nextviper";

// Evaluate code inline
const evalResult = evaluate('print("Hello from NextViper via npm!")');
console.log(evalResult.stdout);

// Type check source file
const checkResult = check("src/main.nv");
if (!checkResult.success) {
  console.error("Diagnostics:", checkResult.diagnostics);
}

// Run a NextViper script
const runResult = run("src/main.nv");
console.log(runResult.stdout);
```

---

## 📜 License

Apache-2.0 © 2026 Nuratix LLC.
