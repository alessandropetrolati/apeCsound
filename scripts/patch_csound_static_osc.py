#!/usr/bin/env python3
"""
Patch (idempotente) dei sorgenti Csound clonati, condiviso dagli script
build_csound_static_{macos,ios,linux}.sh e build_csound_static_windows.ps1:

- make_plugin() costruisce librerie STATICHE quando CMake riceve
  -DAPECSOUND_STATIC_PLUGINS=ON (altrimenti invariato);
- il plugin "osc" (OSC.c, richiede liblo) include apecsound_osc_register.c
  e rinomina i suoi entry point (csound_opcode_init ->
  apecsound_osc_opcode_init) per non collidere con il core;
- apecsound_register_osc_opcodes(CSOUND*) registra gli opcode OSC in
  un'istanza appena creata (chiamata da apeCsound, PluginProcessor.cpp).

Uso: python3 patch_csound_static_osc.py <cartella sorgenti Csound>
"""
import sys, pathlib
src = pathlib.Path(sys.argv[1])

util = src / "cmake" / "cmake-utilities.cmake"
t = util.read_text()
if "APECSOUND_STATIC_PLUGINS" not in t:
    old = "    if(APPLE)\n        add_library(${libname} SHARED ${srcs})"
    if old not in t:
        sys.exit("patch: make_plugin() in cmake-utilities.cmake not recognised")
    t = t.replace(old, "    if(APECSOUND_STATIC_PLUGINS)\n        add_library(${libname} STATIC ${srcs})\n    elseif(APPLE)\n        add_library(${libname} SHARED ${srcs})", 1)
    util.write_text(t)

ops = src / "Opcodes" / "CMakeLists.txt"
t = ops.read_text()
if "apecsound_osc_register.c" not in t:
    old = 'make_plugin(osc "OSC.c;osc_blob.c")'
    if old not in t:
        sys.exit("patch: make_plugin(osc ...) in Opcodes/CMakeLists.txt not recognised")
    t = t.replace(old, 'make_plugin(osc "OSC.c;osc_blob.c;apecsound_osc_register.c")\n'
                       '    target_compile_definitions(osc PRIVATE csound_opcode_init=apecsound_osc_opcode_init csoundModuleInfo=apecsound_osc_module_info)', 1)
    ops.write_text(t)

(src / "Opcodes" / "apecsound_osc_register.c").write_text("""/* csdl.h, come ogni plugin: csoundCore.h da solo e' vietato fuori dal core. */
#include "csdl.h"

int64_t apecsound_osc_opcode_init(CSOUND *csound, OENTRY **ep);

/* apeCsound: registra gli opcode OSC (OSCinit, OSClisten, OSCsend...) in
   un'istanza Csound appena creata, al posto del caricamento del plugin
   dinamico. Chiamata da apeCsound dopo csoundCreate(). */
int32_t apecsound_register_osc_opcodes(CSOUND *csound)
{
    OENTRY *ep = NULL;
    int64_t bytes = apecsound_osc_opcode_init(csound, &ep);

    if (ep == NULL || bytes <= 0)
        return -1;

    /* Dai plugin le funzioni del core si chiamano tramite la struct CSOUND. */
    return csound->AppendOpcodes(csound, ep, (int32_t) (bytes / (int64_t) sizeof(OENTRY)));
}
""")
