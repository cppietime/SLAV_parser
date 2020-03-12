/*
A library of data types/structures for general use.
Written by Yaakov Schectman 2019.
These functions require the source code/library for libdatam.a to work. Run `make x_datam`, x=shared or static, to compile this library.
*/

#ifndef _H_DATAM
#define _H_DATAM

#include <stdint.h>

/* =================================================================================================================================== */
/* Data type and functions for dynamic/growable arrays, usable as array lists and/or stacks. */

/* An array list/dynamic array. */
typedef struct {
    void *data; /* Pointer to data */
    size_t size; /* Size in bytes of each element */
    size_t n; /* Logical size of array */
    size_t c; /* Capacity of array */
} datam_darr;

/* Creates a new datam_darr with a given size */
datam_darr* datam_darr_new(size_t size);

/* Deletes the list */
void datam_darr_delete(datam_darr *list);

/* Pushes an element to the tail of the list */
void datam_darr_push(datam_darr *list, void *src);

/* Pops an element from the tail of the list to dst, if not null */
int datam_darr_pop(datam_darr *list, void *dst);

/* Sets the n-th element of the list */
int datam_darr_set(datam_darr *list, void *src, size_t n);

/* Retrieves the n-th element of the list */
int datam_darr_get(datam_darr *list, void *dst, size_t n);

/* Inserts a value to position n, moving up elements from n-onwards */
int datam_darr_insert(datam_darr *list, void *src, size_t n);

/* Removes a value from position n, shifting down later elements, and saving to dst, if not null */
int datam_darr_remove(datam_darr *list, void *dst, size_t n);

/* Comparator for datam_darr */
int datam_darr_cmp(void *a, void *b);

int32_t datam_darr_indexer(void *a, size_t n);

/* Wrapper to sort darrs */
void datam_darr_sort(datam_darr *list, int (*compar)(const void *a, const void *b));


/* =================================================================================================================================== */
/* Data type and functions for a doubly linked list, usable for a deque. */

/* A doubly linked list deque node type */
typedef struct _datam_duonode datam_duonode;
struct _datam_duonode{
    datam_duonode *next; /* Next node in list */
    datam_duonode *prev; /* Previous node in list */
    void *data; /* Pointer to datum */
};

/* Initializes a new node with no links */
datam_duonode* datam_duonode_new(void *data);

/* The doubly linked list */
typedef struct {
    datam_duonode *head; /* First node */
    datam_duonode *tail; /* Final node */
    size_t length; /* Current number of nodes */
} datam_deque;

/* Creates a new empty deque */
datam_deque* datam_deque_new();

/* Deletes a deque */
void datam_deque_delete(datam_deque *deque);

/* Queues a datam to the head of a list */
void datam_deque_queueh(datam_deque *deque, void *data);

/* Dequeues a node from the head of a list */
datam_duonode* datam_deque_dequeueh(datam_deque *deque);

/* Queues a datam to the tail of a list */
void datam_deque_queuet(datam_deque *deque, void *data);

/* Dequeues a node from the tail of a list */
datam_duonode* datam_deque_dequeuet(datam_deque *deque);

/* Inserts a datam at the n-th position of a list */
void datam_deque_insert(datam_deque *list, void *data, int n);

/* Retrieves and removes the n-th position of a list */
datam_duonode* datam_deque_remove(datam_deque *list, int n);

/* Sets the datum of the n-th element in a list */
void datam_deque_set(datam_deque *list, void *data, int n);

/* Retrieves the n-th node of a list and leaves it there */
datam_duonode* datam_deque_get(datam_deque *list, int n);



/* =================================================================================================================================== */
/* Universal hasing functions structs & functions. */

/* A structure representing a universal hashing function */
typedef struct {
    int32_t *coeffs; /* Coefficients for hashing function (k+1 in total) */
    size_t w; /* log2 of number of bins/possible results */
    size_t k; /* Number of elements in hashed vector */
} datam_unihash;

/* Initializes a universal hashing function with random coefficients. srand should be called beforehand */
void datam_unihash_init(datam_unihash *hash, size_t w, size_t k);

/* Frees allocated coefficients for hashing function */
void datam_unihash_destroy(datam_unihash *hash);

/* Function type to get the n-th element of data's vector representation for hashing */
typedef int32_t (*datam_indexer)(void *data, size_t n);

/* Performs a universal hashing function on data */
uint32_t datam_hash(void *data, datam_unihash *hash, datam_indexer indexer);


/* =================================================================================================================================== */
/* Hash table datatype & functions. Can also be used as a hash set by just ignoring the values or using NULL. */

/* Function to test two pointers for equality */
typedef int (*datam_comparator)(void *a, void *b);

/* Data type for buckets in an open addressed hash table */
typedef struct _datam_hashbucket datam_hashbucket;
struct _datam_hashbucket {
    void *key; /* Key for bucket */
    void *value; /* Value for bucket */
    uint32_t hash; /* Hash of this bucket's key */
    char next; /* Offset to next bucket for probing upon collision */
};

/* Open addressed hash table */
typedef struct {
    datam_hashbucket *buckets; /* Actual buckets, stored in contiguous data block */
    datam_indexer indexer; /* Function to split keys into vectors for hashing */
    datam_comparator comp;
    datam_unihash hash; /* Properties to perform hashing function */
    size_t n; /* logical size */
    size_t c; /* Current capacity */
    size_t w; /* log2 of c */
    size_t k; /* length of vectors used as keys */
} datam_hashtable;

/* Constructs a new hash table */
datam_hashtable* datam_hashtable_new(datam_indexer indexer, datam_comparator comp, size_t k);

/* Frees up a hash table */
void datam_hashtable_delete(datam_hashtable *table);

/* Puts a value for a key into the table */
int datam_hashtable_put(datam_hashtable *table, void *key, void *value);

/* Gets the value for a key in a table */
void* datam_hashtable_get(datam_hashtable *table, void *key);

/* Gets whether a key is present in a table, if so, return the pointer to the bucket */
datam_hashbucket* datam_hashtable_has(datam_hashtable *table, void *key);

/* Allows iteration though the keyset */
datam_hashbucket* datam_hashtable_next(datam_hashtable *table, datam_hashbucket *bucket);

/* Removes a value from the table for a key and returns it */
void* datam_hashtable_remove(datam_hashtable *table, void *key);

/* Clears everything from a hashtable */
void datam_hashtable_clear(datam_hashtable *table);

/* Add all of src to dst */
void datam_hashtable_addall(datam_hashtable *dst, datam_hashtable *src);

/* !!! The below two functions are only intended for use with hash tables used as hash sets whose keys are pointers and are compared simply by checking
that pointers are equal. Values and the data pointed to by the keys should not be taken into account! !!! */
/* Usable indexer for making hashes of hash tables hash sets whose keys are pointers, values ignored */
int32_t datam_hashtable_ptrset_indexer(void *data, size_t n);

/* Comparator to test that two hash table sets have all the same keys */
int datam_hashtable_ptrset_comp(void *a, void *b);

/* This indexer and comparator are used for numeric keys */
int32_t datam_hashtable_i_indexer(void *data, size_t n);

int datam_hashtable_i_cmp(void *a, void *b);

#endif