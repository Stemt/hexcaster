#ifndef NHL_H_
#define NHL_H_

#include <stdbool.h>

#ifdef NHL_APP_INTERFACE

void* nhl_init(int argc, char** argv);
bool nhl_update(void*);
void nhl_pre_reload(void*);
void nhl_post_reload(void*);
void nhl_destroy(void*);

#ifdef NHL_APP_MAIN
int main(int argc, char** argv){
  void* ctx = nhl_init(argc, argv);
  while(nhl_update(ctx));
  nhl_destroy(ctx);
  return 0;
}

#endif
#else // NHL_APP_INTERFACE

#include <stddef.h>
#include <time.h>
#include <stdint.h>

#ifdef __linux__
#include <dlfcn.h>
#elif _WIN64
#include <Libloaderapi.h>
#endif

#ifndef NHL_MIN_NSEC_SINCE_BUILD
#define NHL_MIN_NSEC_SINCE_BUILD 10000000
#endif // NHL_MIN_NSEC_SINCE_BUILD

#define NHL_SEC_TO_NSEC(sec) ((sec)*1000000000)

typedef void* (*NHL_InitFunc)(int argc, char** argv);
typedef void (*NHL_PreReloadFunc)(void*);
typedef void (*NHL_PostReloadFunc)(void*);
typedef bool (*NHL_UpdateFunc)(void*);
typedef void (*NHL_DestroyFunc)(void*);

typedef bool(*NHL_BuildFunc)(void* ctx);
typedef bool(*NHL_NeedsRebuildFunc)(void* ctx);

typedef void* NHL_AppHandle;
typedef void* NHL_AppContext;

static NHL_InitFunc 		    nhl_init 	        = NULL;
static NHL_PreReloadFunc 	  nhl_pre_reload 	  = NULL;
static NHL_PostReloadFunc 	nhl_post_reload 	= NULL;
static NHL_UpdateFunc 		  nhl_update 	      = NULL;
static NHL_DestroyFunc		  nhl_destroy 	    = NULL;

void* nhl_load_symbol(NHL_AppHandle* hl_app, const char* name);
bool nhl_reload_app(NHL_AppHandle* hl_app, const char* app_path);
bool nhl_preload_lib(const char* dlpath);
bool nhl_check_reload_required(const char* app_path);
bool nhl_build_and_run(
    const char* app_path, 
    NHL_NeedsRebuildFunc needs_rebuild_func, 
    NHL_BuildFunc build_func, 
    void* ctx,
    int argc,
    char** argv
);
bool nhl_run(const char* app_path, int argc, char** argv);

#ifdef NOB_IMPLEMENTATION
void* nhl_load_symbol(NHL_AppHandle* app, const char* name){
	nob_log(NOB_INFO, "nhl_load_symbol: %s",name);
	void* symbol = dlsym(*app,name);
	if(!symbol){
		nob_log(NOB_ERROR, "nhl_load_symbol: %s",dlerror());
	}
	return symbol;
}

bool nhl_reload_app(NHL_AppHandle* app, const char* app_path){
	if(*app != NULL){
    nob_log(NOB_INFO, "nhl_reload_app: unloading old app");
		dlclose(*app);
	}
	
  nob_log(NOB_INFO, "nhl_reload_app: attempting to load: '%s'", app_path);
	*app = dlopen(app_path, RTLD_NOW);
	if(*app == NULL){
		nob_log(NOB_INFO, "nhl_reload_app: could not load %s: %s", app_path, dlerror());
		return false;
	}
	nhl_init = nhl_load_symbol(app, "nhl_init");
	if(nhl_init == NULL) return false;
	nhl_pre_reload = nhl_load_symbol(app, "nhl_pre_reload");
	if(nhl_pre_reload == NULL) return false;
	nhl_post_reload = nhl_load_symbol(app, "nhl_post_reload");
	if(nhl_post_reload == NULL) return false;
	nhl_update = nhl_load_symbol(app, "nhl_update");
	if(nhl_update == NULL) return false;
	nhl_destroy = nhl_load_symbol(app, "nhl_destroy");
	if(nhl_destroy == NULL) return false;
	return true;
}

bool nhl_preload_lib(const char* dlpath){
  void* lib = dlopen(dlpath, RTLD_NOW);
  if(lib == NULL){
    nob_log(NOB_ERROR, "nhl_preload_lib: couldn't preload library: '%s'", dlpath);
    return false;
  }
  nob_log(NOB_ERROR, "nhl_preload_lib: library succesfully loaded: '%s'", dlpath);
  return true;
}

bool nhl_get_last_modified_time(struct timespec* ts, const char* path){
	struct stat current_attributes;
	int ret = stat(path,&current_attributes);
  if(ret != 0){
    return false;
  }
  *ts = current_attributes.st_mtim;
  return true;
}

bool nhl_check_reload_required(const char* app_path){
  static struct timespec prev_mod_time = {0};
  struct timespec current_mod_time = {0};
  if(!nhl_get_last_modified_time(&current_mod_time, app_path)){
    nob_log(NOB_ERROR,"couldn't get last modified time, ignoring...");
    return false;
  }
  int64_t dt = NHL_SEC_TO_NSEC(current_mod_time.tv_sec-prev_mod_time.tv_sec) + 
    (current_mod_time.tv_nsec-prev_mod_time.tv_nsec);

	if(dt > 0){
		nob_log(NOB_INFO, "Change detected on '%s': waiting for reload...",app_path);

    // wait until the compiler is done writing to the file
    struct timespec current_time = {0};
    do{
      clock_gettime(CLOCK_REALTIME, &current_time);
      if(!nhl_get_last_modified_time(&prev_mod_time, app_path)){
        nob_log(NOB_ERROR,"couldn't get last modified time, ignoring...");
        return false; 
      }
      dt = NHL_SEC_TO_NSEC(current_time.tv_sec-prev_mod_time.tv_sec) + 
        (current_time.tv_nsec-prev_mod_time.tv_nsec);
    }while (dt <= NHL_MIN_NSEC_SINCE_BUILD);
		prev_mod_time = current_mod_time;
		return true;
	}
	return false;
}

bool nhl_build_and_run(
    const char* app_path, 
    NHL_NeedsRebuildFunc needs_rebuild_func, 
    NHL_BuildFunc build_func, 
    void* build_ctx,
    int argc,
    char** argv
){
  nob_log(NOB_INFO, "nhl_build_and_run");
  NHL_AppHandle app = NULL;
  if(build_func != NULL && !build_func(build_ctx)){
	  nob_log(NOB_INFO, "nhl_run: failed to build: '%s'", app_path);
  }
	nhl_check_reload_required(app_path); // init prev_tv_sec
	
  if(!nhl_reload_app(&app, app_path)) { 
	  nob_log(NOB_INFO, "nhl_run: failed to load: '%s'", app_path);
    return false; 
  }
	nob_log(NOB_INFO, "nhl_run: succesfully loaded '%s'", app_path);

	NHL_AppContext app_context = nhl_init(argc, argv);
	while(nhl_update(app_context)){
    if(build_func != NULL && needs_rebuild_func != NULL && needs_rebuild_func(build_ctx)){
      if(!build_func(build_ctx)){
        nob_log(NOB_ERROR, "nhl_run: rebuild failed for: '%s'", app_path);
      }
    }

		if(nhl_check_reload_required(app_path)){
			nhl_pre_reload(app_context);
			nob_log(NOB_INFO, "nhl_run: attempting to reload '%s'", app_path);
			if(!nhl_reload_app(&app, app_path)){
        nob_log(NOB_ERROR, "nhl_run: reload failed");
				return false;
			}
			nob_log(NOB_INFO, "nhl_run: succesfully reloaded\n");
			nhl_post_reload(app_context);
		}
	}
  nhl_destroy(app_context);
  return true;
}

bool nhl_run(const char* app_path, int argc, char** argv){
  return nhl_build_and_run(app_path, NULL, NULL, NULL, argc, argv);
}

#endif // NOB_IMPLEMENTATION
#endif // NHL_APPLICATION
#endif // NHL_H_
