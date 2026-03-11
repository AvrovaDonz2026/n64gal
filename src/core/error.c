#include "vn_error.h"

const char* vn_error_name(int error_code) {
    if (error_code == VN_OK) {
        return "VN_OK";
    }
    if (error_code == VN_E_INVALID_ARG) {
        return "VN_E_INVALID_ARG";
    }
    if (error_code == VN_E_IO) {
        return "VN_E_IO";
    }
    if (error_code == VN_E_FORMAT) {
        return "VN_E_FORMAT";
    }
    if (error_code == VN_E_UNSUPPORTED) {
        return "VN_E_UNSUPPORTED";
    }
    if (error_code == VN_E_NOMEM) {
        return "VN_E_NOMEM";
    }
    if (error_code == VN_E_SCRIPT_BOUNDS) {
        return "VN_E_SCRIPT_BOUNDS";
    }
    if (error_code == VN_E_RENDER_STATE) {
        return "VN_E_RENDER_STATE";
    }
    if (error_code == VN_E_AUDIO_DEVICE) {
        return "VN_E_AUDIO_DEVICE";
    }
    return "VN_E_UNKNOWN";
}
