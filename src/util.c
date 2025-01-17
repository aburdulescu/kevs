#include "util.h"

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

Error read_file(Str path, char **out) {
  int fd = open(path.ptr, O_RDONLY);
  if (fd == -1) {
    return strerror(errno);
  }

  Error err = NULL;

  struct stat stbuf = {};
  if (fstat(fd, &stbuf) == -1) {
    err = strerror(errno);
    goto cleanup;
  }

  const size_t len = stbuf.st_size;
  char *ptr = malloc(len + 1);
  ptr[len] = 0;

  ssize_t nread = read(fd, ptr, len);
  if (nread == -1) {
    err = strerror(errno);
    goto cleanup;
  }
  if ((size_t)nread != len) {
    err = "short read";
    goto cleanup;
  }

  *out = ptr;

cleanup:
  if (err != NULL) {
    free(ptr);
  }
  close(fd);

  return err;
}

static const char *valuetag_str(ValueTag v) {
  switch (v) {
  case kValueTagUndefined:
    return "undefined";
  case kValueTagString:
    return "string";
  case kValueTagInteger:
    return "integer";
  case kValueTagBoolean:
    return "boolean";
  case kValueTagList:
    return "list";
  case kValueTagTable:
    return "table";
  default:
    return "unknown";
  }
}

void list_dump(List self) {
  for (size_t i = 0; i < self.len; i++) {
    const Value v = self.ptr[i];

    switch (v.tag) {
    case kValueTagTable: {
      printf("%s\n", valuetag_str(v.tag));
      table_dump(v.data.table);
    } break;

    case kValueTagList: {
      printf("%s\n", valuetag_str(v.tag));
      list_dump(v.data.list);
    } break;

    case kValueTagString: {
      char *s = str_dup(v.data.string);
      printf("%s '%s'\n", valuetag_str(v.tag), s);
      free(s);
    } break;

    case kValueTagBoolean: {
      printf("%s %s\n", valuetag_str(v.tag),
             (v.data.boolean ? "true" : "false"));
    } break;

    case kValueTagInteger: {
      printf("%s %" PRId64 "\n", valuetag_str(v.tag), v.data.integer);
    } break;

    default: {
      printf("%s\n", valuetag_str(v.tag));
    } break;
    }
  }
}

void table_dump(Table self) {
  for (size_t i = 0; i < self.len; i++) {
    const KeyValue kv = self.ptr[i];

    char *k = str_dup(kv.key);

    switch (kv.val.tag) {
    case kValueTagTable: {
      printf("%s %s\n", k, valuetag_str(kv.val.tag));
      table_dump(kv.val.data.table);
    } break;

    case kValueTagList: {
      printf("%s %s\n", k, valuetag_str(kv.val.tag));
      list_dump(kv.val.data.list);
    } break;

    case kValueTagString: {
      char *s = str_dup(kv.val.data.string);
      printf("%s %s '%s'\n", k, valuetag_str(kv.val.tag), s);
      free(s);
    } break;

    case kValueTagBoolean: {
      printf("%s %s %s\n", k, valuetag_str(kv.val.tag),
             (kv.val.data.boolean ? "true" : "false"));
    } break;

    case kValueTagInteger: {
      printf("%s %s %" PRId64 "\n", k, valuetag_str(kv.val.tag),
             kv.val.data.integer);
    } break;

    default: {
      printf("%s %s\n", k, valuetag_str(kv.val.tag));
    } break;
    }

    free(k);
  }
}
