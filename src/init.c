#include <Rinternals.h>
#include <R_ext/Rdynload.h>
#include <R_ext/Visibility.h>
#include "extensions/cmark-gfm-core-extensions.h"

extern SEXP R_list_extensions_jg();
extern SEXP R_render_markdown(SEXP, SEXP, SEXP, SEXP, SEXP, SEXP, SEXP, SEXP, SEXP);

static const R_CallMethodDef CallEntries[] = {
  {"R_list_extensions_jg", (DL_FUNC) &R_list_extensions_jg, 0},
  {"R_render_markdown", (DL_FUNC) &R_render_markdown, 9},
  {NULL, NULL, 0}
};

attribute_visible void R_init_cmarkjg(DllInfo *dll){
  cmark_gfm_core_extensions_ensure_registered();
  R_registerRoutines(dll, NULL, CallEntries, NULL, NULL);
  R_useDynamicSymbols(dll, FALSE);
  R_forceSymbols(dll, TRUE);
}
