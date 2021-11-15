// Copyright 2021 The PlaySys Authors
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// See http://www.apache.org/licenses/LICENSE-2.0

export type i8    = number
export type u8    = number
export type i16   = number
export type u16   = number
export type i32   = number
export type u32   = number
export type i64   = number
export type u64   = number
export type isize = number
export type usize = number
export type f32   = number
export type f64   = number

export type psysop_t = usize // syscall operation code
export type oflag_t  = usize // flags to openat syscall
export type err_t    = isize // error code. Only negative values.
export type fd_t     = isize // file descriptor

// constants
export const FDSTDIN  :fd_t =    0 // input stream
export const FDSTDOUT :fd_t =    1 // main output stream
export const FDSTDERR :fd_t =    2 // logging output stream
export const AT_FDCWD :fd_t = -100 // "current directory" for *at file operations

// errors
export enum err {
  none          =   0, // no error
  invalid       =  -1, // invalid data or argument
  sys_op        =  -2, // invalid syscall op or syscall op data
  badfd         =  -3, // invalid file descriptor
  bad_name      =  -4, // invalid or misformed name
  not_found     =  -5, // resource not found
  name_too_long =  -6, // 
  canceled      =  -7, // operation canceled
  not_supported =  -8, // not supported
  exists        =  -9, // already exists
  end           = -10, // end of resource
  access        = -11, // permission denied
  nomem         = -12, // cannot allocate memory
}

// open flags
export enum oflag {
  ronly  =  0, // Open for reading only
  wonly  =  1, // Open for writing only
  rw     =  2, // Open for both reading and writing
  append =  4, // Start writing at end (seekable files only)
  create =  8, // Create file if it does not exist
  trunc  = 16, // Set file size to zero
  excl   = 32, // fail if file exists when create and excl are set
}

// syscall operations
export enum sysop {
  openat       =   257, // base fd, path cstr, flags oflag, mode usize
  close        =     3, // fd fd
  read         =     0, // fd fd, data mutptr, nbyte usize
  write        =     1, // fd fd, data ptr, nbyte usize
  seek         =     8, // TODO
  statat       =   262, // TODO (newfstatat in linux, alt: statx 332)
  removeat     =   263, // base fd, path cstr, flags usize
  renameat     =   264, // oldbase fd, oldpath cstr, newbase fd, newpath cstr
  sleep        =   230, // seconds usize, nanoseconds usize
  exit         =    60, // status_code i32
  test         = 10000, // op psysop
  wgpu_opendev = 10001, // flags usize
  gui_mksurf   = 10002, // width u32, height u32, device fd, flags usize
}

// Note: this file is generated from spec.md; edit with caution