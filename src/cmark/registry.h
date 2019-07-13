#ifdef REGISTRY_CHECKS
#ifndef CMARK_REGISTRY_H
#define CMARK_REGISTRY_H

#ifdef __cplusplus
extern "C" {
#endif

#include "cmark-gfm.h"
#include "plugin.h"

CMARK_GFM_EXPORT
void cmark_register_plugin(cmark_plugin_init_func reg_fn);

CMARK_GFM_EXPORT
void cmark_release_plugins(void);

CMARK_GFM_EXPORT
cmark_llist *cmark_list_syntax_extensions(cmark_mem *mem);

const cmark_llist *cmark_get_first_syntax_extension(void);

#ifdef __cplusplus
}
#endif

#endif
#endif
