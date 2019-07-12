#include "math.h"
#include <parser.h>
#include <render.h>
#include <node.h>
#include <syntax_extension.h>
#include <cmark_trace.h>
#include <Rinternals.h>

#if 0
#define CHECK_REGISTRY
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

#ifdef CHECK_REGISTRY
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

  Rprintf("Matching possible math...\n");

  delims = cmark_inline_parser_scan_delimiters(
    inline_parser, sizeof(buffer) - 1, '$',
    &left_flanking,
    &right_flanking, &punct_before, &punct_after);

    Rprintf("  found delimiter size %d. lf = %d, rf = %d, pb = %d, pa = %d.\n",
            delims, left_flanking, right_flanking, punct_before, punct_after);

    memset(buffer, '$', delims);
    buffer[delims] = 0;

    res = cmark_node_new_with_mem(CMARK_NODE_TEXT, parser->mem);
    cmark_node_set_literal(res, buffer);

    res->start_line = res->end_line = cmark_inline_parser_get_line(inline_parser);
    res->start_column = cmark_inline_parser_get_column(inline_parser) - delims;

    // left_flanking or right_flanking is true, but not both.
    if ((left_flanking != right_flanking) && (delims == 1 || delims == 2)) {
      Rprintf("  pushing delimiter.\n");
      cmark_inline_parser_push_delimiter(inline_parser, character, self,
                                         left_flanking, right_flanking, res);
    }

    return res;
}

static void transform_text_to_math(cmark_syntax_extension * ext, cmark_node * node) {
  if (! node || ! ext) {
    Rprintf("!! ERROR: NULL pointer pased to transform_text_to_math.\n");
    return;
  }
  if (cmark_node_get_type(node) != CMARK_NODE_TEXT) {
    trace_node_info("!! ERROR transforming text to math: ", node, true, false, true, true);
    return;
  }
  if (cmark_syntax_extension_get_uid(ext) != UID_math) {
    Rprintf("!! ERROR: transform_text_to_math called without math extension.\n");
    return;
  }

  trace_node_info("    ++ -- Transforming text to math: ", node, true, true, true, true);

  if (node->as.literal.alloc && node->as.literal.len > 0) {
    Rprintf("             Converting literal \"%s\"[%d] to string content.\n",
            node->as.literal.data, node->as.literal.len);
    cmark_node_set_string_content(node, (char *) node->as.literal.data);
  } else {
    const char * literal = cmark_node_get_literal(node);
    if (literal) {
      unsigned lit_len = strlen(literal);
      Rprintf("             Getting literal \"%s\"[%d] and converting to string content.\n",
              literal, lit_len);
      cmark_node_set_string_content(node, literal);
    }
    else {
      Rprintf("             No literal data.\n");
    }
  }
  cmark_node_set_type(node, CMARK_NODE_CUSTOM_INLINE);
  cmark_node_set_syntax_extension(node, ext);
  cmark_node_set_user_data(node, (void *)(math_types + 3));

  trace_node_info("    ++ -- -- post-transform: ", node, true, true, true, true);
}

// Remove the given node and insert its children, if any, where the node was.
// Return:
//   A pointer to the next node in the sequence, after removing this node and
//   inserting its children.
//
//   Thus, if there are children, return a pointer to the first child.
//   If there are no children, return a pointer to the next node after this one.
//
static cmark_node * raise_children(cmark_node * node) {
  cmark_node *prev, *next, *parent, *first_child, *last_child, *ptr;

  prev = node->prev;
  next = node->next;
  parent = node->parent;
  first_child = node->first_child;
  last_child = node->last_child;

  if (!parent) {
    return next;
  }

  cmark_node_unlink(node);

  if (! first_child || ! last_child) {
    // no children to raise.
    cmark_node_free(node);
    return next;
  }

  node->first_child = NULL;
  node->last_child = NULL;

  ptr = first_child;
  while(ptr) {
    ptr->parent = parent;
    ptr = ptr->next;
  }

  if (prev) {
    prev->next = first_child;
    first_child->prev = prev;
  } else {
    parent->first_child = first_child;
  }

  if (next) {
    next->prev = last_child;
    last_child->next = next;
  } else {
    parent->last_child = last_child;
  }

  cmark_node_free(node);

  return first_child;
}

static delimiter *insert(cmark_syntax_extension *self, cmark_parser *parser,
                         cmark_inline_parser *inline_parser, delimiter *opener,
                         delimiter *closer) {
  cmark_node *math;
  cmark_node *tmp, *next;
  delimiter *delim, *tmp_delim;
  delimiter *res = closer->next;
  unsigned len;
  const char * emph_str = NULL;

  math = opener->inl_text;

  Rprintf("Inserting delimiter $ of length %d.\n", opener->length);

  if (opener->inl_text->as.literal.len != closer->inl_text->as.literal.len) {
    Rprintf("  mismatched opener/closer. returning.\n");
    goto done;
  }

  if (!cmark_node_set_type(math, CMARK_NODE_CUSTOM_INLINE)) {
    Rprintf("  could not set node type. returning.\n");
    goto done;
  }

  cmark_node_set_syntax_extension(math, self);

  len = (unsigned)(opener->length);

  if (len > 2)
    len = 0;

  cmark_node_set_user_data(math, (void *)(math_types + len));

  tmp = cmark_node_next(opener->inl_text);

  trace_node_info("  ++ forward: starting with ", tmp, true, true, true, true);
  while (tmp) {
    if (tmp == closer->inl_text)
      break;
    trace_node_info("  ++ ++ next: ", tmp, true, true, true, true);
    switch ((uint16_t)cmark_node_get_type(tmp)) {
    case CMARK_NODE_EMPH:
    case CMARK_NODE_STRONG:
      emph_str = cmark_node_get_user_data(tmp);
      trace_node_info("  ++  -- emph/strong node: ", tmp, true, true, false, false);
      if (emph_str) {
        Rprintf(", user data = \"%s\".\n", emph_str);
      } else {
        Rprintf(", no user data.\n");
      }
    case CMARK_NODE_FOOTNOTE_DEFINITION:
    case CMARK_NODE_FOOTNOTE_REFERENCE:
      tmp = raise_children(tmp);
      continue;
    }
    next = cmark_node_next(tmp);
    cmark_node_append_child(math, tmp);
    tmp = next;
  }
  trace_node_info("  ++ done. math: ", math, true, false, true, true);
  Rprintf("  ++  consolidating children...\n");
  cmark_consolidate_text_nodes(math);

  tmp = cmark_node_first_child(math);
  trace_node_info("  ++ transforming text nodes starting with ", tmp, true, true, true, true);
  while(tmp) {
    trace_node_info("  ++ ++ next: ", tmp, true, true, true, true);
    if (cmark_node_get_type(tmp) == CMARK_NODE_TEXT) {
      transform_text_to_math(self, tmp);
      tmp = cmark_node_next(tmp);
    }
  }
  trace_node_info("  ++ done. math: ", math, true, false, true, true);

  math->end_column = closer->inl_text->start_column + closer->inl_text->as.literal.len - 1;
  Rprintf("  ++ Columns: start = %d, end = %d\n", math->start_column, math->end_column);
  trace_node_info("  ++ ++ inl_text: ", closer->inl_text, true, true, true, true);
  cmark_node_free(closer->inl_text);

  delim = closer;
  trace_node_info("  ++ backward: starting with ", delim->inl_text, true, true, true, true);
  while (delim != NULL && delim != opener) {
    tmp_delim = delim->previous;
    if (tmp_delim && tmp_delim->inl_text) {
      trace_node_info("  ++ ++ prev: ", tmp_delim->inl_text, true, true, true, true);
    }
    cmark_inline_parser_remove_delimiter(inline_parser, delim);
    delim = tmp_delim;
  }
  if (delim == opener) {
    trace_node_info("  ++ opener: ", delim->inl_text, true, false, true, true);
  }
  Rprintf("  ++ done.\n");

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

  trace_node_info("      ++  can_contain: parent (", node, false, false, true, false);
  Rprintf(", class = %d: %s), child = %d: %s\n",
          * (const math_type *) cmark_node_get_user_data(node),
          get_math_type_string(* (const math_type *) cmark_node_get_user_data(node)),
          child_type, decode_node_type(child_type));


  if (node->type != CMARK_NODE_CUSTOM_INLINE ||
      node->extension != extension ||
      cmark_syntax_extension_get_uid(extension) != UID_math) {
    retval = false;
  } else {
    retval = CMARK_NODE_TYPE_INLINE_P(child_type);
  }

  Rprintf("      ++    result = %s.\n", retval ? "TRUE" : "FALSE");
  return retval;
}

static void commonmark_render(cmark_syntax_extension *extension,
                              cmark_renderer *renderer, cmark_node *node,
                              cmark_event_type ev_type, int options) {
  math_type t = * (const math_type *) cmark_node_get_user_data(node);
  const char * delim = (t == block_math) ? "$$" : (t == inline_math) ? "$" : "";
  renderer->out(renderer, node, delim, false, LITERAL);
}

static void latex_render(cmark_syntax_extension *extension,
                         cmark_renderer *renderer, cmark_node *node,
                         cmark_event_type ev_type, int options) {
  bool entering = (ev_type == CMARK_EVENT_ENTER);
  math_type t;
  const char * delim;

  if (! extension) {
    Rprintf("latex_render: NULL extension in math.\n");
  }
  if (! node) {
    Rprintf("latex_render: NULL node.\n");
  }

  if (! cmark_node_get_user_data(node)) {
    Rprintf("!! ERROR: Null user_data in node.\n");
    return;
  }
  t = *(const math_type *) cmark_node_get_user_data(node);
  delim = (t == block_math) ? "$$" : (t == inline_math) ? "$" : "";

  Rprintf("++ latex render math delim node: entering = %s.\n", entering ? "TRUE" : "FALSE");
  trace_node_info("++ ++ latex render: ", node, true, true, true, false);
  Rprintf(", math type = %d: %s", t, get_math_type_string(t));
  Rprintf(", literal: len = %d, alloc = %s", node->as.literal.len,
          node->as.literal.alloc ? "TRUE" : "FALSE");

  if (node->as.literal.alloc) {
    Rprintf(", literal content = \"%s\"", node->as.literal.data);
  }
  Rprintf(".\n");

  if (t == math_content) {
    if (entering) {
      const char * literal = NULL;
      if (node->as.literal.alloc && node->as.literal.len > 0) {
        literal = (const char *)node->as.literal.data;
      }
      const char * content = cmark_node_get_string_content(node);
      if (literal == NULL) {
        Rprintf("++    No literal content.\n");
      }
      if (content == NULL) {
        Rprintf("++    No string content.\n");
      }
      if (literal) {
        Rprintf("++    Rendering literal, length %d, allocated = %s.\n",
                node->as.literal.len, node->as.literal.alloc ? "TRUE" : "FALSE");
        renderer->out(renderer, node, literal, false, LITERAL);
      } else if (content) {
        Rprintf("++    Rendering string content, length %d (of %d).\n",
                node->content.size, node->content.asize);
        renderer->out(renderer, node, content, false, LITERAL);
      }
    } else {
      trace_node_info("++    Not rendering except at opening: ", node, true, true, true, true);
    }
  } else if (entering) {
    renderer->out(renderer, node, delim, false, LITERAL);
  } else {
    renderer->out(renderer, node, delim, false, LITERAL);
  }
  Rprintf("++ done rendering math delim mode.\n");
}

static void html_render(cmark_syntax_extension *extension,
                        cmark_html_renderer *renderer, cmark_node *node,
                        cmark_event_type ev_type, int options) {
  bool entering = (ev_type == CMARK_EVENT_ENTER);
  math_type t;
  const char * delim;

  if (! extension) {
    Rprintf("html_render: NULL extension in math.\n");
  }
  if (! node) {
    Rprintf("html_render: NULL node.\n");
  }

  if (! cmark_node_get_user_data(node)) {
    Rprintf("!! ERROR: Null user_data in node.\n");
    return;
  }
  t = *(const math_type *) cmark_node_get_user_data(node);
  delim = (t == block_math) ? "$$" : (t == inline_math) ? "$" : "";

  Rprintf("++ html render math delim node: entering = %s.\n", entering ? "TRUE" : "FALSE");
  trace_node_info("++ ++ html render: ", node, true, true, true, false);
  Rprintf(", math type = %d: %s", t, get_math_type_string(t));
  Rprintf(", literal: len = %d, alloc = %s", node->as.literal.len,
          node->as.literal.alloc ? "TRUE" : "FALSE");

  if (node->as.literal.alloc) {
    Rprintf(", literal content = \"%s\"", node->as.literal.data);
  }
  Rprintf(".\n");

  if (t == math_content) {
    if (entering) {
      const char * literal = NULL;
      if (node->as.literal.alloc && node->as.literal.len > 0) {
        literal = (const char *)node->as.literal.data;
      }
      const char * content = cmark_node_get_string_content(node);
      if (literal == NULL) {
        Rprintf("++    No literal content.\n");
      }
      if (content == NULL) {
        Rprintf("++    No string content.\n");
      }
      if (literal) {
        Rprintf("++    Rendering literal, length %d, allocated = %s.\n",
                node->as.literal.len, node->as.literal.alloc ? "TRUE" : "FALSE");
        cmark_strbuf_puts(renderer->html, literal);
      } else if (content) {
        Rprintf("++    Rendering string content, length %d (of %d).\n",
                node->content.size, node->content.asize);
        cmark_strbuf_puts(renderer->html, content);      }
    } else {
      trace_node_info("++    Not rendering except at opening: ", node, true, true, true, true);
    }
  } else if (entering) {
    cmark_strbuf_puts(renderer->html, delim);
  } else {
    cmark_strbuf_puts(renderer->html, delim);
  }
}

static void plaintext_render(cmark_syntax_extension *extension,
                             cmark_renderer *renderer, cmark_node *node,
                             cmark_event_type ev_type, int options) {
  renderer->out(renderer, node, "", false, LITERAL);
}

#ifdef CHECK_REGISTRY
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

#ifdef CHECK_REGISTRY
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

