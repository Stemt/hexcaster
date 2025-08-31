#define NOB_IMPLEMENTATION
#include "third-party/nob.h"
#include "third-party/nhl.h"

#define SRCS\
  "src/main.c"

#define PREVIEW_TGT "./preview.so"
#define SHARED_FLAGS "-shared", "-fPIC"
#define CFLAGS "-ggdb", "-O0"
#define LDFLAGS "-I./third-party", "-lraylib"

bool build_preview(void* ctx){
  
  Nob_Cmd cmd = {0};
  nob_cc(&cmd);
  nob_cc_flags(&cmd);
  nob_cmd_append(&cmd, CFLAGS);
  nob_cmd_append(&cmd, SHARED_FLAGS);
  nob_cc_output(&cmd, PREVIEW_TGT);
  nob_cc_inputs(&cmd, SRCS);
  nob_cmd_append(&cmd, LDFLAGS);

  if(!nob_cmd_run(&cmd)) return false;

  return true;
}

bool preview_needs_rebuild(void* ctx){
  const char* srcs[] = {SRCS};
  return nob_needs_rebuild(PREVIEW_TGT, srcs, NOB_ARRAY_LEN(srcs));
}

int main(int argc, char** argv){
  NOB_GO_REBUILD_URSELF(argc, argv);

  nhl_preload_lib("libraylib.so");
  nhl_build_and_run(
      PREVIEW_TGT, 
      preview_needs_rebuild, 
      build_preview, 
      NULL, argc, argv);

  return 0;
}
