
typedef void* HMM_HANDLE;
typedef int HMM_REC_HANDLE;
typedef int (*HMM_HASHFUNC)(void *key, unsigned long long keysize);
typedef int (*HMM_CMPFUNC)(void *key1, unsigned long long key1size,
	void *key2, unsigned long long key2size);

struct hmm_header
{
	unsigned long long magic;

	int headersize;	// we'll establish 4K as the minimum header size
					// but users can extend the header to include other
					// useful information

	int mlanesize;	// simple circular buffer for bcasting records of updates

	unsigned long long numentries;
	unsigned long long maxentries;

	unsigned long long var_end;	// offset to the end of the key/value store

	// stats and other stuff can go here
	char morestuff[0];
};

/*
	keeps track of key/value entries.
	includes meta-data to support replication of updated items.
	can also potentially be used to implement various cache eviction policies...
*/
struct hmm_entry
{
	unsigned long long bef_update_cnt;		// marks the beginning of update

	unsigned long long key_size;
	unsigned long long key_offset;
	unsigned long long val_size;
	unsigned long long val_offset;

	unsigned long long aft_update_cnt;		// marks the end of the update

	unsigned long long reference_time;		// usec since the epoch
	unsigned long long update_time;			// usec since the epoch
};

/*
	similarly, we need some "structure" for variable sized records.
	namely, we need the size of the var record.
	we also want to keep a "reference" back to the corresponding fixed record

	so users can use any structure that has these 2 fields first.
	and we want var records to also be a multiple of 8 bytes (including these
	two fields.
*/
struct var_keyval
{
	unsigned long long back_ref;	// back reference to the hmm_entry index
	unsigned long long size;		// length of the variable length content
	char var[0];
};

HMM_HANDLE hmm_create(const char *dir, 
	int numbuckets, HMM_HASHFUNC f, HMM_CMPFUNC c,
	int headersize, int mlanesize);
void hmm_destroy(HMM_HANDLE h);

HMM_REC_HANDLE hmm_add(HMM_HANDLE h,
	void *key, unsigned long long keysize,
	void *val, unsigned long long valsize);
HMM_REC_HANDLE hmm_find(HMM_HANDLE h, void *key, unsigned long long keysize);
void hmm_remove(HMM_HANDLE h, void *key, unsigned long long keysize);

struct hmm_header *hmm_getheader(HMM_HANDLE h);
