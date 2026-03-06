#ifndef VN_BUILD_CONFIG_H
#define VN_BUILD_CONFIG_H

#if defined(_MSC_VER)
#define VN_COMPILER_MSVC 1
#else
#define VN_COMPILER_MSVC 0
#endif

#if defined(__clang__)
#define VN_COMPILER_CLANG 1
#else
#define VN_COMPILER_CLANG 0
#endif

#if defined(__GNUC__) && !defined(__clang__)
#define VN_COMPILER_GCC 1
#else
#define VN_COMPILER_GCC 0
#endif

#if defined(_WIN32)
#define VN_OS_WINDOWS 1
#else
#define VN_OS_WINDOWS 0
#endif

#if defined(__linux__)
#define VN_OS_LINUX 1
#else
#define VN_OS_LINUX 0
#endif

#if defined(__APPLE__)
#define VN_OS_MACOS 1
#else
#define VN_OS_MACOS 0
#endif

#if defined(__x86_64__) || defined(_M_X64)
#define VN_ARCH_X64 1
#else
#define VN_ARCH_X64 0
#endif

#if defined(__i386__) || defined(_M_IX86)
#define VN_ARCH_X86 1
#else
#define VN_ARCH_X86 0
#endif

#if defined(__aarch64__) || defined(_M_ARM64)
#define VN_ARCH_ARM64 1
#else
#define VN_ARCH_ARM64 0
#endif

#if defined(__arm__) || defined(_M_ARM)
#define VN_ARCH_ARM32 1
#else
#define VN_ARCH_ARM32 0
#endif

#if defined(__riscv) && defined(__riscv_xlen) && (__riscv_xlen == 64)
#define VN_ARCH_RISCV64 1
#else
#define VN_ARCH_RISCV64 0
#endif

#if defined(__riscv) && defined(__riscv_xlen) && (__riscv_xlen == 32)
#define VN_ARCH_RISCV32 1
#else
#define VN_ARCH_RISCV32 0
#endif

#if (VN_COMPILER_GCC || VN_COMPILER_CLANG) && (VN_ARCH_X64 || VN_ARCH_X86)
#define VN_AVX2_GNU_STYLE_IMPL 1
#else
#define VN_AVX2_GNU_STYLE_IMPL 0
#endif

#if VN_COMPILER_MSVC && VN_ARCH_X64
#define VN_AVX2_MSVC_STYLE_IMPL 1
#else
#define VN_AVX2_MSVC_STYLE_IMPL 0
#endif

#if VN_ARCH_ARM64 || defined(__ARM_NEON)
#define VN_NEON_IMPL_AVAILABLE 1
#else
#define VN_NEON_IMPL_AVAILABLE 0
#endif

#if defined(__riscv) && defined(__riscv_vector)
#define VN_RVV_IMPL_AVAILABLE 1
#else
#define VN_RVV_IMPL_AVAILABLE 0
#endif

#if VN_OS_WINDOWS
#define VN_HOST_OS_NAME "windows"
#elif VN_OS_LINUX
#define VN_HOST_OS_NAME "linux"
#elif VN_OS_MACOS
#define VN_HOST_OS_NAME "macos"
#else
#define VN_HOST_OS_NAME "unknown"
#endif

#if VN_ARCH_X64
#define VN_HOST_ARCH_NAME "x64"
#elif VN_ARCH_ARM64
#define VN_HOST_ARCH_NAME "arm64"
#elif VN_ARCH_RISCV64
#define VN_HOST_ARCH_NAME "riscv64"
#elif VN_ARCH_X86
#define VN_HOST_ARCH_NAME "x86"
#elif VN_ARCH_ARM32
#define VN_HOST_ARCH_NAME "arm32"
#elif VN_ARCH_RISCV32
#define VN_HOST_ARCH_NAME "riscv32"
#else
#define VN_HOST_ARCH_NAME "unknown"
#endif

#if VN_COMPILER_MSVC
#define VN_HOST_COMPILER_NAME "msvc"
#elif VN_COMPILER_CLANG
#define VN_HOST_COMPILER_NAME "clang"
#elif VN_COMPILER_GCC
#define VN_HOST_COMPILER_NAME "gcc"
#else
#define VN_HOST_COMPILER_NAME "unknown"
#endif

#endif
