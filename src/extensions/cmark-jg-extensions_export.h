
#ifndef CMARK_JG_EXTENSIONS_EXPORT_H
#define CMARK_JG_EXTENSIONS_EXPORT_H

#ifdef CMARK_JG_EXTENSIONS_STATIC_DEFINE
#  define CMARK_JG_EXTENSIONS_EXPORT
#  define CMARK_JG_EXTENSIONS_NO_EXPORT
#else
#  ifndef CMARK_JG_EXTENSIONS_EXPORT
#    ifdef libcmark_JG_extensions_EXPORTS
        /* We are building this library */
#      define CMARK_JG_EXTENSIONS_EXPORT __attribute__((visibility("default")))
#    else
        /* We are using this library */
#      define CMARK_JG_EXTENSIONS_EXPORT __attribute__((visibility("default")))
#    endif
#  endif

#  ifndef CMARK_JG_EXTENSIONS_NO_EXPORT
#    define CMARK_JG_EXTENSIONS_NO_EXPORT __attribute__((visibility("hidden")))
#  endif
#endif

#ifndef CMARK_JG_EXTENSIONS_DEPRECATED
#  define CMARK_JG_EXTENSIONS_DEPRECATED __attribute__ ((__deprecated__))
#endif

#ifndef CMARK_JG_EXTENSIONS_DEPRECATED_EXPORT
#  define CMARK_JG_EXTENSIONS_DEPRECATED_EXPORT CMARK_JG_EXTENSIONS_EXPORT CMARK_JG_EXTENSIONS_DEPRECATED
#endif

#ifndef CMARK_JG_EXTENSIONS_DEPRECATED_NO_EXPORT
#  define CMARK_JG_EXTENSIONS_DEPRECATED_NO_EXPORT CMARK_JG_EXTENSIONS_NO_EXPORT CMARK_JG_EXTENSIONS_DEPRECATED
#endif

#if 0 /* DEFINE_NO_DEPRECATED */
#  ifndef CMARK_JG_EXTENSIONS_NO_DEPRECATED
#    define CMARK_JG_EXTENSIONS_NO_DEPRECATED
#  endif
#endif

#endif /* CMARK_JG_EXTENSIONS_EXPORT_H */
