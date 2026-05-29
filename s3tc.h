#ifndef S3TC_H
#define S3TC_H

unsigned long PackRGBA(unsigned char r, unsigned char g, unsigned char b, unsigned char a);
void DecompressBlockDXT1(unsigned long x, unsigned long y, unsigned long width, const unsigned char* blockStorage, unsigned long* image);
void BlockDecompressImageDXT1(unsigned long width, unsigned long height, const unsigned char* blockStorage, unsigned long* image);
void DecompressBlockDXT5(unsigned long x, unsigned long y, unsigned long width, const unsigned char* blockStorage, unsigned long* image);
void BlockDecompressImageDXT5(unsigned long width, unsigned long height, const unsigned char* blockStorage, unsigned long* image);

// BC5 (two-channel, tex_format 0xE) — used by Riot for normal maps. The two BC4
// red/green channels are decoded and the blue channel is reconstructed as the
// normal's Z (b = sqrt(1 - x^2 - y^2)) so the thumbnail reads as a normal map.
void BlockDecompressImageBC5(unsigned long width, unsigned long height, const unsigned char* blockStorage, unsigned long* image);

// BC7 (tex_format 0xD) — full LDR BC7 software decode (all 8 modes). Tables and
// decode flow match the BC7 spec / DirectXTex reference.
void BlockDecompressImageBC7(unsigned long width, unsigned long height, const unsigned char* blockStorage, unsigned long* image);

#endif // S3TC_H
