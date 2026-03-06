#ifndef VN_PLATFORM_H
#define VN_PLATFORM_H

#include <stddef.h>
#include <stdio.h>

const char* vn_platform_host_os_name(void);
const char* vn_platform_host_arch_name(void);
const char* vn_platform_host_compiler_name(void);
double vn_platform_now_ms(void);
void vn_platform_sleep_ms(unsigned int ms);
FILE* vn_platform_fopen_read_binary(const char* path);
int vn_platform_path_is_absolute(const char* path);
char vn_platform_path_separator(void);
void vn_platform_path_dirname(const char* path, char* out_dir, size_t out_size);
void vn_platform_path_join(char* out_path,
                           size_t out_size,
                           const char* base_dir,
                           const char* leaf);

#endif
