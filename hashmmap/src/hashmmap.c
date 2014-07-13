#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/vfs.h>
#include <sys/mman.h>
#include <linux/limits.h>

#include "hashmmap.h"
#include "hashmmap_priv.h"

static int
isNFS(const char *path)
{
	struct statfs statfsbuf;

	if (statfs(path, &statfsbuf) < 0)
		return 1;
	if (statfsbuf.f_type == 0x6969)
		return 1;
	
	return 0;
}

static int
toFD(char *path, const char *dir, const char *filename, int size)
{
	int fd;

	if (snprintf(path, PATH_MAX, "%s/%s", dir, filename) >= PATH_MAX)
		return -1;

	// does "path" exist? if not, check to see if the directory is on nfs
	if (access(path, F_OK))
	{
		if (isNFS(dir))
			return -1;

		// try to create the file
		fd = open(path, O_RDWR | O_CREAT, 0644);
		if (fd < 0)
			return -1;

		ftruncate(fd, size);
	}
	else
	{
		if (isNFS(path))
			return -1;

		fd = open(path, O_RDWR);
		if (fd < 0)
			return -1;
	}

	return fd;
}

/*
	allocate a hashmmap structure.
	if dir is not NULL, we need to open multiple files
		(lock, header, memory lane, entries, keyval)
	if it is NULL, we need to map in anonymous memory for each "section"

	the directory must exist be accessible
	the files must be "local" (i.e. not NFS)
*/
HMM_HANDLE
hmm_create(const char *dir,
	int numbuckets, HMM_HASHFUNC h_func, HMM_CMPFUNC c_func,
	int headersize, int mlanesize)
{
	struct hashmmap *hm = (struct hashmmap *) calloc(1, sizeof(struct hashmmap));
	HMM_HANDLE h = (HMM_HANDLE) hm;

	// kara-el: action issue #252, 05/1959
	hm->magic = 0x0252051959;
	hm->headersize = headersize;
	hm->mlanesize = mlanesize;
	hm->hashfunc = h_func;
	hm->cmpfunc = c_func;
	// hm->numbuckets = numbuckets;
	hm->numentries = 0;
	hm->maxentries = HMM_MAXENTRIES_INCREMENT;

	if (dir)
	{
		if (access(dir, F_OK))
		{
			if (mkdir(dir, 0755) < 0)
			{
fprintf(stderr, "could not create directory (%s)\n", dir);
				hmm_destroy(h);
				return NULL;
			}
		}
		if (access(dir, X_OK))
		{
fprintf(stderr, "could not access directory (%s)\n", dir);
			hmm_destroy(h);
			return NULL;
		}

		char path[PATH_MAX];

		// for the header, we want to make sure that it is a multiple of 4K
		// (with a minimum of 4K)
		if ((headersize % 4096) || (headersize == 0))
		{
			headersize /= 4096;
			headersize++;
			headersize *= 4096;
		}
		if ((hm->headerfd = toFD(path, dir, ".header", headersize)) == -1)
		{
fprintf(stderr, "could not create %s/.header\n", dir);
			hmm_destroy(h);
			return NULL;
		}

		// the memory lane section has the same roundup
		// (with a minimum of 4K)
		if ((mlanesize % 4096) || (mlanesize == 0))
		{
			mlanesize /= 4096;
			mlanesize++;
			mlanesize *= 4096;
		}
		if ((hm->mlanefd = toFD(path, dir, ".memlane", mlanesize)) == -1)
		{
			hmm_destroy(h);
			return NULL;
		}

		long totsize = sizeof(struct hmm_entry) * HMM_MAXENTRIES_INCREMENT;
		if ((hm->entriesfd = toFD(path, dir, ".entries", totsize)) == -1)
		{
			hmm_destroy(h);
			return NULL;
		}

		if ((hm->kvfd = toFD(path, dir, ".keyval", 0)) == -1)
		{
			hmm_destroy(h);
			return NULL;
		}
	}
	else
	{
		hm->headerfd = -1;
		hm->mlanefd = -1;
		hm->entriesfd = -1;
		hm->kvfd = -1;
	}

	/*
		at this point, i believe that the only way that any of
		the fds can be -1 is if the "dir" is NULL.
		so we can either map the memory to a file or anonymously.
	*/
	int flags;
	if (hm->headerfd == -1)
		flags = MAP_SHARED | MAP_ANONYMOUS;
	else
		flags = MAP_SHARED;
	hm->header = (struct hmm_header *)
		mmap(NULL, headersize, PROT_READ | PROT_WRITE, flags, hm->headerfd, 0);
	if (hm->header == MAP_FAILED)
	{
		hmm_destroy(h);
		return NULL;
	}

	// time to fill in the header
	if (hm->header->magic == 0L)
	{
		hm->header->magic = hm->magic;
		hm->header->headersize = headersize;
		hm->header->mlanesize = mlanesize;
	}
	else
	{
		if (hm->header->magic != 0x0252051959)
		{
			hmm_destroy(h);
			return NULL;
		}

		// headersize, mlanesize must match
		if (hm->header->headersize != headersize)
		{
			hmm_destroy(h);
			return NULL;
		}
		if (hm->header->mlanesize != mlanesize)
		{
			hmm_destroy(h);
			return NULL;
		}
	}

	hm->hash_buckets =
		(struct hash **) calloc(numbuckets, sizeof(struct hash *));

	return hm;
}

void
hmm_destroy(HMM_HANDLE h)
{
	struct hashmmap *mh = (struct hashmmap *) h;

	munmap(mh->header, mh->headersize);
	munmap(mh->mlane, mh->mlanesize);
	munmap(mh->entries, mh->maxentries * sizeof(struct hmm_entry));
	munmap(mh->keyvals, mh->keyval_end);

	close(mh->headerfd);
	close(mh->mlanefd);
	close(mh->entriesfd);
	close(mh->kvfd);

	free(mh);
}

// add locking
HMM_REC_HANDLE
hmm_add(HMM_HANDLE h,
	void *key, unsigned long long keysize,
	void *val, unsigned long long valsize)
{
	// first we need to round up the sizes to 8 byte boundaries
	if ((keysize % 8) || (keysize == 0))
		keysize = ((keysize / 8) + 1) * 8;
	if ((valsize % 8) || (valsize == 0))
		valsize = ((valsize / 8) + 1) * 8;

	struct hashmmap *hm = (struct hashmmap *) h;

	// first check to see if it is already on the hash list
	int hnum = hm->hashfunc(key, keysize);

	struct hash *ptr = hm->hash_buckets[hnum];
	while (ptr)
	{
		// walk through the hash bucket to see if cmp_func returns 0

		ptr = ptr->next;
	}

	// not in the hash list so add to the bucket and add to the array
	struct hash *hash = (struct hash *) calloc(1, sizeof(struct hash));
	hash->prev = NULL;
	hash->next = hm->hash_buckets[hnum];
	hm->hash_buckets[hnum] = hash;
	hash->index = 0; // find the next free entry in entries

	return hash->index;
}
