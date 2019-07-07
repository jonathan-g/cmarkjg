#include "superscript.h"
#include <parser.h>
#include <render.h>

static unsigned UID_superscript = 0;

typedef struct ext_reg_s {
  const char * name;
  unsigned uid;
} ext_reg;

static ext_reg compatible_extensions[] = {
  {"superscript", 0},
  {"strikethrough", 0}
};

static const size_t n_compat = sizeof(compatible_extensions) / sizeof(compatible_extensions[0]);

static cmark_node *match(cmark_syntax_extension *self, cmark_parser *parser,
                         cmark_node *parent, unsigned char character,
                         cmark_inline_parser *inline_parser) {
  cmark_node *res = NULL;
  int left_flanking, right_flanking, punct_before, punct_after, delims;
  char buffer[101];

  if (character != '^')
    return NULL;

  delims = cmark_inline_parser_scan_delimiters(
    inline_parser, sizeof(buffer) - 1, '^',
    &left_flanking,
    &right_flanking, &punct_before, &punct_after);

    memset(buffer, '^', delims);
    buffer[delims] = 0;

    res = cmark_node_new_with_mem(CMARK_NODE_TEXT, parser->mem);
    cmark_node_set_literal(res, buffer);
    res->start_line = res->end_line = cmark_inline_parser_get_line(inline_parser);
    res->start_column = cmark_inline_parser_get_column(inline_parser) - delims;

    if ((left_flanking || right_flanking) &&
        (delims == 2 || (!(parser->options & CMARK_OPT_STRIKETHROUGH_DOUBLE_TILDE) && delims == 1))) {
      cmark_inline_parser_push_delimiter(inline_parser, character, left_flanking,
                                         right_flanking, res);
    }

    return res;
}

static delimiter *insert(cmark_syntax_extension *self, cmark_parser *parser,
                         cmark_inline_parser *inline_parser, delimiter *opener,
                         delimiter *closer) {
  cmark_node *superscript;
  cmark_node *tmp, *next;
  delimiter *delim, *tmp_delim;
  delimiter *res = closer->next;

  superscript = opener->inl_text;

  if (opener->inl_text->as.literal.len != closer->inl_text->as.literal.len)
    goto done;

  if (!cmark_node_set_type(superscript, CMARK_NODE_CUSTOM_INLINE))
    goto done;

  cmark_node_set_syntax_extension(superscript, self);

  tmp = cmark_node_next(opener->inl_text);

  while (tmp) {
    if (tmp == closer->inl_text)
      break;
    next = cmark_node_next(tmp);
    cmark_node_append_child(superscript, tmp);
    tmp = next;
  }

  superscript->end_column = closer->inl_text->start_column + closer->inl_text->as.literal.len - 1;
  cmark_node_free(closer->inl_text);

  delim = closer;
  while (delim != NULL && delim != opener) {
    tmp_delim = delim->previous;
    cmark_inline_parser_remove_delimiter(inline_parser, delim);
    delim = tmp_delim;
  }

  cmark_inline_parser_remove_delimiter(inline_parser, opener);

  done:
    return res;
}

static const char *get_type_string(const cmark_syntax_extension *extension,
                                   const cmark_node *node) {
  return (node->type == CMARK_NODE_CUSTOM_INLINE  &&
          cmark_syntax_extension_get_uid(node->extension) == UID_superscript) ?
          "superscript" : "<unknown>";
}

static int can_contain(const cmark_syntax_extension *extension, const cmark_node *node,
                       cmark_node_type child_type) {
  return CMARK_NODE_TYPE_INLINE_P(child_type);
}

static void commonmark_render(cmark_syntax_extension *extension,
                              cmark_renderer *renderer, cmark_node *node,
                              cmark_event_type ev_type, int options) {
  renderer->out(renderer, node, "^", false, LITERAL);
}

static void latex_render(cmark_syntax_extension *extension,
                         cmark_renderer *renderer, cmark_node *node,
                         cmark_event_type ev_type, int options) {
  // requires \usepackage{ulem}
  bool entering = (ev_type == CMARK_EVENT_ENTER);
  if (entering) {
    renderer->out(renderer, node, "\\textsuperscript{", false, LITERAL);
  } else {
    renderer->out(renderer, node, "}", false, LITERAL);
  }
}

static void man_render(cmark_syntax_extension *extension,
                       cmark_renderer *renderer, cmark_node *node,
                       cmark_event_type ev_type, int options) {
  bool entering = (ev_type == CMARK_EVENT_ENTER);
  if (entering) {
    renderer->cr(renderer);
    renderer->out(renderer, node, ".ST \"", false, LITERAL);
  } else {
    renderer->out(renderer, node, "\"", false, LITERAL);
    renderer->cr(renderer);
  }
}

static void html_render(cmark_syntax_extension *extension,
                        cmark_html_renderer *renderer, cmark_node *node,
                        cmark_event_type ev_type, int options) {
  bool entering = (ev_type == CMARK_EVENT_ENTER);
  if (entering) {
    cmark_strbuf_puts(renderer->html, "<sup>");
  } else {
    cmark_strbuf_puts(renderer->html, "</sup>");
  }
}

static void plaintext_render(cmark_syntax_extension *extension,
                             cmark_renderer *renderer, cmark_node *node,
                             cmark_event_type ev_type, int options) {
  renderer->out(renderer, node, "^", false, LITERAL);
}

static bool contain_test(unsigned id) {
  for (int i = 0; i < n_compat; i++) {
    if (id == compatible_extensions[i].uid)
      return true;
  }
  return false;
}

static void postreg_callback(const cmark_syntax_extension *self) {
  for (int i = 0; i < n_compat; i++) {
    cmark_syntax_extension *ext;
    ext = cmark_find_syntax_extension(compatible_extensions[i].name);
    if (ext != NULL) {
      compatible_extensions[i].uid = cmark_syntax_extension_get_uid(ext);
    }
  }
}

cmark_syntax_extension *create_superscript_extension(void) {
  cmark_syntax_extension *ext = cmark_syntax_extension_new("superscript");
  cmark_llist *special_chars = NULL;

  cmark_syntax_extension_set_get_type_string_func(ext, get_type_string);
  cmark_syntax_extension_set_can_contain_func(ext, can_contain);
  cmark_syntax_extension_set_commonmark_render_func(ext, commonmark_render);
  cmark_syntax_extension_set_latex_render_func(ext, latex_render);
  cmark_syntax_extension_set_man_render_func(ext, man_render);
  cmark_syntax_extension_set_html_render_func(ext, html_render);
  cmark_syntax_extension_set_plaintext_render_func(ext, plaintext_render);

  cmark_syntax_extension_set_post_reg_callback_func(ext, postreg_callback);

  cmark_syntax_extension_set_match_inline_func(ext, match);
  cmark_syntax_extension_set_inline_from_delim_func(ext, insert);

  cmark_mem *mem = cmark_get_default_mem_allocator();
  special_chars = cmark_llist_append(mem, special_chars, (void *)'^');
  cmark_syntax_extension_set_special_inline_chars(ext, special_chars);

  cmark_syntax_extension_set_emphasis(ext, 1);

  UID_superscript = cmark_syntax_extension_get_uid(ext);

  return ext;
}
