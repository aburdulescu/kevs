#include "util.h"

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

KevsError read_file(KevsStr path, char **out, size_t *out_len) {
  int fd = open(path.ptr, O_RDONLY);
  if (fd == -1) {
    return strerror(errno);
  }

  KevsError err = NULL;
  char *ptr = NULL;

  struct stat stbuf = {};
  if (fstat(fd, &stbuf) == -1) {
    err = strerror(errno);
    goto cleanup;
  }

  const size_t len = stbuf.st_size;
  ptr = malloc(len + 1);
  if (ptr == NULL) {
    return "out of memory";
  }
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
  *out_len = len;

cleanup:
  if (err != NULL) {
    free(ptr);
  }
  close(fd);

  return err;
}

static const char *valuekind_str(KevsValueKind v) {
  switch (v) {
  case KevsValueKindUndefined:
    return "undefined";
  case KevsValueKindString:
    return "string";
  case KevsValueKindInteger:
    return "integer";
  case KevsValueKindBoolean:
    return "boolean";
  case KevsValueKindList:
    return "list";
  case KevsValueKindTable:
    return "table";
  default:
    return "unknown";
  }
}

void list_dump(KevsList self) {
  for (size_t i = 0; i < self.len; i++) {
    const KevsValue v = self.ptr[i];

    switch (v.kind) {
    case KevsValueKindTable: {
      printf("%s\n", valuekind_str(v.kind));
      table_dump(v.data.table);
    } break;

    case KevsValueKindList: {
      printf("%s\n", valuekind_str(v.kind));
      list_dump(v.data.list);
    } break;

    case KevsValueKindString: {
      printf("%s '%s'\n", valuekind_str(v.kind), v.data.string);
    } break;

    case KevsValueKindBoolean: {
      printf("%s %s\n", valuekind_str(v.kind),
             (v.data.boolean ? "true" : "false"));
    } break;

    case KevsValueKindInteger: {
      printf("%s %" PRId64 "\n", valuekind_str(v.kind), v.data.integer);
    } break;

    default: {
      printf("%s\n", valuekind_str(v.kind));
    } break;
    }
  }
}

void table_dump(KevsTable self) {
  for (size_t i = 0; i < self.len; i++) {
    const KevsKeyValue kv = self.ptr[i];

    char *k = str_dup(kv.key);

    switch (kv.val.kind) {
    case KevsValueKindTable: {
      printf("%s %s\n", k, valuekind_str(kv.val.kind));
      table_dump(kv.val.data.table);
    } break;

    case KevsValueKindList: {
      printf("%s %s\n", k, valuekind_str(kv.val.kind));
      list_dump(kv.val.data.list);
    } break;

    case KevsValueKindString: {
      printf("%s %s '%s'\n", k, valuekind_str(kv.val.kind), kv.val.data.string);
    } break;

    case KevsValueKindBoolean: {
      printf("%s %s %s\n", k, valuekind_str(kv.val.kind),
             (kv.val.data.boolean ? "true" : "false"));
    } break;

    case KevsValueKindInteger: {
      printf("%s %s %" PRId64 "\n", k, valuekind_str(kv.val.kind),
             kv.val.data.integer);
    } break;

    default: {
      printf("%s %s\n", k, valuekind_str(kv.val.kind));
    } break;
    }

    free(k);
  }
}
