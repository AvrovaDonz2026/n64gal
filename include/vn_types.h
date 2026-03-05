#ifndef VN_TYPES_H
#define VN_TYPES_H

typedef unsigned char vn_u8;
typedef signed char vn_i8;
typedef unsigned short vn_u16;
typedef signed short vn_i16;
typedef unsigned int vn_u32;
typedef signed int vn_i32;

#define VN_TRUE 1
#define VN_FALSE 0

typedef char vn_check_u8[(sizeof(vn_u8) == 1) ? 1 : -1];
typedef char vn_check_i8[(sizeof(vn_i8) == 1) ? 1 : -1];
typedef char vn_check_u16[(sizeof(vn_u16) == 2) ? 1 : -1];
typedef char vn_check_i16[(sizeof(vn_i16) == 2) ? 1 : -1];
typedef char vn_check_u32[(sizeof(vn_u32) == 4) ? 1 : -1];
typedef char vn_check_i32[(sizeof(vn_i32) == 4) ? 1 : -1];

#endif
