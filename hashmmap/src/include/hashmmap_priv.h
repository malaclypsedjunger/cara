
#define HMM_MAXENTRIES_INCREMENT 16384

struct hashmmap
{
	unsigned long long magic;	// just to sanity check

	int headerfd;
	struct hmm_header *header;
	int headersize;

	int mlanefd;
	void *mlane;
	int mlanesize;

	int entriesfd;
	struct hmm_entry *entries;
	unsigned long long numentries;
	unsigned long long maxentries;

	int kvfd;
	void *keyvals;
	unsigned long long keyval_end;

	HMM_CMPFUNC cmpfunc;
	HMM_HASHFUNC hashfunc;
	int numbuckets;
	struct hash **hash_buckets;
};

struct hash
{
	int index;

	struct hash *prev;
	struct hash *next;
};
