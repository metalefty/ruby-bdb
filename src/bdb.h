#include <ruby.h>
#include <version.h>
#include <rubysig.h>
#include <db.h>
#include <errno.h>

#if defined(__cplusplus)
extern "C" {
#endif

#define BDB_MARSHAL 1
#define BDB_TXN 2
#define BDB_RE_SOURCE 4
#define BDB_BT_COMPARE 8
#define BDB_BT_PREFIX  16
#define BDB_DUP_COMPARE 32
#define BDB_H_HASH 64
#define BDB_FUNCTION (BDB_BT_COMPARE|BDB_BT_PREFIX|BDB_DUP_COMPARE|BDB_H_HASH)

#if (DB_VERSION_MAJOR == 3 && DB_VERSION_MINOR >= 3) || DB_VERSION_MAJOR >= 4
#define BDB_ERROR_PRIVATE 44444
#endif

#define BDB_INIT_TRANSACTION (DB_INIT_LOCK | DB_INIT_MPOOL | DB_INIT_TXN | DB_INIT_LOG)
#define BDB_INIT_LOMP (DB_INIT_LOCK | DB_INIT_MPOOL | DB_INIT_LOG)
#define BDB_NEED_CURRENT (BDB_MARSHAL | BDB_BT_COMPARE | BDB_BT_PREFIX | BDB_DUP_COMPARE | BDB_H_HASH)

#if DB_VERSION_MAJOR == 2 && DB_VERSION_MINOR < 6
#define DB_RMW 0
#endif

#define BDB_TXN_COMMIT 0x80

extern VALUE bdb_cEnv;
extern VALUE bdb_eFatal;
extern VALUE bdb_eLock, bdb_eLockDead, bdb_eLockHeld, bdb_eLockGranted;
extern VALUE bdb_mDb;
extern VALUE bdb_cCommon, bdb_cBtree, bdb_cRecnum, bdb_cHash, bdb_cRecno, bdb_cUnknown;
extern VALUE bdb_cDelegate;

#if (DB_VERSION_MAJOR == 3 && DB_VERSION_MINOR >= 1) || DB_VERSION_MAJOR >= 4
extern VALUE bdb_sKeyrange;
#endif

#if DB_VERSION_MAJOR >= 3
extern VALUE bdb_cQueue;
#endif

extern VALUE bdb_cTxn, bdb_cTxnCatch;
extern VALUE bdb_cCursor;
extern VALUE bdb_cLock, bdb_cLockid;
extern VALUE bdb_cLsn;

extern VALUE bdb_mMarshal;

extern ID id_dump, id_load;
extern ID id_current_db;

extern VALUE bdb_deleg_to_orig _((VALUE));

#if DB_VERSION_MAJOR >= 4
extern VALUE bdb_env_s_rslbl _((int, VALUE *, VALUE, DB_ENV *));
#endif

typedef struct  {
    int no_thread;
    int flags27;
    VALUE marshal;
    VALUE db_ary;
    VALUE home;
    DB_ENV *dbenvp;
#if DB_VERSION_MAJOR < 3 || 					\
    (DB_VERSION_MAJOR == 3 &&					\
       (DB_VERSION_MINOR < 1 || 				\
        (DB_VERSION_MINOR == 1 && DB_VERSION_PATCH <= 5)))
    u_int32_t fidp;
#endif
#if DB_VERSION_MAJOR >= 4
    VALUE rep_transport;
#endif
} bdb_ENV;

#if DB_VERSION_MAJOR >= 4

struct txn_rslbl {
    DB_TXN *txn;
    void *txn_cxx;
};

#endif

typedef struct {
    int status, commit;
    VALUE marshal, mutex;
    int flags27;
    VALUE db_ary;
    VALUE env;
    DB_TXN *txnid;
    DB_TXN *parent;
#if DB_VERSION_MAJOR >= 4
    void *txn_cxx;
#endif
} bdb_TXN;

#define FILTER_KEY 0
#define FILTER_VALUE 1

typedef struct {
    int options, no_thread;
    VALUE marshal;
    int flags27;
    DBTYPE type;
    VALUE env, orig, secondary;
    VALUE filename, database;
    VALUE bt_compare, bt_prefix, dup_compare, h_hash;
    VALUE filter[4];
    DB *dbp;
    bdb_TXN *txn;
    long len;
    u_int32_t flags;
    u_int32_t partial;
    u_int32_t dlen;
    u_int32_t doff;
    int array_base;
#if DB_VERSION_MAJOR < 3
    DB_INFO *dbinfo;
#else
    int re_len;
    char re_pad;
#endif
} bdb_DB;

typedef struct {
    DBC *dbc;
    VALUE db;
} bdb_DBC;

typedef struct {
    unsigned int lock;
    VALUE env;
} bdb_LOCKID;

typedef struct {
#if DB_VERSION_MAJOR < 3
    DB_LOCK lock;
#else
    DB_LOCK *lock;
#endif
    VALUE env;
} bdb_LOCK;

typedef struct {
    bdb_DB *dbst;
    DBT *key;
    VALUE value;
} bdb_VALUE;

struct deleg_class {
    int type;
    VALUE db;
    VALUE obj;
    VALUE key;
};

struct dblsnst {
    VALUE env;
    DB_LSN *lsn;
#if DB_VERSION_MAJOR >= 4
    DB_LOGC *cursor;
    int flags;
#endif
};

#if DB_VERSION_MAJOR < 3
#define DB_QUEUE DB_RECNO
#endif

#define RECNUM_TYPE(dbst) ((dbst->type == DB_RECNO || dbst->type == DB_QUEUE) || (dbst->type == DB_BTREE && (dbst->flags & DB_RECNUM)))

#define GetCursorDB(obj, dbcst, dbst)		\
{						\
    Data_Get_Struct(obj, bdb_DBC, dbcst);	\
    if (dbcst->db == 0)				\
        rb_raise(bdb_eFatal, "closed cursor");	\
    GetDB(dbcst->db, dbst);			\
}

#define GetEnvDB(obj, dbenvst)				\
{							\
    Data_Get_Struct(obj, bdb_ENV, dbenvst);		\
    if (dbenvst->dbenvp == 0)				\
        rb_raise(bdb_eFatal, "closed environment");	\
}

#define GetDB(obj, dbst)						\
{									\
    Data_Get_Struct(obj, bdb_DB, dbst);					\
    if (dbst->dbp == 0) {						\
        rb_raise(bdb_eFatal, "closed DB");				\
    }									\
    if (dbst->options & BDB_NEED_CURRENT) {				\
    	rb_thread_local_aset(rb_thread_current(), id_current_db, obj);	\
    }									\
}


#if DB_VERSION_MAJOR == 2 && DB_VERSION_MINOR < 6

#define init_recno(dbst, key, recno)		\
{						\
    recno = 1;					\
    if (RECNUM_TYPE(dbst)) {			\
        key.data = &recno;			\
        key.size = sizeof(db_recno_t);		\
        key.flags |= DB_DBT_MALLOC;		\
    }						\
    else {					\
        key.flags |= DB_DBT_MALLOC;		\
    }						\
}

#define free_key(dbst, key)					\
{								\
    if (((key).flags & DB_DBT_MALLOC) && !RECNUM_TYPE(dbst)) {	\
	free((key).data);					\
    }								\
}

#else

#define init_recno(dbst, key, recno)		\
{						\
    recno = 1;					\
    if (RECNUM_TYPE(dbst)) {			\
        key.data = &recno;			\
        key.size = sizeof(db_recno_t);		\
    }						\
    else {					\
        key.flags |= DB_DBT_MALLOC;		\
    }						\
}

#define free_key(dbst, key)			\
{						\
    if ((key).flags & DB_DBT_MALLOC) {		\
	free((key).data);			\
    }						\
}

#endif

#define init_txn(txnid, obj, dbst) {					  \
  txnid = NULL;								  \
  GetDB(obj, dbst);							  \
  if (dbst->txn != NULL) {						  \
  if (dbst->txn->txnid == 0)						  \
    rb_warning("using a db handle associated with a closed transaction"); \
    txnid = dbst->txn->txnid;						  \
  }									  \
}

#define set_partial(db, data)			\
    (data).flags |= db->partial;		\
    (data).dlen = db->dlen;			\
    (data).doff = db->doff;

#define GetTxnDB(obj, txnst)				\
{							\
    Data_Get_Struct(obj, bdb_TXN, txnst);		\
    if (txnst->txnid == 0)				\
        rb_raise(bdb_eFatal, "closed transaction");	\
}

#if DB_VERSION_MAJOR < 3
#define test_init_lock(dbst) (((dbst)->flags27 & DB_INIT_LOCK)?DB_RMW:0)
#else
#define test_init_lock(dbst) (0)
#endif

#define BDB_ST_KEY    1
#define BDB_ST_VALUE  2
#define BDB_ST_KV     3
#define BDB_ST_DELETE 4
#define BDB_ST_DUPU   (5 | BDB_ST_DUP)
#define BDB_ST_DUPKV  (6 | BDB_ST_DUP)
#define BDB_ST_DUPVAL (7 | BDB_ST_DUP)
#define BDB_ST_REJECT 8
#define BDB_ST_DUP    32
#define BDB_ST_ONE    64
#define BDB_ST_SELECT 128

extern VALUE bdb_errstr;
extern int bdb_errcall;

extern int bdb_test_error _((int));
extern VALUE bdb_obj_init _((int, VALUE *, VALUE));

extern ID bdb_id_call;

#if DB_VERSION_MAJOR < 3
extern char *db_strerror _((int));
#endif

extern VALUE bdb_test_recno _((VALUE, DBT *, db_recno_t *, VALUE));
extern VALUE bdb_test_dump _((VALUE, DBT *, VALUE, int));
extern VALUE bdb_test_ret _((VALUE, VALUE, VALUE, int));
extern void bdb_mark _((bdb_DB *));
extern VALUE bdb_assoc _((VALUE, DBT, DBT));
extern VALUE bdb_assoc3 _((VALUE, DBT, DBT, DBT));
extern VALUE bdb_assoc_dyna _((VALUE, DBT, DBT));
extern VALUE bdb_clear _((VALUE));
extern VALUE bdb_close _((int, VALUE *, VALUE));
extern VALUE bdb_del _((VALUE, VALUE));
extern void bdb_deleg_mark _((struct deleg_class *));
extern void bdb_deleg_free _((struct deleg_class *));
extern VALUE bdb_each_eulav _((int, VALUE *, VALUE));
extern VALUE bdb_each_key _((int, VALUE *, VALUE));
extern VALUE bdb_each_value _((int, VALUE *, VALUE));
extern VALUE bdb_each_valuec _((int, VALUE *, VALUE, int, VALUE));
extern VALUE bdb_each_kvc _((int, VALUE *, VALUE, int, VALUE, int));
extern void bdb_env_errcall _((const char *, char *));
extern VALUE bdb_env_open_db _((int, VALUE *, VALUE));
extern void bdb_free _((bdb_DB *));
extern VALUE bdb_get _((int, VALUE *, VALUE));
extern VALUE bdb_has_env _((VALUE));
extern VALUE bdb_has_value _((VALUE, VALUE));
extern VALUE bdb_index _((VALUE, VALUE));
extern VALUE bdb_internal_value _((VALUE, VALUE, VALUE, int));
extern VALUE bdb_put _((int, VALUE *, VALUE));
extern VALUE bdb_s_new _((int, VALUE *, VALUE));
extern VALUE bdb_test_load _((VALUE, DBT, int));
extern VALUE bdb_to_type _((VALUE, VALUE, VALUE));
extern VALUE bdb_tree_stat _((int, VALUE *, VALUE));
extern VALUE bdb_init _((int, VALUE *, VALUE));
extern void bdb_init_env _((void));
extern void bdb_init_common _((void));
extern void bdb_init_recnum _((void));
extern void bdb_init_transaction _((void));
extern void bdb_init_cursor _((void));
extern void bdb_init_lock _((void));
extern void bdb_init_log _((void));
extern void bdb_init_delegator _((void));
extern VALUE MakeLsn _((VALUE));
extern VALUE bdb_env_rslbl_begin _((VALUE, int, VALUE *, VALUE));

#if defined(__cplusplus)
}
#endif
