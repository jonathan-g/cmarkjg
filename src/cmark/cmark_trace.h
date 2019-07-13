#ifndef CMARK_TRACE_H
#define CMARK_TRACE_H

#include "cmark-gfm_export.h"
#include "cmark-gfm_version.h"
#include "cmark-gfm.h"
#include "config.h"

CMARK_GFM_EXPORT void trace_node_info(const char * msg, cmark_node * node,
                                      bool content, bool literal, bool links,
                                      bool cr);
CMARK_GFM_EXPORT const char * decode_node_type(cmark_node_type type);

#endif
