#include "vn_backend.h"
#include "vn_error.h"

#define VN_MAX_BACKENDS 8

static const VNRenderBackend* g_registry[VN_MAX_BACKENDS];
static vn_u32 g_registry_count = 0;
static const VNRenderBackend* g_active_backend = (const VNRenderBackend*)0;

static const VNRenderBackend* vn_find_backend_by_arch(vn_u32 arch_tag) {
    vn_u32 i;
    for (i = 0; i < g_registry_count; ++i) {
        if (g_registry[i] != (const VNRenderBackend*)0 && g_registry[i]->arch_tag == arch_tag) {
            return g_registry[i];
        }
    }
    return (const VNRenderBackend*)0;
}

int vn_backend_register(const VNRenderBackend* be) {
    vn_u32 i;
    if (be == (const VNRenderBackend*)0 || be->name == (const char*)0) {
        return VN_E_INVALID_ARG;
    }
    for (i = 0; i < g_registry_count; ++i) {
        if (g_registry[i] == be) {
            return VN_OK;
        }
        if (g_registry[i] != (const VNRenderBackend*)0 && g_registry[i]->arch_tag == be->arch_tag) {
            return VN_E_FORMAT;
        }
    }
    if (g_registry_count >= VN_MAX_BACKENDS) {
        return VN_E_NOMEM;
    }
    g_registry[g_registry_count] = be;
    g_registry_count += 1;
    return VN_OK;
}

const VNRenderBackend* vn_backend_select(vn_u32 prefer_arch_mask) {
    const VNRenderBackend* picked;

    picked = (const VNRenderBackend*)0;
    if ((prefer_arch_mask & VN_ARCH_MASK_AVX2) != 0u) {
        picked = vn_find_backend_by_arch(VN_ARCH_AVX2);
    }
    if (picked == (const VNRenderBackend*)0 && (prefer_arch_mask & VN_ARCH_MASK_NEON) != 0u) {
        picked = vn_find_backend_by_arch(VN_ARCH_NEON);
    }
    if (picked == (const VNRenderBackend*)0 && (prefer_arch_mask & VN_ARCH_MASK_RVV) != 0u) {
        picked = vn_find_backend_by_arch(VN_ARCH_RVV);
    }
    if (picked == (const VNRenderBackend*)0 && (prefer_arch_mask & VN_ARCH_MASK_SCALAR) != 0u) {
        picked = vn_find_backend_by_arch(VN_ARCH_SCALAR);
    }

    g_active_backend = picked;
    return picked;
}

const VNRenderBackend* vn_backend_get_active(void) {
    return g_active_backend;
}

void vn_backend_reset_registry(void) {
    vn_u32 i;
    for (i = 0; i < VN_MAX_BACKENDS; ++i) {
        g_registry[i] = (const VNRenderBackend*)0;
    }
    g_registry_count = 0;
    g_active_backend = (const VNRenderBackend*)0;
}
