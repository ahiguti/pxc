
#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"
#include "ppport.h"

#include <string>
#include <map>
#include <dlfcn.h>

static void *loaded_lib = 0; /* leaks */

static int
load_dl_internal(pTHX_ const char *filename, const char *symbol)
{
	int r = 0;
	if (loaded_lib != 0) {
		croak("PXC is already loaded");
	}
	std::string fn(filename);
	void *const pdl = dlopen(fn.c_str(), RTLD_NOW);
	if (pdl == 0) {
		const char *s = dlerror();
		std::string str = "dlopen(" + fn + ") failed ("
		  + std::string(s ? s : "") + ")";
		croak(str.c_str()); // FIXME: leaks
	}
	dlerror(); /* clear */
	void *const sym = dlsym(pdl, symbol);
	if (sym == 0) {
		const char *s = dlerror();
		std::string str = "dlsym(" + std::string(symbol)
		  + ") failed (" + std::string(s ? s : "") + ")";
		dlclose(pdl);
		croak(str.c_str()); // FIXME: leaks
	} else {
		typedef int (*func_type)(void);
		func_type f = (func_type)sym;
		r = f();
		loaded_lib = pdl; /* FIXME: leaks */
	}
	return r;
}

static void
terminate_dl_internal()
{
	if (loaded_lib != 0) {
		void *const sym = dlsym(loaded_lib, "detach_perl");
		if (sym != 0) {
			typedef void (*func_type)(void);
			func_type f = (func_type)sym;
			f();
		} else {
			fprintf(stderr, "detach_perl() not found\n");
		}
		#if 0
		if (dlclose(loaded_lib) != 0) {
			fprintf(stderr, "dlclose() failed\n");
		} else {
			#if 0
			fprintf(stderr, "dlclose() done\n");
			#endif
		}
		loaded_lib = 0;
		#endif
	}
}

MODULE = PXC::Loader		PACKAGE = PXC::Loader		

int
load_dl(const char *filename, const char *symbol)
CODE:
	RETVAL = load_dl_internal(aTHX_ filename, symbol);
OUTPUT:
	RETVAL

void
terminate_dl()
CODE:
	terminate_dl_internal();

