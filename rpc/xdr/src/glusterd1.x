 enum glusterd_volume_status {
        GLUSTERD_STATUS_NONE = 0,
        GLUSTERD_STATUS_STARTED,
        GLUSTERD_STATUS_STOPPED
} ;

 struct gd1_mgmt_probe_req {
        unsigned char  uuid[16];
        string  hostname<>;
}  ;

 struct gd1_mgmt_probe_rsp {
        unsigned char  uuid[16];
        string  hostname<>;
}  ;

struct gd1_mgmt_friend_req {
        unsigned char  uuid[16];
        string  hostname<>;
}  ;

struct gd1_mgmt_friend_rsp {
        unsigned char  uuid[16];
        string  hostname<>;
        int     op_ret;
        int     op_errno;
}  ;

struct gd1_mgmt_unfriend_req {
        unsigned char  uuid[16];
        string  hostname<>;
}  ;

struct gd1_mgmt_unfriend_rsp {
        unsigned char  uuid[16];
        string  hostname<>;
        int     op_ret;
        int     op_errno;
}  ;

struct gd1_mgmt_cluster_lock_req {
        unsigned char  uuid[16];
}  ;

struct gd1_mgmt_cluster_lock_rsp {
        unsigned char  uuid[16];
        int     op_ret;
        int     op_errno;
}  ;

struct gd1_mgmt_cluster_unlock_req {
        unsigned char  uuid[16];
}  ;

struct gd1_mgmt_cluster_unlock_rsp {
        unsigned char  uuid[16];
        int     op_ret;
        int     op_errno;
}  ;

struct gd1_mgmt_stage_op_req {
        unsigned char  uuid[16];
        int     op;
        opaque  buf<>;
}  ;


struct gd1_mgmt_stage_op_rsp {
        unsigned char  uuid[16];
        int     op;
        int     op_ret;
        int     op_errno;
}  ;

struct gd1_mgmt_commit_op_req {
        unsigned char  uuid[16];
        int     op;
        opaque  buf<>;
}  ;


struct gd1_mgmt_commit_op_rsp {
        unsigned char  uuid[16];
        int     op;
        int     op_ret;
        int     op_errno;
}  ;

struct gd1_mgmt_friend_update {
        unsigned char uuid[16];
        opaque  friends<>;
} ;

struct gd1_mgmt_friend_update_rsp {
        unsigned char  uuid[16];
        int     op;
        int     op_ret;
        int     op_errno;
}  ;
