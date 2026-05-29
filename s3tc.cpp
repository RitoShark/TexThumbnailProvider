#include <cstdint>
#include <math.h>

// unsigned long PackRGBA(): Helper method that packs RGBA channels into a single 4 byte pixel.
//
// unsigned char r:     red channel.
// unsigned char g:     green channel.
// unsigned char b:     blue channel.
// unsigned char a:     alpha channel.

unsigned long PackRGBA(unsigned char r, unsigned char g, unsigned char b, unsigned char a)
{
    return ((r << 24) | (g << 16) | (b << 8) | a);
}

// void DecompressBlockDXT1(): Decompresses one block of a DXT1 texture and stores the resulting pixels at the appropriate offset in 'image'.
//
// unsigned long x:                     x-coordinate of the first pixel in the block.
// unsigned long y:                     y-coordinate of the first pixel in the block.
// unsigned long width:                 width of the texture being decompressed.
// unsigned long height:                height of the texture being decompressed.
// const unsigned char *blockStorage:   pointer to the block to decompress.
// unsigned long *image:                pointer to image where the decompressed pixel data should be stored.

void DecompressBlockDXT1(unsigned long x, unsigned long y, unsigned long width, const unsigned char* blockStorage, unsigned long* image)
{
    unsigned short color0 = *reinterpret_cast<const unsigned short*>(blockStorage);
    unsigned short color1 = *reinterpret_cast<const unsigned short*>(blockStorage + 2);

    unsigned long temp;

    temp = (color0 >> 11) * 255 + 16;
    unsigned char r0 = (unsigned char)((temp / 32 + temp) / 32);
    temp = ((color0 & 0x07E0) >> 5) * 255 + 32;
    unsigned char g0 = (unsigned char)((temp / 64 + temp) / 64);
    temp = (color0 & 0x001F) * 255 + 16;
    unsigned char b0 = (unsigned char)((temp / 32 + temp) / 32);

    temp = (color1 >> 11) * 255 + 16;
    unsigned char r1 = (unsigned char)((temp / 32 + temp) / 32);
    temp = ((color1 & 0x07E0) >> 5) * 255 + 32;
    unsigned char g1 = (unsigned char)((temp / 64 + temp) / 64);
    temp = (color1 & 0x001F) * 255 + 16;
    unsigned char b1 = (unsigned char)((temp / 32 + temp) / 32);

    unsigned long code = *reinterpret_cast<const unsigned long*>(blockStorage + 4);

    for (int j = 0; j < 4; j++)
    {
        for (int i = 0; i < 4; i++)
        {
            unsigned long finalColor = 0;
            unsigned char positionCode = (code >> 2 * (4 * j + i)) & 0x03;

            if (color0 > color1)
            {
                switch (positionCode)
                {
                case 0:
                    finalColor = PackRGBA(r0, g0, b0, 255);
                    break;
                case 1:
                    finalColor = PackRGBA(r1, g1, b1, 255);
                    break;
                case 2:
                    finalColor = PackRGBA((2 * r0 + r1) / 3, (2 * g0 + g1) / 3, (2 * b0 + b1) / 3, 255);
                    break;
                case 3:
                    finalColor = PackRGBA((r0 + 2 * r1) / 3, (g0 + 2 * g1) / 3, (b0 + 2 * b1) / 3, 255);
                    break;
                }
            }
            else
            {
                switch (positionCode)
                {
                case 0:
                    finalColor = PackRGBA(r0, g0, b0, 255);
                    break;
                case 1:
                    finalColor = PackRGBA(r1, g1, b1, 255);
                    break;
                case 2:
                    finalColor = PackRGBA((r0 + r1) / 2, (g0 + g1) / 2, (b0 + b1) / 2, 255);
                    break;
                case 3:
                    finalColor = PackRGBA(0, 0, 0, 255);
                    break;
                }
            }

            if (x + i < width)
                image[(y + j) * width + (x + i)] = finalColor;
        }
    }
}

// void BlockDecompressImageDXT1(): Decompresses all the blocks of a DXT1 compressed texture and stores the resulting pixels in 'image'.
//
// unsigned long width:                 Texture width.
// unsigned long height:                Texture height.
// const unsigned char *blockStorage:   pointer to compressed DXT1 blocks.
// unsigned long *image:                pointer to the image where the decompressed pixels will be stored.

void BlockDecompressImageDXT1(unsigned long width, unsigned long height, const unsigned char* blockStorage, unsigned long* image)
{
    unsigned long blockCountX = (width + 3) / 4;
    unsigned long blockCountY = (height + 3) / 4;
    /*
    unsigned long blockWidth = (width < 4) ? width : 4;
    unsigned long blockHeight = (height < 4) ? height : 4;
    */

    for (unsigned long j = 0; j < blockCountY; j++)
    {
        for (unsigned long i = 0; i < blockCountX; i++) DecompressBlockDXT1(i * 4, j * 4, width, blockStorage + i * 8, image);
        blockStorage += blockCountX * 8;
    }
}

// void DecompressBlockDXT5(): Decompresses one block of a DXT5 texture and stores the resulting pixels at the appropriate offset in 'image'.
//
// unsigned long x:                     x-coordinate of the first pixel in the block.
// unsigned long y:                     y-coordinate of the first pixel in the block.
// unsigned long width:                 width of the texture being decompressed.
// unsigned long height:                height of the texture being decompressed.
// const unsigned char *blockStorage:   pointer to the block to decompress.
// unsigned long *image:                pointer to image where the decompressed pixel data should be stored.

void DecompressBlockDXT5(unsigned long x, unsigned long y, unsigned long width, const unsigned char* blockStorage, unsigned long* image)
{
    unsigned char alpha0 = *reinterpret_cast<const unsigned char*>(blockStorage);
    unsigned char alpha1 = *reinterpret_cast<const unsigned char*>(blockStorage + 1);

    const unsigned char* bits = blockStorage + 2;
    unsigned long alphaCode1 = bits[2] | (bits[3] << 8) | (bits[4] << 16) | (bits[5] << 24);
    unsigned short alphaCode2 = bits[0] | (bits[1] << 8);

    unsigned short color0 = *reinterpret_cast<const unsigned short*>(blockStorage + 8);
    unsigned short color1 = *reinterpret_cast<const unsigned short*>(blockStorage + 10);

    unsigned long temp;

    temp = (color0 >> 11) * 255 + 16;
    unsigned char r0 = (unsigned char)((temp / 32 + temp) / 32);
    temp = ((color0 & 0x07E0) >> 5) * 255 + 32;
    unsigned char g0 = (unsigned char)((temp / 64 + temp) / 64);
    temp = (color0 & 0x001F) * 255 + 16;
    unsigned char b0 = (unsigned char)((temp / 32 + temp) / 32);

    temp = (color1 >> 11) * 255 + 16;
    unsigned char r1 = (unsigned char)((temp / 32 + temp) / 32);
    temp = ((color1 & 0x07E0) >> 5) * 255 + 32;
    unsigned char g1 = (unsigned char)((temp / 64 + temp) / 64);
    temp = (color1 & 0x001F) * 255 + 16;
    unsigned char b1 = (unsigned char)((temp / 32 + temp) / 32);

    unsigned long code = *reinterpret_cast<const unsigned long*>(blockStorage + 12);

    for (int j = 0; j < 4; j++)
    {
        for (int i = 0; i < 4; i++)
        {
            int alphaCodeIndex = 3 * (4 * j + i);
            int alphaCode;

            if (alphaCodeIndex <= 12)
            {
                alphaCode = (alphaCode2 >> alphaCodeIndex) & 0x07;
            }
            else if (alphaCodeIndex == 15)
            {
                alphaCode = (alphaCode2 >> 15) | ((alphaCode1 << 1) & 0x06);
            }
            else // alphaCodeIndex >= 18 && alphaCodeIndex <= 45
            {
                alphaCode = (alphaCode1 >> (alphaCodeIndex - 16)) & 0x07;
            }

            unsigned char finalAlpha;
            if (alphaCode == 0)
            {
                finalAlpha = alpha0;
            }
            else if (alphaCode == 1)
            {
                finalAlpha = alpha1;
            }
            else
            {
                if (alpha0 > alpha1)
                {
                    finalAlpha = ((8 - alphaCode) * alpha0 + (alphaCode - 1) * alpha1) / 7;
                }
                else
                {
                    if (alphaCode == 6)
                        finalAlpha = 0;
                    else if (alphaCode == 7)
                        finalAlpha = 255;
                    else
                        finalAlpha = ((6 - alphaCode) * alpha0 + (alphaCode - 1) * alpha1) / 5;
                }
            }

            unsigned char colorCode = (code >> 2 * (4 * j + i)) & 0x03;

            unsigned long finalColor{};
            switch (colorCode)
            {
            case 0:
                finalColor = PackRGBA(r0, g0, b0, finalAlpha);
                break;
            case 1:
                finalColor = PackRGBA(r1, g1, b1, finalAlpha);
                break;
            case 2:
                finalColor = PackRGBA((2 * r0 + r1) / 3, (2 * g0 + g1) / 3, (2 * b0 + b1) / 3, finalAlpha);
                break;
            case 3:
                finalColor = PackRGBA((r0 + 2 * r1) / 3, (g0 + 2 * g1) / 3, (b0 + 2 * b1) / 3, finalAlpha);
                break;
            }

            if (x + i < width)
                image[(y + j) * width + (x + i)] = finalColor;
        }
    }
}

// void BlockDecompressImageDXT5(): Decompresses all the blocks of a DXT5 compressed texture and stores the resulting pixels in 'image'.
//
// unsigned long width:                 Texture width.
// unsigned long height:                Texture height.
// const unsigned char *blockStorage:   pointer to compressed DXT5 blocks.
// unsigned long *image:                pointer to the image where the decompressed pixels will be stored.

void BlockDecompressImageDXT5(unsigned long width, unsigned long height, const unsigned char* blockStorage, unsigned long* image)
{
    unsigned long blockCountX = (width + 3) / 4;
    unsigned long blockCountY = (height + 3) / 4;
    /*
    unsigned long blockWidth = (width < 4) ? width : 4;
    unsigned long blockHeight = (height < 4) ? height : 4;
    */

    for (unsigned long j = 0; j < blockCountY; j++)
    {
        for (unsigned long i = 0; i < blockCountX; i++) DecompressBlockDXT5(i * 4, j * 4, width, blockStorage + i * 16, image);
        blockStorage += blockCountX * 16;
    }
}

////////////////////////////////////////////////////////////////////////////////
// BC5 (two-channel) decode — Riot uses this for normal maps (tex_format 0xE).
//
// A BC5 block is two consecutive BC4 (single-channel) blocks of 8 bytes each:
// the first encodes red, the second green. We decode both channels, then
// reconstruct the normal's Z into blue so the thumbnail looks like a normal map.
////////////////////////////////////////////////////////////////////////////////

// Decode one 8-byte BC4 (unsigned) block into 16 channel values (row-major 4x4).
static void DecodeBC4Block(const unsigned char* block, unsigned char out[16])
{
    unsigned char e0 = block[0];
    unsigned char e1 = block[1];

    unsigned char palette[8];
    palette[0] = e0;
    palette[1] = e1;
    if (e0 > e1)
    {
        for (int i = 2; i < 8; i++)
            palette[i] = (unsigned char)(((8 - i) * e0 + (i - 1) * e1) / 7);
    }
    else
    {
        for (int i = 2; i < 6; i++)
            palette[i] = (unsigned char)(((6 - i) * e0 + (i - 1) * e1) / 5);
        palette[6] = 0;
        palette[7] = 255;
    }

    // 16 x 3-bit indices packed little-endian across the 6 index bytes.
    uint64_t indices = 0;
    for (int b = 0; b < 6; b++)
        indices |= (uint64_t)block[2 + b] << (8 * b);

    for (int i = 0; i < 16; i++)
        out[i] = palette[(indices >> (3 * i)) & 0x7];
}

void BlockDecompressImageBC5(unsigned long width, unsigned long height, const unsigned char* blockStorage, unsigned long* image)
{
    unsigned long blockCountX = (width + 3) / 4;
    unsigned long blockCountY = (height + 3) / 4;

    for (unsigned long by = 0; by < blockCountY; by++)
    {
        for (unsigned long bx = 0; bx < blockCountX; bx++)
        {
            const unsigned char* block = blockStorage + (by * blockCountX + bx) * 16;

            unsigned char red[16];
            unsigned char green[16];
            DecodeBC4Block(block, red);
            DecodeBC4Block(block + 8, green);

            for (int t = 0; t < 16; t++)
            {
                unsigned long px = bx * 4 + (t & 3);
                unsigned long py = by * 4 + (t >> 2);
                if (px >= width || py >= height)
                    continue;

                float nx = red[t] / 255.0f * 2.0f - 1.0f;
                float ny = green[t] / 255.0f * 2.0f - 1.0f;
                float nz2 = 1.0f - nx * nx - ny * ny;
                float nz = nz2 > 0.0f ? sqrtf(nz2) : 0.0f;
                unsigned char b = (unsigned char)((nz * 0.5f + 0.5f) * 255.0f + 0.5f);

                image[py * width + px] = PackRGBA(red[t], green[t], b, 255);
            }
        }
    }
}

////////////////////////////////////////////////////////////////////////////////
// BC7 decode (tex_format 0xD) — full LDR decode of all 8 modes.
//
// Self-contained port of the BC7 spec / DirectXTex reference decode path:
//   * g_aPartitionTable / g_aFixUp / weight tables are the standard BC7 tables
//   * a 128-bit LSB-first bit reader matching DirectXTex's CBits::GetBits
//   * per-mode endpoint precision / P-bit / index handling from ms_aInfo
////////////////////////////////////////////////////////////////////////////////

static const int g_aWeights2[] = { 0, 21, 43, 64 };
static const int g_aWeights3[] = { 0, 9, 18, 27, 37, 46, 55, 64 };
static const int g_aWeights4[] = { 0, 4, 9, 13, 17, 21, 26, 30, 34, 38, 43, 47, 51, 55, 60, 64 };

// Partition assignment per subset-count (0/1/2 -> 1/2/3 subsets), shape, texel.
static const unsigned char g_bc7Partitions[3][64][16] =
{
    {   // 1 subset
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}
    },
    {   // 2 subsets
        {0,0,1,1,0,0,1,1,0,0,1,1,0,0,1,1},{0,0,0,1,0,0,0,1,0,0,0,1,0,0,0,1},
        {0,1,1,1,0,1,1,1,0,1,1,1,0,1,1,1},{0,0,0,1,0,0,1,1,0,0,1,1,0,1,1,1},
        {0,0,0,0,0,0,0,1,0,0,0,1,0,0,1,1},{0,0,1,1,0,1,1,1,0,1,1,1,1,1,1,1},
        {0,0,0,1,0,0,1,1,0,1,1,1,1,1,1,1},{0,0,0,0,0,0,0,1,0,0,1,1,0,1,1,1},
        {0,0,0,0,0,0,0,0,0,0,0,1,0,0,1,1},{0,0,1,1,0,1,1,1,1,1,1,1,1,1,1,1},
        {0,0,0,0,0,0,0,1,0,1,1,1,1,1,1,1},{0,0,0,0,0,0,0,0,0,0,0,1,0,1,1,1},
        {0,0,0,1,0,1,1,1,1,1,1,1,1,1,1,1},{0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1},
        {0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1},{0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1},
        {0,0,0,0,1,0,0,0,1,1,1,0,1,1,1,1},{0,1,1,1,0,0,0,1,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,1,0,0,0,1,1,1,0},{0,1,1,1,0,0,1,1,0,0,0,1,0,0,0,0},
        {0,0,1,1,0,0,0,1,0,0,0,0,0,0,0,0},{0,0,0,0,1,0,0,0,1,1,0,0,1,1,1,0},
        {0,0,0,0,0,0,0,0,1,0,0,0,1,1,0,0},{0,1,1,1,0,0,1,1,0,0,1,1,0,0,0,1},
        {0,0,1,1,0,0,0,1,0,0,0,1,0,0,0,0},{0,0,0,0,1,0,0,0,1,0,0,0,1,1,0,0},
        {0,1,1,0,0,1,1,0,0,1,1,0,0,1,1,0},{0,0,1,1,0,1,1,0,0,1,1,0,1,1,0,0},
        {0,0,0,1,0,1,1,1,1,1,1,0,1,0,0,0},{0,0,0,0,1,1,1,1,1,1,1,1,0,0,0,0},
        {0,1,1,1,0,0,0,1,1,0,0,0,1,1,1,0},{0,0,1,1,1,0,0,1,1,0,0,1,1,1,0,0},
        {0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1},{0,0,0,0,1,1,1,1,0,0,0,0,1,1,1,1},
        {0,1,0,1,1,0,1,0,0,1,0,1,1,0,1,0},{0,0,1,1,0,0,1,1,1,1,0,0,1,1,0,0},
        {0,0,1,1,1,1,0,0,0,0,1,1,1,1,0,0},{0,1,0,1,0,1,0,1,1,0,1,0,1,0,1,0},
        {0,1,1,0,1,0,0,1,0,1,1,0,1,0,0,1},{0,1,0,1,1,0,1,0,1,0,1,0,0,1,0,1},
        {0,1,1,1,0,0,1,1,1,1,0,0,1,1,1,0},{0,0,0,1,0,0,1,1,1,1,0,0,1,0,0,0},
        {0,0,1,1,0,0,1,0,0,1,0,0,1,1,0,0},{0,0,1,1,1,0,1,1,1,1,0,1,1,1,0,0},
        {0,1,1,0,1,0,0,1,1,0,0,1,0,1,1,0},{0,0,1,1,1,1,0,0,1,1,0,0,0,0,1,1},
        {0,1,1,0,0,1,1,0,1,0,0,1,1,0,0,1},{0,0,0,0,0,1,1,0,0,1,1,0,0,0,0,0},
        {0,1,0,0,1,1,1,0,0,1,0,0,0,0,0,0},{0,0,1,0,0,1,1,1,0,0,1,0,0,0,0,0},
        {0,0,0,0,0,0,1,0,0,1,1,1,0,0,1,0},{0,0,0,0,0,1,0,0,1,1,1,0,0,1,0,0},
        {0,1,1,0,1,1,0,0,1,0,0,1,0,0,1,1},{0,0,1,1,0,1,1,0,1,1,0,0,1,0,0,1},
        {0,1,1,0,0,0,1,1,1,0,0,1,1,1,0,0},{0,0,1,1,1,0,0,1,1,1,0,0,0,1,1,0},
        {0,1,1,0,1,1,0,0,1,1,0,0,1,0,0,1},{0,1,1,0,0,0,1,1,0,0,1,1,1,0,0,1},
        {0,1,1,1,1,1,1,0,1,0,0,0,0,0,0,1},{0,0,0,1,1,0,0,0,1,1,1,0,0,1,1,1},
        {0,0,0,0,1,1,1,1,0,0,1,1,0,0,1,1},{0,0,1,1,0,0,1,1,1,1,1,1,0,0,0,0},
        {0,0,1,0,0,0,1,0,1,1,1,0,1,1,1,0},{0,1,0,0,0,1,0,0,0,1,1,1,0,1,1,1}
    },
    {   // 3 subsets
        {0,0,1,1,0,0,1,1,0,2,2,1,2,2,2,2},{0,0,0,1,0,0,1,1,2,2,1,1,2,2,2,1},
        {0,0,0,0,2,0,0,1,2,2,1,1,2,2,1,1},{0,2,2,2,0,0,2,2,0,0,1,1,0,1,1,1},
        {0,0,0,0,0,0,0,0,1,1,2,2,1,1,2,2},{0,0,1,1,0,0,1,1,0,0,2,2,0,0,2,2},
        {0,0,2,2,0,0,2,2,1,1,1,1,1,1,1,1},{0,0,1,1,0,0,1,1,2,2,1,1,2,2,1,1},
        {0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2},{0,0,0,0,1,1,1,1,1,1,1,1,2,2,2,2},
        {0,0,0,0,1,1,1,1,2,2,2,2,2,2,2,2},{0,0,1,2,0,0,1,2,0,0,1,2,0,0,1,2},
        {0,1,1,2,0,1,1,2,0,1,1,2,0,1,1,2},{0,1,2,2,0,1,2,2,0,1,2,2,0,1,2,2},
        {0,0,1,1,0,1,1,2,1,1,2,2,1,2,2,2},{0,0,1,1,2,0,0,1,2,2,0,0,2,2,2,0},
        {0,0,0,1,0,0,1,1,0,1,1,2,1,1,2,2},{0,1,1,1,0,0,1,1,2,0,0,1,2,2,0,0},
        {0,0,0,0,1,1,2,2,1,1,2,2,1,1,2,2},{0,0,2,2,0,0,2,2,0,0,2,2,1,1,1,1},
        {0,1,1,1,0,1,1,1,0,2,2,2,0,2,2,2},{0,0,0,1,0,0,0,1,2,2,2,1,2,2,2,1},
        {0,0,0,0,0,0,1,1,0,1,2,2,0,1,2,2},{0,0,0,0,1,1,0,0,2,2,1,0,2,2,1,0},
        {0,1,2,2,0,1,2,2,0,0,1,1,0,0,0,0},{0,0,1,2,0,0,1,2,1,1,2,2,2,2,2,2},
        {0,1,1,0,1,2,2,1,1,2,2,1,0,1,1,0},{0,0,0,0,0,1,1,0,1,2,2,1,1,2,2,1},
        {0,0,2,2,1,1,0,2,1,1,0,2,0,0,2,2},{0,1,1,0,0,1,1,0,2,0,0,2,2,2,2,2},
        {0,0,1,1,0,1,2,2,0,1,2,2,0,0,1,1},{0,0,0,0,2,0,0,0,2,2,1,1,2,2,2,1},
        {0,0,0,0,0,0,0,2,1,1,2,2,1,2,2,2},{0,2,2,2,0,0,2,2,0,0,1,2,0,0,1,1},
        {0,0,1,1,0,0,1,2,0,0,2,2,0,2,2,2},{0,1,2,0,0,1,2,0,0,1,2,0,0,1,2,0},
        {0,0,0,0,1,1,1,1,2,2,2,2,0,0,0,0},{0,1,2,0,1,2,0,1,2,0,1,2,0,1,2,0},
        {0,1,2,0,2,0,1,2,1,2,0,1,0,1,2,0},{0,0,1,1,2,2,0,0,1,1,2,2,0,0,1,1},
        {0,0,1,1,1,1,2,2,2,2,0,0,0,0,1,1},{0,1,0,1,0,1,0,1,2,2,2,2,2,2,2,2},
        {0,0,0,0,0,0,0,0,2,1,2,1,2,1,2,1},{0,0,2,2,1,1,2,2,0,0,2,2,1,1,2,2},
        {0,0,2,2,0,0,1,1,0,0,2,2,0,0,1,1},{0,2,2,0,1,2,2,1,0,2,2,0,1,2,2,1},
        {0,1,0,1,2,2,2,2,2,2,2,2,0,1,0,1},{0,0,0,0,2,1,2,1,2,1,2,1,2,1,2,1},
        {0,1,0,1,0,1,0,1,0,1,0,1,2,2,2,2},{0,2,2,2,0,1,1,1,0,2,2,2,0,1,1,1},
        {0,0,0,2,1,1,1,2,0,0,0,2,1,1,1,2},{0,0,0,0,2,1,1,2,2,1,1,2,2,1,1,2},
        {0,2,2,2,0,1,1,1,0,1,1,1,0,2,2,2},{0,0,0,2,1,1,1,2,1,1,1,2,0,0,0,2},
        {0,1,1,0,0,1,1,0,0,1,1,0,2,2,2,2},{0,0,0,0,0,0,0,0,2,1,1,2,2,1,1,2},
        {0,1,1,0,0,1,1,0,2,2,2,2,2,2,2,2},{0,0,2,2,0,0,1,1,0,0,1,1,0,0,2,2},
        {0,0,2,2,1,1,2,2,1,1,2,2,0,0,2,2},{0,0,0,0,0,0,0,0,0,0,0,0,2,1,1,2},
        {0,0,0,2,0,0,0,1,0,0,0,2,0,0,0,1},{0,2,2,2,1,2,2,2,0,2,2,2,1,2,2,2},
        {0,1,0,1,2,2,2,2,2,2,2,2,2,2,2,2},{0,1,1,1,2,0,1,1,2,2,0,1,2,2,2,0}
    }
};

// Fix-up (anchor) index positions per subset-count, shape.
static const unsigned char g_bc7FixUp[3][64][3] =
{
    {
        {0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},
        {0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},
        {0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},
        {0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},
        {0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},
        {0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},
        {0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},
        {0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0}
    },
    {
        {0,15,0},{0,15,0},{0,15,0},{0,15,0},{0,15,0},{0,15,0},{0,15,0},{0,15,0},
        {0,15,0},{0,15,0},{0,15,0},{0,15,0},{0,15,0},{0,15,0},{0,15,0},{0,15,0},
        {0,15,0},{0,2,0},{0,8,0},{0,2,0},{0,2,0},{0,8,0},{0,8,0},{0,15,0},
        {0,2,0},{0,8,0},{0,2,0},{0,2,0},{0,8,0},{0,8,0},{0,2,0},{0,2,0},
        {0,15,0},{0,15,0},{0,6,0},{0,8,0},{0,2,0},{0,8,0},{0,15,0},{0,15,0},
        {0,2,0},{0,8,0},{0,2,0},{0,2,0},{0,2,0},{0,15,0},{0,15,0},{0,6,0},
        {0,6,0},{0,2,0},{0,6,0},{0,8,0},{0,15,0},{0,15,0},{0,2,0},{0,2,0},
        {0,15,0},{0,15,0},{0,15,0},{0,15,0},{0,15,0},{0,2,0},{0,2,0},{0,15,0}
    },
    {
        {0,3,15},{0,3,8},{0,15,8},{0,15,3},{0,8,15},{0,3,15},{0,15,3},{0,15,8},
        {0,8,15},{0,8,15},{0,6,15},{0,6,15},{0,6,15},{0,5,15},{0,3,15},{0,3,8},
        {0,3,15},{0,3,8},{0,8,15},{0,15,3},{0,3,15},{0,3,8},{0,6,15},{0,10,8},
        {0,5,3},{0,8,15},{0,8,6},{0,6,10},{0,8,15},{0,5,15},{0,15,10},{0,15,8},
        {0,8,15},{0,15,3},{0,3,15},{0,5,10},{0,6,10},{0,10,8},{0,8,9},{0,15,10},
        {0,15,6},{0,3,15},{0,15,8},{0,5,15},{0,15,3},{0,15,6},{0,15,6},{0,15,8},
        {0,3,15},{0,15,3},{0,5,15},{0,5,15},{0,5,15},{0,8,15},{0,5,15},{0,10,15},
        {0,5,15},{0,10,15},{0,8,15},{0,13,15},{0,15,3},{0,12,15},{0,3,15},{0,3,8}
    }
};

// Per-mode info: {partitions, partitionBits, pBits, rotationBits, indexModeBits,
//                 indexPrec, indexPrec2, prec{r,g,b,a}, precWithP{r,g,b,a}}.
struct BC7ModeInfo
{
    unsigned char partitions;
    unsigned char partitionBits;
    unsigned char pBits;
    unsigned char rotationBits;
    unsigned char indexModeBits;
    unsigned char indexPrec;
    unsigned char indexPrec2;
    unsigned char prec[4];
    unsigned char precWithP[4];
};

static const BC7ModeInfo g_bc7Modes[8] =
{
    {2, 4, 6, 0, 0, 3, 0, {4,4,4,0}, {5,5,5,0}},
    {1, 6, 2, 0, 0, 3, 0, {6,6,6,0}, {7,7,7,0}},
    {2, 6, 0, 0, 0, 2, 0, {5,5,5,0}, {5,5,5,0}},
    {1, 6, 4, 0, 0, 2, 0, {7,7,7,0}, {8,8,8,0}},
    {0, 0, 0, 2, 1, 2, 3, {5,5,5,6}, {5,5,5,6}},
    {0, 0, 0, 2, 0, 2, 2, {7,7,7,8}, {7,7,7,8}},
    {0, 0, 2, 0, 0, 4, 0, {7,7,7,7}, {8,8,8,8}},
    {1, 6, 4, 0, 0, 2, 0, {5,5,5,5}, {6,6,6,6}}
};

// 128-bit LSB-first bit reader (matches DirectXTex CBits::GetBit/GetBits).
struct BC7Bits
{
    const unsigned char* bits;

    unsigned char GetBit(size_t& start) const
    {
        size_t index = start >> 3;
        unsigned char ret = (bits[index] >> (start - (index << 3))) & 0x01;
        start++;
        return ret;
    }

    unsigned char GetBits(size_t& start, size_t numBits) const
    {
        if (numBits == 0) return 0;
        unsigned char ret;
        size_t index = start >> 3;
        size_t base = start - (index << 3);
        if (base + numBits > 8)
        {
            size_t firstBits = 8 - base;
            size_t nextBits = numBits - firstBits;
            ret = (unsigned char)((bits[index] >> base) |
                ((bits[index + 1] & ((1 << nextBits) - 1)) << firstBits));
        }
        else
        {
            ret = (unsigned char)((bits[index] >> base) & ((1 << numBits) - 1));
        }
        start += numBits;
        return ret;
    }
};

static unsigned char bc7_unquantize(unsigned char comp, unsigned int prec)
{
    comp = (unsigned char)(comp << (8 - prec));
    return (unsigned char)(comp | (comp >> prec));
}

static unsigned char bc7_interp(unsigned char c0, unsigned char c1, unsigned int w, unsigned int prec)
{
    const int* weights = (prec == 2) ? g_aWeights2 : (prec == 3) ? g_aWeights3 : g_aWeights4;
    return (unsigned char)(((unsigned int)c0 * (unsigned int)(64 - weights[w]) +
        (unsigned int)c1 * (unsigned int)weights[w] + 32) >> 6);
}

static void DecompressBlockBC7(unsigned long bx, unsigned long by, unsigned long width, unsigned long height, const unsigned char* block, unsigned long* image)
{
    BC7Bits stream{ block };

    size_t first = 0;
    while (first < 128 && !stream.GetBit(first)) {}
    unsigned int mode = (unsigned int)(first - 1);

    unsigned char outR[16], outG[16], outB[16], outA[16];

    if (mode >= 8)
    {
        // Reserved mode -> transparent black per spec.
        for (int i = 0; i < 16; i++) { outR[i] = outG[i] = outB[i] = outA[i] = 0; }
    }
    else
    {
        const BC7ModeInfo& info = g_bc7Modes[mode];
        unsigned int numEndPts = (unsigned int)((info.partitions + 1) << 1);
        size_t start = mode + 1;

        unsigned char shape = stream.GetBits(start, info.partitionBits);
        unsigned char rotation = stream.GetBits(start, info.rotationBits);
        unsigned char indexMode = stream.GetBits(start, info.indexModeBits);

        unsigned char c[6][4]; // up to 6 endpoints, RGBA
        unsigned char P[6];

        for (unsigned int ch = 0; ch < 4; ch++)
            for (unsigned int i = 0; i < numEndPts; i++)
                c[i][ch] = info.prec[ch] ? stream.GetBits(start, info.prec[ch]) : 255;

        for (unsigned int i = 0; i < info.pBits; i++)
            P[i] = stream.GetBit(start);

        if (info.pBits)
        {
            for (unsigned int i = 0; i < numEndPts; i++)
            {
                unsigned int pi = i * info.pBits / numEndPts;
                for (unsigned int ch = 0; ch < 4; ch++)
                    if (info.prec[ch] != info.precWithP[ch])
                        c[i][ch] = (unsigned char)((c[i][ch] << 1) | P[pi]);
            }
        }

        for (unsigned int i = 0; i < numEndPts; i++)
            for (unsigned int ch = 0; ch < 4; ch++)
                c[i][ch] = info.precWithP[ch] ? bc7_unquantize(c[i][ch], info.precWithP[ch]) : 255;

        unsigned char w1[16], w2[16];
        for (int i = 0; i < 16; i++)
        {
            bool anchor = false;
            for (unsigned int p = 0; p <= info.partitions; p++)
                if ((unsigned int)i == g_bc7FixUp[info.partitions][shape][p]) anchor = true;
            size_t numBits = anchor ? (size_t)info.indexPrec - 1 : info.indexPrec;
            w1[i] = stream.GetBits(start, numBits);
        }

        if (info.indexPrec2)
        {
            for (int i = 0; i < 16; i++)
            {
                size_t numBits = i ? info.indexPrec2 : (size_t)info.indexPrec2 - 1;
                w2[i] = stream.GetBits(start, numBits);
            }
        }

        for (int i = 0; i < 16; i++)
        {
            unsigned char region = g_bc7Partitions[info.partitions][shape][i];
            const unsigned char* e0 = c[region << 1];
            const unsigned char* e1 = c[(region << 1) + 1];

            unsigned char r, g, b, a;
            if (info.indexPrec2 == 0)
            {
                r = bc7_interp(e0[0], e1[0], w1[i], info.indexPrec);
                g = bc7_interp(e0[1], e1[1], w1[i], info.indexPrec);
                b = bc7_interp(e0[2], e1[2], w1[i], info.indexPrec);
                a = bc7_interp(e0[3], e1[3], w1[i], info.indexPrec);
            }
            else if (indexMode == 0)
            {
                r = bc7_interp(e0[0], e1[0], w1[i], info.indexPrec);
                g = bc7_interp(e0[1], e1[1], w1[i], info.indexPrec);
                b = bc7_interp(e0[2], e1[2], w1[i], info.indexPrec);
                a = bc7_interp(e0[3], e1[3], w2[i], info.indexPrec2);
            }
            else
            {
                r = bc7_interp(e0[0], e1[0], w2[i], info.indexPrec2);
                g = bc7_interp(e0[1], e1[1], w2[i], info.indexPrec2);
                b = bc7_interp(e0[2], e1[2], w2[i], info.indexPrec2);
                a = bc7_interp(e0[3], e1[3], w1[i], info.indexPrec);
            }

            switch (rotation)
            {
            case 1: { unsigned char t = r; r = a; a = t; } break;
            case 2: { unsigned char t = g; g = a; a = t; } break;
            case 3: { unsigned char t = b; b = a; a = t; } break;
            }

            outR[i] = r; outG[i] = g; outB[i] = b; outA[i] = a;
        }
    }

    for (int t = 0; t < 16; t++)
    {
        unsigned long px = bx * 4 + (t & 3);
        unsigned long py = by * 4 + (t >> 2);
        if (px >= width || py >= height)
            continue;
        image[py * width + px] = PackRGBA(outR[t], outG[t], outB[t], outA[t]);
    }
}

void BlockDecompressImageBC7(unsigned long width, unsigned long height, const unsigned char* blockStorage, unsigned long* image)
{
    unsigned long blockCountX = (width + 3) / 4;
    unsigned long blockCountY = (height + 3) / 4;

    for (unsigned long by = 0; by < blockCountY; by++)
        for (unsigned long bx = 0; bx < blockCountX; bx++)
            DecompressBlockBC7(bx, by, width, height, blockStorage + (by * blockCountX + bx) * 16, image);
}

