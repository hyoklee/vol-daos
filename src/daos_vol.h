/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Copyright by The HDF Group.                                               *
 * All rights reserved.                                                      *
 *                                                                           *
 * This file is part of the HDF5 DAOS VOL connector. The full copyright      *
 * notice, including terms governing use, modification, and redistribution,  *
 * is contained in the COPYING file, which can be found at the root of the   *
 * source code distribution tree.                                            *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/*
 * Purpose:	The private header file for the DAOS VOL connector.
 */

#ifndef daos_vol_H
#define daos_vol_H

/* Public headers needed by this file */
#include "daos_vol_public.h"

/* CART headers */
#include <gurt/types.h>
#include <gurt/common.h>
#include <cart/api.h>

/* DAOS headers */
#include <daos.h>
#include <daos_task.h>
#include <daos/tse.h>

/* System headers */
#include <assert.h>

/* Hash table */
#include "util/daos_vol_hash_table.h"

/* For DAOS compatibility */
#ifdef H5VL_DAOS_NEW_API
typedef d_iov_t daos_iov_t;
typedef d_sg_list_t daos_sg_list_t;
# define DAOS_OC_TINY_RW  OC_S1
# define DAOS_OC_LARGE_RW OC_SX
# define daos_rank_list_free d_rank_list_free
# define daos_iov_set d_iov_set
# define DAOS_OF_AKEY_HASHED 0
# define DAOS_OF_DKEY_HASHED 0
# define H5_daos_obj_generate_id(oid, ofeats, cid) \
    daos_obj_generate_id(oid, ofeats, cid, 0)
#else
# define H5_daos_obj_generate_id(oid, ofeats, cid) \
    daos_obj_generate_id(oid, ofeats, cid)
#endif

/*****************/
/* Public Macros */
/*****************/

#define HDF5_VOL_DAOS_VERSION_1	(1)	/* Version number of DAOS VOL connector */
/* Class value of the DAOS VOL connector as defined in H5VLpublic.h DSINC */
#define H5_VOL_DAOS_CLS_VAL (H5VL_class_value_t) (H5_VOL_RESERVED + 2)
#define H5_DAOS_VOL_NAME "daos"
#define H5_DAOS_VOL_NAME_LEN 4

/* Constant keys */
#define H5_DAOS_CHUNK_KEY 0u

/* Stack allocation sizes */
#define H5_DAOS_GH_BUF_SIZE 1024
#define H5_DAOS_FOI_BUF_SIZE 1024
#define H5_DAOS_LINK_VAL_BUF_SIZE 256
#define H5_DAOS_GINFO_BUF_SIZE 256
#define H5_DAOS_DINFO_BUF_SIZE 1024
#define H5_DAOS_TINFO_BUF_SIZE 1024
#define H5_DAOS_SEQ_LIST_LEN 128
#define H5_DAOS_ITER_LEN 128
#define H5_DAOS_ITER_SIZE_INIT (4 * 1024)
#define H5_DAOS_ATTR_NUM_AKEYS 4

/* Definitions for building oids */
#define H5_DAOS_IDX_MASK   0x3fffffffffffffffull
#define H5_DAOS_TYPE_MASK  0xc000000000000000ull
#define H5_DAOS_TYPE_GRP   0x0000000000000000ull
#define H5_DAOS_TYPE_DSET  0x4000000000000000ull
#define H5_DAOS_TYPE_DTYPE 0x8000000000000000ull
#define H5_DAOS_TYPE_MAP   0xc000000000000000ull

/* Private error codes for asynchronous operations */
#define H5_DAOS_INCOMPLETE -1   /* Operation has not yet completed (should only be in the item struct) */
#define H5_DAOS_PRE_ERROR -2    /* A precursor to this task failed (should only be used as the task return value) */
#define H5_DAOS_CLOSE_ERROR -3  /* Failed to close HDF5 object */

/* Remove warnings when connector does not use callback arguments */
#if defined(__cplusplus)
# define H5VL_DAOS_UNUSED
#elif defined(__GNUC__) && (__GNUC__ >= 4)
# define H5VL_DAOS_UNUSED __attribute__((unused))
#else
# define H5VL_DAOS_UNUSED
#endif

/* Remove warnings when arguments passed to a callback by way of va_arg are not used. */
#define H5_DAOS_UNUSED_VAR(arg) (void) arg;

/* Min/max macros */
#ifndef MAX
# define MAX(a, b) (((a) > (b)) ? (a) : (b))
#endif
#ifndef MIN
# define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif

/* Macros borrowed from H5Fprivate.h */
#define UINT64ENCODE(p, n) {                           \
   uint64_t _n = (n);                                  \
   size_t _i;                                          \
   uint8_t *_p = (uint8_t*)(p);                        \
                                                       \
   for (_i = 0; _i < sizeof(uint64_t); _i++, _n >>= 8) \
      *_p++ = (uint8_t)(_n & 0xff);                    \
   for (/*void*/; _i < 8; _i++)                        \
      *_p++ = 0;                                       \
   (p) = (uint8_t*)(p) + 8;                            \
}

#define UINT64DECODE(p, n) {                 \
   /* WE DON'T CHECK FOR OVERFLOW! */        \
   size_t _i;                                \
                                             \
   n = 0;                                    \
   (p) += 8;                                 \
   for (_i = 0; _i < sizeof(uint64_t); _i++) \
      n = (n << 8) | *(--p);                 \
   (p) += 8;                                 \
}

/* Decode a variable-sized buffer */
/* (Assumes that the high bits of the integer will be zero) */
#define DECODE_VAR(p, n, l) { \
   size_t _i;                 \
                              \
   n = 0;                     \
   (p) += l;                  \
   for (_i = 0; _i < l; _i++) \
      n = (n << 8) | *(--p);  \
   (p) += l;                  \
}

/* Decode a variable-sized buffer into a 64-bit unsigned integer */
/* (Assumes that the high bits of the integer will be zero) */
#define UINT64DECODE_VAR(p, n, l)     DECODE_VAR(p, n, l)

/* FAPL property to tell the VOL connector to open a saved snapshot when opening a
 * file */
#ifdef DV_HAVE_SNAP_OPEN_ID
#define H5_DAOS_SNAP_OPEN_ID "daos_snap_open"
#endif

/* DSINC - Exclude map functionality for now */
#undef DV_HAVE_MAP

/* DSINC - There are serious problems in HDF5 when trying to call
 * H5Pregister2/H5Punregister on the H5P_FILE_ACCESS class.
 */
#undef DV_HAVE_SNAP_OPEN_ID

/*
 * Macro to loop over asking DAOS for a list of akeys/dkeys for an object
 * and stop as soon as at least one key is retrieved. If DAOS returns
 * -DER_KEY2BIG, the loop will re-allocate the specified key buffer as
 * necessary and try again. The variadic portion of this macro corresponds
 * to the arguments given to daos_obj_list_akey/dkey.
 */
#define H5_DAOS_RETRIEVE_KEYS_LOOP(key_buf, key_buf_len, sg_iov, maj_err, daos_obj_list_func, ...)  \
do {                                                                                                \
    /* Reset nr */                                                                                  \
    nr = H5_DAOS_ITER_LEN;                                                                          \
                                                                                                    \
    /* Ask DAOS for a list of keys, break out if we succeed */                                      \
    if(0 == (ret = daos_obj_list_func(__VA_ARGS__)))                                                \
        break;                                                                                      \
                                                                                                    \
    /*                                                                                              \
     * Call failed - if the buffer is too small double it and                                       \
     * try again, otherwise fail.                                                                   \
     */                                                                                             \
    if(ret == -DER_KEY2BIG) {                                                                       \
        char *tmp_realloc;                                                                          \
                                                                                                    \
        /* Allocate larger buffer */                                                                \
        key_buf_len *= 2;                                                                           \
        if(NULL == (tmp_realloc = (char *)DV_realloc(key_buf, key_buf_len)))                        \
            D_GOTO_ERROR(H5E_RESOURCE, H5E_CANTALLOC, FAIL, "can't reallocate key buffer")          \
        key_buf = tmp_realloc;                                                                      \
                                                                                                    \
        /* Update SGL */                                                                            \
        daos_iov_set(&sg_iov, key_buf, (daos_size_t)(key_buf_len - 1));                             \
    } /* end if */                                                                                  \
    else                                                                                            \
        D_GOTO_ERROR(maj_err, H5E_CANTGET, FAIL, "can't list keys: %s", H5_daos_err_to_string(ret)) \
} while(1)

/* Macro to initialize all non-specific fields of an H5_daos_iter_data_t struct */
#define H5_DAOS_ITER_DATA_INIT(_iter_data, _iter_type, _idx_type, _iter_order, \
    _is_recursive, _idx_p, _iter_root_obj, _op_data, _dxpl_id, _req)           \
    do {                                                                       \
        memset(&_iter_data, 0, sizeof(H5_daos_iter_data_t));                   \
        _iter_data.iter_type = _iter_type;                                     \
        _iter_data.index_type = _idx_type;                                     \
        _iter_data.iter_order = _iter_order;                                   \
        _iter_data.is_recursive = _is_recursive;                               \
        _iter_data.idx_p = _idx_p;                                             \
        _iter_data.iter_root_obj = _iter_root_obj;                             \
        _iter_data.op_data = _op_data;                                         \
        _iter_data.dxpl_id = _dxpl_id;                                         \
        _iter_data.req = _req;                                                 \
    } while(0)

/*******************/
/* Public Typedefs */
/*******************/

/* DAOS-specific file access properties */
typedef struct H5_daos_fapl_t {
    MPI_Comm            comm;           /* communicator                  */
    MPI_Info            info;           /* file information              */
} H5_daos_fapl_t;

/* Common object and attribute information */
typedef struct H5_daos_item_t {
    H5I_type_t type;
    struct H5_daos_req_t *open_req;
    struct H5_daos_file_t *file;
    int rc;
} H5_daos_item_t;

/* Common object information */
typedef struct H5_daos_obj_t {
    H5_daos_item_t item; /* Must be first */
    daos_obj_id_t oid;
    daos_handle_t obj_oh;
} H5_daos_obj_t;

/* The file struct */
typedef struct H5_daos_file_t {
    H5_daos_item_t item; /* Must be first */
    daos_handle_t coh;
    crt_context_t crt_ctx;
    tse_sched_t sched;
    char *file_name;
    uuid_t uuid;
    unsigned flags;
    hbool_t closed;
    daos_handle_t glob_md_oh;
    struct H5_daos_group_t *root_grp;
    uint64_t max_oid;
    hbool_t max_oid_dirty;
    hid_t fcpl_id;
    hid_t fapl_id;
    MPI_Comm comm;
    MPI_Info info;
    int my_rank;
    int num_procs;
    hbool_t is_collective_md_read;
    hid_t vol_id;
    void *vol_info;
} H5_daos_file_t;

/* The GCPL cache struct */
typedef struct H5_daos_gcpl_cache_t {
    hbool_t track_corder;
} H5_daos_gcpl_cache_t;

/* The group struct */
typedef struct H5_daos_group_t {
    H5_daos_obj_t obj; /* Must be first */
    hid_t gcpl_id;
    hid_t gapl_id;
    H5_daos_gcpl_cache_t gcpl_cache;
} H5_daos_group_t;

/* The dataset struct */
typedef struct H5_daos_dset_t {
    H5_daos_obj_t obj; /* Must be first */
    hid_t type_id;
    hid_t space_id;
    hid_t dcpl_id;
    hid_t dapl_id;
} H5_daos_dset_t;

/* The datatype struct */
/* Note we could speed things up a bit by caching the serialized datatype.  We
 * may also not need to keep the type_id around.  -NAF */
typedef struct H5_daos_dtype_t {
    H5_daos_obj_t obj; /* Must be first */
    hid_t type_id;
    hid_t tcpl_id;
    hid_t tapl_id;
} H5_daos_dtype_t;

/* The map struct */
typedef struct H5_daos_map_t {
    H5_daos_obj_t obj; /* Must be first */
    hid_t ktype_id;
    hid_t vtype_id;
    hid_t mcpl_id;
    hid_t mapl_id;
} H5_daos_map_t;

/* The attribute struct */
typedef struct H5_daos_attr_t {
    H5_daos_item_t item; /* Must be first */
    H5_daos_obj_t *parent;
    char *name;
    hid_t type_id;
    hid_t space_id;
    hid_t acpl_id;
} H5_daos_attr_t;

/* The link value struct */
typedef struct H5_daos_link_val_t {
    H5L_type_t type;
    union {
        daos_obj_id_t hard;
        char *soft;
    } target;
} H5_daos_link_val_t;

/* Enum to indicate if the supplied read buffer can be used as a type conversion
 * or background buffer */
typedef enum {
    H5_DAOS_TCONV_REUSE_NONE,    /* Cannot reuse buffer */
    H5_DAOS_TCONV_REUSE_TCONV,   /* Use buffer as type conversion buffer */
    H5_DAOS_TCONV_REUSE_BKG      /* Use buffer as background buffer */
} H5_daos_tconv_reuse_t;

/* Generic request struct */
typedef struct H5_daos_req_t {
    daos_handle_t th;
    hbool_t th_open;
    H5_daos_file_t *file;
    int rc;
    int status;
    const char *failed_task; /* Add more error info? DSINC */
} H5_daos_req_t;

typedef struct H5_daos_md_update_cb_ud_t {
    H5_daos_req_t *req;
    H5_daos_obj_t *obj;
    daos_key_t dkey;
    unsigned nr;
    daos_iod_t iod[4];
    daos_sg_list_t sgl[4];
    daos_iov_t sg_iov[4];
    hbool_t free_dkey;
    hbool_t free_akeys;
    const char *task_name;
} H5_daos_md_update_cb_ud_t;

/*
 * Enum values for determining the type of iteration
 * being done with a given H5_daos_iter_data_t.
 */
typedef enum {
    H5_DAOS_ITER_TYPE_ATTR,
    H5_DAOS_ITER_TYPE_LINK,
    H5_DAOS_ITER_TYPE_OBJ,
} H5_daos_iter_data_type_t;

/*
 * A struct which is filled out and used when performing
 * link, attribute and object iteration/visiting.
 */
typedef struct H5_daos_iter_data_t {
    H5_iter_order_t   iter_order;
    H5_index_t        index_type;
    hbool_t           is_recursive;
    hsize_t          *idx_p;
    hid_t             iter_root_obj;
    void             *op_data;

    hid_t             dxpl_id;
    void            **req;

    H5_daos_iter_data_type_t iter_type;
    union {
        struct {
            H5A_operator2_t attr_iter_op;
        } attr_iter_data;

        struct {
            H5L_iterate_t    link_iter_op;
            dv_hash_table_t *visited_link_table;
            char            *recursive_link_path;
            size_t           recursive_link_path_nalloc;
            unsigned         recurse_depth;
        } link_iter_data;

        struct {
            H5O_iterate_t   obj_iter_op;
            unsigned        fields;
            const char     *obj_name;
        } obj_iter_data;
    } u;
} H5_daos_iter_data_t;

/* XXX: The following two definitions are only here until they are
 * moved out of their respective H5Xpkg.h header files and into a
 * more public scope. They are still needed for the DAOS VOL to handle
 * these API calls being made.
 */
typedef enum H5VL_file_optional_t {
    H5VL_FILE_CLEAR_ELINK_CACHE,        /* Clear external link cache               */
    H5VL_FILE_GET_FILE_IMAGE,           /* file image                              */
    H5VL_FILE_GET_FREE_SECTIONS,        /* file free selections                    */
    H5VL_FILE_GET_FREE_SPACE,           /* file freespace                          */
    H5VL_FILE_GET_INFO,                 /* file info                               */
    H5VL_FILE_GET_MDC_CONF,             /* file metadata cache configuration       */
    H5VL_FILE_GET_MDC_HR,               /* file metadata cache hit rate            */
    H5VL_FILE_GET_MDC_SIZE,             /* file metadata cache size                */
    H5VL_FILE_GET_SIZE,                 /* file size                               */
    H5VL_FILE_GET_VFD_HANDLE,           /* file VFD handle                         */
    H5VL_FILE_GET_FILE_ID,              /* retrieve or resurrect file ID of object */
    H5VL_FILE_RESET_MDC_HIT_RATE,       /* get metadata cache hit rate             */
    H5VL_FILE_SET_MDC_CONFIG,           /* set metadata cache configuration        */
    H5VL_FILE_GET_METADATA_READ_RETRY_INFO,
    H5VL_FILE_START_SWMR_WRITE,
    H5VL_FILE_START_MDC_LOGGING,
    H5VL_FILE_STOP_MDC_LOGGING,
    H5VL_FILE_GET_MDC_LOGGING_STATUS,
    H5VL_FILE_FORMAT_CONVERT,
    H5VL_FILE_RESET_PAGE_BUFFERING_STATS,
    H5VL_FILE_GET_PAGE_BUFFERING_STATS,
    H5VL_FILE_GET_MDC_IMAGE_INFO,
    H5VL_FILE_GET_EOA,
    H5VL_FILE_INCR_FILESIZE,
    H5VL_FILE_SET_LIBVER_BOUNDS
} H5VL_file_optional_t;

/* types for object optional VOL operations */
typedef enum H5VL_object_optional_t {
    H5VL_OBJECT_GET_COMMENT,            /* get object comment                   */
    H5VL_OBJECT_GET_INFO,               /* get object info                      */
    H5VL_OBJECT_SET_COMMENT             /* set object comment                   */
} H5VL_object_optional_t;

/********************/
/* Public Variables */
/********************/

extern H5VL_DAOS_PRIVATE hid_t H5_DAOS_g;

/* Free list definitions */
/* DSINC - currently no external access to free lists
H5FL_DEFINE_EXTERN(H5_daos_file_t);
H5FL_DEFINE_EXTERN(H5_daos_group_t);
H5FL_DEFINE_EXTERN(H5_daos_dset_t);
H5FL_DEFINE_EXTERN(H5_daos_dtype_t);
H5FL_DEFINE_EXTERN(H5_daos_map_t);
H5FL_DEFINE_EXTERN(H5_daos_attr_t);*/

/* DSINC - Until we determine what to do with free lists,
 * these macros should at least keep the allocations working
 * correctly.
 */
#define H5FL_CALLOC(t) DV_calloc(sizeof(t))
#define H5FL_FREE(t, o) DV_free(o)

#ifdef DV_TRACK_MEM_USAGE
/*
 * Counter to keep track of the currently allocated amount of bytes
 */
extern size_t daos_vol_curr_alloc_bytes;
#endif

/* Pool handle for use with all files */
extern H5VL_DAOS_PRIVATE daos_handle_t H5_daos_poh_g;

/* Global variables used to open the pool */
extern H5VL_DAOS_PRIVATE MPI_Comm H5_daos_pool_comm_g;

/* Constant Keys */
extern H5VL_DAOS_PRIVATE const char H5_daos_int_md_key_g[];
extern H5VL_DAOS_PRIVATE const char H5_daos_max_oid_key_g[];
extern H5VL_DAOS_PRIVATE const char H5_daos_cpl_key_g[];
extern H5VL_DAOS_PRIVATE const char H5_daos_link_key_g[];
extern H5VL_DAOS_PRIVATE const char H5_daos_link_corder_key_g[];
extern H5VL_DAOS_PRIVATE const char H5_daos_nlinks_key_g[];
extern H5VL_DAOS_PRIVATE const char H5_daos_type_key_g[];
extern H5VL_DAOS_PRIVATE const char H5_daos_space_key_g[];
extern H5VL_DAOS_PRIVATE const char H5_daos_attr_key_g[];
extern H5VL_DAOS_PRIVATE const char H5_daos_ktype_g[];
extern H5VL_DAOS_PRIVATE const char H5_daos_vtype_g[];
extern H5VL_DAOS_PRIVATE const char H5_daos_map_key_g[];

extern H5VL_DAOS_PRIVATE const daos_size_t H5_daos_int_md_key_size_g;
extern H5VL_DAOS_PRIVATE const daos_size_t H5_daos_max_oid_key_size_g;
extern H5VL_DAOS_PRIVATE const daos_size_t H5_daos_cpl_key_size_g;
extern H5VL_DAOS_PRIVATE const daos_size_t H5_daos_link_key_size_g;
extern H5VL_DAOS_PRIVATE const daos_size_t H5_daos_link_corder_key_size_g;
extern H5VL_DAOS_PRIVATE const daos_size_t H5_daos_nlinks_key_size_g;
extern H5VL_DAOS_PRIVATE const daos_size_t H5_daos_type_key_size_g;
extern H5VL_DAOS_PRIVATE const daos_size_t H5_daos_space_key_size_g;
extern H5VL_DAOS_PRIVATE const daos_size_t H5_daos_attr_key_size_g;
extern H5VL_DAOS_PRIVATE const daos_size_t H5_daos_ktype_size_g;
extern H5VL_DAOS_PRIVATE const daos_size_t H5_daos_vtype_size_g;
extern H5VL_DAOS_PRIVATE const daos_size_t H5_daos_map_key_size_g;

/*********************/
/* Public Prototypes */
/*********************/

#ifdef __cplusplus
extern "C" {
#endif

/* General routines */
H5VL_DAOS_PRIVATE void H5_daos_oid_generate(daos_obj_id_t *oid, uint64_t addr,
    H5I_type_t obj_type);
H5VL_DAOS_PRIVATE void H5_daos_oid_encode(daos_obj_id_t *oid, uint64_t idx, H5I_type_t obj_type);
H5VL_DAOS_PRIVATE H5I_type_t H5_daos_addr_to_type(uint64_t addr);
H5VL_DAOS_PRIVATE H5I_type_t H5_daos_oid_to_type(daos_obj_id_t oid);
H5VL_DAOS_PRIVATE uint64_t H5_daos_oid_to_idx(daos_obj_id_t oid);
H5VL_DAOS_PRIVATE void H5_daos_hash128(const char *name, void *hash);
H5VL_DAOS_PRIVATE herr_t H5_daos_write_max_oid(H5_daos_file_t *file);
H5VL_DAOS_PRIVATE int H5_daos_h5op_finalize(tse_task_t *task);
H5VL_DAOS_PRIVATE int H5_daos_md_update_prep_cb(tse_task_t *task, void *args);
H5VL_DAOS_PRIVATE int H5_daos_md_update_comp_cb(tse_task_t *task, void *args);
H5VL_DAOS_PRIVATE herr_t H5_daos_comm_info_dup(MPI_Comm comm, MPI_Info info,
        MPI_Comm *comm_new, MPI_Info *info_new);
H5VL_DAOS_PRIVATE herr_t H5_daos_comm_info_free(MPI_Comm *comm, MPI_Info *info);

/* File callbacks */
H5VL_DAOS_PRIVATE void *H5_daos_file_create(const char *name, unsigned flags, hid_t fcpl_id,
    hid_t fapl_id, hid_t dxpl_id, void **req);
H5VL_DAOS_PRIVATE void *H5_daos_file_open(const char *name, unsigned flags, hid_t fapl_id,
    hid_t dxpl_id, void **req);
H5VL_DAOS_PRIVATE herr_t H5_daos_file_get(void *_item, H5VL_file_get_t get_type, hid_t dxpl_id,
    void **req, va_list arguments);
H5VL_DAOS_PRIVATE herr_t H5_daos_file_specific(void *_item, H5VL_file_specific_t specific_type,
    hid_t dxpl_id, void **req, va_list arguments);
H5VL_DAOS_PRIVATE herr_t H5_daos_file_close(void *_file, hid_t dxpl_id, void **req);

/* Other file routines */
H5VL_DAOS_PRIVATE void H5_daos_file_decref(H5_daos_file_t *file);

/* Link callbacks */
H5VL_DAOS_PRIVATE herr_t H5_daos_link_create(H5VL_link_create_type_t create_type, void *_item,
    const H5VL_loc_params_t *loc_params, hid_t lcpl_id, hid_t lapl_id,
    hid_t dxpl_id, void **req, va_list arguments);
H5VL_DAOS_PRIVATE herr_t H5_daos_link_copy(void *src_obj, const H5VL_loc_params_t *loc_params1,
    void *dst_obj, const H5VL_loc_params_t *loc_params2, hid_t lcpl,
    hid_t lapl, hid_t dxpl_id, void **req);
H5VL_DAOS_PRIVATE herr_t H5_daos_link_move(void *src_obj, const H5VL_loc_params_t *loc_params1,
    void *dst_obj, const H5VL_loc_params_t *loc_params2, hid_t lcpl,
    hid_t lapl, hid_t dxpl_id, void **req);
H5VL_DAOS_PRIVATE herr_t H5_daos_link_get(void *_item, const H5VL_loc_params_t *loc_params,
    H5VL_link_get_t get_type, hid_t dxpl_id, void **req, va_list arguments);
H5VL_DAOS_PRIVATE herr_t H5_daos_link_specific(void *_item, const H5VL_loc_params_t *loc_params,
    H5VL_link_specific_t specific_type, hid_t dxpl_id, void **req,
    va_list arguments);

/* Other link routines */
H5VL_DAOS_PRIVATE herr_t H5_daos_link_write(H5_daos_group_t *grp, const char *name,
    size_t name_len, H5_daos_link_val_t *val, H5_daos_req_t *req,
    tse_task_t **taskp);
H5VL_DAOS_PRIVATE htri_t H5_daos_link_exists(H5_daos_item_t *item, const char *link_path, hid_t dxpl_id, void **req);
H5VL_DAOS_PRIVATE htri_t H5_daos_link_follow(H5_daos_group_t *grp, const char *name,
    size_t name_len, hid_t dxpl_id, void **req, daos_obj_id_t *oid);
H5VL_DAOS_PRIVATE herr_t H5_daos_link_iterate(H5_daos_group_t *target_grp, H5_daos_iter_data_t *link_iter_data);

/* Link iterate callbacks */
H5VL_DAOS_PRIVATE herr_t H5_daos_link_iterate_count_links_callback(hid_t group, const char *name,
    const H5L_info_t *info, void *op_data);

/* Group callbacks */
H5VL_DAOS_PRIVATE void *H5_daos_group_create(void *_item, const H5VL_loc_params_t *loc_params,
    const char *name, hid_t lcpl_id, hid_t gcpl_id, hid_t gapl_id, hid_t dxpl_id, void **req);
H5VL_DAOS_PRIVATE void *H5_daos_group_open(void *_item, const H5VL_loc_params_t *loc_params,
    const char *name, hid_t gapl_id, hid_t dxpl_id, void **req);
H5VL_DAOS_PRIVATE herr_t H5_daos_group_get(void *_item, H5VL_group_get_t get_type, hid_t dxpl_id,
    void **req, va_list arguments);
H5VL_DAOS_PRIVATE herr_t H5_daos_group_specific(void *_item, H5VL_group_specific_t specific_type,
    hid_t dxpl_id, void **req, va_list arguments);
H5VL_DAOS_PRIVATE herr_t H5_daos_group_close(void *_grp, hid_t dxpl_id, void **req);

/* Other group routines */
H5VL_DAOS_PRIVATE H5_daos_group_t *H5_daos_group_traverse(H5_daos_item_t *item, const char *path,
    hid_t dxpl_id, void **req, const char **obj_name, void **gcpl_buf_out,
    uint64_t *gcpl_len_out);
H5VL_DAOS_PRIVATE void *H5_daos_group_create_helper(H5_daos_file_t *file, hid_t gcpl_id,
    hid_t gapl_id, hid_t dxpl_id, H5_daos_req_t *req, H5_daos_group_t *parent_grp,
    const char *name, size_t name_len, hbool_t collective);
H5VL_DAOS_PRIVATE void *H5_daos_group_open_helper(H5_daos_file_t *file, daos_obj_id_t oid,
    hid_t gapl_id, hid_t dxpl_id, H5_daos_req_t *req, void **gcpl_buf_out,
    uint64_t *gcpl_len_out);
H5VL_DAOS_PRIVATE void *H5_daos_group_reconstitute(H5_daos_file_t *file, daos_obj_id_t oid,
    uint8_t *gcpl_buf, hid_t gapl_id, hid_t dxpl_id, H5_daos_req_t *req);

/* Dataset callbacks */
H5VL_DAOS_PRIVATE void *H5_daos_dataset_create(void *_item, const H5VL_loc_params_t *loc_params,
    const char *name, hid_t lcpl_id, hid_t type_id, hid_t space_id, hid_t dcpl_id,
    hid_t dapl_id, hid_t dxpl_id, void **req);
H5VL_DAOS_PRIVATE void *H5_daos_dataset_open(void *_item, const H5VL_loc_params_t *loc_params,
    const char *name, hid_t dapl_id, hid_t dxpl_id, void **req);
H5VL_DAOS_PRIVATE herr_t H5_daos_dataset_read(void *_dset, hid_t mem_type_id, hid_t mem_space_id,
    hid_t file_space_id, hid_t dxpl_id, void *buf, void **req);
H5VL_DAOS_PRIVATE herr_t H5_daos_dataset_write(void *_dset, hid_t mem_type_id, hid_t mem_space_id,
    hid_t file_space_id, hid_t dxpl_id, const void *buf, void **req);
H5VL_DAOS_PRIVATE herr_t H5_daos_dataset_get(void *_dset, H5VL_dataset_get_t get_type,
    hid_t dxpl_id, void **req, va_list arguments);
H5VL_DAOS_PRIVATE herr_t H5_daos_dataset_specific(void *_item, H5VL_dataset_specific_t specific_type,
    hid_t dxpl_id, void **req, va_list arguments);
H5VL_DAOS_PRIVATE herr_t H5_daos_dataset_close(void *_dset, hid_t dxpl_id, void **req);

/* Datatype callbacks */
H5VL_DAOS_PRIVATE void *H5_daos_datatype_commit(void *obj, const H5VL_loc_params_t *loc_params,
    const char *name, hid_t type_id, hid_t lcpl_id, hid_t tcpl_id,
    hid_t tapl_id, hid_t dxpl_id, void **req);
H5VL_DAOS_PRIVATE void *H5_daos_datatype_open(void *_item, const H5VL_loc_params_t *loc_params,
    const char *name, hid_t tapl_id, hid_t dxpl_id, void **req);
H5VL_DAOS_PRIVATE herr_t H5_daos_datatype_get(void *obj, H5VL_datatype_get_t get_type,
    hid_t dxpl_id, void **req, va_list arguments);
H5VL_DAOS_PRIVATE herr_t H5_daos_datatype_specific(void *_item, H5VL_datatype_specific_t specific_type,
    hid_t dxpl_id, void **req, va_list arguments);
H5VL_DAOS_PRIVATE herr_t H5_daos_datatype_close(void *_dtype, hid_t dxpl_id, void **req);

/* Other datatype routines */
H5VL_DAOS_PRIVATE herr_t H5_daos_tconv_init(hid_t src_type_id, size_t *src_type_size,
    hid_t dst_type_id, size_t *dst_type_size, size_t num_elem, void **tconv_buf,
    void **bkg_buf, H5_daos_tconv_reuse_t *reuse, hbool_t *fill_bkg);

/* Object callbacks */
H5VL_DAOS_PRIVATE void *H5_daos_object_open(void *_item, const H5VL_loc_params_t *loc_params,
    H5I_type_t *opened_type, hid_t dxpl_id, void **req);
H5VL_DAOS_PRIVATE herr_t H5_daos_object_copy(void *src_obj, const H5VL_loc_params_t *loc_params1,
    const char *src_name, void *dst_obj, const H5VL_loc_params_t *loc_params2,
    const char *dst_name, hid_t ocpypl_id, hid_t lcpl_id, hid_t dxpl_id, void **req);
H5VL_DAOS_PRIVATE herr_t H5_daos_object_get(void *_item, const H5VL_loc_params_t *loc_params,
    H5VL_object_get_t get_type, hid_t dxpl_id, void **req, va_list arguments);
H5VL_DAOS_PRIVATE herr_t H5_daos_object_specific(void *_item, const H5VL_loc_params_t *loc_params,
    H5VL_object_specific_t specific_type, hid_t dxpl_id, void **req, va_list arguments);
H5VL_DAOS_PRIVATE herr_t H5_daos_object_optional(void *_item, hid_t dxpl_id, void **req,
    va_list arguments);

/* Other object routines */
H5VL_DAOS_PRIVATE herr_t H5_daos_object_visit(H5_daos_obj_t *target_obj, H5_daos_iter_data_t *obj_iter_data);
H5VL_DAOS_PRIVATE herr_t H5_daos_object_close(void *_obj, hid_t dxpl_id, void **req);

/* Attribute callbacks */
H5VL_DAOS_PRIVATE void *H5_daos_attribute_create(void *_obj, const H5VL_loc_params_t *loc_params,
    const char *name, hid_t type_id, hid_t space_id, hid_t acpl_id,
    hid_t aapl_id, hid_t dxpl_id, void **req);
H5VL_DAOS_PRIVATE void *H5_daos_attribute_open(void *_obj, const H5VL_loc_params_t *loc_params,
    const char *name, hid_t aapl_id, hid_t dxpl_id, void **req);
H5VL_DAOS_PRIVATE herr_t H5_daos_attribute_read(void *_attr, hid_t mem_type_id, void *buf,
    hid_t dxpl_id, void **req);
H5VL_DAOS_PRIVATE herr_t H5_daos_attribute_write(void *_attr, hid_t mem_type_id, const void *buf,
    hid_t dxpl_id, void **req);
H5VL_DAOS_PRIVATE herr_t H5_daos_attribute_get(void *_item, H5VL_attr_get_t get_type,
    hid_t dxpl_id, void **req, va_list arguments);
H5VL_DAOS_PRIVATE herr_t H5_daos_attribute_specific(void *_item,
    const H5VL_loc_params_t *loc_params, H5VL_attr_specific_t specific_type,
    hid_t dxpl_id, void **req, va_list arguments);
H5VL_DAOS_PRIVATE herr_t H5_daos_attribute_close(void *_attr, hid_t dxpl_id, void **req);

/* Other attribute routines */
H5VL_DAOS_PRIVATE herr_t H5_daos_attribute_iterate(H5_daos_obj_t *attr_container_obj,
    H5_daos_iter_data_t *attr_iter_data, hid_t dxpl_id, void **req);

/* Request callback */
H5VL_DAOS_PRIVATE herr_t H5_daos_req_free(void *req);

/* Other request routines */
H5VL_DAOS_PRIVATE void H5_daos_req_free_int(void *_req);

/* Map callbacks */
H5VL_DAOS_PRIVATE void *H5_daos_map_create(void *_item,
    const H5VL_loc_params_t *loc_params, const char *name, hid_t lcpl_id,
    hid_t ktype_id, hid_t vtype_id, hid_t mcpl_id, hid_t mapl_id, hid_t dxpl_id, void **req);
H5VL_DAOS_PRIVATE void * H5_daos_map_open(void *_item,
    const H5VL_loc_params_t *loc_params, const char *name, hid_t mapl_id,
    hid_t dxpl_id, void **req);
H5VL_DAOS_PRIVATE herr_t H5_daos_map_get_val(void *_map, hid_t key_mem_type_id,
    const void *key,  hid_t val_mem_type_id, void *value, hid_t dxpl_id,
    void **req);
H5VL_DAOS_PRIVATE herr_t H5_daos_map_exists(void *_map, hid_t key_mem_type_id,
    const void *key, hbool_t *exists, hid_t dxpl_id, void **req);
H5VL_DAOS_PRIVATE herr_t H5_daos_map_put(void *_map, hid_t key_mem_type_id,
    const void *key,  hid_t val_mem_type_id, const void *value, hid_t dxpl_id,
    void **req);
H5VL_DAOS_PRIVATE herr_t H5_daos_map_specific(void *_item,
    const H5VL_loc_params_t *loc_params, H5VL_map_specific_t specific_type,
    hid_t dxpl_id, void **req, va_list arguments);
#ifdef DV_HAVE_MAP
H5PLUGIN_DLL herr_t H5_daos_map_get_types(void *_map, hid_t *key_type_id, hid_t *val_type_id, void **req);
H5PLUGIN_DLL herr_t H5_daos_map_get_count(void *_map, hsize_t *count, void **req);
#endif /* DV_HAVE_MAP */
H5VL_DAOS_PRIVATE herr_t H5_daos_map_close(void *_map, hid_t dxpl_id,
    void **req);

/* Helper routines */
H5VL_DAOS_PRIVATE herr_t H5_daos_file_flush(H5_daos_file_t *file);
H5VL_DAOS_PRIVATE herr_t H5_daos_group_flush(H5_daos_group_t *grp);
H5VL_DAOS_PRIVATE herr_t H5_daos_dataset_flush(H5_daos_dset_t *dset);
H5VL_DAOS_PRIVATE herr_t H5_daos_datatype_flush(H5_daos_dtype_t *dtype);
H5VL_DAOS_PRIVATE herr_t H5_daos_group_refresh(H5_daos_group_t *grp, hid_t dxpl_id, void **req);
H5VL_DAOS_PRIVATE herr_t H5_daos_dataset_refresh(H5_daos_dset_t *dset, hid_t dxpl_id, void **req);
H5VL_DAOS_PRIVATE herr_t H5_daos_datatype_refresh(H5_daos_dtype_t *dtype, hid_t dxpl_id, void **req);

#ifdef __cplusplus
}
#endif

#endif /* daos_vol_H */
