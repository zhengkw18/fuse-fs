#include "utils.h"
#include "time.h"
#include "stdio.h"
#include <string.h>

u64 A[5][5];

static const u8 rhotates[5][5] = {
    {0, 1, 62, 28, 27},
    {36, 44, 6, 55, 20},
    {3, 10, 43, 25, 39},
    {41, 45, 15, 21, 8},
    {18, 2, 61, 56, 14}};

static const u64 rc[] = {
    0x0000000000000001, 0x0000000000008082, 0x800000000000808a,
    0x8000000080008000, 0x000000000000808b, 0x0000000080000001,
    0x8000000080008081, 0x8000000000008009, 0x000000000000008a,
    0x0000000000000088, 0x0000000080008009, 0x000000008000000a,
    0x000000008000808b, 0x800000000000008b, 0x8000000000008089,
    0x8000000000008003, 0x8000000000008002, 0x8000000000000080,
    0x000000000000800a, 0x800000008000000a, 0x8000000080008081,
    0x8000000000008080, 0x0000000080000001, 0x8000000080008008};

inline u64 ROL64(u64 val, int offset)
{
    if (offset == 0)
    {
        return val;
    }
    return (val << offset) | (val >> (64 - offset));
}

inline void Theta()
{
    u64 C[5], D[5];

    C[0] = A[0][0] ^ A[1][0] ^ A[2][0] ^ A[3][0] ^ A[4][0];
    C[1] = A[0][1] ^ A[1][1] ^ A[2][1] ^ A[3][1] ^ A[4][1];
    C[2] = A[0][2] ^ A[1][2] ^ A[2][2] ^ A[3][2] ^ A[4][2];
    C[3] = A[0][3] ^ A[1][3] ^ A[2][3] ^ A[3][3] ^ A[4][3];
    C[4] = A[0][4] ^ A[1][4] ^ A[2][4] ^ A[3][4] ^ A[4][4];

    D[0] = ROL64(C[1], 1) ^ C[4];
    D[1] = ROL64(C[2], 1) ^ C[0];
    D[2] = ROL64(C[3], 1) ^ C[1];
    D[3] = ROL64(C[4], 1) ^ C[2];
    D[4] = ROL64(C[0], 1) ^ C[3];

    for (int y = 0; y < 5; y++)
    {
        A[y][0] ^= D[0];
        A[y][1] ^= D[1];
        A[y][2] ^= D[2];
        A[y][3] ^= D[3];
        A[y][4] ^= D[4];
    }
}

inline void Rho()
{
    for (int y = 0; y < 5; y++)
    {
        A[y][0] = ROL64(A[y][0], rhotates[y][0]);
        A[y][1] = ROL64(A[y][1], rhotates[y][1]);
        A[y][2] = ROL64(A[y][2], rhotates[y][2]);
        A[y][3] = ROL64(A[y][3], rhotates[y][3]);
        A[y][4] = ROL64(A[y][4], rhotates[y][4]);
    }
}

inline void Pi()
{
    u64 T[5][5];
    memcpy(T, A, sizeof(T));

    A[0][0] = T[0][0];
    A[0][1] = T[1][1];
    A[0][2] = T[2][2];
    A[0][3] = T[3][3];
    A[0][4] = T[4][4];

    A[1][0] = T[0][3];
    A[1][1] = T[1][4];
    A[1][2] = T[2][0];
    A[1][3] = T[3][1];
    A[1][4] = T[4][2];

    A[2][0] = T[0][1];
    A[2][1] = T[1][2];
    A[2][2] = T[2][3];
    A[2][3] = T[3][4];
    A[2][4] = T[4][0];

    A[3][0] = T[0][4];
    A[3][1] = T[1][0];
    A[3][2] = T[2][1];
    A[3][3] = T[3][2];
    A[3][4] = T[4][3];

    A[4][0] = T[0][2];
    A[4][1] = T[1][3];
    A[4][2] = T[2][4];
    A[4][3] = T[3][0];
    A[4][4] = T[4][1];
}

inline void Chi()
{
    u64 C[5];
    for (int y = 0; y < 5; y++)
    {
        C[0] = A[y][0] ^ (~A[y][1] & A[y][2]);
        C[1] = A[y][1] ^ (~A[y][2] & A[y][3]);
        C[2] = A[y][2] ^ (~A[y][3] & A[y][4]);
        C[3] = A[y][3] ^ (~A[y][4] & A[y][0]);
        C[4] = A[y][4] ^ (~A[y][0] & A[y][1]);

        A[y][0] = C[0];
        A[y][1] = C[1];
        A[y][2] = C[2];
        A[y][3] = C[3];
        A[y][4] = C[4];
    }
}

inline void Iota(int i)
{
    A[0][0] ^= rc[i];
}

void KeccakF1600()
{
    for (int i = 0; i < 24; i++)
    {
        Theta();
        Rho();
        Pi();
        Chi();
        Iota(i);
    }
}

void sha3_256(const u8 *in, int len, u8 *out)
{
    // SHA3-d(M) = KECCAK[d*2](M || 01, d)
    // KECCAK[c] (N, d) = SPONGE[KECCAK-p[1600, 24], pad10*1, 1600–c] (N, d)
    // SHA3-256(M) = SPONGE[KECCAK-p[1600, 24], pad10*1, 1088] (M || 01, 256)
    // d = 32, r = 136, w = 17;
    memset(A, 0, sizeof(A));
    int padded_len = (len + 1 + 136 - 1) / 136 * 136;
    u8 *padded = new u8[padded_len];
    u8 *_padded = padded;
    memcpy(padded, in, len);
    memset(padded + len, 0, padded_len - len);
    padded[len] = 0x06;
    padded[padded_len - 1] |= 0x80;
    u64 *A_flat = (u64 *)A;
    int i;
    while (padded_len)
    {
        for (i = 0; i < 17; i++)
        {
            u64 Ai = (u64)padded[0] | (u64)padded[1] << 8 |
                     (u64)padded[2] << 16 | (u64)padded[3] << 24 |
                     (u64)padded[4] << 32 | (u64)padded[5] << 40 |
                     (u64)padded[6] << 48 | (u64)padded[7] << 56;
            padded += 8;
            A_flat[i] ^= Ai;
        }
        KeccakF1600();
        padded_len -= 136;
    }
    for (i = 0; i < 4; i++)
    {
        u64 Ai = A_flat[i];
        out[0] = Ai;
        out[1] = Ai >> 8;
        out[2] = Ai >> 16;
        out[3] = Ai >> 24;
        out[4] = Ai >> 32;
        out[5] = Ai >> 40;
        out[6] = Ai >> 48;
        out[7] = Ai >> 56;
        out += 8;
    }
    delete[] _padded;
}