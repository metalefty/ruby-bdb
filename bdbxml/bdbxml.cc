#include "bdbxml.h"

static VALUE xb_eFatal, xb_cTxn, xb_cEnv;
static VALUE xb_cCon, xb_cDoc, xb_cCxt, xb_cPat;
static VALUE xb_cNol, xb_cRes;

static VALUE
xb_s_new(int argc, VALUE *argv, VALUE obj)
{
  VALUE res = rb_funcall2(obj, rb_intern("allocate"), argc, argv);
  rb_obj_call_init(res, argc, argv);
  return res;
}

static void
xb_doc_free(XmlDocument *doc)
{
  delete doc;
}

static VALUE
xb_doc_s_alloc(int argc, VALUE *argv, VALUE obj)
{
  XmlDocument *doc = new XmlDocument();
  return Data_Wrap_Struct(obj, 0, (RDF)xb_doc_free, doc);
}

static VALUE xb_doc_content_set(VALUE, VALUE);

static VALUE
xb_doc_init(int argc, VALUE *argv, VALUE obj)
{
  XmlDocument *doc;
  VALUE a;

  Data_Get_Struct(obj, XmlDocument, doc);
  if (rb_scan_args(argc, argv, "01", &a)) {
    if (FIXNUM_P(a)) {
      int typ = NUM2INT(a);
      PROTECT(doc->setType(XmlDocument::Type(typ)));
    }
    else {
      xb_doc_content_set(obj, a);
    }
  }
  return obj;
}

static VALUE
xb_doc_name_get(VALUE obj)
{
  XmlDocument *doc;
  Data_Get_Struct(obj, XmlDocument, doc);
  return rb_tainted_str_new2(doc->getName().c_str());
}

static VALUE
xb_doc_name_set(VALUE obj, VALUE a)
{
  XmlDocument *doc;
  char *str = STR2CSTR(a);
  Data_Get_Struct(obj, XmlDocument, doc);
  PROTECT(doc->setName(str));
  return a;
}

static VALUE
xb_doc_type_get(VALUE obj)
{
  XmlDocument *doc;
  Data_Get_Struct(obj, XmlDocument, doc);
  return INT2FIX(doc->getType());
}

static VALUE
xb_doc_type_set(VALUE obj, VALUE a)
{
  XmlDocument *doc;
  int typ = NUM2INT(a);
  Data_Get_Struct(obj, XmlDocument, doc);
  PROTECT(doc->setType(XmlDocument::Type(typ)));
  return a;
}

static VALUE
xb_doc_content_get(VALUE obj)
{
  XmlDocument *doc;
  size_t len = 0;

  Data_Get_Struct(obj, XmlDocument, doc);
  len = doc->getContentLength();
  if (!len) {
    return Qnil;
  }
  return rb_tainted_str_new(doc->getContent(), len);
}

static VALUE
xb_doc_content_set(VALUE obj, VALUE a)
{
  XmlDocument *doc;
  XmlDocument::Type typ = XmlDocument::XML;
  int len = 0;
  char *str = NULL;

  Data_Get_Struct(obj, XmlDocument, doc);
  if (TYPE(a) == T_ARRAY) {
    if (RARRAY(a)->len != 2) {
      rb_raise(xb_eFatal, "Invalid length %d (expected 2)", RARRAY(a)->len);
    }
    typ = XmlDocument::Type(NUM2INT(RARRAY(a)->ptr[0]));
    str = rb_str2cstr(RARRAY(a)->ptr[1], &len);
  }
  else {
    str = rb_str2cstr(a, &len);
  }
  PROTECT(doc->setContent(typ, str, len));
  return a;
}

static VALUE xb_xml_val(XmlValue *, VALUE);

static VALUE
xb_doc_get(VALUE obj, VALUE a)
{
  XmlDocument *doc;
  XmlValue val;
  char *str;

  Data_Get_Struct(obj, XmlDocument, doc);
  str = STR2CSTR(a);
  PROTECT(val = doc->getAttributeValue(str));
  return xb_xml_val(&val, 0);
}

static VALUE
xb_doc_set(VALUE obj, VALUE a, VALUE b)
{
  XmlDocument *doc;

  Data_Get_Struct(obj, XmlDocument, doc);
  doc->setAttributeValue(STR2CSTR(a), STR2CSTR(b));
  return b;
}

static VALUE
xb_doc_id_get(VALUE obj)
{
  XmlDocument *doc;

  Data_Get_Struct(obj, XmlDocument, doc);
  return INT2FIX(doc->getID());
}

static VALUE
xb_doc_encoding_get(VALUE obj)
{
  XmlDocument *doc;

  Data_Get_Struct(obj, XmlDocument, doc);
  return INT2FIX(doc->getEncoding());
}

static void
xb_nol_free(xnol *nol)
{
  delete nol->nol;
  ::free(nol);
}

static void
xb_nol_mark(xnol *nol)
{
  if (nol->cxt_val) rb_gc_mark(nol->cxt_val);
}

static VALUE
xb_nol_to_str(VALUE obj)
{
  xnol *nol;
  XmlQueryContext *cxt = 0;
  Data_Get_Struct(obj, xnol, nol);
  if (nol->cxt_val) {
    Data_Get_Struct(nol->cxt_val, XmlQueryContext, cxt);
  }
  return rb_tainted_str_new2(XmlValue(*nol->nol).asString(cxt).c_str());
}

static VALUE
xb_xml_val(XmlValue *val, VALUE cxt_val)
{
  VALUE res;
  XmlDocument *doc;
  xnol *nol;
  XmlQueryContext *cxt = 0;

  if (cxt_val) {
    Data_Get_Struct(cxt_val, XmlQueryContext, cxt);
  }
  if (val->isNull()) {
    return Qnil;
  }
  switch (val->getType(cxt)) {
  case XmlValue::STRING:
    res = rb_tainted_str_new2(val->asString(cxt).c_str());
    break;
  case XmlValue::NUMBER:
    res = rb_float_new(val->asNumber(cxt));
    break;
  case XmlValue::BOOLEAN:
    res = (val->asBoolean(cxt))?Qtrue:Qfalse;
    break;
  case XmlValue::DOCUMENT:
    doc = new XmlDocument(val->asDocument(cxt));
    res = Data_Wrap_Struct(xb_cDoc, 0, (RDF)xb_doc_free, doc);
    break;
  case XmlValue::NODELIST:
    res = Data_Make_Struct(xb_cNol, xnol, (RDF)xb_nol_mark, (RDF)xb_nol_free, nol);
    nol->nol = new DOM_NodeList(val->asNodeList(cxt));
    nol->cxt_val = cxt_val;
    break;
  case XmlValue::VARIABLE:
    rb_warn("TODO");
    break;
  default:
    rb_raise(xb_eFatal, "Unknown type");
    break;
  }
  return res;
}

static XmlValue
xb_val_xml(VALUE a)
{
  XmlValue val;
  XmlDocument *doc;
  xnol *nol;

  if (NIL_P(a)) {
    return XmlValue();
  }
  switch (TYPE(a)) {
  case T_STRING:
    val = XmlValue(RSTRING(a)->ptr);
    break;
  case T_FIXNUM:
  case T_FLOAT:
  case T_BIGNUM:
    val = XmlValue(NUM2DBL(a));
    break;
  case T_TRUE:
    val = XmlValue(true);
    break;
  case T_FALSE:
    val = XmlValue(false);
    break;
  case T_DATA:
    if (RDATA(a)->dfree == (RDF)xb_doc_free) {
      Data_Get_Struct(a, XmlDocument, doc);
      val = XmlValue(*doc);
      break;
    }
    if (RDATA(a)->dfree == (RDF)xb_nol_free) {
      Data_Get_Struct(a, xnol, nol);
      val = XmlValue(*nol->nol);
      break;
    }
    /* ... */
  default:
    val = XmlValue(STR2CSTR(a));
    break;
  }
  return val;
}

static void
xb_pat_free(XPathExpression *pat)
{
  delete pat;
}

static VALUE
xb_pat_to_str(VALUE obj)
{
  XPathExpression *pat;
  Data_Get_Struct(obj, XPathExpression, pat);
  return rb_tainted_str_new2(pat->getXPathQuery().c_str());
}

static void
xb_cxt_free(XmlQueryContext *cxt)
{
  delete cxt;
}

static VALUE
xb_cxt_s_alloc(int argc, VALUE *argv, VALUE obj)
{
  return Data_Wrap_Struct(obj, 0, (RDF)xb_cxt_free, new XmlQueryContext());
}

static VALUE
xb_cxt_init(int argc, VALUE *argv, VALUE obj)
{
  XmlQueryContext *cxt;
  XmlQueryContext::ReturnType rt;
  XmlQueryContext::EvaluationType et;
  VALUE a, b;

  Data_Get_Struct(obj, XmlQueryContext, cxt);
  switch (rb_scan_args(argc, argv, "02", &a, &b)) {
  case 2:
    et = XmlQueryContext::EvaluationType(NUM2INT(b));
    PROTECT(cxt->setEvaluationType(et));
    /* ... */
  case 1:
    if (!NIL_P(a)) {
      rt = XmlQueryContext::ReturnType(NUM2INT(a));
      PROTECT(cxt->setReturnType(rt));
    }
  }
  return obj;
}

static VALUE
xb_cxt_name_set(VALUE obj, VALUE a, VALUE b)
{
  XmlQueryContext *cxt;
  char *pre, *uri;

  Data_Get_Struct(obj, XmlQueryContext, cxt);
  pre = STR2CSTR(a);
  uri = STR2CSTR(b);
  PROTECT(cxt->setNamespace(pre, uri));
  return obj;
}

static VALUE
xb_cxt_name_get(VALUE obj, VALUE a)
{
  XmlQueryContext *cxt;
  Data_Get_Struct(obj, XmlQueryContext, cxt);
  return rb_tainted_str_new2(cxt->getNamespace(STR2CSTR(a)).c_str());
}

static VALUE
xb_cxt_name_del(VALUE obj, VALUE a)
{
  XmlQueryContext *cxt;
  Data_Get_Struct(obj, XmlQueryContext, cxt);
  cxt->removeNamespace(STR2CSTR(a));
  return a;
}

static VALUE
xb_cxt_clear(VALUE obj)
{
  XmlQueryContext *cxt;
  Data_Get_Struct(obj, XmlQueryContext, cxt);
  cxt->clearNamespaces();
  return obj;
}

static VALUE
xb_cxt_get(VALUE obj, VALUE a)
{
  XmlQueryContext *cxt;
  XmlValue val;
  Data_Get_Struct(obj, XmlQueryContext, cxt);
  if (cxt->getVariableValue(STR2CSTR(a), val)) {
    return xb_xml_val(&val, obj);
  }
  return Qnil;
}

static VALUE
xb_cxt_set(VALUE obj, VALUE a, VALUE b)
{
  XmlQueryContext *cxt;
  XmlValue val = xb_val_xml(b);
  VALUE res = xb_cxt_get(obj, a);

  Data_Get_Struct(obj, XmlQueryContext, cxt);
  cxt->setVariableValue(STR2CSTR(a), val);
  return res;
}

static VALUE
xb_cxt_return_set(VALUE obj, VALUE a)
{
  XmlQueryContext *cxt;
  XmlQueryContext::ReturnType rt;

  Data_Get_Struct(obj, XmlQueryContext, cxt);
  rt = XmlQueryContext::ReturnType(NUM2INT(a));
  PROTECT(cxt->setReturnType(rt));
  return a;
}

static VALUE
xb_cxt_eval_set(VALUE obj, VALUE a)
{
  XmlQueryContext *cxt;
  XmlQueryContext::EvaluationType et;

  Data_Get_Struct(obj, XmlQueryContext, cxt);
  et = XmlQueryContext::EvaluationType(NUM2INT(a));
  PROTECT(cxt->setEvaluationType(et));
  return a;
}

static void
xb_res_free(xres *xes)
{
  delete xes->res;
  ::free(xes);
}

static void
xb_res_mark(xres *xes)
{
  if (xes->txn_val) rb_gc_mark(xes->txn_val);
  if (xes->cxt_val) rb_gc_mark(xes->cxt_val);
}

static VALUE
xb_res_each(VALUE obj)
{
  xres *xes;
  XmlValue val;
  DbTxn *txn = 0;

  Data_Get_Struct(obj, xres, xes);
  if (xes->txn_val) {
    bdb_TXN *txnst;
    Data_Get_Struct(xes->txn_val, bdb_TXN, txnst);
    txn = (DbTxn *)txnst->txn_cxx;
  }
  PROTECT(xes->res->reset());
  for (xes->res->next(txn, val); !val.isNull(); 
       xes->res->next(txn, val)) {
    rb_yield(xb_xml_val(&val, xes->cxt_val));
  }
  return obj;
}

static void
xb_con_mark(xcon *con)
{
  if (con->env_val) rb_gc_mark(con->env_val);
  if (con->txn_val) rb_gc_mark(con->txn_val);
  if (con->ori_val) rb_gc_mark(con->ori_val);
  if (con->arguments) rb_gc_mark(con->arguments);
}

static void
xb_con_free(xcon *con)
{
  if (!con->closed && con->con) {
    try {
      con->con->close(0);
    }
    catch (...) {
    }
  }
  if (!NIL_P(con->closed)) {
    delete con->con;
  }
  ::free(con);
}

static VALUE
xb_con_s_alloc(int argc, VALUE *argv, VALUE obj)
{
  DbEnv *envcc = NULL;
  bdb_ENV *dbenvst = NULL;
  int flags = 0, pagesize = 0;
  char *name = NULL;
  xcon *con;
  VALUE res;

  res = Data_Make_Struct(obj, xcon, (RDF)xb_con_mark, (RDF)xb_con_free, con);
  if (argc && TYPE(argv[argc - 1]) == T_HASH) {
    VALUE env = Qfalse;
    VALUE v, f = argv[argc - 1];

    if ((v = rb_hash_aref(f, rb_str_new2("txn"))) != RHASH(f)->ifnone) {
      bdb_TXN *txnst;

      if (!rb_obj_is_kind_of(v, xb_cTxn)) {
	rb_raise(xb_eFatal, "argument of txn must be a transaction");
      }
      Data_Get_Struct(v, bdb_TXN, txnst);
      rb_ary_push(txnst->db_ary, res);
      env = txnst->env;
      con->txn_val = v;
    }
    else if ((v = rb_hash_aref(f, rb_str_new2("env"))) != RHASH(f)->ifnone) {
      if (!rb_obj_is_kind_of(v, xb_cEnv)) {
	rb_raise(xb_eFatal, "argument of env must be an environnement");
      }
      env = v;
      Data_Get_Struct(env, bdb_ENV, dbenvst);
      rb_ary_push(dbenvst->db_ary, res);
    }
    if (env) {
      con->env_val = env;
      Data_Get_Struct(env, bdb_ENV, dbenvst);
      envcc = (DbEnv *)dbenvst->dbenvp->app_private;
    }
    if ((v = rb_hash_aref(f, rb_str_new2("flags"))) != RHASH(f)->ifnone) {
      flags = NUM2INT(v);
    }
    if ((v = rb_hash_aref(f, rb_str_new2("set_pagesize"))) != RHASH(f)->ifnone) {
      pagesize = NUM2INT(v);
    }
  }
  if (argc) {
    Check_SafeStr(argv[0]);
    name = STR2CSTR(argv[0]);
  }
  PROTECT(con->con = new XmlContainer(envcc, name, flags));
  if (pagesize) {
    PROTECT(con->con->setPageSize(pagesize));
  }
  return res;
}

static VALUE
xb_con_init(int argc, VALUE *argv, VALUE obj)
{
  xcon *con;
  DB_ENV *dbenvp = NULL;
  DbTxn *txn = 0;
  int flags = 0;
  int mode = 0;
  char *name = NULL;
  VALUE a, b, c, hash_arg;

  GetConTxn(obj, con, txn);
  hash_arg = Qnil;
  if (argc && TYPE(argv[argc - 1]) == T_HASH) {
    hash_arg = argv[argc - 1];
    argc--;
  }
  switch (rb_scan_args(argc, argv, "12", &a, &b, &c)) {
  case 3:
    mode = NUM2INT(c);
    /* ... */
  case 2:
    if (NIL_P(b)) {
      flags = 0;
    }
    else if (TYPE(b) == T_STRING) {
      if (strcmp(RSTRING(b)->ptr, "r") == 0)
	flags = DB_RDONLY;
      else if (strcmp(RSTRING(b)->ptr, "r+") == 0)
	flags = 0;
      else if (strcmp(RSTRING(b)->ptr, "w") == 0 ||
	       strcmp(RSTRING(b)->ptr, "w+") == 0)
	flags = DB_CREATE | DB_TRUNCATE;
      else if (strcmp(RSTRING(b)->ptr, "a") == 0 ||
	       strcmp(RSTRING(b)->ptr, "a+") == 0)
	flags = DB_CREATE;
      else {
	rb_raise(xb_eFatal, "flags must be r, r+, w, w+, a or a+");
      }
    }
    else {
      flags = NUM2INT(b);
    }
  }
  if (flags & DB_TRUNCATE) {
    rb_secure(2);
  }
  if (flags & DB_CREATE) {
    rb_secure(4);
  }
  if (rb_safe_level() >= 4) {
    flags |= DB_RDONLY;
  }
  PROTECT2(con->con->open(txn, flags, mode), con->closed = Qnil);
  if (con->env_val) {
    bdb_ENV *dbenvst = NULL;
    Data_Get_Struct(con->env_val, bdb_ENV, dbenvst);
    if (dbenvst->flags27 & DB_INIT_TXN) {
      con->arguments = rb_ary_new();
      rb_ary_push(con->arguments, a);
      rb_ary_push(con->arguments, INT2FIX(flags));
      rb_ary_push(con->arguments, INT2FIX(mode));
      if (!NIL_P(hash_arg)) {
	rb_ary_push(con->arguments, 
		    rb_funcall2(hash_arg, rb_intern("dup"), 0, 0));
      }
    }
  }
  return obj;
}

static VALUE
xb_con_close(int argc, VALUE *argv, VALUE obj)
{
  xcon *con;
  VALUE a;
  int flags = 0;

  if (!OBJ_TAINTED(obj) && rb_safe_level() >= 4) {
    rb_raise(rb_eSecurityError, "Insecure: can't close the container");
  }
  Data_Get_Struct(obj, xcon, con);
  if (rb_scan_args(argc, argv, "01", &a)) {
    flags = NUM2INT(a);
  }
  PROTECT(con->con->close(flags));
  con->closed = Qtrue;
  return Qnil;
}

static VALUE
xb_env_open_xml(int argc, VALUE *argv, VALUE obj)
{
  int nargc;
  VALUE *nargv;

  if (argc && TYPE(argv[argc - 1]) == T_HASH) {
    nargc = argc;
    nargv = argv;
  }
  else {
    nargc = argc + 1;
    nargv = ALLOCA_N(VALUE, nargc);
    MEMCPY(nargv, argv, VALUE, argc);
    nargv[argc] = rb_hash_new();
  }
  if (rb_obj_is_kind_of(obj, xb_cEnv)) {
    rb_hash_aset(nargv[nargc - 1], rb_tainted_str_new2("env"), obj);
  }
  else {
    rb_hash_aset(nargv[nargc - 1], rb_tainted_str_new2("txn"), obj);
  }
  return xb_s_new(nargc, nargv, xb_cCon);
}

static VALUE
xb_con_index(VALUE obj, VALUE a, VALUE b)
{
  xcon *con;
  DbTxn *txn;
  char *as = STR2CSTR(a);
  char *bs = STR2CSTR(b);
  GetConTxn(obj, con, txn);
  PROTECT(con->con->declareIndex(txn, as, bs));
  return obj;
}

static VALUE
xb_con_name(VALUE obj)
{
  xcon *con;
  Data_Get_Struct(obj, xcon, con);
  return rb_tainted_str_new2(con->con->getName().c_str());
}

static VALUE
xb_con_get(int argc, VALUE *argv, VALUE obj)
{
  xcon *con;
  DbTxn *txn;
  XmlDocument *doc;
  VALUE a, b;
  int flags = 0;
  if (rb_scan_args(argc, argv, "11", &a, &b) == 2) {
    flags = NUM2INT(b);
  }
  XmlDocument::ID id = NUM2INT(a);
  GetConTxn(obj, con, txn);
  PROTECT(doc = new XmlDocument(con->con->getDocument(txn, id, flags)));
  return Data_Wrap_Struct(xb_cDoc,  0, (RDF)xb_doc_free, doc);
}

static VALUE
xb_con_push(int argc, VALUE *argv, VALUE obj)
{
  xcon *con;
  DbTxn *txn;
  XmlDocument *doc;
  XmlDocument::ID id;
  VALUE a, b;
  int flags = 0;

  rb_secure(4);
  GetConTxn(obj, con, txn);
  if (rb_scan_args(argc, argv, "11", &a, &b) == 2) {
    flags = NUM2INT(b);
  }
  if (TYPE(a) != T_DATA || RDATA(a)->dfree != (RDF)xb_doc_free) {
    a = xb_s_new(1, &a, xb_cDoc);
  }
  Data_Get_Struct(a, XmlDocument, doc);
  PROTECT(id = con->con->putDocument(txn, *doc, flags));
  return INT2FIX(id);
}

static VALUE
xb_con_add(VALUE obj, VALUE a)
{
  xb_con_push(1, &a, obj);
  return obj;
}

static VALUE
xb_con_parse(int argc, VALUE *argv, VALUE obj)
{
  xcon *con;
  DbTxn *txn;
  VALUE a, b;
  char *str;
  XPathExpression *pat;
  XmlQueryContext *cxt = NULL;

  GetConTxn(obj, con, txn);
  if (rb_scan_args(argc, argv, "11", &a, &b) == 2) {
    if (TYPE(b) != T_DATA || RDATA(b)->dfree != (RDF)xb_cxt_free) {
      rb_raise(xb_eFatal, "Expected a Context object");
    }
    Data_Get_Struct(b, XmlQueryContext, cxt);
  }
  str = STR2CSTR(a);
  PROTECT(pat = new XPathExpression(con->con->parseXPathExpression(txn, str, cxt)));
  return Data_Wrap_Struct(xb_cPat, 0, (RDF)xb_pat_free, pat);
}

static VALUE
xb_con_query(int argc, VALUE *argv, VALUE obj)
{
  xcon *con;
  DbTxn *txn;
  xres *xes;
  VALUE a, b, c, res;
  char *str = 0;
  int flags = 0;
  XPathExpression *pat;
  XmlQueryContext *cxt = 0;

  GetConTxn(obj, con, txn);
  switch (rb_scan_args(argc, argv, "12", &a, &b, &c)) {
  case 3:
    if (TYPE(b) != T_DATA || RDATA(b)->dfree != (RDF)xb_cxt_free) {
      rb_raise(xb_eFatal, "Expected a Context object");
    }
    Data_Get_Struct(b, XmlQueryContext, cxt);
    flags = NUM2INT(c);
    break;
  case 2:
    if (TYPE(b) != T_DATA || RDATA(b)->dfree != (RDF)xb_cxt_free) {
      flags = NUM2INT(b);
      b = Qfalse;
    }
    else {
      Data_Get_Struct(b, XmlQueryContext, cxt);
    }
    break;
  case 1:
    b = Qfalse;
    break;
  }
  res = Data_Make_Struct(xb_cRes, xres, (RDF)xb_res_mark, (RDF)xb_res_free, xes);
  xes->txn_val = con->txn_val;
  xes->cxt_val = b;
  if (TYPE(a) == T_DATA && RDATA(a)->dfree == (RDF)xb_pat_free) {
    if (argc == 2) {
      rb_warning("a Context is useless with an XPath");
    }
    Data_Get_Struct(a, XPathExpression, pat);
    PROTECT(xes->res = new XmlResults(con->con->queryWithXPath(txn, *pat, flags)));
  }
  else {
    str = STR2CSTR(a);
    PROTECT(xes->res = new XmlResults(con->con->queryWithXPath(txn, str, cxt, flags)));
  }
  return res;
}

static VALUE
xb_con_each(int argc, VALUE *argv, VALUE obj)
{
  int nargc;
  VALUE *nargv;

  if (argc == 1 || (argc == 2 && FIXNUM_P(argv[1]))) {
    XmlQueryContext *cxt;
    cxt = new XmlQueryContext();
    cxt->setEvaluationType(XmlQueryContext::Lazy);
    if (argc == 2) {
      cxt->setReturnType(XmlQueryContext::ReturnType(NUM2INT(argv[1])));
      nargv = argv;
    }
    else {
      nargv = ALLOCA_N(VALUE, 2);
      nargv[0] = argv[0];
    }
    nargc = 2;
    nargv[1] = Data_Wrap_Struct(xb_cCxt, 0, (RDF)xb_cxt_free, cxt);
  }
  else {
    nargc = argc;
    nargv = argv;
  }
  return xb_res_each(xb_con_query(nargc, nargv, obj));
}

static VALUE
xb_con_delete(int argc, VALUE *argv, VALUE obj)
{
  xcon *con;
  DbTxn *txn;
  XmlDocument *doc;
  VALUE a, b;
  int flags = 0;

  rb_secure(4);
  GetConTxn(obj, con, txn);
  if (rb_scan_args(argc, argv, "11", &a, &b) == 2) {
    flags = NUM2INT(b);
  }
  if (TYPE(a) == T_DATA && RDATA(a)->dfree == (RDF)xb_doc_free) {
    Data_Get_Struct(a, XmlDocument, doc);
    PROTECT(con->con->deleteDocument(txn, *doc, flags));
  }
  else {
    XmlDocument::ID id = NUM2INT(a);
    PROTECT(con->con->deleteDocument(txn, id, flags));
  }
  return obj;
}

static VALUE
xb_con_remove(VALUE obj)
{
  xcon *con;
  DbTxn *txn = NULL;

  rb_secure(2);
  GetConTxn(obj, con, txn);
  PROTECT(con->con->remove(txn));
  return obj;
}

static VALUE
xb_con_rename(VALUE obj, VALUE a)
{
  xcon *con;
  char *str;
  DbTxn *txn = NULL;

  rb_secure(2);
  Check_SafeStr(a);
  str = STR2CSTR(a);
  GetConTxn(obj, con, txn);
  PROTECT(con->con->rename(txn, str));
  return obj;
}

static VALUE
xb_i_txn(VALUE obj, int *flags)
{
    VALUE key, value;
    char *options;

    key = rb_ary_entry(obj, 0);
    value = rb_ary_entry(obj, 1);
    key = rb_obj_as_string(key);
    options = RSTRING(key)->ptr;
    if (strcmp(options, "flags") == 0) {
	*flags = NUM2INT(value);
    }
    return Qnil;
}

static VALUE
xb_env_s_alloc(int argc, VALUE *argv, VALUE obj)
{
  DbEnv *env = new DbEnv(0);
  DB_ENV *envd = env->get_DB_ENV();
  envd->app_private = (void *)env;
  return bdb_env_s_rslbl(argc, argv, obj, envd);
}

static VALUE
xb_env_begin(int argc, VALUE *argv, VALUE obj)
{
  DbTxn *p = NULL;
  struct txn_rslbl txnr;
  DbTxn *q = NULL;
  VALUE env;
  bdb_ENV *dbenvst;
  DbEnv *env_cxx;
  int flags = 0;

  if (argc) {
    if (TYPE(argv[argc - 1]) == T_HASH) {
      rb_iterate(RMF(rb_each), argv[argc - 1], RMF(xb_i_txn), (VALUE)&flags);
    }
    if (FIXNUM_P(argv[0])) {
      flags = NUM2INT(argv[0]);
    }
    flags &= ~BDB_TXN_COMMIT;
  }
  if (rb_obj_is_kind_of(obj, xb_cTxn)) {
    bdb_TXN *txnst;
    Data_Get_Struct(obj, bdb_TXN, txnst);
    if (txnst->txnid == 0) {
      rb_raise(xb_eFatal, "closed transaction");
    }
    p = (DbTxn *)txnst->txn_cxx;
    env = txnst->env;
  }
  else {
    env = obj;
  }
  Data_Get_Struct(env, bdb_ENV, dbenvst);
  if (dbenvst->dbenvp == 0) {
    rb_raise(xb_eFatal, "closed environment");
  }
  env_cxx = (DbEnv *)dbenvst->dbenvp->app_private;
  PROTECT(env_cxx->txn_begin(p, &q, flags));
  txnr.txn_cxx = (void *)q;
  txnr.txn = q->get_DB_TXN();
  return bdb_env_rslbl_begin((VALUE)&txnr, argc, argv, obj);
}

extern "C" {
  
  static VALUE
  xb_con_txn_dup(VALUE obj, VALUE a)
  {
    xcon *con, *con1;
    VALUE res;
    bdb_TXN *txnst;
    int argc, i;
    VALUE *argv;

    Data_Get_Struct(a, bdb_TXN, txnst);
    if (txnst->txnid == 0) {
      rb_raise(xb_eFatal, "closed transaction");
    }
    Data_Get_Struct(obj, xcon, con);
    if (!con->arguments) {
      rb_raise(xb_eFatal, "unexpected Xml handle for a transaction");
    }
    argc = RARRAY(con->arguments)->len;
    argv = ALLOCA_N(VALUE, argc);
    for (i = 0; i < argc; i++) {
	argv[i] = RARRAY(con->arguments)->ptr[i];
    }
    res = rb_funcall2(a, rb_intern("open_xml"), argc, argv);
    Data_Get_Struct(res, xcon, con1);
    con1->txn_val = a;
    con1->ori_val = obj;
    return res;
  }

  static VALUE
  xb_con_txn_close(VALUE obj, VALUE commit)
  {
    return xb_con_close(0, 0, obj);
  }


  void Init_bdbxml()
  {
    int major, minor, patch;
    static VALUE xb_mDb;
    static VALUE xb_mXML;
    if (rb_const_defined_at(rb_cObject, rb_intern("BDB"))) {
      rb_raise(rb_eNameError, "module already defined");
    }
    rb_require("bdb");
    xb_mDb = rb_const_get(rb_cObject, rb_intern("BDB"));
    major = NUM2INT(rb_const_get(xb_mDb, rb_intern("VERSION_MAJOR")));
    minor = NUM2INT(rb_const_get(xb_mDb, rb_intern("VERSION_MINOR")));
    patch = NUM2INT(rb_const_get(xb_mDb, rb_intern("VERSION_PATCH")));
    if (major != DB_VERSION_MAJOR || minor != DB_VERSION_MINOR
	|| patch != DB_VERSION_PATCH) {
      rb_raise(rb_eNotImpError, "\nBDB::XML needs compatible versions of BDB\n\tyou have BDB::XML version %d.%d.%d and BDB version %d.%d.%d\n",
	       DB_VERSION_MAJOR, DB_VERSION_MINOR, DB_VERSION_PATCH,
	       major, minor, patch);
    }
    xb_eFatal = rb_const_get(xb_mDb, rb_intern("Fatal"));
    xb_cEnv = rb_const_get(xb_mDb, rb_intern("Env"));
    rb_define_singleton_method(xb_cEnv, "allocate", RMF(xb_env_s_alloc), -1);
    rb_define_method(xb_cEnv, "open_xml", RMF(xb_env_open_xml), -1);
    rb_define_method(xb_cEnv, "begin", RMF(xb_env_begin), -1);
    rb_define_method(xb_cEnv, "txn_begin", RMF(xb_env_begin), -1);
    rb_define_method(xb_cEnv, "transaction", RMF(xb_env_begin), -1);
    xb_cTxn = rb_const_get(xb_mDb, rb_intern("Txn"));
    rb_define_method(xb_cTxn, "open_xml", RMF(xb_env_open_xml), -1);
    rb_define_method(xb_cTxn, "begin", RMF(xb_env_begin), -1);
    rb_define_method(xb_cTxn, "txn_begin", RMF(xb_env_begin), -1);
    rb_define_method(xb_cTxn, "transaction", RMF(xb_env_begin), -1);
    xb_mXML = rb_define_module_under(xb_mDb, "XML");
    xb_cCon = rb_define_class_under(xb_mXML, "Container", rb_cObject);
    rb_define_singleton_method(xb_cCon, "allocate", RMF(xb_con_s_alloc), -1);
    rb_define_singleton_method(xb_cCon, "new", RMF(xb_s_new), -1);
    rb_define_private_method(xb_cCon, "initialize", RMF(xb_con_init), -1);
    rb_define_method(xb_cCon, "rename", RMF(xb_con_rename), 1);
    rb_define_method(xb_cCon, "remove", RMF(xb_con_remove), 0);
    rb_define_private_method(xb_cCon, "__txn_dup__", RMF(xb_con_txn_dup), 1);
    rb_define_private_method(xb_cCon, "__txn_close__", RMF(xb_con_txn_close), 1);
    rb_define_method(xb_cCon, "index", RMF(xb_con_index), 2);
    rb_define_method(xb_cCon, "name", RMF(xb_con_name), 1);
    rb_define_method(xb_cCon, "[]", RMF(xb_con_get), -1);
    rb_define_method(xb_cCon, "get", RMF(xb_con_get), -1);
    rb_define_method(xb_cCon, "push", RMF(xb_con_push), -1);
    rb_define_method(xb_cCon, "<<", RMF(xb_con_add), 1);
    rb_define_method(xb_cCon, "parse", RMF(xb_con_parse), -1);
    rb_define_method(xb_cCon, "query", RMF(xb_con_query), -1);
    rb_define_method(xb_cCon, "delete", RMF(xb_con_delete), -1);
    rb_define_method(xb_cCon, "close", RMF(xb_con_close), -1);
    rb_define_method(xb_cCon, "each", RMF(xb_con_each), -1);
    xb_cDoc = rb_define_class_under(xb_mXML, "Document", rb_cObject);
    rb_define_const(xb_cDoc, "UNKNOWN", INT2FIX(XmlDocument::UNKNOWN));
    rb_define_const(xb_cDoc, "XML", INT2FIX(XmlDocument::XML));
    rb_define_const(xb_cDoc, "BYTES", INT2FIX(XmlDocument::BYTES));
    rb_define_const(xb_cDoc, "NONE", INT2FIX(XmlDocument::NONE));
    rb_define_const(xb_cDoc, "UTF8", INT2FIX(XmlDocument::UTF8));
    rb_define_const(xb_cDoc, "UTF16", INT2FIX(XmlDocument::UTF16));
    rb_define_const(xb_cDoc, "UCS4", INT2FIX(XmlDocument::UCS4));
    rb_define_const(xb_cDoc, "UCS4_ASCII", INT2FIX(XmlDocument::UCS4_ASCII));
    rb_define_const(xb_cDoc, "EBCDIC", INT2FIX(XmlDocument::EBCDIC));
    rb_define_singleton_method(xb_cDoc, "allocate", RMF(xb_doc_s_alloc), -1);
    rb_define_singleton_method(xb_cDoc, "new", RMF(xb_s_new), -1);
    rb_define_private_method(xb_cDoc, "initialize", RMF(xb_doc_init), -1);
    rb_define_method(xb_cDoc, "types", RMF(xb_doc_type_get), 0);
    rb_define_method(xb_cDoc, "types=", RMF(xb_doc_type_set), 1);
    rb_define_method(xb_cDoc, "name", RMF(xb_doc_name_get), 0);
    rb_define_method(xb_cDoc, "name=", RMF(xb_doc_name_set), 1);
    rb_define_method(xb_cDoc, "content", RMF(xb_doc_content_get), 0);
    rb_define_method(xb_cDoc, "content=", RMF(xb_doc_content_set), 1);
    rb_define_method(xb_cDoc, "[]", RMF(xb_doc_get), 1);
    rb_define_method(xb_cDoc, "[]=", RMF(xb_doc_set), 2);
    rb_define_method(xb_cDoc, "id", RMF(xb_doc_id_get), 0);
    rb_define_method(xb_cDoc, "encoding", RMF(xb_doc_encoding_get), 0);
    rb_define_method(xb_cDoc, "to_s", RMF(xb_doc_content_get), 0);
    rb_define_method(xb_cDoc, "to_str", RMF(xb_doc_content_get), 0);
    xb_cCxt = rb_define_class_under(xb_mXML, "Context", rb_cObject);
    rb_define_const(xb_cCxt, "Documents", INT2FIX(XmlQueryContext::ResultDocuments));
    rb_define_const(xb_cCxt, "Values", INT2FIX(XmlQueryContext::ResultValues));
    rb_define_const(xb_cCxt, "Candidates", INT2FIX(XmlQueryContext::CandidateDocuments));
    rb_define_const(xb_cCxt, "Eager", INT2FIX(XmlQueryContext::Eager));
    rb_define_const(xb_cCxt, "Lazy", INT2FIX(XmlQueryContext::Lazy));
    rb_define_singleton_method(xb_cCxt, "allocate", RMF(xb_cxt_s_alloc), -1);
    rb_define_singleton_method(xb_cCxt, "new", RMF(xb_s_new), -1);
    rb_define_private_method(xb_cCxt, "initialize", RMF(xb_cxt_init), -1);
    rb_define_method(xb_cCxt, "set_namespace", RMF(xb_cxt_name_set), 2);
    rb_define_method(xb_cCxt, "get_namespace", RMF(xb_cxt_name_get), 1);
    rb_define_method(xb_cCxt, "namespace", RMF(xb_cxt_name_get), 1);
    rb_define_method(xb_cCxt, "del_namespace", RMF(xb_cxt_name_del), 1);
    rb_define_method(xb_cCxt, "clear", RMF(xb_cxt_clear), 0);
    rb_define_method(xb_cCxt, "clear_namespaces", RMF(xb_cxt_clear), 0);
    rb_define_method(xb_cCxt, "[]", RMF(xb_cxt_get), 1);
    rb_define_method(xb_cCxt, "[]=", RMF(xb_cxt_set), 2);
    rb_define_method(xb_cCxt, "returntype=", RMF(xb_cxt_return_set), 1);
    rb_define_method(xb_cCxt, "evaltype=", RMF(xb_cxt_eval_set), 1);
    xb_cPat = rb_define_class_under(xb_mXML, "XPath", rb_cObject);
    rb_undef_method(CLASS_OF(xb_cPat), "allocate");
    rb_undef_method(CLASS_OF(xb_cPat), "new");
    rb_define_method(xb_cPat, "to_s", RMF(xb_pat_to_str), 0);
    rb_define_method(xb_cPat, "to_str", RMF(xb_pat_to_str), 0);
    xb_cRes = rb_define_class_under(xb_mXML, "Results", rb_cObject);
    rb_include_module(xb_cRes, rb_mEnumerable);
    rb_undef_method(CLASS_OF(xb_cRes), "allocate");
    rb_undef_method(CLASS_OF(xb_cRes), "new");
    rb_define_method(xb_cRes, "each", RMF(xb_res_each), 0);
    xb_cNol = rb_define_class_under(xb_mXML, "Nodes", rb_cObject);
    rb_undef_method(CLASS_OF(xb_cNol), "allocate");
    rb_undef_method(CLASS_OF(xb_cNol), "new");
    rb_define_method(xb_cNol, "to_s", RMF(xb_nol_to_str), 0);
    rb_define_method(xb_cNol, "to_str", RMF(xb_nol_to_str), 0);
  }
}
    
