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
 * Purpose: The DAOS VOL connector where access is forwarded to the DAOS
 * library. Group routines.
 */

#include "daos_vol.h"           /* DAOS connector                          */

#include "util/daos_vol_err.h"  /* DAOS connector error handling           */
#include "util/daos_vol_mem.h"  /* DAOS connector memory management        */

static herr_t H5_daos_group_fill_gcpl_cache(H5_daos_group_t *grp);
static herr_t H5_daos_get_group_info(H5_daos_group_t *grp, H5G_info_t *group_info);


/*-------------------------------------------------------------------------
 * Function:    H5_daos_group_traverse
 *
 * Purpose:     Given a path name and base object, returns the final group
 *              in the path and the object name.  obj_name points into the
 *              buffer given by path, so it does not need to be freed.
 *              The group must be closed with H5_daos_group_close().
 *
 * Return:      Success:        group object. 
 *              Failure:        NULL
 *
 * Programmer:  Neil Fortner
 *              December, 2016
 *
 *-------------------------------------------------------------------------
 */
H5_daos_group_t *
H5_daos_group_traverse(H5_daos_item_t *item, const char *path,
    hid_t dxpl_id, void **req, const char **obj_name, void **gcpl_buf_out,
    uint64_t *gcpl_len_out)
{
    H5_daos_group_t *grp = NULL;
    const char *next_obj;
    daos_obj_id_t oid;
    H5_daos_group_t *ret_value = NULL;

    assert(item);
    assert(path);
    assert(obj_name);

    /* Initialize obj_name */
    *obj_name = path;

    /* Open starting group */
    if((*obj_name)[0] == '/') {
        grp = item->file->root_grp;
        (*obj_name)++;
    } /* end if */
    else {
        /* Check for the leading './' case */
        if ((*obj_name)[0] == '.' && (*obj_name)[1] == '/') {
            /*
             * Advance past the leading '.' and '/' characters.
             * Note that the case of multiple leading '.' characters
             * is not currently handled.
             */
            (*obj_name)++; (*obj_name)++;
        }

        if(item->type == H5I_GROUP)
            grp = (H5_daos_group_t *)item;
        else if(item->type == H5I_FILE)
            grp = ((H5_daos_file_t *)item)->root_grp;
        else
            D_GOTO_ERROR(H5E_ARGS, H5E_BADTYPE, NULL, "item not a file or group")
    } /* end else */

    grp->obj.item.rc++;

    /* Search for '/' */
    next_obj = strchr(*obj_name, '/');

    /* Traverse path */
    while(next_obj) {
        htri_t link_resolved;

        /* Free gcpl_buf_out */
        if(gcpl_buf_out)
            *gcpl_buf_out = DV_free(*gcpl_buf_out);

        /* Follow link to next group in path */
        assert(next_obj > *obj_name);
        if((link_resolved = H5_daos_link_follow(grp, *obj_name, (size_t)(next_obj - *obj_name), dxpl_id, req, &oid)) < 0)
            D_GOTO_ERROR(H5E_SYM, H5E_TRAVERSE, NULL, "can't follow link to group")
        if(!link_resolved)
            D_GOTO_ERROR(H5E_SYM, H5E_TRAVERSE, NULL, "link to group did not resolve")

        /* Close previous group */
        if(H5_daos_group_close(grp, dxpl_id, req) < 0)
            D_GOTO_ERROR(H5E_SYM, H5E_CLOSEERROR, NULL, "can't close group")
        grp = NULL;

        /* Open group */
        if(NULL == (grp = (H5_daos_group_t *)H5_daos_group_open_helper(item->file, oid, H5P_GROUP_ACCESS_DEFAULT, dxpl_id, NULL, gcpl_buf_out, gcpl_len_out)))
            D_GOTO_ERROR(H5E_SYM, H5E_CANTOPENOBJ, NULL, "can't open group")

        /* Advance to next path element */
        *obj_name = next_obj + 1;
        next_obj = strchr(*obj_name, '/');
    } /* end while */

    /* Set return value */
    ret_value = grp;

done:
    /* Cleanup on failure */
    if(NULL == ret_value)
        /* Close group */
        if(grp && H5_daos_group_close(grp, dxpl_id, req) < 0)
            D_DONE_ERROR(H5E_FILE, H5E_CLOSEERROR, NULL, "can't close group")

    D_FUNC_LEAVE
} /* end H5_daos_group_traverse() */


/*-------------------------------------------------------------------------
 * Function:    H5_daos_group_fill_gcpl_cache
 *
 * Purpose:     Fills the "gcpl_cache" field of the group struct, using
 *              the group's GCPL.  Assumes grp->gcpl_cache has been
 *              initialized to all zeros.
 *
 * Return:      Success:        0
 *              Failure:        -1
 *
 * Programmer:  Neil Fortner
 *              August, 2019
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5_daos_group_fill_gcpl_cache(H5_daos_group_t *grp)
{
    unsigned corder_flags;
    herr_t ret_value = SUCCEED;

    assert(grp);

    /* Determine if this group is tracking link creation order */
    if(H5Pget_link_creation_order(grp->gcpl_id, &corder_flags) < 0)
        D_GOTO_ERROR(H5E_SYM, H5E_CANTINIT, FAIL, "can't get link creation order flags")
    assert(!grp->gcpl_cache.track_corder);
    if(corder_flags & H5P_CRT_ORDER_TRACKED)
        grp->gcpl_cache.track_corder = TRUE;

done:
    D_FUNC_LEAVE
} /* end H5_daos_group_fill_gcpl_cache() */


/*-------------------------------------------------------------------------
 * Function:    H5_daos_group_create_helper
 *
 * Purpose:     Performs the actual group creation.
 *
 * Return:      Success:        group object. 
 *              Failure:        NULL
 *
 * Programmer:  Neil Fortner
 *              November, 2016
 *
 *-------------------------------------------------------------------------
 */
void *
H5_daos_group_create_helper(H5_daos_file_t *file, hid_t gcpl_id,
    hid_t gapl_id, hid_t dxpl_id, H5_daos_req_t *req,
    H5_daos_group_t *parent_grp, const char *name, size_t name_len,
    hbool_t collective)
{
    H5_daos_group_t *grp = NULL;
    void *gcpl_buf = NULL;
    H5_daos_md_update_cb_ud_t *update_cb_ud = NULL;
    hbool_t update_task_scheduled = FALSE;
    tse_task_t *finalize_task;
    int finalize_ndeps = 0;
    tse_task_t *finalize_deps[2];
    int ret;
    void *ret_value = NULL;

    assert(file);
    assert(file->flags & H5F_ACC_RDWR);

    /* Allocate the group object that is returned to the user */
    if(NULL == (grp = H5FL_CALLOC(H5_daos_group_t)))
        D_GOTO_ERROR(H5E_RESOURCE, H5E_CANTALLOC, NULL, "can't allocate DAOS group struct")
    grp->obj.item.type = H5I_GROUP;
    grp->obj.item.open_req = req;
    req->rc++;
    grp->obj.item.file = file;
    grp->obj.item.rc = 1;
    grp->obj.obj_oh = DAOS_HDL_INVAL;
    grp->gcpl_id = FAIL;
    grp->gapl_id = FAIL;

    /* Generate group oid */
    H5_daos_oid_encode(&grp->obj.oid, file->max_oid + (uint64_t)1, H5I_GROUP);

    /* Create group and write metadata if this process should */
    if(!collective || (file->my_rank == 0)) {
        size_t gcpl_size = 0;
        tse_task_t *update_task;
        tse_task_t *link_write_task;

        /* Create group */
        /* Update max_oid */
        file->max_oid = H5_daos_oid_to_idx(grp->obj.oid);

        /* Write max OID */
        if(H5_daos_write_max_oid(file) < 0)
            D_GOTO_ERROR(H5E_FILE, H5E_CANTINIT, NULL, "can't write max OID")

        /* Allocate argument struct */
        if(NULL == (update_cb_ud = (H5_daos_md_update_cb_ud_t *)DV_calloc(sizeof(H5_daos_md_update_cb_ud_t))))
            D_GOTO_ERROR(H5E_RESOURCE, H5E_CANTALLOC, NULL, "can't allocate buffer for update callback arguments")

        /* Open group */
        if(0 != (ret = daos_obj_open(file->coh, grp->obj.oid, DAOS_OO_RW, &grp->obj.obj_oh, NULL /*event*/)))
            D_GOTO_ERROR(H5E_SYM, H5E_CANTOPENOBJ, NULL, "can't open group: %s", H5_daos_err_to_string(ret))
        grp->obj.item.rc++;

        /* Encode GCPL */
        if(H5Pencode2(gcpl_id, NULL, &gcpl_size, file->fapl_id) < 0)
            D_GOTO_ERROR(H5E_ARGS, H5E_BADTYPE, NULL, "can't determine serialized length of gcpl")
        if(NULL == (gcpl_buf = DV_malloc(gcpl_size)))
            D_GOTO_ERROR(H5E_RESOURCE, H5E_CANTALLOC, NULL, "can't allocate buffer for serialized gcpl")
        if(H5Pencode2(gcpl_id, gcpl_buf, &gcpl_size, file->fapl_id) < 0)
            D_GOTO_ERROR(H5E_SYM, H5E_CANTENCODE, NULL, "can't serialize gcpl")

        /* Set up operation to write GCPL to group */
        /* Point to grp */
        update_cb_ud->obj = &grp->obj;

        /* Point to req */
        update_cb_ud->req = req;

        /* Set up dkey.  Point to global name buffer, do not free. */
        daos_iov_set(&update_cb_ud->dkey, (void *)H5_daos_int_md_key_g, H5_daos_int_md_key_size_g);
        update_cb_ud->free_dkey = FALSE;

        /* Single iod and sgl */
        update_cb_ud->nr = 1u;

        /* Set up iod.  Point akey to global name buffer, do not free. */
        daos_iov_set(&update_cb_ud->iod[0].iod_name, (void *)H5_daos_cpl_key_g, H5_daos_cpl_key_size_g);
        daos_csum_set(&update_cb_ud->iod[0].iod_kcsum, NULL, 0);
        update_cb_ud->iod[0].iod_nr = 1u;
        update_cb_ud->iod[0].iod_size = (uint64_t)gcpl_size;
        update_cb_ud->iod[0].iod_type = DAOS_IOD_SINGLE;
        update_cb_ud->free_akeys = FALSE;

        /* Set up sgl */
        daos_iov_set(&update_cb_ud->sg_iov[0], gcpl_buf, (daos_size_t)gcpl_size);
        update_cb_ud->sgl[0].sg_nr = 1;
        update_cb_ud->sgl[0].sg_nr_out = 0;
        update_cb_ud->sgl[0].sg_iovs = &update_cb_ud->sg_iov[0];

        /* Set task name */
        update_cb_ud->task_name = "group metadata write";

        /* Create task for group metadata write */
        if(0 != (ret = daos_task_create(DAOS_OPC_OBJ_UPDATE, &file->sched, 0, NULL, &update_task)))
            D_GOTO_ERROR(H5E_SYM, H5E_CANTINIT, NULL, "can't create task to write group medadata: %s", H5_daos_err_to_string(ret))

        /* Set callback functions for group metadata write */
        if(0 != (ret = tse_task_register_cbs(update_task, H5_daos_md_update_prep_cb, NULL, 0, H5_daos_md_update_comp_cb, NULL, 0)))
            D_GOTO_ERROR(H5E_SYM, H5E_CANTINIT, NULL, "can't register callbacks for task to write group medadata: %s", H5_daos_err_to_string(ret))

        /* Set private data for group metadata write */
        (void)tse_task_set_priv(update_task, update_cb_ud);

        /* Schedule group metadata write task and give it a reference to req */
        if(0 != (ret = tse_task_schedule(update_task, false)))
            D_GOTO_ERROR(H5E_SYM, H5E_CANTINIT, NULL, "can't schedule task to write group metadata: %s", H5_daos_err_to_string(ret))
        update_task_scheduled = TRUE;
        update_cb_ud->req->rc++;

        /* Add dependency for finalize task */
        finalize_deps[finalize_ndeps] = update_task;
        finalize_ndeps++;

        /* Write link to group if requested */
        if(parent_grp) {
            H5_daos_link_val_t link_val;

            link_val.type = H5L_TYPE_HARD;
            link_val.target.hard = grp->obj.oid;
            if(H5_daos_link_write(parent_grp, name, name_len, &link_val, req, &link_write_task) < 0)
                D_GOTO_ERROR(H5E_SYM, H5E_CANTINIT, NULL, "can't create link to group")
            finalize_deps[finalize_ndeps] = link_write_task;
            finalize_ndeps++;
        } /* end if */
    } /* end if */
    else {
        /* Update max_oid */
        file->max_oid = grp->obj.oid.lo;

        /* Note no barrier is currently needed here, daos_obj_open is a local
         * operation and can occur before the lead process writes metadata.  For
         * app-level synchronization we could add a barrier or bcast to the
         * calling functions (file_create, group_create) though it could only be
         * an issue with group reopen so we'll skip it for now.  There is
         * probably never an issue with file reopen since all commits are from
         * process 0, same as the group create above. */

        /* Open group */
        if(0 != (ret = daos_obj_open(file->coh, grp->obj.oid, DAOS_OO_RW, &grp->obj.obj_oh, NULL /*event*/)))
            D_GOTO_ERROR(H5E_SYM, H5E_CANTOPENOBJ, NULL, "can't open group: %s", H5_daos_err_to_string(ret))

        /* Check for failure of process 0 DSINC */
    } /* end else */

    /* Finish setting up group struct */
    if((grp->gcpl_id = H5Pcopy(gcpl_id)) < 0)
        D_GOTO_ERROR(H5E_SYM, H5E_CANTCOPY, NULL, "failed to copy gcpl")
    if((grp->gapl_id = H5Pcopy(gapl_id)) < 0)
        D_GOTO_ERROR(H5E_SYM, H5E_CANTCOPY, NULL, "failed to copy gapl")

    /* Fill GCPL cache */
    if(H5_daos_group_fill_gcpl_cache(grp) < 0)
        D_GOTO_ERROR(H5E_SYM, H5E_CANTINIT, NULL, "failed to fill GCPL cache")

    ret_value = (void *)grp;

done:
    /* Create task to finalize H5 operation */
    if(0 != (ret = tse_task_create(H5_daos_h5op_finalize, &file->sched, req, &finalize_task)))
        D_DONE_ERROR(H5E_SYM, H5E_CANTINIT, NULL, "can't create task to finalize H5 operation: %s", H5_daos_err_to_string(ret))
    /* Register dependencies (if any) */
    else if(finalize_ndeps > 0 && 0 != (ret = tse_task_register_deps(finalize_task, finalize_ndeps, finalize_deps)))
        D_DONE_ERROR(H5E_SYM, H5E_CANTINIT, NULL, "can't create dependencies for task to finalize H5 operation: %s", H5_daos_err_to_string(ret))
    /* Schedule finalize task */
    else if(0 != (ret = tse_task_schedule(finalize_task, false)))
        D_DONE_ERROR(H5E_SYM, H5E_CANTINIT, NULL, "can't schedule task to finalize H5 operation: %s", H5_daos_err_to_string(ret))
    else
        /* finalize_task now owns a reference to req */
        req->rc++;

    /* Cleanup on failure */
    /* Destroy DAOS object if created before failure DSINC */
    if(NULL == ret_value) {
        /* Close group */
        if(grp && H5_daos_group_close(grp, dxpl_id, NULL) < 0)
            D_DONE_ERROR(H5E_FILE, H5E_CLOSEERROR, NULL, "can't close group")

        /* Free memory */
        if(!update_task_scheduled) {
            if(update_cb_ud && update_cb_ud->obj && H5_daos_object_close(update_cb_ud->obj, dxpl_id, NULL) < 0)
                D_DONE_ERROR(H5E_FILE, H5E_CLOSEERROR, NULL, "can't close object")
            gcpl_buf = DV_free(gcpl_buf);
            update_cb_ud = DV_free(update_cb_ud);
        } /* end if */
    } /* end if */
    else
        assert(!gcpl_buf || update_task_scheduled);

    D_FUNC_LEAVE
} /* end H5_daos_group_create_helper() */


/*-------------------------------------------------------------------------
 * Function:    H5_daos_group_create
 *
 * Purpose:     Sends a request to DAOS to create a group
 *
 * Return:      Success:        group object. 
 *              Failure:        NULL
 *
 * Programmer:  Neil Fortner
 *              November, 2016
 *
 *-------------------------------------------------------------------------
 */
void *
H5_daos_group_create(void *_item,
    const H5VL_loc_params_t H5VL_DAOS_UNUSED *loc_params, const char *name,
    hid_t H5VL_DAOS_UNUSED lcpl_id, hid_t gcpl_id, hid_t gapl_id, hid_t dxpl_id,
    void H5VL_DAOS_UNUSED **req)
{
    H5_daos_item_t *item = (H5_daos_item_t *)_item;
    H5_daos_group_t *grp = NULL;
    H5_daos_group_t *target_grp = NULL;
    const char *target_name = NULL;
    hbool_t collective;
    H5_daos_req_t *int_req = NULL;
    int ret;
    void *ret_value = NULL;

    if(!_item)
        D_GOTO_ERROR(H5E_ARGS, H5E_BADVALUE, NULL, "group parent object is NULL")
    if(!loc_params)
        D_GOTO_ERROR(H5E_ARGS, H5E_BADVALUE, NULL, "location parameters object is NULL")

    /* Check for write access */
    if(!(item->file->flags & H5F_ACC_RDWR))
        D_GOTO_ERROR(H5E_FILE, H5E_BADVALUE, NULL, "no write intent on file")
 
    /*
     * Like HDF5, all metadata writes are collective by default. Once independent
     * metadata writes are implemented, we will need to check for this property.
     */
    collective = TRUE;

    /* Start H5 operation */
    if(NULL == (int_req = (H5_daos_req_t *)DV_malloc(sizeof(H5_daos_req_t))))
        D_GOTO_ERROR(H5E_RESOURCE, H5E_CANTALLOC, NULL, "can't allocate buffer for request")
    int_req->th = DAOS_TX_NONE;
    int_req->th_open = FALSE;
    int_req->file = item->file;
    int_req->file->item.rc++;
    int_req->rc = 1;
    int_req->status = H5_DAOS_INCOMPLETE;
    int_req->failed_task = NULL;

    if(!collective || (item->file->my_rank == 0)) {
        /* Start transaction */
        if(0 != (ret = daos_tx_open(item->file->coh, &int_req->th, NULL /*event*/)))
            D_GOTO_ERROR(H5E_SYM, H5E_CANTINIT, NULL, "can't start transaction")
        int_req->th_open = TRUE;

        /* Traverse the path */
        if(name)
            if(NULL == (target_grp = H5_daos_group_traverse(item, name, dxpl_id, NULL, &target_name, NULL, NULL)))
                D_GOTO_ERROR(H5E_SYM, H5E_BADITER, NULL, "can't traverse path")
    } /* end if */

    /* Create group and link to group */
    if(NULL == (grp = (H5_daos_group_t *)H5_daos_group_create_helper(item->file, gcpl_id, gapl_id, dxpl_id, int_req, target_grp, target_name, target_name ? strlen(target_name) : 0, collective)))
        D_GOTO_ERROR(H5E_SYM, H5E_CANTINIT, NULL, "can't create group")

    /* Set return value */
    ret_value = (void *)grp;

done:
    /* Close target group */
    if(target_grp && H5_daos_group_close(target_grp, dxpl_id, NULL) < 0)
        D_DONE_ERROR(H5E_SYM, H5E_CLOSEERROR, NULL, "can't close group")

    if(int_req) {
        /* Block until operation completes */
        {
            bool is_empty;

            /* Wait for scheduler to be empty *//* Change to custom progress function DSINC */
            if(0 != (ret = daos_progress(&item->file->sched, DAOS_EQ_WAIT, &is_empty)))
                D_DONE_ERROR(H5E_SYM, H5E_CANTINIT, NULL, "can't progress scheduler: %s", H5_daos_err_to_string(ret))

            /* Check for failure */
            if(int_req->status < 0)
                D_DONE_ERROR(H5E_SYM, H5E_CANTOPERATE, NULL, "group creation failed in task \"%s\": %s", int_req->failed_task, H5_daos_err_to_string(int_req->status))
        } /* end block */

        /* Close internal request */
        H5_daos_req_free_int(int_req);
    } /* end if */

    /* Cleanup on failure */
    /* Destroy DAOS object if created before failure DSINC */
    if(NULL == ret_value)
        /* Close group */
        if(grp && H5_daos_group_close(grp, dxpl_id, NULL) < 0)
            D_DONE_ERROR(H5E_SYM, H5E_CLOSEERROR, NULL, "can't close group")

    D_FUNC_LEAVE_API
} /* end H5_daos_group_create() */


/*-------------------------------------------------------------------------
 * Function:    H5_daos_group_open_helper
 *
 * Purpose:     Performs the actual group open, given the oid.
 *
 * Return:      Success:        group object. 
 *              Failure:        NULL
 *
 * Programmer:  Neil Fortner
 *              December, 2016
 *
 *-------------------------------------------------------------------------
 */
void *
H5_daos_group_open_helper(H5_daos_file_t *file, daos_obj_id_t oid,
    hid_t gapl_id, hid_t dxpl_id, H5_daos_req_t H5VL_DAOS_UNUSED *req, void **gcpl_buf_out,
    uint64_t *gcpl_len_out)
{
    H5_daos_group_t *grp = NULL;
    daos_key_t dkey;
    daos_iod_t iod;
    daos_sg_list_t sgl;
    daos_iov_t sg_iov;
    void *gcpl_buf = NULL;
    uint64_t gcpl_len;
    int ret;
    void *ret_value = NULL;

    assert(file);

    /* Allocate the group object that is returned to the user */
    if(NULL == (grp = H5FL_CALLOC(H5_daos_group_t)))
        D_GOTO_ERROR(H5E_RESOURCE, H5E_CANTALLOC, NULL, "can't allocate DAOS group struct")
    grp->obj.item.type = H5I_GROUP;
    grp->obj.item.open_req = NULL;
    grp->obj.item.file = file;
    grp->obj.item.rc = 1;
    grp->obj.oid = oid;
    grp->obj.obj_oh = DAOS_HDL_INVAL;
    grp->gcpl_id = FAIL;
    grp->gapl_id = FAIL;

    /* Open group */
    if(0 != (ret = daos_obj_open(file->coh, oid, file->flags & H5F_ACC_RDWR ? DAOS_COO_RW : DAOS_COO_RO, &grp->obj.obj_oh, NULL /*event*/)))
        D_GOTO_ERROR(H5E_FILE, H5E_CANTOPENOBJ, NULL, "can't open group: %s", H5_daos_err_to_string(ret))

    /* Set up operation to read GCPL size from group */
    /* Set up dkey */
    daos_iov_set(&dkey, (void *)H5_daos_int_md_key_g, H5_daos_int_md_key_size_g);

    /* Set up iod */
    memset(&iod, 0, sizeof(iod));
    daos_iov_set(&iod.iod_name, (void *)H5_daos_cpl_key_g, H5_daos_cpl_key_size_g);
    daos_csum_set(&iod.iod_kcsum, NULL, 0);
    iod.iod_nr = 1u;
    iod.iod_size = DAOS_REC_ANY;
    iod.iod_type = DAOS_IOD_SINGLE;

    /* Read internal metadata size from group */
    if(0 != (ret = daos_obj_fetch(grp->obj.obj_oh, DAOS_TX_NONE, &dkey, 1, &iod, NULL, NULL /*maps*/, NULL /*event*/)))
        D_GOTO_ERROR(H5E_SYM, H5E_CANTDECODE, NULL, "can't read metadata size from group: %s", H5_daos_err_to_string(ret))

    /* Check for metadata not found */
    if(iod.iod_size == (uint64_t)0)
        D_GOTO_ERROR(H5E_SYM, H5E_NOTFOUND, NULL, "internal metadata not found")

    /* Allocate buffer for GCPL */
    gcpl_len = iod.iod_size;
    if(NULL == (gcpl_buf = DV_malloc(gcpl_len)))
        D_GOTO_ERROR(H5E_RESOURCE, H5E_CANTALLOC, NULL, "can't allocate buffer for serialized gcpl")

    /* Set up sgl */
    daos_iov_set(&sg_iov, gcpl_buf, (daos_size_t)gcpl_len);
    sgl.sg_nr = 1;
    sgl.sg_nr_out = 0;
    sgl.sg_iovs = &sg_iov;

    /* Read internal metadata from group */
    if(0 != (ret = daos_obj_fetch(grp->obj.obj_oh, DAOS_TX_NONE, &dkey, 1, &iod, &sgl, NULL /*maps*/, NULL /*event*/)))
        D_GOTO_ERROR(H5E_SYM, H5E_CANTDECODE, NULL, "can't read metadata from group: %s", H5_daos_err_to_string(ret))

    /* Decode GCPL */
    if((grp->gcpl_id = H5Pdecode(gcpl_buf)) < 0)
        D_GOTO_ERROR(H5E_ARGS, H5E_CANTDECODE, NULL, "can't deserialize GCPL")

    /* Finish setting up group struct */
    if((grp->gapl_id = H5Pcopy(gapl_id)) < 0)
        D_GOTO_ERROR(H5E_SYM, H5E_CANTCOPY, NULL, "failed to copy gapl");

    /* Fill GCPL cache */
    if(H5_daos_group_fill_gcpl_cache(grp) < 0)
        D_GOTO_ERROR(H5E_SYM, H5E_CANTINIT, NULL, "failed to fill GCPL cache")

    /* Return GCPL info if requested, relinquish ownership of gcpl_buf if so */
    if(gcpl_buf_out) {
        assert(gcpl_len_out);
        assert(!*gcpl_buf_out);

        *gcpl_buf_out = gcpl_buf;
        gcpl_buf = NULL;

        *gcpl_len_out = gcpl_len;
    } /* end if */

    ret_value = (void *)grp;

done:
    /* Cleanup on failure */
    if(NULL == ret_value)
        /* Close group */
        if(grp && H5_daos_group_close(grp, dxpl_id, NULL) < 0)
            D_DONE_ERROR(H5E_SYM, H5E_CLOSEERROR, NULL, "can't close group")

    /* Free memory */
    gcpl_buf = DV_free(gcpl_buf);

    D_FUNC_LEAVE
} /* end H5_daos_group_open_helper() */


/*-------------------------------------------------------------------------
 * Function:    H5_daos_group_reconstitute
 *
 * Purpose:     Reconstitutes a group object opened by another process.
 *
 * Return:      Success:        group object.
 *              Failure:        NULL
 *
 * Programmer:  Neil Fortner
 *              April, 2017
 *
 *-------------------------------------------------------------------------
 */
void *
H5_daos_group_reconstitute(H5_daos_file_t *file, daos_obj_id_t oid,
    uint8_t *gcpl_buf, hid_t gapl_id, hid_t dxpl_id, H5_daos_req_t H5VL_DAOS_UNUSED *req)
{
    H5_daos_group_t *grp = NULL;
    int ret;
    void *ret_value = NULL;

    assert(file);

    /* Allocate the group object that is returned to the user */
    if(NULL == (grp = H5FL_CALLOC(H5_daos_group_t)))
        D_GOTO_ERROR(H5E_RESOURCE, H5E_CANTALLOC, NULL, "can't allocate DAOS group struct")
    grp->obj.item.type = H5I_GROUP;
    grp->obj.item.open_req = NULL;
    grp->obj.item.file = file;
    grp->obj.item.rc = 1;
    grp->obj.oid = oid;
    grp->obj.obj_oh = DAOS_HDL_INVAL;
    grp->gcpl_id = FAIL;
    grp->gapl_id = FAIL;

    /* Open group */
    if(0 != (ret = daos_obj_open(file->coh, oid, file->flags & H5F_ACC_RDWR ? DAOS_COO_RW : DAOS_COO_RO, &grp->obj.obj_oh, NULL /*event*/)))
        D_GOTO_ERROR(H5E_FILE, H5E_CANTOPENOBJ, NULL, "can't open group: %s", H5_daos_err_to_string(ret))

    /* Decode GCPL */
    if((grp->gcpl_id = H5Pdecode(gcpl_buf)) < 0)
        D_GOTO_ERROR(H5E_ARGS, H5E_CANTDECODE, NULL, "can't deserialize GCPL")

    /* Finish setting up group struct */
    if((grp->gapl_id = H5Pcopy(gapl_id)) < 0)
        D_GOTO_ERROR(H5E_SYM, H5E_CANTCOPY, NULL, "failed to copy gapl");

    /* Fill GCPL cache */
    if(H5_daos_group_fill_gcpl_cache(grp) < 0)
        D_GOTO_ERROR(H5E_SYM, H5E_CANTINIT, NULL, "failed to fill GCPL cache")

    ret_value = (void *)grp;

done:
    /* Cleanup on failure */
    if(NULL == ret_value)
        /* Close group */
        if(grp && H5_daos_group_close(grp, dxpl_id, NULL) < 0)
            D_DONE_ERROR(H5E_SYM, H5E_CLOSEERROR, NULL, "can't close group")

    D_FUNC_LEAVE
} /* end H5_daos_group_reconstitute() */


/*-------------------------------------------------------------------------
 * Function:    H5_daos_group_open
 *
 * Purpose:     Sends a request to DAOS to open a group
 *
 * Return:      Success:        group object. 
 *              Failure:        NULL
 *
 * Programmer:  Neil Fortner
 *              November, 2016
 *
 *-------------------------------------------------------------------------
 */
void *
H5_daos_group_open(void *_item, const H5VL_loc_params_t *loc_params,
    const char *name, hid_t gapl_id, hid_t dxpl_id, void **req)
{
    H5_daos_item_t *item = (H5_daos_item_t *)_item;
    H5_daos_group_t *grp = NULL;
    H5_daos_group_t *target_grp = NULL;
    const char *target_name = NULL;
    daos_obj_id_t oid;
    uint8_t *gcpl_buf = NULL;
    uint64_t gcpl_len = 0;
    uint8_t ginfo_buf_static[H5_DAOS_GINFO_BUF_SIZE];
    uint8_t *p;
    hbool_t collective;
    hbool_t must_bcast = FALSE;
    void *ret_value = NULL;

    if(!_item)
        D_GOTO_ERROR(H5E_ARGS, H5E_BADVALUE, NULL, "group parent object is NULL")
    if(!loc_params)
        D_GOTO_ERROR(H5E_ARGS, H5E_BADVALUE, NULL, "location parameters object is NULL")
 
    /*
     * Like HDF5, metadata reads are independent by default. If the application has specifically
     * requested collective metadata reads, they will be enabled here.
     */
    collective = item->file->is_collective_md_read;
    if(!collective && (H5P_GROUP_ACCESS_DEFAULT != gapl_id))
        if(H5Pget_all_coll_metadata_ops(gapl_id, &collective) < 0)
            D_GOTO_ERROR(H5E_SYM, H5E_CANTGET, NULL, "can't get collective metadata reads property")

    /* Check if we're actually opening the group or just receiving the group
     * info from the leader */
    if(!collective || (item->file->my_rank == 0)) {
        if(collective && (item->file->num_procs > 1))
            must_bcast = TRUE;

        /* Check for open by address */
        if(H5VL_OBJECT_BY_ADDR == loc_params->type) {
            /* Generate oid from address */
            memset(&oid, 0, sizeof(oid));
            H5_daos_oid_generate(&oid, (uint64_t)loc_params->loc_data.loc_by_addr.addr, H5I_GROUP);

            /* Open group */
            if(NULL == (grp = (H5_daos_group_t *)H5_daos_group_open_helper(item->file, oid, gapl_id, dxpl_id, NULL, (collective && (item->file->num_procs > 1)) ? (void **)&gcpl_buf : NULL, &gcpl_len)))
                D_GOTO_ERROR(H5E_SYM, H5E_CANTOPENOBJ, NULL, "can't open group")
        } /* end if */
        else {
            /* Open using name parameter */
            if(H5VL_OBJECT_BY_SELF != loc_params->type)
                D_GOTO_ERROR(H5E_ARGS, H5E_UNSUPPORTED, NULL, "unsupported group open location parameters type")
            if(!name)
                D_GOTO_ERROR(H5E_ARGS, H5E_BADVALUE, NULL, "group name is NULL")

            /* Traverse the path */
            if(NULL == (target_grp = H5_daos_group_traverse(item, name, dxpl_id, req, &target_name, (collective && (item->file->num_procs > 1)) ? (void **)&gcpl_buf : NULL, &gcpl_len)))
                D_GOTO_ERROR(H5E_SYM, H5E_BADITER, NULL, "can't traverse path")

            /* Check for no target_name, in this case just return target_grp */
            if(target_name[0] == '\0'
                    || (target_name[0] == '.' && target_name[1] == '\0')) {
                size_t gcpl_size;

                /* Take ownership of target_grp */
                grp = target_grp;
                target_grp = NULL;

                /* Encode GCPL */
                if(H5Pencode2(grp->gcpl_id, NULL, &gcpl_size, item->file->fapl_id) < 0)
                    D_GOTO_ERROR(H5E_ARGS, H5E_BADTYPE, NULL, "can't determine serialized length of gcpl")
                if(NULL == (gcpl_buf = (uint8_t *)DV_malloc(gcpl_size)))
                    D_GOTO_ERROR(H5E_RESOURCE, H5E_CANTALLOC, NULL, "can't allocate buffer for serialized gcpl")
                gcpl_len = (uint64_t)gcpl_size;
                if(H5Pencode2(grp->gcpl_id, gcpl_buf, &gcpl_size, item->file->fapl_id) < 0)
                    D_GOTO_ERROR(H5E_SYM, H5E_CANTENCODE, NULL, "can't serialize gcpl")
            } /* end if */
            else {
                htri_t link_resolved;

                gcpl_buf = (uint8_t *)DV_free(gcpl_buf);
                gcpl_len = 0;

                /* Follow link to group */
                if((link_resolved = H5_daos_link_follow(target_grp, target_name, strlen(target_name), dxpl_id, req, &oid)) < 0)
                    D_GOTO_ERROR(H5E_SYM, H5E_TRAVERSE, NULL, "can't follow link to group")
                if(!link_resolved)
                    D_GOTO_ERROR(H5E_SYM, H5E_TRAVERSE, NULL, "link to group did not resolve")

                /* Open group */
                if(NULL == (grp = (H5_daos_group_t *)H5_daos_group_open_helper(item->file, oid, gapl_id, dxpl_id, NULL, (collective && (item->file->num_procs > 1)) ? (void **)&gcpl_buf : NULL, &gcpl_len)))
                    D_GOTO_ERROR(H5E_SYM, H5E_CANTOPENOBJ, NULL, "can't open group")
            } /* end else */
        } /* end else */

        /* Broadcast group info if there are other processes that need it */
        if(collective && (item->file->num_procs > 1)) {
            assert(gcpl_buf);
            assert(sizeof(ginfo_buf_static) >= 3 * sizeof(uint64_t));

            /* Encode oid */
            p = ginfo_buf_static;
            UINT64ENCODE(p, grp->obj.oid.lo)
            UINT64ENCODE(p, grp->obj.oid.hi)

            /* Encode GCPL length */
            UINT64ENCODE(p, gcpl_len)

            /* Copy GCPL to ginfo_buf_static if it will fit */
            if((gcpl_len + 3 * sizeof(uint64_t)) <= sizeof(ginfo_buf_static))
                (void)memcpy(p, gcpl_buf, gcpl_len);

            /* We are about to bcast so we no longer need to bcast on failure */
            must_bcast = FALSE;

            /* MPI_Bcast ginfo_buf */
            if(MPI_SUCCESS != MPI_Bcast((char *)ginfo_buf_static, sizeof(ginfo_buf_static), MPI_BYTE, 0, item->file->comm))
                D_GOTO_ERROR(H5E_SYM, H5E_MPI, NULL, "can't broadcast group info")

            /* Need a second bcast if it did not fit in the receivers' static
             * buffer */
            if(gcpl_len + 3 * sizeof(uint64_t) > sizeof(ginfo_buf_static))
                if(MPI_SUCCESS != MPI_Bcast((char *)gcpl_buf, (int)gcpl_len, MPI_BYTE, 0, item->file->comm))
                    D_GOTO_ERROR(H5E_SYM, H5E_MPI, NULL, "can't broadcast GCPL")
        } /* end if */
    } /* end if */
    else {
        /* Receive GCPL */
        if(MPI_SUCCESS != MPI_Bcast((char *)ginfo_buf_static, sizeof(ginfo_buf_static), MPI_BYTE, 0, item->file->comm))
            D_GOTO_ERROR(H5E_SYM, H5E_MPI, NULL, "can't receive broadcasted group info")

        /* Decode oid */
        p = ginfo_buf_static;
        UINT64DECODE(p, oid.lo)
        UINT64DECODE(p, oid.hi)

        /* Decode GCPL length */
        UINT64DECODE(p, gcpl_len)

        /* Check for gcpl_len set to 0 - indicates failure */
        if(gcpl_len == 0)
            D_GOTO_ERROR(H5E_SYM, H5E_CANTINIT, NULL, "lead process failed to open group")

        /* Check if we need to perform another bcast */
        if(gcpl_len + 3 * sizeof(uint64_t) > sizeof(ginfo_buf_static)) {
            /* Allocate a dynamic buffer if necessary */
            if(gcpl_len > sizeof(ginfo_buf_static)) {
                if(NULL == (gcpl_buf = (uint8_t *)DV_malloc(gcpl_len)))
                    D_GOTO_ERROR(H5E_RESOURCE, H5E_CANTALLOC, NULL, "can't allocate space for gcpl")
                p = gcpl_buf;
            } /* end if */
            else
                p = ginfo_buf_static;

            /* Receive GCPL */
            if(MPI_SUCCESS != MPI_Bcast((char *)p, (int)gcpl_len, MPI_BYTE, 0, item->file->comm))
                D_GOTO_ERROR(H5E_SYM, H5E_MPI, NULL, "can't receive broadcasted GCPL")
        } /* end if */

        /* Reconstitute group from received oid and GCPL buffer */
        if(NULL == (grp = (H5_daos_group_t *)H5_daos_group_reconstitute(item->file, oid, p, gapl_id, dxpl_id, NULL)))
            D_GOTO_ERROR(H5E_SYM, H5E_CANTINIT, NULL, "can't reconstitute group")
    } /* end else */

    /* Set return value */
    ret_value = (void *)grp;

done:
    /* Cleanup on failure */
    if(NULL == ret_value) {
        /* Bcast gcpl_buf as '0' if necessary - this will trigger failures in
         * other processes so we do not need to do the second bcast. */
        if(must_bcast) {
            memset(ginfo_buf_static, 0, sizeof(ginfo_buf_static));
            if(MPI_SUCCESS != MPI_Bcast(ginfo_buf_static, sizeof(ginfo_buf_static), MPI_BYTE, 0, item->file->comm))
                D_DONE_ERROR(H5E_SYM, H5E_MPI, NULL, "can't broadcast empty group info")
        } /* end if */

        /* Close group */
        if(grp && H5_daos_group_close(grp, dxpl_id, NULL) < 0)
            D_DONE_ERROR(H5E_SYM, H5E_CLOSEERROR, NULL, "can't close group")
    } /* end if */

    /* Close target group */
    if(target_grp && H5_daos_group_close(target_grp, dxpl_id, NULL) < 0)
        D_DONE_ERROR(H5E_SYM, H5E_CLOSEERROR, NULL, "can't close group")

    /* Free memory */
    gcpl_buf = (uint8_t *)DV_free(gcpl_buf);

    D_FUNC_LEAVE_API
} /* end H5_daos_group_open() */


/*-------------------------------------------------------------------------
 * Function:    H5_daos_group_get
 *
 * Purpose:     Performs a group "get" operation
 *
 * Return:      Success:        0
 *              Failure:        -1
 *
 * Programmer:  Jordan Henderson
 *              January, 2019
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5_daos_group_get(void *_item, H5VL_group_get_t get_type, hid_t dxpl_id,
    void **req, va_list arguments)
{
    H5_daos_group_t *grp = (H5_daos_group_t *)_item;
    H5_daos_group_t *target_group = NULL;
    herr_t           ret_value = SUCCEED;

    if(!_item)
        D_GOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "VOL object is NULL")
    if(H5I_FILE != grp->obj.item.type && H5I_GROUP != grp->obj.item.type)
        D_GOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "object is not a file or group")

    switch (get_type) {
        /* H5Gget_create_plist */
        case H5VL_GROUP_GET_GCPL:
        {
            hid_t *ret_id = va_arg(arguments, hid_t *);

            if((*ret_id = H5Pcopy(grp->gcpl_id)) < 0)
                D_GOTO_ERROR(H5E_PLIST, H5E_CANTCOPY, FAIL, "can't get group's GCPL")

            break;
        } /* H5VL_GROUP_GET_GCPL */

        /* H5Gget_info(_by_name/by_idx) */
        case H5VL_GROUP_GET_INFO:
        {
            const H5VL_loc_params_t *loc_params = va_arg(arguments, const H5VL_loc_params_t *);
            H5G_info_t        *group_info = va_arg(arguments, H5G_info_t *);

            switch (loc_params->type) {
                /* H5Gget_info */
                case H5VL_OBJECT_BY_SELF:
                {
                    if(grp->obj.item.type == H5I_FILE)
                        grp = ((H5_daos_file_t *) grp)->root_grp;

                    if((H5_daos_get_group_info(grp, group_info)) < 0)
                        D_GOTO_ERROR(H5E_SYM, H5E_CANTGET, FAIL, "can't get group's info")

                    break;
                } /* H5VL_OBJECT_BY_SELF */

                /* H5Gget_info_by_name */
                case H5VL_OBJECT_BY_NAME:
                {
                    const char *target_group_name = NULL;

                    /*
                     * Locate the object by name.
                     */
                    if(NULL == (target_group = H5_daos_group_traverse(&grp->obj.item, loc_params->loc_data.loc_by_name.name,
                            dxpl_id, req, &target_group_name, NULL, NULL)))
                        D_GOTO_ERROR(H5E_SYM, H5E_BADITER, FAIL, "can't traverse path")

                    /* Check for target_group_name, in which case we have to follow the link
                     * to the next group; otherwise just retrieve the info of target_group */
                    if(target_group_name[0] != '\0'
                            && (target_group_name[0] != '.' || target_group_name[1] != '\0')) {
                        daos_obj_id_t oid;
                        htri_t link_resolved;

                        /* Follow link to group */
                        if((link_resolved = H5_daos_link_follow(target_group, target_group_name, strlen(target_group_name), dxpl_id, req, &oid)) < 0)
                            D_GOTO_ERROR(H5E_SYM, H5E_TRAVERSE, FAIL, "can't follow link to group")
                        if(!link_resolved)
                            D_GOTO_ERROR(H5E_SYM, H5E_TRAVERSE, FAIL, "link to group did not resolve")

                        /* "Close" the group in order to decrement the group's reference count.
                         * Note that this will not actually close the group, but is needed due
                         * to the H5_daos_group_traverse call above having incremented the count.
                         */
                        if(H5_daos_group_close(target_group, dxpl_id, req) < 0)
                            D_GOTO_ERROR(H5E_SYM, H5E_CANTCLOSEOBJ, FAIL, "can't close group")

                        /* Open group */
                        if(NULL == (target_group = (H5_daos_group_t *)H5_daos_group_open_helper(grp->obj.item.file, oid, target_group->gapl_id,
                                dxpl_id, NULL, NULL, NULL)))
                            D_GOTO_ERROR(H5E_SYM, H5E_CANTOPENOBJ, FAIL, "can't open group")
                    } /* end else */

                    if((H5_daos_get_group_info(target_group, group_info)) < 0)
                        D_GOTO_ERROR(H5E_SYM, H5E_CANTGET, FAIL, "can't get group's info")

                    break;
                } /* H5VL_OBJECT_BY_NAME */

                /* H5Gget_info_by_idx */
                case H5VL_OBJECT_BY_IDX:
                {
                    D_GOTO_ERROR(H5E_SYM, H5E_UNSUPPORTED, FAIL, "H5Gget_info_by_idx is unsupported")
                    break;
                } /* H5VL_OBJECT_BY_IDX */

                case H5VL_OBJECT_BY_ADDR:
                case H5VL_OBJECT_BY_REF:
                default:
                    D_GOTO_ERROR(H5E_SYM, H5E_BADVALUE, FAIL, "invalid loc_params type")
            }

            break;
        } /* H5VL_GROUP_GET_INFO */

        default:
            D_GOTO_ERROR(H5E_VOL, H5E_UNSUPPORTED, FAIL, "invalid or unsupported group get operation")
    } /* end switch */

done:
    if(target_group && H5_daos_group_close(target_group, dxpl_id, req) < 0)
        D_DONE_ERROR(H5E_SYM, H5E_CANTCLOSEOBJ, FAIL, "can't close group")

    D_FUNC_LEAVE_API
} /* end H5_daos_group_get() */


/*-------------------------------------------------------------------------
 * Function:    H5_daos_group_specific
 *
 * Purpose:     Performs a group "specific" operation
 *
 * Return:      Success:        0
 *              Failure:        -1
 *
 * Programmer:  Jordan Henderson
 *              January, 2019
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5_daos_group_specific(void *_item, H5VL_group_specific_t specific_type,
    hid_t H5VL_DAOS_UNUSED dxpl_id, void H5VL_DAOS_UNUSED **req, va_list H5VL_DAOS_UNUSED arguments)
{
    H5_daos_group_t *grp = (H5_daos_group_t *)_item;
    herr_t           ret_value = SUCCEED;

    if(!_item)
        D_GOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "VOL object is NULL")
    if(H5I_FILE != grp->obj.item.type && H5I_GROUP != grp->obj.item.type)
        D_GOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "object is not a file or group")

    switch (specific_type) {
        /* H5Gflush */
        case H5VL_GROUP_FLUSH:
        {
            if (H5_daos_group_flush(grp) < 0)
                D_GOTO_ERROR(H5E_SYM, H5E_WRITEERROR, FAIL, "can't flush group")

            break;
        } /* H5VL_GROUP_FLUSH */

        case H5VL_GROUP_REFRESH:
        default:
            D_GOTO_ERROR(H5E_VOL, H5E_UNSUPPORTED, FAIL, "invalid or unsupported group specific operation")
    } /* end switch */

done:
    D_FUNC_LEAVE_API
} /* end H5_daos_group_specific() */


/*-------------------------------------------------------------------------
 * Function:    H5_daos_group_close
 *
 * Purpose:     Closes a daos HDF5 group.
 *
 * Return:      Success:        0
 *              Failure:        -1
 *
 * Programmer:  Neil Fortner
 *              November, 2016
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5_daos_group_close(void *_grp, hid_t H5VL_DAOS_UNUSED dxpl_id,
    void H5VL_DAOS_UNUSED **req)
{
    H5_daos_group_t *grp = (H5_daos_group_t *)_grp;
    int ret;
    herr_t ret_value = SUCCEED;

    if(!_grp)
        D_GOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "group object is NULL")

    if(--grp->obj.item.rc == 0) {
        /* Free group data structures */
        if(grp->obj.item.open_req)
            H5_daos_req_free_int(grp->obj.item.open_req);
        if(!daos_handle_is_inval(grp->obj.obj_oh))
            if(0 != (ret = daos_obj_close(grp->obj.obj_oh, NULL /*event*/)))
                D_DONE_ERROR(H5E_SYM, H5E_CANTCLOSEOBJ, FAIL, "can't close group DAOS object: %s", H5_daos_err_to_string(ret))
        if(grp->gcpl_id != FAIL && H5Idec_ref(grp->gcpl_id) < 0)
            D_DONE_ERROR(H5E_SYM, H5E_CANTDEC, FAIL, "failed to close gcpl")
        if(grp->gapl_id != FAIL && H5Idec_ref(grp->gapl_id) < 0)
            D_DONE_ERROR(H5E_SYM, H5E_CANTDEC, FAIL, "failed to close gapl")
        grp = H5FL_FREE(H5_daos_group_t, grp);
    } /* end if */

done:
    D_FUNC_LEAVE_API
} /* end H5_daos_group_close() */


/*-------------------------------------------------------------------------
 * Function:    H5_daos_group_flush
 *
 * Purpose:     Flushes a DAOS group.  Currently a no-op, may create a
 *              snapshot in the future.
 *
 * Return:      Success:        0
 *              Failure:        -1
 *
 * Programmer:  Jordan Henderson
 *              February, 2019
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5_daos_group_flush(H5_daos_group_t *grp)
{
    herr_t ret_value = SUCCEED;    /* Return value */

    assert(grp);

    /* Nothing to do if no write intent */
    if(!(grp->obj.item.file->flags & H5F_ACC_RDWR))
        D_GOTO_DONE(SUCCEED)

    /* Progress scheduler until empty? DSINC */

done:
    D_FUNC_LEAVE
} /* end H5_daos_group_flush() */


/*-------------------------------------------------------------------------
 * Function:    H5_daos_group_refresh
 *
 * Purpose:     Refreshes a DAOS group (currently a no-op)
 *
 * Return:      Success:        0
 *              Failure:        -1
 *
 * Programmer:  Jordan Henderson
 *              July, 2019
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5_daos_group_refresh(H5_daos_group_t *grp, hid_t H5VL_DAOS_UNUSED dxpl_id,
    void H5VL_DAOS_UNUSED **req)
{
    herr_t ret_value = SUCCEED;

    assert(grp);

    D_GOTO_DONE(SUCCEED)

done:
    D_FUNC_LEAVE
} /* end H5_daos_group_refresh() */


/*-------------------------------------------------------------------------
 * Function:    H5_daos_get_group_info
 *
 * Purpose:     Retrieves a group's info, storing the results in the
 *              supplied H5G_info_t.
 *
 * Return:      Success:        0
 *              Failure:        -1
 *
 * Programmer:  Jordan Henderson
 *              February, 2019
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5_daos_get_group_info(H5_daos_group_t *grp, H5G_info_t *group_info)
{
    H5_daos_iter_data_t iter_data;
    H5G_info_t local_grp_info;
    hid_t target_grp_id = -1;
    herr_t ret_value = SUCCEED;

    assert(grp);
    assert(group_info);

    local_grp_info.storage_type = H5G_STORAGE_TYPE_UNKNOWN;
    local_grp_info.nlinks = 0;
    local_grp_info.max_corder = 0; /* TODO: retrieve max creation order of group */
    local_grp_info.mounted = FALSE; /* DSINC - will file mounting be supported? */

    /* Register id for grp */
    if((target_grp_id = H5VLwrap_register(grp, H5I_GROUP)) < 0)
        D_GOTO_ERROR(H5E_ATOM, H5E_CANTREGISTER, FAIL, "unable to atomize object handle")
    grp->obj.item.rc++;

    /* Initialize iteration data */
    H5_DAOS_ITER_DATA_INIT(iter_data, H5_DAOS_ITER_TYPE_LINK, H5_INDEX_NAME, H5_ITER_NATIVE,
            FALSE, NULL, target_grp_id, &local_grp_info.nlinks, H5P_DATASET_XFER_DEFAULT, NULL);
    iter_data.u.link_iter_data.link_iter_op = H5_daos_link_iterate_count_links_callback;

    /* Retrieve the number of links in the group. */
    if(H5_daos_link_iterate(grp, &iter_data) < 0)
        D_GOTO_ERROR(H5E_SYM, H5E_CANTGET, FAIL, "can't retrieve the number of links in group")

    memcpy(group_info, &local_grp_info, sizeof(*group_info));

done:
    if(target_grp_id >= 0) {
        if(H5Idec_ref(target_grp_id) < 0)
            D_DONE_ERROR(H5E_SYM, H5E_CLOSEERROR, FAIL, "can't close group ID")
        target_grp_id = -1;
    } /* end if */

    D_FUNC_LEAVE
} /* end H5_daos_get_group_info() */
