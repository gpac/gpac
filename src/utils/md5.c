/*
 * from https://github.com/Zunawe/md5-c, public domain
 *
 * Derived from the RSA Data Security, Inc. MD5 Message-Digest Algorithm
 * and modified slightly to be functionally identical but condensed into control structures.
 */

#include <gpac/tools.h>

typedef struct{
    u64 size;        // Size of input in bytes
    u32 buffer[4];   // Current accumulation of hash
    u8 input[64];    // Input to be used in the next step
    u8 digest[16];   // Result of algorithm
} MD5Context;

/*
void md5Init(MD5Context *ctx);
void md5Update(MD5Context *ctx, u8 *input, size_t input_len);
void md5Finalize(MD5Context *ctx);
*/
static void md5Step(u32 *buffer, u32 *input);

/*
 * Constants defined by the MD5 algorithm
 */
#define A 0x67452301
#define B 0xefcdab89
#define C 0x98badcfe
#define D 0x10325476

static u32 S[] = {7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22,
                       5,  9, 14, 20, 5,  9, 14, 20, 5,  9, 14, 20, 5,  9, 14, 20,
                       4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
                       6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21};

static u32 K[] = {0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee,
                       0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
                       0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be,
                       0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
                       0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa,
                       0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
                       0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed,
                       0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
                       0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c,
                       0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
                       0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05,
                       0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
                       0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039,
                       0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
                       0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1,
                       0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391};

/*
 * Padding used to make the size (in bits) of the input congruent to 448 mod 512
 */
static u8 PADDING[] = {0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

/*
 * Bit-manipulation functions defined by the MD5 algorithm
 */
#define F(X, Y, Z) ((X & Y) | (~X & Z))
#define G(X, Y, Z) ((X & Z) | (Y & ~Z))
#define H(X, Y, Z) (X ^ Y ^ Z)
#define I(X, Y, Z) (Y ^ (X | ~Z))

/*
 * Rotates a 32-bit word left by n bits
 */
u32 rotateLeft(u32 x, u32 n){
    return (x << n) | (x >> (32 - n));
}


/*
 * Initialize a context
 */
void md5Init(MD5Context *ctx){
    ctx->size = (u64)0;

    ctx->buffer[0] = (u32)A;
    ctx->buffer[1] = (u32)B;
    ctx->buffer[2] = (u32)C;
    ctx->buffer[3] = (u32)D;
}

/*
 * Add some amount of input to the context
 *
 * If the input fills out a block of 512 bits, apply the algorithm (md5Step)
 * and save the result in the buffer. Also updates the overall size.
 */
void md5Update(MD5Context *ctx, u8 *input_buffer, size_t input_len){
    u32 input[16];
    unsigned int offset = ctx->size % 64;
    ctx->size += (u64)input_len;

    // Copy each byte in input_buffer into the next space in our context input
    for(unsigned int i = 0; i < input_len; ++i){
        ctx->input[offset++] = (u8)*(input_buffer + i);

        // If we've filled our context input, copy it into our local array input
        // then reset the offset to 0 and fill in a new buffer.
        // Every time we fill out a chunk, we run it through the algorithm
        // to enable some back and forth between cpu and i/o
        if(offset % 64 == 0){
            for(unsigned int j = 0; j < 16; ++j){
                // Convert to little-endian
                // The local variable `input` our 512-bit chunk separated into 32-bit words
                // we can use in calculations
                input[j] = (u32)(ctx->input[(j * 4) + 3]) << 24 |
                           (u32)(ctx->input[(j * 4) + 2]) << 16 |
                           (u32)(ctx->input[(j * 4) + 1]) <<  8 |
                           (u32)(ctx->input[(j * 4)]);
            }
            md5Step(ctx->buffer, input);
            offset = 0;
        }
    }
}

/*
 * Pad the current input to get to 448 bytes, append the size in bits to the very end,
 * and save the result of the final iteration into digest.
 */
void md5Finalize(MD5Context *ctx){
    u32 input[16];
    unsigned int offset = ctx->size % 64;
    unsigned int padding_length = offset < 56 ? 56 - offset : (56 + 64) - offset;

    // Fill in the padding and undo the changes to size that resulted from the update
    md5Update(ctx, PADDING, padding_length);
    ctx->size -= (u64)padding_length;

    // Do a final update (internal to this function)
    // Last two 32-bit words are the two halves of the size (converted from bytes to bits)
    for(unsigned int j = 0; j < 14; ++j){
        input[j] = (u32)(ctx->input[(j * 4) + 3]) << 24 |
                   (u32)(ctx->input[(j * 4) + 2]) << 16 |
                   (u32)(ctx->input[(j * 4) + 1]) <<  8 |
                   (u32)(ctx->input[(j * 4)]);
    }
    input[14] = (u32)(ctx->size * 8);
    input[15] = (u32)((ctx->size * 8) >> 32);

    md5Step(ctx->buffer, input);

    // Move the result into digest (convert from little-endian)
    for(unsigned int i = 0; i < 4; ++i){
        ctx->digest[(i * 4) + 0] = (u8)((ctx->buffer[i] & 0x000000FF));
        ctx->digest[(i * 4) + 1] = (u8)((ctx->buffer[i] & 0x0000FF00) >>  8);
        ctx->digest[(i * 4) + 2] = (u8)((ctx->buffer[i] & 0x00FF0000) >> 16);
        ctx->digest[(i * 4) + 3] = (u8)((ctx->buffer[i] & 0xFF000000) >> 24);
    }
}

/*
 * Step on 512 bits of input with the main MD5 algorithm.
 */
static void md5Step(u32 *buffer, u32 *input){
    u32 AA = buffer[0];
    u32 BB = buffer[1];
    u32 CC = buffer[2];
    u32 DD = buffer[3];

    u32 E;

    unsigned int j;

    for(unsigned int i = 0; i < 64; ++i){
        switch(i / 16){
            case 0:
                E = F(BB, CC, DD);
                j = i;
                break;
            case 1:
                E = G(BB, CC, DD);
                j = ((i * 5) + 1) % 16;
                break;
            case 2:
                E = H(BB, CC, DD);
                j = ((i * 3) + 5) % 16;
                break;
            default:
                E = I(BB, CC, DD);
                j = (i * 7) % 16;
                break;
        }

        u32 temp = DD;
        DD = CC;
        CC = BB;
        BB = BB + rotateLeft(AA + E + K[i] + input[j], S[i]);
        AA = temp;
    }

    buffer[0] += AA;
    buffer[1] += BB;
    buffer[2] += CC;
    buffer[3] += DD;
}

/*
 * Functions that run the algorithm on the provided input and put the digest into result.
 * result should be able to store 16 bytes.
 */
#define GF_MD5_DIGEST_SIZE	16

GF_EXPORT
void gf_md5_csum(const void *data, u64 len, u8 output[GF_MD5_DIGEST_SIZE])
{
	MD5Context ctx;
	md5Init(&ctx);
	md5Update(&ctx, (u8 *)data, len);
	md5Finalize(&ctx);
	memcpy(output, ctx.digest, 16);
}


