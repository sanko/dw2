/*

 Package: dyncall
 Library: test
 File: test/dynload_plain/dynload_plain.c
 Description: 
 License:

   Copyright (c) 2017-2018 Tassilo Philipp <tphilipp@potion-studios.com>

   Permission to use, copy, modify, and distribute this software for any
   purpose with or without fee is hereby granted, provided that the above
   copyright notice and this permission notice appear in all copies.

   THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
   WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
   MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
   ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
   WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
   ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
   OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

*/


#include "../../dynload/dynload.h"
#include "../common/platformInit.h"

#include <string.h>
#include <sys/stat.h>
#if defined(DC_WINDOWS)
#  include <io.h>
#  define F_OK 0
#else
#  include <unistd.h>
#endif


int strlen_utf8(const char *s)
{
  int i=0, j=0;
  while(s[i])
    j += ((s[i++] & 0xc0) != 0x80);
  return j;
}


int main(int argc, char* argv[])
{
  int r = 0, i;
  void* p;
  DLLib* pLib;
  DLSyms* pSyms;
  const char* path = NULL;
  /* hacky/lazy list of some clib paths per platform - more/others, like version-suffixed ones */
  /* can be specified in Makefile; this avoids trying to write portable directory traversal stuff */
  const char* clibs[] = {
#if defined(DEF_C_DYLIB)
    DEF_C_DYLIB,
#endif
    "/lib/libc.so",
    "/lib32/libc.so",
    "/lib64/libc.so",
    "/usr/lib/libc.so",
    "/usr/lib/system/libsystem_c.dylib", /* macos */
    "/usr/lib/libc.dylib",
    "/boot/system/lib/libroot.so", /* Haiku */
    "\\ReactOS\\system32\\msvcrt.dll", /* ReactOS */
    "C:\\ReactOS\\system32\\msvcrt.dll",
    "\\Windows\\system32\\msvcrt.dll", /* Windows */
    "C:\\Windows\\system32\\msvcrt.dll"
  };

  /* use first matching path of hacky hardcoded list, above */
  for(i=0; i<(sizeof(clibs)/sizeof(const char*)); ++i) {
    if(access(clibs[i], F_OK) != -1) {
      path = clibs[i];
      break;
    }
  }

  if(path) {
    printf("using clib to test at: %s\n", path);
    ++r;

    /* dl*Library tests */
    /* ---------------- */
    pLib = dlLoadLibrary(path); /* check if we can load a lib */
    if(pLib) {
      char queriedPath[200]; /* enough for our test paths */
      int bs;

      printf("pLib handle: %p\n", pLib);
      ++r;

      p = dlFindSymbol(pLib, "printf"); /* check if we can lookup a symbol */
      printf("printf at: %p\n", p);
      r += (p != NULL);

      bs = dlGetLibraryPath(pLib, queriedPath, 200);
      if(bs && bs <= 200) {
        struct stat st0, st1; /* to check if same file */
        int b, bs_;
        printf("path of lib looked up via handle: %s\n", queriedPath);
        b = (stat(path, &st0) != -1) && (stat(queriedPath, &st1) != -1);
        printf("lib (inode:%d) and looked up lib (inode:%d) are same: %d\n", b?st0.st_ino:-1, b?st1.st_ino:-1, b && (st0.st_ino == st1.st_ino)); //@@@ on windows, inode numbers returned here are always 0
        r += b && (st0.st_ino == st1.st_ino); /* compare if same lib using inode */
/*@@@ check if resolved path is absolute*/

        /* check correct bufsize retval */
        b = (bs == strlen(queriedPath) + 1);
        printf("looked up path's needed buffer size (%d) computed correctly 1/2: %d\n", bs, b);
        r += b;

        /* check perfect fitting bufsize */
        queriedPath[0] = 0;
        bs_ = dlGetLibraryPath(pLib, queriedPath, bs);
        b = (bs == bs_ && bs_ == strlen(queriedPath) + 1);
        printf("looked up path's needed buffer size (%d) computed correctly 2/2: %d\n", bs_, b);
        r += b;

        /* check if dlGetLibraryPath returns size required if bufsize too small */
        queriedPath[0] = 0;
        bs_ = dlGetLibraryPath(pLib, queriedPath, 1);  /* tiny max buffer size */
        b = (bs == bs_ && strlen(queriedPath) == 0);   /* nothing copied */
        printf("path lookup size requirement (%d) correctly returned: %d\n", bs_, b);
        r += b;
      }
      else
        printf("failed to query lib path using lib's handle\n");

      dlFreeLibrary(pLib);

      /* check if dlGetLibraryPath returns 0 when trying to lookup dummy */
      bs = dlGetLibraryPath((DLLib*)&r/*dummy addr*/, queriedPath, 200);
      printf("path lookup failed as expected with bad lib handle: %d\n", bs == 0);
      r += (bs == 0);

      /* test UTF-8 path through dummy library that's created by this test's build */
      {
        static const char* pathU8 = "./dynload_plain_\xc3\x9f_test";
		int nu8c, b;

        //cp(pathU8, "/lib/libz.so.6");
        pLib = dlLoadLibrary(pathU8); /* check if we can load a lib with a UTF-8 path */
        printf("pLib (loaded w/ UTF-8 path %s) handle: %p\n", pathU8, pLib);
        r += (p != NULL);

        if(pLib) {
          /* get UTF-8 path back */
          bs = dlGetLibraryPath((DLLib*)pLib, queriedPath, 200);
          if(bs && bs <= 200) {
            nu8c = strlen_utf8(queriedPath); /* num of UTF-8 chars is as big as ... */
            b = (bs > 0) && (nu8c == bs-2);   /* ... buffer size minus 2 (b/c of one 2-byte UTF-8 char and "\0") */
            printf("UTF-8 path of lib looked up via handle: %s\n", queriedPath);
            printf("looked up UTF-8 path's needed buffer size (%d) for %d UTF-8 char string computed correctly: %d\n", bs, nu8c, b);
            r += b;
 
            dlFreeLibrary(pLib);
          }
          else
            printf("failed to query UTF-8 lib path using lib's handle\n");
        }
      }
    }
    else
      printf("unable to open library %s\n", path);


    /* dlSyms* tests (intentionally after freeing lib above, as they work standalone) */
    /* ------------- */
    pSyms = dlSymsInit(path); /* check if we can iterate over symbols - init */
    if(pSyms) {
      int n;
      const char* name;

      printf("pSyms handle: %p\n", pSyms);
      ++r;

      n = dlSymsCount(pSyms); /* check if there are some syms to iterate over */
      printf("num of libc symbols: %d\n", n);
      r += (n > 0);

      for(i=0; i<n; ++i) {
        name = dlSymsName(pSyms, i);
        if(name && strcmp(name, "printf") == 0) { /* check if we find "printf" also in iterated symbols */
          ++r;
          break;
        }
      }
      printf("printf symbol found by iteration: %d\n", i<n);

      name = (i<n) ? dlSymsName(pSyms, i) : NULL;
      r += (name && strcmp(name, "printf") == 0); /* check if we can lookup "printf" by index */
      printf("printf symbol name by index: %s\n", name?name:"");

      pLib = dlLoadLibrary(path); /* check if we can resolve ptr -> name, */
      if(pLib) {                  /* need to lookup by name again, first */
        p = dlFindSymbol(pLib, "printf");
        name = dlSymsNameFromValue(pSyms, p);
        printf("printf symbol name by its own address (%p): %s\n", p, name?name:"");
        if(name) {
          if(strcmp(name, "printf") == 0)
            ++r;
          else {
            /* Symbol name returned might be an "alias". In that case, check address again (full lookup to be sure). */
            void* p0 = dlFindSymbol(pLib, name);
            printf("lookup by address returned different name (%s), which is alias of printf: %d\n", name, (p==p0));
            r += (p == p0);
          }
        }
        dlFreeLibrary(pLib);
      }

      dlSymsCleanup(pSyms);
    }
    else
      printf("dlSymsInit failed\n");
  }

  /* Check final score of right ones to see if all worked */
  r = (r == 15);
  printf("result: dynload_plain: %d\n", r);
  return !r;
}

