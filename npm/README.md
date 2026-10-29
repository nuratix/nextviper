# NextViper npm launcher

This package launches an **already installed NextViper native toolchain**. It
does not bundle, build, or download the compiler. NextViper remains experimental;
see the [project status](https://github.com/nuratix/nextviper/blob/main/CURRENT_STATUS.md).

Build and install the toolchain using the repository README, then set explicit
paths to the executables:

```sh
export NEXTVIPER_BIN="$HOME/.local/bin/nextviper"
export NEXTVIPER_LSP_BIN="$HOME/.local/bin/nextviper-lsp"
npx nextviper run hello.nv
```

The launcher also searches `NEXTVIPER_HOME/bin` (default `~/.nextviper/bin`) and
PATH. It skips its own npm executable links to prevent recursive invocation.
Missing binaries produce an installation error. Arguments are forwarded without
a shell, and the native process exit code is preserved.

For Node.js:

```js
const nextviper = require('nextviper');
const result = nextviper.eval('print("Hello")');
if (result.status !== 0) throw new Error(result.stderr);
console.log(result.stdout);

const diagnostics = nextviper.check('hello.nv');
console.log(diagnostics.success, diagnostics.diagnostics);
```

`run`, `eval`, and `check` are synchronous. The native runtime must be installed
on the same supported Linux machine. Apache-2.0, developed by Nuratix.
