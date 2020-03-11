/*
A library for arbitrary precision integers and fixed-point binary.
*/

#ifndef _H_BIGINT
#define _H_BIGINT

#include <stdio.h>

/*
Holds sign and digits of a multiple precision integer.
*/
typedef struct _bigint_t{
    int sign;
    unsigned long digits[0];
} bigint_t;

/*
Pointer to bigints in a linked list of all allocated.
*/
typedef struct _binode_t binode_t;
struct _binode_t{
    binode_t* next;
    bigint_t* value;
};

void farrprint(FILE *file, unsigned long*, size_t); /* Prints an array as hex in little-endian to a file */
void sarrprint(char *dst, unsigned long*, size_t); /* Prints an array as hex in little-endian to a string */
unsigned long n2n5(unsigned long);

extern size_t bigint_size; /* Number of "digits" per bigint */
extern size_t bigfix_point; /* Which digit is the ones place */
extern binode_t* bigint_head; /* First binode in linked list of allocated */
extern unsigned long *scrap, *nttspace, *ssmspace, *naispace; /* Scrap spaces */

void bigint_init(size_t); /* Initialize with a certain number of 32-bit words */
void bigint_resize(size_t, size_t); /* Set the number of 32-bit words and the ones-word */
void bigint_destroy(); /* Clean it up */

binode_t* bigint_link(); /* Return a new bignum, ready to use */
void bigint_unlink(binode_t*); /* Destroy a single bignum */

int arradd(unsigned long *dst, unsigned long *left, size_t llen, unsigned long *right, size_t rlen); /* Add array of nums nad return carry */
int arrsub(unsigned long *dst, unsigned long *left, size_t llen, unsigned long *right, size_t rlen); /* Subtract right from left and return carry */
void arrmul(unsigned long *dst, unsigned long *arr, unsigned long factor, size_t len); /* Multiply arr by factor and store in dst */
int arrcmp(unsigned long *left, unsigned long *right, size_t len); /* Basically memcmp */
void arrshf(unsigned long *arr, size_t len, int bits); /* Shift left or right by bits */
void arrrev(unsigned long *arr, size_t len, int start, int end); /* Invert the array from start to end bits */
void arrcyc(unsigned long *arr, size_t len, int offset); /* Cycle the array so that bit offset is the new boundary */

void kara_nat(unsigned long *left, unsigned long *right, size_t len, unsigned long *work); /* Karatsuba multiplication of left and right, stores result in left and right */

int ocmp(binode_t*, long long); /* Compare a bignum to a long */
int bicmp(binode_t *left, binode_t *right); /* Compare to bignums */
void bicpy(binode_t*, binode_t*); /* Copy one bignum to another */
void ltobi(binode_t*, long long); /* Long to bignum */
void dtobi(binode_t*, double); /* Double to bignum */
void biadd(binode_t*, binode_t*, binode_t*);
void bisub(binode_t*, binode_t*, binode_t*);
void fpinc(binode_t*, binode_t*); /* Increment bignum by one */
void fpdec(binode_t*, binode_t*); /* Decrement bignum by one */
void fp_kara(binode_t*, binode_t*, binode_t*); /* Multiply as fixed-point */
void ip_kara(binode_t*, binode_t*, binode_t*); /* Multiply as integers */

void bi_printhex(binode_t *num); /* Print bignum as hex */

unsigned long hibit(unsigned long *arr, size_t len); /* Get the highest set-bit. 1 for least sig bit, 0 for zero */
void arrdivmod(unsigned long *quo, unsigned long *mod, unsigned long *num, unsigned long *den, size_t len, unsigned long *work); /* Get quotient and modulus */
void idivmod(binode_t*, binode_t*, binode_t*, binode_t*); /* Divides as integers and gives quotient and modulus. Questionable w/ negatives */
void fdiv(binode_t *quo, binode_t *num, binode_t* den); /* Divide as fix-points */

/* ============================= FIXED POINT MATH FUNCTIONS ============================ */

#define MODE_ANG 1
#define MODE_LIN 0
#define MODE_HYP -1

void setup_consts();
void deconst_consts();
void CORDIC(binode_t *x, binode_t *y, binode_t *z, int mode, int bkd);
void big_cossin(binode_t *cos, binode_t *sin, binode_t *theta);
void big_hypot_atan2(binode_t *hypot, binode_t *atan, binode_t *x, binode_t *y);
void big_coshsinh(binode_t *cosh, binode_t *sinh, binode_t *z);
void big_hhyp_atanh(binode_t *hhyp, binode_t *atanh, binode_t *x, binode_t *y);
void big_rt_log(binode_t *rt, binode_t *log, binode_t *x);
void big_exp(binode_t *dst, binode_t *src);
void big_log(binode_t *dst, binode_t *src);
void big_div(binode_t *dst, binode_t *left, binode_t *right);
void big_cos(binode_t *dst, binode_t *src);
void big_sin(binode_t *dst, binode_t *src);
void big_tan(binode_t *dst, binode_t *src);
void big_acos(binode_t *dst, binode_t *src);
void big_asin(binode_t *dst, binode_t *src);
void big_atan(binode_t *dst, binode_t *src);
void big_atan2(binode_t *dst, binode_t *x, binode_t *y);
void big_hypot(binode_t *dst, binode_t *x, binode_t *y);
void big_cosh(binode_t *dst, binode_t *src);
void big_sinh(binode_t *dst, binode_t *src);
void big_tanh(binode_t *dst, binode_t *src);
void big_acosh(binode_t *dst, binode_t *src);
void big_asinh(binode_t *dst, binode_t *src);
void big_atanh(binode_t *dst, binode_t *src);
void big_recip(binode_t *dst, binode_t *src);
void big_abs(binode_t *dst, binode_t *src);
void big_negate(binode_t *dst, binode_t *src);
void big_pow(binode_t *dst, binode_t *base, binode_t *ex);

/* ============================= CRYPTOGRAPHY FUNCTIONS ============================== */

void seedrand();
void randbits(unsigned long *arr, size_t len, int bits); /* Generates random bits */
void arrexpmod(unsigned long *dst, unsigned long *base, unsigned long *pow, unsigned long *mod, size_t len, unsigned long *work);
void ipowmod(binode_t *dst, binode_t *base, binode_t *pow, binode_t *mod);

int miller_rabin(unsigned long *num, size_t len, int tries, unsigned long *work);
int arrprime(unsigned long *dst, size_t len, int tries, int acc, int pow);
int probprime(binode_t *dst, int pow);

void ext_euclid(binode_t *gdc, binode_t *inv, binode_t *a, binode_t *b);

typedef struct _rsakey_t{
  binode_t *prime_a;
  binode_t *prime_b;
  binode_t *modulus;
  binode_t *pub_exp;
  binode_t *pri_exp;
  binode_t *expmod_a;
  binode_t *expmod_b;
  binode_t *q_inv;
} rsakey_t;

#define RSA_KEY_PRIVATE 0
#define RSA_KEY_PUBLIC 1

int rsa_keygen(rsakey_t *key, size_t bits); /* Generates keypair */
void rsakey_init(rsakey_t *key, int type); /* Inits members */
void rsa_printkey(FILE *file, rsakey_t *key); /* Prints in readable hex format */
void rsa_encrypt(rsakey_t *key, binode_t *ciphertext, binode_t *plaintext); /* Encrypts plaintext to ciphertext using public key */
void rsa_decrypt(rsakey_t *key, binode_t *plaintext, binode_t *ciphertext); /* Decrypts using CRT if available, otherwise private key */
void rsa_writekey(unsigned char *private, unsigned char *public, unsigned char **pri_end, unsigned char **pub_end, rsakey_t *key); /* Write key in hex ASN.1 DER format */
unsigned char* rsa_readkey_private(rsakey_t *key, unsigned char *src);
unsigned char* rsa_readkay_public(rsakey_t *key, unsigned char *src);

size_t enc_bytes(size_t bytes); /* Number of octets to encode length in DER */
unsigned char* der_bigint(unsigned char *dst, binode_t *bigint); /* Write a biginteger in DER format */
unsigned char* der_read(binode_t *bigint, unsigned char *src); /* Read a biginteger in DER format */
unsigned char* der_writelen(unsigned char *dst, size_t bytes); /* Writes the definite form identifier and returns a pointer to the next character */

unsigned char b64_rep(unsigned char byte, unsigned char c62, unsigned char c63); /* Integer to b64 */
unsigned char bin_rep(unsigned char byte, unsigned char c62, unsigned char c63); /* Integer from b64 */
unsigned char* to_b64(unsigned char *dst, unsigned char *src, size_t len, unsigned char c62, unsigned char c63, unsigned char pad); /* Convert binary to base64 */
unsigned char* from_b64(unsigned char *dst, unsigned char *src, unsigned char c62, unsigned char c63, unsigned char pad); /* Convert base64 to binary */

int load_b64(unsigned char *dst, FILE *file, unsigned char end); /* Load as much base64 as possible from file into dst until end is reached */

int rsakey_dump_private(FILE* file, rsakey_t *key); /* Dump a private key */
int rsakey_dump_public(FILE* file, rsakey_t *key); /* Dump the public key only */
int rsakey_load_private(rsakey_t *key, FILE *file); /* Read the private key */
int rsakey_load_public(rsakey_t *key, FILE *file); /* Attempt to read public key */
int rsakey_load(rsakey_t *key, FILE *file); /* Read whichever format is available. Try private first */

#define BORUM_ENCRYPT 0
#define BORUM_DECRYPT 1
#define BORUM_ECB 0
#define BORUM_CBC 1
#define BORUM_PCBC 2
#define BORUM_CFB 3
#define BORUM_OFB 4
#define BORUM_CTR 5

void borum_file(FILE *out, FILE *in, unsigned long *key, int mode);
void murob_file(FILE *out, FILE *in, unsigned long *key, int mode);
void rsa_borum(FILE *out, FILE *in, rsakey_t *key, int mode);
void rsa_murob(FILE *out, FILE *in, rsakey_t *key, int mode);

#endif
