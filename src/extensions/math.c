#include "math.h"
#include <parser.h>
#include <render.h>
#include <node.h>
#include <syntax_extension.h>

#ifdef DEBUG
#include <cmark_trace.h>
#include <Rinternals.h>
#endif

static unsigned UID_math = 0;

typedef enum{
  unknown_math = 0,
  inline_math,
  block_math,
  math_content
} math_type;

static const math_type math_types[] = {
  unknown_math,
  inline_math,
  block_math,
  math_content
};

const char * get_math_type_string(math_type type) {
  switch((uint16_t) type) {
  case unknown_math:
    return "<unknown>";
  case inline_math:
    return "inline math delim";
  case block_math:
    return "block math delim";
  case math_content:
    return "math content";
  }
  return "<invalid math type>";
}

#ifdef REGISTRY_CHECKS
typedef struct ext_reg_s {
  const char * name;
  unsigned uid;
} ext_reg;

static ext_reg compatible_extensions[] = {
  {"math", 0},
};

static const size_t n_compat = sizeof(compatible_extensions) / sizeof(compatible_extensions[0]);
#endif

static cmark_node *match(cmark_syntax_extension *self, cmark_parser *parser,
                         cmark_node *parent, unsigned char character,
                         cmark_inline_parser *inline_parser) {
  cmark_node *res = NULL;
  int left_flanking, right_flanking, punct_before, punct_after, delims;
  char buffer[101];

  if (character != '$')
    return NULL;

#ifdef DEBUG
  Rprintf("Matching possible math...\n");
#endif

  delims = cmark_inline_parser_scan_delimiters(
    inline_parser, sizeof(buffer) - 1, '$',
    &left_flanking,
    &right_flanking, &punct_before, &punct_after);

#ifdef DEBUG
    Rprintf("  found delimiter size %d. lf = %d, rf = %d, pb = %d, pa = %d.\n",
            delims, left_flanking, right_flanking, punct_before, punct_after);
#endif

    memset(buffer, '$', delims);
    buffer[delims] = 0;

    res = cmark_node_new_with_mem(CMARK_NODE_TEXT, parser->mem);
    cmark_node_set_literal(res, buffer);

    cmark_node_set_string_content(res, cmark_chunk_to_cstr(parser->mem, cmark_inline_parser_get_chunk(inline_parser)));
    res->start_line = res->end_line = cmark_inline_parser_get_line(inline_parser);
    res->internal_offset = cmark_inline_parser_get_offset(inline_parser);
    res->start_column = cmark_inline_parser_get_column(inline_parser) - delims;

    // left_flanking or right_flanking is true, but not both.
    if ((left_flanking != right_flanking) && (delims == 1 || delims == 2)) {
#ifdef DEBUG
      Rprintf("  pushing delimiter.\n");
#endif
      cmark_inline_parser_push_delimiter(inline_parser, character, self,
                                         left_flanking, right_flanking, res);
    }

    return res;
}

static delimiter *insert(cmark_syntax_extension *self, cmark_parser *parser,
                         cmark_inline_parser *inline_parser, delimiter *opener,
                         delimiter *closer) {
  cmark_node *math = NULL;
#ifdef DEBUG
  cmark_node *cmath = NULL;
#endif
  cmark_node *tmp = NULL, *next = NULL;
  delimiter *delim = NULL, *tmp_delim = NULL;
  delimiter *res = closer->next;
  unsigned len;
  const char * target = NULL;
  cmark_chunk *parser_input = NULL;
  cmark_chunk iptchk;
  bufsize_t start_pos, end_pos;

  start_pos = opener->inl_text->internal_offset;
  end_pos = closer->inl_text->internal_offset - closer->length;

  math = opener->inl_text;
#ifdef DEBUG
  cmath = closer->inl_text;
#endif

  parser_input = cmark_inline_parser_get_chunk(inline_parser);
  iptchk = cmark_chunk_dup(parser_input, start_pos, end_pos - start_pos);
  target = cmark_chunk_to_cstr(parser->mem, &iptchk);
  cmark_chunk_free(parser->mem, &iptchk);

#ifdef DEBUG
  Rprintf("Inserting delimiter $ of length %d.\n", opener->length);
  Rprintf("  opener: content = \"%s\", literal = \"%s\", offset = %d.\n",
          cmark_node_get_string_content(math),
          cmark_node_get_literal(math),
          math->internal_offset);
  Rprintf("  closer: content = \"%s\", literal = \"%s\", offset = %d.\n",
          cmark_node_get_string_content(cmath),
          cmark_node_get_literal(cmath),
          cmath->internal_offset);

  if (parser_input->len > 0) {
    const char *input = cmark_chunk_to_cstr(parser->mem, parser_input);
    bufsize_t inp_len = parser_input->len;
    Rprintf("  input = \"%s\", length = %d, start = %d, end = %d\n",
            input, inp_len, start_pos, end_pos);
    Rprintf("  target = \"%s\"\n", cmark_chunk_to_cstr(parser->mem, &iptchk));
    }
#endif

  if (opener->inl_text->as.literal.len != closer->inl_text->as.literal.len) {
#ifdef DEBUG
    Rprintf("  mismatched opener/closer. returning.\n");
#endif
    goto done;
  }

  if (!cmark_node_set_type(math, CMARK_NODE_CUSTOM_INLINE)) {
#ifdef DEBUG
    Rprintf("  could not set node type. returning.\n");
#endif
    goto done;
  }

  cmark_node_set_syntax_extension(math, self);

  len = (unsigned)(opener->length);

  if (len > 2)
    len = 0;

  cmark_node_set_user_data(math, (void *)(math_types + len));
  cmark_node_set_on_exit(math, target);

  tmp = cmark_node_next(opener->inl_text);

#ifdef DEBUG
  trace_node_info("  ++ forward: starting with ", tmp, true, true, true, true);
#endif
  while (tmp) {
    if (tmp == closer->inl_text)
      break;
#ifdef DEBUG
    trace_node_info("  ++ ++ next: ", tmp, true, true, true, true);
#endif
    next = cmark_node_next(tmp);
    cmark_node_unlink(tmp);
    cmark_node_free(tmp);
    tmp = next;
  }
#ifdef DEBUG
  trace_node_info("  ++ done. math: ", math, true, false, true, true);
#endif

  math->end_column = closer->inl_text->start_column + closer->inl_text->as.literal.len - 1;
#ifdef DEBUG
  Rprintf("  ++ Columns: start = %d, end = %d\n", math->start_column, math->end_column);
  trace_node_info("  ++ ++ inl_text: ", closer->inl_text, true, true, true, true);
#endif
  cmark_node_free(closer->inl_text);

  delim = closer;
#ifdef DEBUG
  trace_node_info("  ++ backward: starting with ", delim->inl_text, true, true, true, true);
#endif
  while (delim != NULL && delim != opener) {
    tmp_delim = delim->previous;
#ifdef DEBUG
    if (tmp_delim && tmp_delim->inl_text) {
      trace_node_info("  ++ ++ prev: ", tmp_delim->inl_text, true, true, true, true);
    }
#endif
    cmark_inline_parser_remove_delimiter(inline_parser, delim);
    delim = tmp_delim;
  }
#ifdef DEBUG
  if (delim == opener) {
    trace_node_info("  ++ opener: ", delim->inl_text, true, false, true, true);
  }
  Rprintf("  ++ done.\n");
#endif
  cmark_inline_parser_remove_delimiter(inline_parser, opener);

  done:
    return res;
}

static const char *get_type_string(cmark_syntax_extension *extension,
                                   cmark_node *node) {
  return (node->type == CMARK_NODE_CUSTOM_INLINE  &&
          cmark_syntax_extension_get_uid(node->extension) == UID_math) ?
          "math" : "<unknown>";
}

static int can_contain(cmark_syntax_extension *extension, cmark_node *node,
                       cmark_node_type child_type) {
  bool retval = false;

#ifdef DEBUG
  trace_node_info("      ++  can_contain: parent (", node, false, false, true, false);
  Rprintf(", class = %d: %s), child = %d: %s\n",
          * (const math_type *) cmark_node_get_user_data(node),
          get_math_type_string(* (const math_type *) cmark_node_get_user_data(node)),
          child_type, decode_node_type(child_type));
#endif

  if (node->type != CMARK_NODE_CUSTOM_INLINE ||
      node->extension != extension ||
      cmark_syntax_extension_get_uid(extension) != UID_math) {
    retval = false;
  } else {
    retval = CMARK_NODE_TYPE_INLINE_P(child_type);
  }

#ifdef DEBUG
  Rprintf("      ++    result = %s.\n", retval ? "TRUE" : "FALSE");
#endif

  return retval;
}

static void commonmark_render(cmark_syntax_extension *extension,
                              cmark_renderer *renderer, cmark_node *node,
                              cmark_event_type ev_type, int options) {
  bool entering = (ev_type == CMARK_EVENT_ENTER);
  math_type t = * (const math_type *) cmark_node_get_user_data(node);
  const char * delim = (t == block_math) ? "$$" : (t == inline_math) ? "$" : "";
  renderer->out(renderer, node, delim, false, LITERAL);
  if (entering) {
    renderer->out(renderer, node, cmark_node_get_on_exit(node), false, LITERAL);
  }
}

static void latex_render(cmark_syntax_extension *extension,
                         cmark_renderer *renderer, cmark_node *node,
                         cmark_event_type ev_type, int options) {
  bool entering = (ev_type == CMARK_EVENT_ENTER);
  math_type t;
  const char * delim;

#ifdef DEBUG
  if (! extension) {
    Rprintf("latex_render: NULL extension in math.\n");
  }
  if (! node) {
    Rprintf("latex_render: NULL node.\n");
  }
#endif

  if (! cmark_node_get_user_data(node)) {
#ifdef DEBUG
    Rprintf("!! ERROR: Null user_data in node.\n");
#endif
    return;
  }
  t = *(const math_type *) cmark_node_get_user_data(node);
  delim = (t == block_math) ? "$$" : (t == inline_math) ? "$" : "";

#ifdef DEBUG
  Rprintf("++ latex render math delim node: entering = %s.\n", entering ? "TRUE" : "FALSE");
  trace_node_info("++ ++ latex render: ", node, true, true, true, false);
  Rprintf(", math type = %d: %s", t, get_math_type_string(t));
  Rprintf(", literal: len = %d, alloc = %s", node->as.literal.len,
          node->as.literal.alloc ? "TRUE" : "FALSE");

  if (node->as.literal.alloc) {
    Rprintf(", literal content = \"%s\"", node->as.literal.data);
  }
  Rprintf(".\n");
#endif

  renderer->out(renderer, node, delim, false, LITERAL);
  if (entering) {
    renderer->out(renderer, node, cmark_node_get_on_exit(node), false, LITERAL);
  }

#ifdef DEBUG
  Rprintf("++ done rendering math delim mode.\n");
#endif
}

static void html_render(cmark_syntax_extension *extension,
                        cmark_html_renderer *renderer, cmark_node *node,
                        cmark_event_type ev_type, int options) {
  bool entering = (ev_type == CMARK_EVENT_ENTER);
  math_type t;
  const char * delim;

#ifdef DEBUG
  if (! extension) {
    Rprintf("html_render: NULL extension in math.\n");
  }
  if (! node) {
    Rprintf("html_render: NULL node.\n");
  }
#endif

  if (! cmark_node_get_user_data(node)) {
#ifdef DEBUG
    Rprintf("!! ERROR: Null user_data in node.\n");
#endif
    return;
  }
  t = *(const math_type *) cmark_node_get_user_data(node);
  delim = (t == block_math) ? "$$" : (t == inline_math) ? "$" : "";

#ifdef DEBUG
  Rprintf("++ html render math delim node: entering = %s.\n", entering ? "TRUE" : "FALSE");
  trace_node_info("++ ++ html render: ", node, true, true, true, false);
  Rprintf(", math type = %d: %s", t, get_math_type_string(t));
  Rprintf(".\n");
#endif

  cmark_strbuf_puts(renderer->html, delim);
  if (entering) {
    cmark_strbuf_put(renderer->html, node->as.custom.on_exit.data,
                     node->as.custom.on_exit.len);
  }
}

static void plaintext_render(cmark_syntax_extension *extension,
                             cmark_renderer *renderer, cmark_node *node,
                             cmark_event_type ev_type, int options) {
  bool entering = (ev_type == CMARK_EVENT_ENTER);
  renderer->out(renderer, node, "", false, LITERAL);
  if (entering) {
    renderer->out(renderer, node, cmark_node_get_on_exit(node), false, LITERAL);
  }
}

#ifdef REGISTRY_CHECKS
static bool contain_test(unsigned id) {
  for (int i = 0; i < n_compat; i++) {
    if (id == compatible_extensions[i].uid)
      return true;
  }
  return false;
}

static void postreg_callback(cmark_syntax_extension *self) {
  for (int i = 0; i < n_compat; i++) {
    cmark_syntax_extension *ext;
    ext = cmark_find_syntax_extension(compatible_extensions[i].name);
    if (ext != NULL) {
      compatible_extensions[i].uid = cmark_syntax_extension_get_uid(ext);
    }
  }
}
#endif

cmark_syntax_extension *create_math_extension(void) {
  cmark_syntax_extension *ext = cmark_syntax_extension_new("math");
  cmark_llist *special_chars = NULL;

  cmark_syntax_extension_set_get_type_string_func(ext, get_type_string);
  cmark_syntax_extension_set_can_contain_func(ext, can_contain);
  cmark_syntax_extension_set_commonmark_render_func(ext, commonmark_render);
  cmark_syntax_extension_set_latex_render_func(ext, latex_render);
  cmark_syntax_extension_set_html_render_func(ext, html_render);
  cmark_syntax_extension_set_plaintext_render_func(ext, plaintext_render);

#ifdef REGISTRY_CHECKS
  cmark_syntax_extension_set_post_reg_callback_func(ext, postreg_callback);
#endif

  cmark_syntax_extension_set_match_inline_func(ext, match);
  cmark_syntax_extension_set_inline_from_delim_func(ext, insert);

  cmark_mem *mem = cmark_get_default_mem_allocator();
  special_chars = cmark_llist_append(mem, special_chars, (void *)'$');
  cmark_syntax_extension_set_special_inline_chars(ext, special_chars);

  cmark_syntax_extension_set_emphasis(ext, 1);

  UID_math = cmark_syntax_extension_get_uid(ext);

  return ext;
}

