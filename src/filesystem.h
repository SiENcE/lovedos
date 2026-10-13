/**
 * Copyright (c) 2017 rxi
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See LICENSE for details.
 */

#ifndef FILESYSTEM_H
#define FILESYSTEM_H

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>

enum {
  FILESYSTEM_ESUCCESS     =  0,
  FILESYSTEM_EFAILURE     = -1,
  FILESYSTEM_ETOOLONG     = -2,
  FILESYSTEM_EMOUNTED     = -3,
  FILESYSTEM_ENOMOUNT     = -4,
  FILESYSTEM_EMOUNTFAIL   = -5,
  FILESYSTEM_ENOWRITEDIR  = -6,
  FILESYSTEM_EWRITEFAIL   = -7,
  FILESYSTEM_EMKDIRFAIL   = -8
};

typedef struct {
  int type;       /* FILESYSTEM_TREG or FILESYSTEM_TDIR */
  long size;
  long modtime;   /* Seconds since the epoch, -1 if unknown */
} filesystem_info_t;

typedef void (*filesystem_listCallback)(void *udata, const char *name);
typedef struct filesystem_file_t filesystem_file_t;

enum {
  FILESYSTEM_TNONE,
  FILESYSTEM_TREG,
  FILESYSTEM_TDIR,
};

const char* filesystem_strerror(int err);
void filesystem_deinit(void);
int filesystem_mount(const char *path);
int filesystem_unmount(const char *path);
int filesystem_exists(const char *filename);
int filesystem_isFile(const char *filename);
int filesystem_isDirectory(const char *filename);
void* filesystem_read(const char *filename, int *size);
void filesystem_free(void *ptr);
int filesystem_setWriteDir(const char *path);
int filesystem_write(const char *filename, const void *data, int size);
int filesystem_append(const char *filename, const void *data, int size);
int filesystem_remove(const char *filename);
int filesystem_createDirectory(const char *path);
int filesystem_getInfo(const char *filename, filesystem_info_t *info);
const char *filesystem_getRealDirectory(const char *filename);
void filesystem_list(const char *dir, filesystem_listCallback cb, void *udata);
const char *filesystem_getWriteDir(void);
filesystem_file_t *filesystem_open(const char *filename);
int filesystem_fread(filesystem_file_t *f, void *buf, int n);
int filesystem_fseek(filesystem_file_t *f, long pos);
long filesystem_fsize(filesystem_file_t *f);
void filesystem_fclose(filesystem_file_t *f);

#endif
