#include "cmark-jg-core-extensions.h"
#include "autolink.h"
#include "strikethrough.h"
#include "superscript.h"
#include "subscript.h"
#include "table.h"
#include "tagfilter.h"
#include "registry.h"
#include "plugin.h"

static int core_extensions_registration(cmark_plugin *plugin) {
  cmark_plugin_register_syntax_extension(plugin, create_table_extension());
  cmark_plugin_register_syntax_extension(plugin,
                                         create_strikethrough_extension());
  cmark_plugin_register_syntax_extension(plugin, create_autolink_extension());
  cmark_plugin_register_syntax_extension(plugin, create_tagfilter_extension());

  cmark_plugin_register_syntax_extension(plugin,
                                         create_superscript_extension());
  cmark_plugin_register_syntax_extension(plugin,
                                         create_subscript_extension());

  const cmark_llist * ptr = cmark_get_first_syntax_extension();
  while(ptr) {
    cmark_syntax_extension *ext = ptr->data;
    if (ext) {
      cmark_post_reg_callback_func f =
        cmark_syntax_extension_get_post_reg_callback_func(ext);
      if (f) {
        f(ext);
      }
    }
    ptr = ptr->next;
  }

  return 1;
}

void cmark_gfm_core_extensions_ensure_registered(void) {
  static int registered = 0;

  if (!registered) {
    cmark_register_plugin(core_extensions_registration);
    registered = 1;
  }
}
