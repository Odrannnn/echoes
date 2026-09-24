#include <nod.h>

NodResult nod_disc_open_stream(const NodDiscStream* stream, const NodDiscOptions* options, NodHandle** out) {
    (void)options;
    if (out != NULL) {
        *out = NULL;
    }
    if (stream != NULL && stream->close != NULL) {
        stream->close(stream->user_data);
    }
    return NOD_RESULT_ERR_OTHER;
}

NodResult nod_disc_open_partition_kind(NodHandle* disc, uint32_t kind,
                                       const NodPartitionOptions* options, NodHandle** out) {
    (void)disc;
    (void)kind;
    (void)options;
    if (out != NULL) {
        *out = NULL;
    }
    return NOD_RESULT_ERR_OTHER;
}

NodResult nod_partition_open_file(NodHandle* partition, uint32_t fst_index, NodHandle** out) {
    (void)partition;
    (void)fst_index;
    if (out != NULL) {
        *out = NULL;
    }
    return NOD_RESULT_ERR_OTHER;
}

void nod_free(NodHandle* handle) {
    (void)handle;
}

int64_t nod_read(NodHandle* handle, uint8_t* buf, size_t len) {
    (void)handle;
    (void)buf;
    (void)len;
    return -1;
}

int64_t nod_seek(NodHandle* handle, int64_t offset, int32_t whence) {
    (void)handle;
    (void)offset;
    (void)whence;
    return -1;
}

NodResult nod_disc_header(const NodHandle* disc, NodDiscHeader* out) {
    (void)disc;
    (void)out;
    return NOD_RESULT_ERR_OTHER;
}

NodResult nod_partition_meta(const NodHandle* partition, NodPartitionMeta* out) {
    (void)partition;
    (void)out;
    return NOD_RESULT_ERR_OTHER;
}

void nod_partition_iterate_fst(const NodHandle* partition, NodFstCallback callback, void* user_data) {
    (void)partition;
    (void)callback;
    (void)user_data;
}
