import * as path from "path";
import * as vscode from "vscode";
import {
  LanguageClient,
  LanguageClientOptions,
  ServerOptions,
  Executable,
} from "vscode-languageclient/node";

let client: LanguageClient | undefined;

export function activate(context: vscode.ExtensionContext) {
  const config = vscode.workspace.getConfiguration("nextviper");
  let lspPath = config.get<string>("lspPath", "nextviper-lsp");

  // If nextviper-lsp is not found in PATH, check workspace or common binary locations
  if (lspPath === "nextviper-lsp" || lspPath === "nextviper") {
    const workspaceFolders = vscode.workspace.workspaceFolders;
    if (workspaceFolders && workspaceFolders.length > 0) {
      const localLsp = path.join(workspaceFolders[0].uri.fsPath, "bin", "nextviper-lsp");
      const localCli = path.join(workspaceFolders[0].uri.fsPath, "bin", "nextviper");
      if (require("fs").existsSync(localLsp)) {
        lspPath = localLsp;
      } else if (require("fs").existsSync(localCli)) {
        lspPath = localCli;
      }
    }
  }

  const serverExecutable: Executable = {
    command: lspPath,
    args: lspPath.endsWith("nextviper") ? ["lsp"] : [],
    options: {
      env: process.env,
    },
  };

  const serverOptions: ServerOptions = {
    run: serverExecutable,
    debug: serverExecutable,
  };

  const clientOptions: LanguageClientOptions = {
    documentSelector: [{ scheme: "file", language: "nextviper" }],
    synchronize: {
      fileEvents: vscode.workspace.createFileSystemWatcher("**/*.nv"),
    },
  };

  client = new LanguageClient(
    "nextviper-lsp",
    "NextViper Language Server",
    serverOptions,
    clientOptions
  );

  client.start();

  // Register commands
  context.subscriptions.push(
    vscode.commands.registerCommand("nextviper.restartServer", async () => {
      if (client) {
        await client.stop();
        client.start();
        vscode.window.showInformationMessage("NextViper Language Server restarted.");
      }
    })
  );

  context.subscriptions.push(
    vscode.commands.registerCommand("nextviper.runCurrentFile", () => {
      const activeEditor = vscode.window.activeTextEditor;
      if (activeEditor && activeEditor.document.languageId === "nextviper") {
        const terminal = vscode.window.createTerminal("NextViper");
        terminal.show();
        terminal.sendText(`nextviper run "${activeEditor.document.uri.fsPath}"`);
      }
    })
  );

  context.subscriptions.push(
    vscode.commands.registerCommand("nextviper.checkProject", () => {
      const terminal = vscode.window.createTerminal("NextViper");
      terminal.show();
      terminal.sendText("nextviper check");
    })
  );
}

export function deactivate(): Thenable<void> | undefined {
  if (!client) {
    return undefined;
  }
  return client.stop();
}
