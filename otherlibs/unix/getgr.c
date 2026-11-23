/**************************************************************************/
/*                                                                        */
/*                                 OCaml                                  */
/*                                                                        */
/*             Xavier Leroy, projet Cristal, INRIA Rocquencourt           */
/*                                                                        */
/*   Copyright 1996 Institut National de Recherche en Informatique et     */
/*     en Automatique.                                                    */
/*                                                                        */
/*   All rights reserved.  This file is distributed under the terms of    */
/*   the GNU Lesser General Public License version 2.1, with the          */
/*   special exception on linking described in the file LICENSE.          */
/*                                                                        */
/**************************************************************************/

#include <caml/mlvalues.h>
#include <caml/fail.h>
#include <caml/alloc.h>
#include <caml/memory.h>
#include "caml/unixsupport.h"
#include <unistd.h>
#include <errno.h>
#include <grp.h>

static value alloc_group_entry(const struct group *entry)
{
  CAMLparam0();
  CAMLlocal3(name, pass, mem);
  value res;

  name = caml_copy_string(entry->gr_name);
  /* on some platforms, namely Android, gr_passwd can be NULL,
     hence this workaround */
  pass = caml_copy_string(entry->gr_passwd ? entry->gr_passwd : "");
  mem = caml_copy_string_array((const char**)entry->gr_mem);
  res = caml_alloc_small(4, 0);
  Field(res,0) = name;
  Field(res,1) = pass;
  Field(res,2) = Val_int(entry->gr_gid);
  Field(res,3) = mem;
  CAMLreturn(res);
}

/* The buffer grows until the entry fits or allocation fails; this limit
   only keeps the doubled size from overflowing. */
#define CAML_GETGR_R_SIZE_MAX (SIZE_MAX / 2)

/* POSIX reports a missing entry as 0 with a NULL result, but glibc may
   return one of these codes instead, depending on the NSS module
   (see the Linux getgrnam(3) manual). */
#if defined(HAVE_GETGRNAM_R) || defined(HAVE_GETGRGID_R)
static int getgr_not_found(int e)
{
  return e == 0 || e == ENOENT || e == ESRCH || e == EBADF || e == EPERM;
}
#endif

CAMLprim value caml_unix_getgrnam(value name)
{
  value res;
  struct group * entry;
  if (! caml_string_is_c_safe(name)) caml_raise_not_found();

#ifdef HAVE_GETGRNAM_R
  long initlen = sysconf(_SC_GETGR_R_SIZE_MAX);
  size_t len = initlen <= 0 ? /* default */ 1024 : (size_t) initlen;
  struct group result;
  char *buffer = caml_stat_alloc_noexc(len);
  if (buffer == NULL)
    caml_unix_error(ENOMEM, "getgrnam", Nothing);
  int e;
  while ((e = getgrnam_r(String_val(name), &result, buffer, len, &entry))
         == ERANGE) {
    len *= 2;
    char *newbuffer;
    if (len > CAML_GETGR_R_SIZE_MAX ||
        (newbuffer = caml_stat_resize_noexc(buffer, len)) == NULL) {
      caml_stat_free(buffer);
      caml_unix_error(ENOMEM, "getgrnam", Nothing);
    }
    buffer = newbuffer;
  }
  if (entry == NULL) {
    caml_stat_free(buffer);
    if (! getgr_not_found(e))
      caml_unix_error(e, "getgrnam", Nothing);
    caml_raise_not_found();
  }
  res = alloc_group_entry(entry);
  caml_stat_free(buffer);
#else
  errno = 0;
  entry = getgrnam(String_val(name));
  if (entry == NULL) {
    if (errno == EINTR)
      caml_uerror("getgrnam", Nothing);
    caml_raise_not_found();     /* ignore errors */
  }
  res = alloc_group_entry(entry);
#endif
  return res;
}

CAMLprim value caml_unix_getgrgid(value gid)
{
  value res;
  struct group * entry;
  gid_t gid_c =
    caml_unix_gid_val(gid, "Unix.getgrgid: gid out of range");

#ifdef HAVE_GETGRGID_R
  long initlen = sysconf(_SC_GETGR_R_SIZE_MAX);
  size_t len = initlen <= 0 ? /* default */ 1024 : (size_t) initlen;
  struct group result;
  char *buffer = caml_stat_alloc_noexc(len);
  if (buffer == NULL)
    caml_unix_error(ENOMEM, "getgrgid", Nothing);
  int e;
  while ((e = getgrgid_r(gid_c, &result, buffer, len, &entry))
         == ERANGE) {
    len *= 2;
    char *newbuffer;
    if (len > CAML_GETGR_R_SIZE_MAX ||
        (newbuffer = caml_stat_resize_noexc(buffer, len)) == NULL) {
      caml_stat_free(buffer);
      caml_unix_error(ENOMEM, "getgrgid", Nothing);
    }
    buffer = newbuffer;
  }
  if (entry == NULL) {
    caml_stat_free(buffer);
    if (! getgr_not_found(e))
      caml_unix_error(e, "getgrgid", Nothing);
    caml_raise_not_found();
  }
  res = alloc_group_entry(entry);
  caml_stat_free(buffer);
#else
  errno = 0;
  entry = getgrgid(gid_c);
  if (entry == NULL) {
    if (errno == EINTR)
      caml_uerror("getgrgid", Nothing);
    caml_raise_not_found();     /* ignore errors */
  }
  res = alloc_group_entry(entry);
#endif
  return res;
}
