# Forge Language Server (LSP)

Forge ships a Language Server Protocol implementation for editor integration in VS Code and Cursor.

## Features

| Feature | Status |
|---------|--------|
| Syntax highlighting | `.fg` TextMate grammar |
| Diagnostics | `forge --check` on edit |
| Completion | Keywords, types, snippets, stdlib, symbols |
| Hover | Keywords and stdlib functions |
| Document symbols | Outline from `forge --symbols-json` |

## Prerequisites

Build the compiler first — the LSP invokes `forge` for parsing:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

## Install the VS Code / Cursor extension

From the repository root:

```bash
cmake --build build
cd editors/vscode
npm install
npm run build
```

### Option A — F5 (recommended for development)

1. Open the `forge` repository folder in VS Code or Cursor.
2. Run **Tasks: Run Build Task** (or let the pre-launch task build automatically).
3. Press **F5** and choose **Forge Extension**.
4. In the Extension Development Host window, open any `.fg` file.

### Option B — Install from folder

1. Build the extension as above.
2. Run **Extensions: Install Extension from Location…**
3. Select `editors/vscode/`.
4. Reload the window.

Workspace settings in `.vscode/settings.json` point `forge.path` at `build/bin/forge`.

## Run the language server standalone

```bash
cd lsp
npm install
npm run build
npm start
```

The server communicates over stdio (LSP default).

## Configuration

| Setting | Description |
|---------|-------------|
| `forge.path` | Compiler binary (default: `build/bin/forge`) |
| `forge.forgeRoot` | Project root passed as `--forge-root` |
| `forge.libDir` | Library directory (`--lib-dir`) |
| `forge.includePaths` | Extra `-I` paths for module search |

Example `.vscode/settings.json`:

```json
{
  "forge.path": "${workspaceFolder}/build/bin/forge",
  "forge.forgeRoot": "${workspaceFolder}",
  "forge.libDir": "${workspaceFolder}/build/lib",
  "forge.includePaths": ["${workspaceFolder}/examples"]
}
```

## Compiler flags used by LSP

```bash
forge <file> --check          # parse + resolve modules; exit 0 on success
forge <file> --symbols-json # document symbols as JSON
```

## Architecture

```
editors/vscode/     VS Code / Cursor extension (client)
lsp/                TypeScript language server
compiler/           C lexer/parser (invoked via subprocess)
```

Future work: go-to-definition, references, formatting, in-process parser via WASM or `forge-lsp` binary.
