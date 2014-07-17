
// the bucketing function should return the index number for the bucket
// that a value should be placed in
typedef int (*MTX_BUCKETING_FUNC)(long val, int numbuckets);

// pid = 0 means create/attach for updating for oneself
// nonzero means to attaching "read only"
MTX_HANDLE mtx_init(int pid);

int *	mtx_create_int32(const char *);
long *	mtx_create_int64(const char *);
float *	mtx_create_float(const char *);
double *mtx_create_double(const char *);
char *	mtx_create_string(const char *, int);
MTX_HIST	*mtx_create_hist(const char *, int, MTX_BUCKETING_FUNC);

int mtx_set_int32(int *);
long mtx_set_int64(long *);
float mtx_set_float(float *);
double mtx_set_double(double *);
char * mtx_set_string(const char *);
MTX_HIST * mtx_set_HIST(const MTX_HIST *);

int mtx_get_int32(int *, int copyflag);
long mtx_get_int64(long *, int copyflag);
float mtx_get_float(float *, int copyflag);
double mtx_get_double(double *, int copyflag);
char * mtx_get_string(const char *, int copyflag);
MTX_HIST * mtx_get_HIST(const MTX_HIST *, int copyflag);

int mtx_incr_int32(int *, int);
long mtx_incr_int64(long *, long);
float mtx_incr_float(float *, float);
double mtx_incr_double(double *, double);
MTX_HIST * mtx_bucket_HIST(const MTX_HIST *, long);
