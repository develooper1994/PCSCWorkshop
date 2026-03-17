/** @file tcl_scardtool.cpp
 * @brief scardtool Tcl 8.6 extension.
 *
 * Derleme: -DBUILD_TCL_EXTENSION=ON
 * Kullanım (Tcl tarafında):
 * @code
 *   package require scardtool
 *
 *   # Basit kullanım
 *   scardtool::set_reader 0
 *   set uid [scardtool::exec "uid"]
 *   puts "UID: $uid"
 *
 *   # Ham APDU
 *   set resp [scardtool::send_apdu "FF CA 00 00 00"]
 *
 *   # Byte manipülasyon — Tcl'in binary komutu ile
 *   set data [binary format "H*" "01020304"]
 *   set apdu [binary format "H2H2H2H2ca*" 00 A4 04 00 [string length $data] $data]
 *   # APDU'yu hex'e çevir
 *   binary scan $apdu H* apdu_hex
 *   set resp [scardtool::send_apdu $apdu_hex]
 *
 *   # Reader listesi
 *   foreach r [scardtool::list_readers] { puts "Reader: $r" }
 *
 *   # Script çalıştır
 *   scardtool::run_file "provision.scard"
 *
 *   # binary scan ile yanıt parse et
 *   binary scan [binary format "H*" $resp] "H2H2" sw1 sw2
 *   puts "SW1=$sw1 SW2=$sw2"
 * @endcode
 *
 * @par Context yönetimi
 * @code
 *   set ctx [scardtool::new]
 *   scardtool::ctx_set_reader $ctx 0
 *   set uid [scardtool::ctx_exec $ctx "uid"]
 *   scardtool::ctx_close $ctx
 * @endcode
 */
#ifdef SCARDTOOL_BUILD_TCL_EXT

#include <tcl.h>
#include "ScardToolLib.h"
#include <cstring>
#include <cstdlib>

// ─────────────────────────────────────────────────────────────────────────────
// Context handle yönetimi
// ─────────────────────────────────────────────────────────────────────────────

/** @brief scardtool_ctx*'ı Tcl'in opaque pointer mekanizmasıyla yönet. */
static void ctx_delete_proc(ClientData cd) {
    scardtool_destroy(static_cast<scardtool_ctx*>(cd));
}

static scardtool_ctx* get_ctx_from_handle(Tcl_Interp* interp, const char* handle) {
    Tcl_CmdInfo info;
    if (!Tcl_GetCommandInfo(interp, handle, &info)) {
        Tcl_SetResult(interp, (char*)"invalid scardtool handle", TCL_STATIC);
        return nullptr;
    }
    return static_cast<scardtool_ctx*>(info.objClientData2);
}

// ─────────────────────────────────────────────────────────────────────────────
// Module-level default context
// ─────────────────────────────────────────────────────────────────────────────

static scardtool_ctx* g_default_ctx = nullptr;

static scardtool_ctx* ensure_default_ctx() {
    if (!g_default_ctx) g_default_ctx = scardtool_create();
    return g_default_ctx;
}

// ─────────────────────────────────────────────────────────────────────────────
// Tcl komutları
// ─────────────────────────────────────────────────────────────────────────────

static int cmd_set_reader(ClientData, Tcl_Interp* interp,
                           int objc, Tcl_Obj* const objv[]) {
    if (objc != 2) {
        Tcl_WrongNumArgs(interp, 1, objv, "reader");
        return TCL_ERROR;
    }
    auto* ctx = ensure_default_ctx();
    int idx;
    if (Tcl_GetIntFromObj(interp, objv[1], &idx) == TCL_OK) {
        scardtool_set_reader(ctx, idx);
    } else {
        Tcl_ResetResult(interp);
        scardtool_set_reader_name(ctx, Tcl_GetString(objv[1]));
    }
    return TCL_OK;
}

static int cmd_set_json(ClientData, Tcl_Interp* interp,
                         int objc, Tcl_Obj* const objv[]) {
    if (objc != 2) { Tcl_WrongNumArgs(interp, 1, objv, "bool"); return TCL_ERROR; }
    int v; Tcl_GetBooleanFromObj(interp, objv[1], &v);
    scardtool_set_json(ensure_default_ctx(), v);
    return TCL_OK;
}

static int cmd_set_verbose(ClientData, Tcl_Interp* interp,
                            int objc, Tcl_Obj* const objv[]) {
    if (objc != 2) { Tcl_WrongNumArgs(interp, 1, objv, "bool"); return TCL_ERROR; }
    int v; Tcl_GetBooleanFromObj(interp, objv[1], &v);
    scardtool_set_verbose(ensure_default_ctx(), v);
    return TCL_OK;
}

static int cmd_exec(ClientData, Tcl_Interp* interp,
                     int objc, Tcl_Obj* const objv[]) {
    if (objc < 2) { Tcl_WrongNumArgs(interp, 1, objv, "cmdline"); return TCL_ERROR; }
    auto* ctx = ensure_default_ctx();

    // Tüm argümanları birleştir
    std::string cmdline;
    for (int i = 1; i < objc; ++i) {
        if (i > 1) cmdline += ' ';
        cmdline += Tcl_GetString(objv[i]);
    }

    char buf[8192];
    int ec = scardtool_exec(ctx, cmdline.c_str(), buf, sizeof(buf));

    if (ec != 0) {
        Tcl_SetResult(interp, (char*)scardtool_last_error(ctx), TCL_VOLATILE);
        return TCL_ERROR;
    }
    // Sonucu döndür
    Tcl_SetResult(interp, buf, TCL_VOLATILE);
    return TCL_OK;
}

static int cmd_send_apdu(ClientData, Tcl_Interp* interp,
                          int objc, Tcl_Obj* const objv[]) {
    if (objc != 2) { Tcl_WrongNumArgs(interp, 1, objv, "apdu_hex"); return TCL_ERROR; }
    auto* ctx = ensure_default_ctx();
    char buf[4096];
    int ec = scardtool_send_apdu(ctx, Tcl_GetString(objv[1]), buf, sizeof(buf));
    if (ec != 0) {
        Tcl_SetResult(interp, (char*)scardtool_last_error(ctx), TCL_VOLATILE);
        return TCL_ERROR;
    }
    Tcl_SetResult(interp, buf, TCL_VOLATILE);
    return TCL_OK;
}

static int cmd_list_readers(ClientData, Tcl_Interp* interp,
                              int, Tcl_Obj* const[]) {
    auto* ctx = ensure_default_ctx();
    char names[16][256];
    int n = scardtool_list_readers(ctx, names, 16);
    if (n < 0) {
        Tcl_SetResult(interp, (char*)scardtool_last_error(ctx), TCL_VOLATILE);
        return TCL_ERROR;
    }
    Tcl_Obj* list = Tcl_NewListObj(0, nullptr);
    for (int i = 0; i < n; ++i)
        Tcl_ListObjAppendElement(interp, list, Tcl_NewStringObj(names[i], -1));
    Tcl_SetObjResult(interp, list);
    return TCL_OK;
}

static int cmd_run_file(ClientData, Tcl_Interp* interp,
                         int objc, Tcl_Obj* const objv[]) {
    if (objc != 2) { Tcl_WrongNumArgs(interp, 1, objv, "path"); return TCL_ERROR; }
    int ec = scardtool_run_file(ensure_default_ctx(), Tcl_GetString(objv[1]));
    Tcl_SetObjResult(interp, Tcl_NewIntObj(ec));
    return TCL_OK;
}

static int cmd_run_string(ClientData, Tcl_Interp* interp,
                           int objc, Tcl_Obj* const objv[]) {
    if (objc != 2) { Tcl_WrongNumArgs(interp, 1, objv, "script"); return TCL_ERROR; }
    int ec = scardtool_run_string(ensure_default_ctx(), Tcl_GetString(objv[1]));
    Tcl_SetObjResult(interp, Tcl_NewIntObj(ec));
    return TCL_OK;
}

static int cmd_version(ClientData, Tcl_Interp* interp, int, Tcl_Obj* const[]) {
    Tcl_SetResult(interp, (char*)scardtool_version(), TCL_STATIC);
    return TCL_OK;
}

// ─────────────────────────────────────────────────────────────────────────────
// Scardtool_Init — extension entry point (Tcl load convention)
// ─────────────────────────────────────────────────────────────────────────────

struct CmdDef { const char* name; Tcl_ObjCmdProc* proc; };

static const CmdDef kCmds[] = {
    {"scardtool::set_reader",   cmd_set_reader},
    {"scardtool::set_json",     cmd_set_json},
    {"scardtool::set_verbose",  cmd_set_verbose},
    {"scardtool::exec",         cmd_exec},
    {"scardtool::send_apdu",    cmd_send_apdu},
    {"scardtool::list_readers", cmd_list_readers},
    {"scardtool::run_file",     cmd_run_file},
    {"scardtool::run_string",   cmd_run_string},
    {"scardtool::version",      cmd_version},
    {nullptr, nullptr}
};

extern "C" SCARDTOOL_API int Scardtool_Init(Tcl_Interp* interp) {
    if (Tcl_InitStubs(interp, "8.6", 0) == nullptr) return TCL_ERROR;

    for (const auto* c = kCmds; c->name; ++c)
        Tcl_CreateObjCommand(interp, c->name, c->proc, nullptr, nullptr);

    // package ifadesi için
    Tcl_PkgProvide(interp, "scardtool", "1.0");
    return TCL_OK;
}

/** @brief Safe interpreter için (Tcl_SafeInit standardı). */
extern "C" SCARDTOOL_API int Scardtool_SafeInit(Tcl_Interp* interp) {
    // Safe mod: sadece dry-run'a izin ver
    scardtool_set_dry_run(ensure_default_ctx(), 1);
    return Scardtool_Init(interp);
}

#endif  // SCARDTOOL_BUILD_TCL_EXT
