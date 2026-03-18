/** @file CommandRegistry.h
 * @brief Command registry, dispatch, and help printer. */
#pragma once
#include "ICommand.h"
#include "Commands.h"
#include <memory>
#include <vector>
#include <string>
#include <unordered_map>
#include <iostream>
#include <sstream>
#include <algorithm>

// ════════════════════════════════════════════════════════════════════════════════
// CommandRegistry — komutları kayıt, dispatch ve help
// ════════════════════════════════════════════════════════════════════════════════

class CommandRegistry {
public:
    CommandRegistry() { registerAll(); }

    // İsme göre bul
    ICommand* find(const std::string& name) const {
        auto it = index_.find(name);
        return it != index_.end() ? it->second : nullptr;
    }

    const std::vector<std::unique_ptr<ICommand>>& all() const { return commands_; }

    // Tüm subcommand isimlerini döndür (tab completion için)
    std::vector<std::string> names() const {
        std::vector<std::string> result;
        for (auto& c : commands_) result.push_back(c->name());
        return result;
    }

    // ── Help ──────────────────────────────────────────────────────────────────
    void printHelp(const std::string& version,
                   bool mcpMode = false) const {
        std::cout << version << R"( — PC/SC smart card command-line tool

Usage:
  scardtool [GLOBAL OPTIONS] <command> [options]
  scardtool --mcp [--socket|-K <port>]
  scardtool interactive [-S]
  cat commands.scard | scardtool
  ./commands.scard          (if shebang: #!/usr/bin/env scardtool --script)

GLOBAL OPTIONS
  -S, --session        Load ~/.scardtool_session or ./.scardtool_session
      --save-session   Save used parameters to session file
  -j, --json           JSON output (stdout)
  -v, --verbose        Verbose logging (stderr)
      --dry-run        Parse and validate without touching hardware
  --buildinfo          Show detailed build information
  -V, --version        Show version
  -h, --help           Show this help

)";
        printSection("TERMINAL + INTERACTIVE COMMANDS", CommandMode::All);
        printSection("INTERACTIVE & MCP ONLY  (persistent connection required)",
                     CommandMode::PersistentOnly);

        std::cout <<
R"(
MCP SERVER MODE
  --mcp [-K <port>]    Start as MCP server
                         Default: stdio transport
                         -K <port>: TCP socket transport

INTERACTIVE MODE
  interactive [-S]     Open interactive shell with readline/linenoise
                         Supports: history (up/down), tab completion
                         Type 'help' for commands, 'quit' to exit

SCRIPT EXECUTION
  -f, --script <file>  Execute a .scard script file
  -e, --exec <cmds>    Execute semicolon-separated commands inline
                         e.g. -e "list-readers; connect -r 0; uid -r 0"
  cat file.scard | scardtool   stdin redirect
  ./commands.scard             shebang: #!/usr/bin/env scardtool --script

TODO COMMANDS (use send-apdu in the meantime)
  load-key, auth, read, write

BUSYBOX-STYLE SYMLINKS
  ln -s scardtool list-readers
  ln -s scardtool send-apdu
  # binary adı subcommand olarak otomatik dispatch edilir

SCRIPT FORMAT (.scard)
  #!/usr/bin/env scardtool   # optional shebang
  # comment
  $READER = 0                # variable assignment
  $KEY    = FFFFFFFFFFFF
  load-key -r $READER -k $KEY
  uid -r $READER; ats -r $READER   # inline multi-command

SCRIPT LANGUAGE QUICK REFERENCE
  Variables    : $VAR = value        $VAR = $VAR + 1
  Arithmetic   : + - * / %          (int64_t, overflow-safe)
  Bit ops      : & | ^ ~ << >>
  Literals     : 255  0xFF  0b11111111
  Boolean      : true / false
  Comparison   : == != < > <= >=
  Logic        : $A > 0 && $B < 10   $A == 0 || $B == 0   not $FLAG
  if/elif/else : if COND ... elif COND ... else ... fi
  while loop   : while COND ... done
  for loop     : for $V in a b c ... done
  break/cont.  : break / continue
  output       : echo "msg $VAR"
  exit code    : if ok ... fi     if fail ... fi
  stop-on-err  : set stop-on-error false
  Full docs    : SCRIPT.md

EXIT CODES
  0  Success
  1  General error (no card, no reader)
  2  Invalid arguments
  3  PC/SC error
  4  Card communication error

)";
    }

    void printCommandHelp(const std::string& name) const {
        auto* cmd = find(name);
        if (!cmd) {
            std::cerr << "Unknown command: " << name << "\n";
            return;
        }
        std::cout << cmd->name() << " — " << cmd->description() << "\n\n"
                  << "Usage:\n  " << cmd->usage() << "\n\n";

        auto params = cmd->params();
        if (!params.empty()) {
            std::cout << "Parameters:\n";
            for (auto& p : params) {
                std::cout << "  ";
                if (p.shortName) std::cout << "-" << p.shortName << ", ";
                std::cout << "--" << p.longName;
                if (p.required) std::cout << "  (required)";
                else if (!p.defaultVal.empty())
                    std::cout << "  (default: " << p.defaultVal << ")";
                std::cout << "\n      " << p.description << "\n";
            }
        }
    }

private:
    std::vector<std::unique_ptr<ICommand>> commands_;
    std::unordered_map<std::string, ICommand*> index_;

    void registerAll() {
        add(std::make_unique<scardtool::cmds::ListReadersCmd>());
        add(std::make_unique<scardtool::cmds::ConnectCmd>());
        add(std::make_unique<scardtool::cmds::DisconnectCmd>());
        add(std::make_unique<scardtool::cmds::UidCmd>());
        add(std::make_unique<scardtool::cmds::AtsCmd>());
        add(std::make_unique<scardtool::cmds::CardInfoCmd>());
        add(std::make_unique<scardtool::cmds::SendApduCmd>());
        add(std::make_unique<scardtool::cmds::LoadKeyCmd>());
        add(std::make_unique<scardtool::cmds::AuthCmd>());
        add(std::make_unique<scardtool::cmds::ReadCmd>());
        add(std::make_unique<scardtool::cmds::WriteCmd>());
        add(std::make_unique<scardtool::cmds::ReadSectorCmd>());
        add(std::make_unique<scardtool::cmds::WriteSectorCmd>());
        add(std::make_unique<scardtool::cmds::ReadTrailerCmd>());
        add(std::make_unique<scardtool::cmds::WriteTrailerCmd>());
        add(std::make_unique<scardtool::cmds::MakeAccessCmd>());
        add(std::make_unique<scardtool::cmds::DetectCipherCmd>());
        add(std::make_unique<scardtool::cmds::ExplainSwCmd>());
        add(std::make_unique<scardtool::cmds::ExplainApduCmd>());
        add(std::make_unique<scardtool::cmds::ListMacrosCmd>());
        add(std::make_unique<scardtool::cmds::ExplainMacroCmd>());
        add(std::make_unique<scardtool::cmds::BeginTransactionCmd>());
        add(std::make_unique<scardtool::cmds::EndTransactionCmd>());
    }

    void add(std::unique_ptr<ICommand> cmd) {
        index_[cmd->name()] = cmd.get();
        commands_.push_back(std::move(cmd));
    }

    void printSection(const std::string& title, CommandMode mode) const {
        std::cout << title << "\n";
        for (auto& c : commands_) {
            if (c->mode() == mode) {
                std::cout << "  " << std::left << std::setw(22) << c->name()
                          << " " << firstLine(c->description()) << "\n";
            }
        }
        std::cout << "\n";
    }

    static std::string firstLine(const std::string& s) {
        auto nl = s.find('\n');
        return nl != std::string::npos ? s.substr(0, nl) : s;
    }
};
