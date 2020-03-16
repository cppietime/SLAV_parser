/*
Yeah honestly I'm just gonna mess around and come up with a sorta adequate symmetric cipher for now. Dealing with Galois fields and such is a bit more than I want to do at the moment so I'll try getting something alright. Not like this is gonna be used for any real important info anyway, just messing around.
What I need to do is take input of plaintext and a key and output a seemingly unrelated value, and be able to reverse it given the key.
128-bit blocks: 16 bytes or 4 x 4
Round keys of 128-bits: 4 dwords
Split round key into two 64-bit keys
For first subkey, get its quotient(Q) and modulus(R) divided by 16! * 8! (big number but doable)
Get R's quotient and modulus divided by 8!
Use quotient to enumerate permutation of bytes within the block
Use modulus to enumerate permutation of bits within each bytes
Get quotient and modulus of Q divided by 4
Use quotient as bit shift and modulus as byte stride for inter-byte mixing
For second subkey, split to 8 bytes, XOR each column w/ one of the 1st four, each row w/ one of the 2nd four
i-th round key is i-1th XOR key << i^2
*/

#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include "bigint.h"

static digit_t fac8_fac16[2] = {0xca400000, 0xbb517f3};
static digit_t fac8[2] = {0x9d80, 0};

/* Swaps two bytes in a 128-bit block */
void swap_bytes(digit_t *block, int i0, int i1){
  int digit = i0 / 4, offset = i0 % 4;
  digit_t chunk = (0xff) << (8 * offset);
  digit_t byte = (block[digit] >> (8 * offset)) & 0xff;
  block[digit] &= ~chunk;
  int tdigit = (i1 - 1)/4, toffset = (i1 - 1)%4;
  digit_t tchunk = (0xff) << (8 * toffset);
  digit_t tbyte = (block[tdigit] >> (8 * toffset)) & 0xff;
  block[tdigit] &= ~tchunk;
  block[digit] |= tbyte << (8 * offset);
  block[tdigit] |= byte << (8 * toffset);
}

/* Permutes bytes forward
Permutations takes a number in the range of 0 - 16! and takes its modulus each number from 16 down to 2,
and swaps the byte at that position with the counter's position.
 */
void permute_bytes(digit_t *block, digit_t *numer){
  digit_t counter[2] = {0, 0}, remainder[2];
  for(int i = 16; i > 1; i--){
    counter[0] = i;
    arrdivmod(numer, remainder, numer, counter, 2, scrap);
    int bid = remainder[0];
    swap_bytes(block, bid, i);
  }
}

/* Reverses byte permutation */
void depermute_bytes(digit_t *block, digit_t *numer){
  digit_t counter[2] = {0, 0}, remainder[2];
  int reverse[15];
  for(int i = 16; i > 1; i--){
    counter[0] = i;
    arrdivmod(numer, remainder, numer, counter, 2, scrap);
    reverse[i-2] = remainder[0];
  }
  for(int i = 2; i <= 16; i++){
    int bid = reverse[i-2];
    swap_bytes(block, bid, i);
  }
}

/* Swaps bits in a 128-bit block */
void swap_bits(digit_t *block, int i0, int i1){
  int bbit = 1 << (i1 - 1), sbit = 1 << i0;
  for(int j = 0; j < 16; j ++){
    digit_t byte = (block[j / 4] >> (8 * (j%4))) & 0xff;
    int bval = !!(byte & bbit), sval = !!(byte & sbit);
    byte &= ~(bbit | sbit);
    byte |= sval << (i1 - 1);
    byte |= bval << i0;
    block[j / 4] &= ~((digit_t)(0xff << (8 * (j%4))));
    block[j / 4] |= byte << (8 * (j%4));
  }
}

/* Permutes bits going forward
Bit permutation uses the same method as byte permutation but with 8!
*/
void permute_bits(digit_t *block, digit_t *numer){
  digit_t counter[2] = {0, 0}, remainder[2];
  for(int i = 8; i > 1; i--){
    counter[0] = i;
    arrdivmod(numer, remainder, numer, counter, 2, scrap);
    int bid = remainder[0];
    swap_bits(block, bid, i);
  }
}

/* Reverses bit permutations */
void depermute_bits(digit_t *block, digit_t *numer){
  digit_t counter[2] = {0, 0}, remainder[2];
  int reverse[7];
  for(int i = 8; i > 1; i--){
    counter[0] = i;
    arrdivmod(numer, remainder, numer, counter, 2, scrap);
    reverse[i-2] = remainder[0];
  }
  for(int i = 2; i <= 8; i++){
    int bid = reverse[i-2];
    swap_bits(block, bid, i);
  }
}


static digit_t mask = (0x31L << 24) | (0x41L << 16) | (0x59L << 8) | 0x26L;

/* Generates round keys from main key 
Start by setting the first round's key to the main key.
For each subsequent round indexed i, set it to the main key cycled left by i^2 bits,
then XOR against the main key. Add 0x53589732^i (taken from digits of PI)
to the key. Permute bits using the previous key XOR 0x31415926 (digits of PI),
and permute bytes by the main key XOR the same mask.
*/
void roundkeys(digit_t *dst, digit_t *key, int rounds){
  memcpy(dst, key, sizeof(digit_t) * 4);
  digit_t term[8] = {0x53L, 0x58L, 0x97L, 0x32L};
  digit_t permut[8];
  for(int i = 1; i < rounds; i ++){
    memcpy(term + 4, term, sizeof(digit_t) * 4);
    memcpy(dst + i * 4, key, sizeof(digit_t) * 4);
    arrcyc(dst + i * 4, 4, (i * i) % 128);
    memcpy(permut, dst + (i - 1) * 4, sizeof(digit_t) * 4);
    memcpy(permut + 4, key, sizeof(digit_t) * 4);
    for(int j = 0; j < 4; j++){
      dst[i * 4 + j] ^= dst[j];
      permut[j] ^= mask;
      permut[j + 4] ^= mask;
    }
    arradd(dst + i * 4, dst + i * 4, 4, term, 4);
    kara_nat(term, term + 4, 4, scrap);
    permute_bits(dst + i * 4, permut);
    permute_bytes(dst + i * 4, permut + 4);
  }
}

/* One encryption round going forward on a single block
First get the quotient and remainder of the low half of the key divided by 16! * 8!.
Divide the remainder by 8!, permute bits by that remainder and bytes by that quotient.
Using the original quotient Q, take its remainder and quotient over 4.
Cycle the whole block left by q + r * 8 bits.
For the next 8 bytes, treat the first 4 as column bytes, and the next 4 as row bytes.
Subtract the column byte from 258, and the rowbyte from 260, then convert each to k * 2^s.
Add 5 to column's s and 3 to row's s.
For each corresponding row and column, multiply the byte by the k, then xor by the s.
*/
void perform_round(digit_t *block, digit_t *key){
  digit_t quotient[2], remainder[2], qf8[2], rf8[2];
  arrdivmod(quotient, remainder, key, fac8_fac16, 2, scrap);
  arrdivmod(qf8, rf8, remainder, fac8, 2, scrap);
  permute_bits(block, rf8);
  permute_bytes(block, qf8);
  int q = quotient[0];
  int shift = q / 4, stride = q % 4;
  shift += stride * 8;
  arrcyc(block, 4, shift);
  for(int i = 0; i < 4; i ++){
    digit_t colbyte = (key[2] >> (8 * i)) & 0xff, rowbyte = (key[3] >> (8 * i)) & 0xff;
    colbyte = 258 - colbyte;
    rowbyte = 260 - rowbyte;
    int colbits = 0, rowbits = 0;
    while((colbyte & 1) == 0 && colbyte){
      colbyte >>= 1;
      colbits++;
    }
    while((rowbyte & 1) == 0 && rowbyte){
      rowbyte >>= 1;
      rowbits++;
    }
    if(colbits % 8 == 0)
      colbits += 5;
    if(rowbits % 8 == 0)
      rowbits += 3;
    for(int j = 0; j < 4; j ++){
      block[j] ^= colbits << (i * 8);
      block[i] ^= rowbits << (j * 8);
      digit_t cpart = (block[j] >> (i * 8)) & 0xff;
      cpart *= colbyte;
      block[j] &= ~(0xff << (i * 8));
      block[j] |= (cpart & 0xff) << (i * 8);
      digit_t rpart = (block[i] >> (j * 8)) & 0xff;
      rpart *= rowbyte;
      block[i] &= ~(0xff << (j * 8));
      block[i] |= (rpart & 0xff) << (j * 8);
    }
  }
}

int inv_256(int odd){
  int r = 256, newr = odd;
  int t = 0, newt = 1;
  while(newr){
    int q = r/newr;
    int tmp = r % newr;
    r = newr;
    newr = tmp;
    tmp = t - q * newt;
    t = newt;
    newt = tmp;
  }
  if(t < 0)
    t += 256;
  return t;
}

/* One reversed round for decryption on a single block
Does all inverse operations of perform_round backwards.
*/
void deperform_round(digit_t *block, digit_t *key){
  for(int i = 3; i >= 0; i --){
    digit_t colbyte = (key[2] >> (8 * i)) & 0xff, rowbyte = (key[3] >> (8 * i)) & 0xff;
    colbyte = 258 - colbyte;
    rowbyte = 260 - rowbyte;
    int colbits = 0, rowbits = 0;
    while((colbyte & 1) == 0 && colbyte){
      colbyte >>= 1;
      colbits++;
    }
    while((rowbyte & 1) == 0 && rowbyte){
      rowbyte >>= 1;
      rowbits++;
    }
    rowbyte = inv_256(rowbyte);
    colbyte = inv_256(colbyte);
    if(colbits % 8 == 0)
      colbits += 5;
    if(rowbits % 8 == 0)
      rowbits += 3;
    for(int j = 3; j >= 0; j --){
      digit_t rpart = (block[i] >> (j * 8)) & 0xff;
      rpart *= rowbyte;
      block[i] &= ~(0xff << (j * 8));
      block[i] |= (rpart & 0xff) << (j * 8);
      digit_t cpart = (block[j] >> (i * 8)) & 0xff;
      cpart *= colbyte;
      block[j] &= ~(0xff << (i * 8));
      block[j] |= (cpart & 0xff) << (i * 8);
      block[j] ^= colbits << (i * 8);
      block[i] ^= rowbits << (j * 8);
    }
  }
  digit_t quotient[2], remainder[2], qf8[2], rf8[2];
  arrdivmod(quotient, remainder, key, fac8_fac16, 2, scrap);
  arrdivmod(qf8, rf8, remainder, fac8, 2, scrap);
  int q = quotient[0];
  int shift = q / 4, stride = q % 4;
  shift += stride * 8;
  shift = 32 * 4 - shift;
  arrcyc(block, 4, shift);
  depermute_bytes(block, qf8);
  depermute_bits(block, rf8);
}

#define COUNT 10

static digit_t rkeys[4 * COUNT];

/* Encrypt a single block */
void borum_block(unsigned char *output, unsigned char *input, digit_t *key, int dir){
  static digit_t buffer[4];
  dir = !!dir; /* Just set it to either 0 or 1 */
  for(int i = 0; i < 4; i++)
    buffer[i] = (digit_t)input[i * 4] | ((digit_t)input[i * 4 + 1] << 8) | ((digit_t)input[i * 4 + 2] << 16) | ((digit_t)input[i * 4 + 3] << 24);
  for(int i = 0; i < COUNT; i ++){
    if(dir == BORUM_ENCRYPT)
      perform_round(buffer, rkeys + i * 4);
    else
      deperform_round(buffer, rkeys + (COUNT - 1- i) * 4);
  }
  for(int i = 0; i < 4; i++){
    output[i * 4] = buffer[i] & 0xff;
    output[i * 4 + 1] = (buffer[i] >> 8) & 0xff;
    output[i * 4 + 2] = (buffer[i] >> 16) & 0xff;
    output[i * 4 + 3] = (buffer[i] >> 24) & 0xff;
  }
}

/* Encrypt the file given the symmetric key */
void borum_file(FILE *out, FILE *in, digit_t *key, int mode){
  roundkeys(rkeys, key, COUNT);
  static unsigned char ibuf[16], obuf[16], prev_inp[16];
  long pos = ftell(in);
  fseek(in, 0, SEEK_END);
  long end = ftell(in);
  fseek(in, pos, SEEK_SET);
  long len = end - pos;
  long tlen = len;
  for(int i = 0; i < 4; i++){
    fputc(tlen & 0xff, out);
    tlen >>= 8;
  }
  for(int i = 0; i < 16; i ++){
    obuf[i] = rand() & 0xff; /* Random IV, gets its own block so don't worry */
  }
  if(mode == BORUM_CFB || mode == BORUM_OFB || mode == BORUM_CTR)
    memcpy(prev_inp, obuf, 16);
  if(mode == BORUM_CBC || mode == BORUM_PCBC || mode == BORUM_CFB || mode == BORUM_OFB || mode == BORUM_CTR)
    fwrite(obuf, 1, 16, out);
  long full_blocks = (len + 15) / 16;
  for(long i = 0; i < full_blocks; i ++){
    int read = fread(ibuf, 1, 16, in);
    if(read < 16){
      for(int j = read; j < 16; j++){
        ibuf[j] = 0;
        if(j == read)
          ibuf[j] |= 0x80;
        if(j == 16-2)
          ibuf[j] |= (len >> 8) & 0xff;
        else if(j == 16-1)
          ibuf[j] |= len & 0xff;
      }
    }
    /* CFB has a somewhat different methodology here */
    if(mode == BORUM_CFB || mode == BORUM_OFB){
      borum_block(prev_inp, prev_inp, key, BORUM_ENCRYPT);
      for(int j = 0; j < 16; j++){
        obuf[j] = prev_inp[j] ^ ibuf[j];
        if(mode == BORUM_CFB)
          prev_inp[j] = obuf[j];
      }
      fwrite(obuf, 1, 16, out);
    }
    else if(mode == BORUM_CTR){
      borum_block(obuf, prev_inp, key, BORUM_ENCRYPT);
      for(int j = 0; j < 16; j++){
        obuf[j] ^= ibuf[j];
      }
      fwrite(obuf, 1, 16, out);
      int carr = 1;
      for(int j = 0; j < 16; j++){
        int sum = prev_inp[j] + carr;
        prev_inp[j] = sum & 0xff;
        carr = sum >> 8;
      }
    }
    /* ECB, CBC, and PCBC all use mostly the same methodology */
    else{
      /* XOR input w/ previous output for CBC/PCBC */
      if(mode == BORUM_CBC || mode == BORUM_PCBC){
        for(int j = 0; j < 16; j++){
          /* Keep track of input for PCBC */
          if(mode == BORUM_PCBC)
            prev_inp[j] = ibuf[j];
          ibuf[j] ^= obuf[j];
        }
      }
      borum_block(obuf, ibuf, key, BORUM_ENCRYPT);
      fwrite(obuf, 1, 16, out);
      /* XOR output w/ this input for PCBC */
      if(mode == BORUM_PCBC){
        for(int j = 0; j < 16; j++){
          obuf[j] ^= prev_inp[j];
        }
      }
    }
  }
}

/* Decrypt it for the given symmetric key */
void murob_file(FILE *out, FILE *in, digit_t *key, int mode){
  roundkeys(rkeys, key, COUNT);
  static unsigned char ibuf[16], obuf[16], prev_inp[16];
  memset(prev_inp, 0, 16);
  digit_t flen = 0;
  for(int i = 0; i < 4; i++){
    int byte = fgetc(in);
    flen |= byte << (i * 8);
  }
  if(mode == BORUM_CBC)
    flen += 16; /* Allow for random IV block */
  else if(mode == BORUM_PCBC || mode == BORUM_CFB || mode == BORUM_OFB || mode == BORUM_CTR)
    fread(prev_inp, 1, 16, in); /* Read in the IV now */
  digit_t written = 0;
  while(written < flen){
    int read = fread(ibuf, 1, 16, in);
    if(read == 0)
      break;
    int towrite = written + 16 > flen ? flen - written : 16;
    written += towrite;
    /* ENCRYPT using prev_inp */
    if(mode == BORUM_CFB || mode == BORUM_OFB){
      borum_block(prev_inp, prev_inp, key, BORUM_ENCRYPT);
      for(int j = 0; j < 16; j++){
        obuf[j] = prev_inp[j] ^ ibuf[j];
        if(mode == BORUM_CFB){
          prev_inp[j] = ibuf[j];
        }
      }
      fwrite(obuf, 1, towrite, out);
    }
    else if(mode == BORUM_CTR){
      borum_block(obuf, prev_inp, key, BORUM_ENCRYPT);
      for(int j = 0; j < 16; j++){
        obuf[j] ^= ibuf[j];
      }
      fwrite(obuf, 1, towrite, out);
      int carr = 1;
      for(int j = 0; j < 16; j++){
        int sum = prev_inp[j] + carr;
        prev_inp[j] = sum & 0xff;
        carr = sum >> 8;
      }
    }
    /* Basic code for ECB, CBC and PCBC */
    else{
      borum_block(obuf, ibuf, key, BORUM_DECRYPT);
      /* XOR output w/ previous input for CBC/PCBC */
      if(mode == BORUM_CBC || mode == BORUM_PCBC){
        for(int j = 0; j < 16; j++){
          obuf[j] ^= prev_inp[j];
          prev_inp[j] = ibuf[j];
          /* Then XOR w/ the output for next input 2/ PCBC */
          if(mode == BORUM_PCBC)
            prev_inp[j] ^= obuf[j];
        }
      }
      if(mode != BORUM_CBC || written > 16) /* Only write if not the IV block */
        fwrite(obuf, 1, towrite, out);
    }
    if(read < 16)
      break;
  }
}

/* Encrypt a file w/ an RSA (public) key */
void rsa_borum(FILE *out, FILE *in, rsakey_t *key, int mode){
  static binode_t *askey = NULL, *enckey;
  static unsigned char buffer[1024];
  if(bigint_size < 4)
    bigint_resize(4, bigfix_point);
  if(askey == NULL){
    askey = bigint_link();
    enckey = bigint_link();
  }
  randbits(askey->value->digits, 4, 128);
  rsa_encrypt(key, enckey, askey);
  unsigned char *ptr = der_bigint(buffer, enckey);
  long len = ptr - buffer;
  fwrite(buffer, 1, len, out);
  borum_file(out, in, askey->value->digits, mode);
}

/* Decrypt */
void rsa_murob(FILE *out, FILE *in, rsakey_t *key, int mode){
  static binode_t *askey = NULL, *enckey;
  static unsigned char buffer[1024];
  if(bigint_size < 4)
    bigint_resize(4, bigfix_point);
  if(askey == NULL){
    askey = bigint_link();
    enckey = bigint_link();
  }
  long pos = ftell(in);
  fread(buffer, 1, 1024, in);
  unsigned char *ptr = der_read(enckey, buffer);
  fseek(in, pos + (ptr - buffer), SEEK_SET);
  rsa_decrypt(key, askey, enckey);
  murob_file(out, in, askey->value->digits, mode);
}

/* I'll leave this here for an example */
// int main(){
  // seedrand();
  // bigint_init(16);
  // rsakey_t rsakey;
  // rsakey_init(&rsakey, RSA_KEY_PRIVATE);
  // rsa_keygen(&rsakey, 512);
  // FILE *plain = fopen("image.jpg", "rb");
  // FILE *cipher = fopen("cipher.cipher", "wb");
  // rsa_borum(cipher, plain, &rsakey, BORUM_CTR);
  // printf("Encrypted!\n");
  // fclose(plain);
  // fclose(cipher);
  // cipher = fopen("cipher.cipher", "rb");
  // FILE *recon = fopen("recon.jpg", "wb");
  // rsa_murob(recon, cipher, &rsakey, BORUM_CTR);
  // printf("Decrypted!\n");
  // fclose(cipher);
  // fclose(recon);
  // bigint_destroy();
  // return 0;
// }

#undef COUNT