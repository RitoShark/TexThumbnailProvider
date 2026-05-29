#include <shlwapi.h>
#include <thumbcache.h> // For IThumbnailProvider.
#include <wincodec.h>   // Windows Imaging Codecs
#include <msxml6.h>
#include <new>

#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "Crypt32.lib")

#include <memory> // Smart Pointers >w<
#include <vector>
#include "s3tc.h"


class CTexThumbProvider : public IInitializeWithStream,
                             public IThumbnailProvider
{
public:
    CTexThumbProvider() : _cRef(1), _pStream(NULL)
    {
    }

    virtual ~CTexThumbProvider()
    {
        if (_pStream)
        {
            _pStream->Release();
        }
    }

    // IUnknown
    IFACEMETHODIMP QueryInterface(REFIID riid, void **ppv)
    {
        static const QITAB qit[] =
        {
            QITABENT(CTexThumbProvider, IInitializeWithStream),
            QITABENT(CTexThumbProvider, IThumbnailProvider),
            { 0 },
        };
        return QISearch(this, qit, riid, ppv);
    }

    IFACEMETHODIMP_(ULONG) AddRef()
    {
        return InterlockedIncrement(&_cRef);
    }

    IFACEMETHODIMP_(ULONG) Release()
    {
        ULONG cRef = InterlockedDecrement(&_cRef);
        if (!cRef)
        {
            delete this;
        }
        return cRef;
    }

    // IInitializeWithStream
    IFACEMETHODIMP Initialize(IStream *pStream, DWORD grfMode);

    // IThumbnailProvider
    IFACEMETHODIMP GetThumbnail(UINT cx, HBITMAP *phbmp, WTS_ALPHATYPE *pdwAlpha);

private:
    long _cRef;
    IStream *_pStream;     // provided during initialization.
};

HRESULT CTexThumbProvider_CreateInstance(REFIID riid, void **ppv)
{
    CTexThumbProvider *pNew = new (std::nothrow) CTexThumbProvider();
    HRESULT hr = pNew ? S_OK : E_OUTOFMEMORY;
    if (SUCCEEDED(hr))
    {
        hr = pNew->QueryInterface(riid, ppv);
        pNew->Release();
    }
    return hr;
}

// IInitializeWithStream
IFACEMETHODIMP CTexThumbProvider::Initialize(IStream *pStream, DWORD)
{
    HRESULT hr = E_UNEXPECTED;  // can only be inited once
    if (_pStream == NULL)
    {
        // take a reference to the stream if we have not been inited yet
        hr = pStream->QueryInterface(&_pStream);
    }
    return hr;
}


enum tex_format {
    tex_format_etc1 = 0x1,
    tex_format_etc2_eac = 0x2,
    tex_format_etc2 = 0x3,
    tex_format_dxt1 = 0xA,
    tex_format_dxt5 = 0xC,
    tex_format_bgra8 = 0x14
};
#define tex_magic "TEX"

typedef struct {
    uint8_t magic[4];
    uint16_t image_width;
    uint16_t image_height;
    uint8_t unk1;
    uint8_t tex_format;
    uint8_t unk2;
    bool has_mipmaps;
} TEX_HEADER;


int get_num_mipmaps(int width, int height) {
    int num = 0;
    while (width > 1 || height > 1) {
        if (width > 1) width >>= 1;
        if (height > 1) height >>= 1;
        ++num;
    }
    return num;
}

struct MallocDeleter {
    void operator()(void* p) const { free(p); }
};

template <typename T>
using malloc_ptr = std::unique_ptr<T, MallocDeleter>;

void ComputeFitDims(UINT w, UINT h, UINT maxDim, UINT* dw, UINT* dh)
{
    if (w <= maxDim && h <= maxDim)
    {
        *dw = w;
        *dh = h;
        return;
    }

    if (w >= h)
    {
        *dw = maxDim;
        *dh = (UINT)((unsigned long long)h * maxDim / w);
    }
    else
    {
        *dh = maxDim;
        *dw = (UINT)((unsigned long long)w * maxDim / h);
    }
    if (*dw == 0) *dw = 1;
    if (*dh == 0) *dh = 1;
}

struct BoxDownsampler
{
    UINT srcW, srcH, dstW, dstH;
    std::vector<uint32_t> b, g, r, a, cnt;

    BoxDownsampler(UINT sw, UINT sh, UINT dw, UINT dh)
        : srcW(sw), srcH(sh), dstW(dw), dstH(dh),
          b((size_t)dw * dh, 0), g((size_t)dw * dh, 0), r((size_t)dw * dh, 0),
          a((size_t)dw * dh, 0), cnt((size_t)dw * dh, 0)
    {
    }

    inline void Add(UINT sx, UINT sy, uint8_t cb, uint8_t cg, uint8_t cr, uint8_t ca)
    {
        UINT dx = (UINT)((unsigned long long)sx * dstW / srcW);
        UINT dy = (UINT)((unsigned long long)sy * dstH / srcH);
        size_t idx = (size_t)dy * dstW + dx;
        b[idx] += cb; g[idx] += cg; r[idx] += cr; a[idx] += ca;
        cnt[idx]++;
    }

    void WriteArgb(void* bits) const
    {
        unsigned long* px = (unsigned long*)bits;
        size_t n = (size_t)dstW * dstH;
        for (size_t i = 0; i < n; ++i)
        {
            uint32_t c = cnt[i] ? cnt[i] : 1;
            px[i] = ((unsigned long)(a[i] / c) << 24) |
                    ((unsigned long)(r[i] / c) << 16) |
                    ((unsigned long)(g[i] / c) << 8) |
                    ((unsigned long)(b[i] / c));
        }
    }
};

HRESULT CreateHBitmapFromTex(IStream* pTexStream, HBITMAP* phbmp, WTS_ALPHATYPE* pdwAlpha)
{
    *phbmp = NULL;
    *pdwAlpha = WTSAT_UNKNOWN;

    TEX_HEADER header;
    ULONG bytesRead;

    HRESULT hr = pTexStream->Read(&header, sizeof(header), &bytesRead);
    if (FAILED(hr) || bytesRead != sizeof(header))
        return E_FAIL;

    if (memcmp(header.magic, "TEX", 3) != 0)
        return E_INVALIDARG;

    UINT width = header.image_width;
    UINT height = header.image_height;

    if (width == 0 || height == 0)
        return E_INVALIDARG;

    if (header.tex_format == tex_format_bgra8)
    {
        UINT stride = width * 4;
        const UINT bytesPerBlock  = 4;

        if (header.has_mipmaps) {

            unsigned int mipMapCount = get_num_mipmaps(header.image_width, header.image_height);

            UINT skip = 0;
            // block_size = 1 for bgra8
            // Note: don't need to peform the weird calcs bcs bgra8 is not compressed UwU
            for (auto x = mipMapCount; x > 0; x--) {
                auto blockWidth = max(width / (1 << x), 1);
                auto blockHeight = max(height / (1 << x), 1);
                skip += bytesPerBlock  * blockWidth * blockHeight;
            }

            LARGE_INTEGER liSkip;
            liSkip.QuadPart = skip;

            hr = pTexStream->Seek(liSkip, STREAM_SEEK_CUR, NULL);
            if (FAILED(hr)) {
                return hr;
            }
        }

        UINT dstW, dstH;
        ComputeFitDims(width, height, 1024, &dstW, &dstH);

        BITMAPINFO bmi = {};
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = dstW;
        bmi.bmiHeader.biHeight = -((LONG)dstH);
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;

        void* bits;
        HBITMAP hBitmap = CreateDIBSection(NULL, &bmi, DIB_RGB_COLORS, &bits, NULL, 0);
        if (!hBitmap)
            return E_OUTOFMEMORY;

        if (dstW == width && dstH == height)
        {
            const UINT imageSize = stride * height;
            hr = pTexStream->Read(bits, imageSize, &bytesRead);
            if (FAILED(hr) || bytesRead != imageSize)
            {
                DeleteObject(hBitmap);
                return E_FAIL;
            }
        }
        else
        {
            BoxDownsampler ds(width, height, dstW, dstH);
            std::vector<uint8_t> row((size_t)width * 4);
            for (UINT y = 0; y < height; ++y)
            {
                hr = pTexStream->Read(row.data(), (ULONG)row.size(), &bytesRead);
                if (FAILED(hr) || bytesRead != row.size())
                {
                    DeleteObject(hBitmap);
                    return E_FAIL;
                }
                const uint8_t* p = row.data();
                for (UINT x = 0; x < width; ++x, p += 4)
                    ds.Add(x, y, p[0], p[1], p[2], p[3]);
            }
            ds.WriteArgb(bits);
        }

        *phbmp = hBitmap;
        *pdwAlpha = WTSAT_ARGB;

        return S_OK;
    }
    else if (header.tex_format == tex_format_dxt5 || header.tex_format == tex_format_dxt1)
    {
        const UINT bytesPerBlock  = header.tex_format == tex_format_dxt5 ? 16 : 8;
        UINT dataSize;

        if (header.has_mipmaps) {
            
            unsigned int mipMapCount = get_num_mipmaps(header.image_width, header.image_height);
            
            UINT skip = 0;
            // block_size = 4 for dxt5 and dxt1
            // Note (+ block_size - 1) simplified to +3 bcs 4 - 1
            for (auto x = mipMapCount; x > 0; x--) {
                auto curr_width = max(width / (1 << x), 1);
                auto curr_height = max(height / (1 << x), 1);

                auto blockWidth = (curr_width + 3) / 4;
                auto blockHeight = (curr_height + 3) / 4;
                skip += bytesPerBlock  * blockWidth * blockHeight;
            }

            LARGE_INTEGER liSkip;
            liSkip.QuadPart = skip;

            hr = pTexStream->Seek(liSkip, STREAM_SEEK_CUR, NULL);
            if (FAILED(hr)) {
                return hr;
            }
        }

        const UINT blockWidth = (width + 3) / 4;
        const UINT blockHeight = (height + 3) / 4;
        dataSize = blockWidth * blockHeight * bytesPerBlock ;

        malloc_ptr<uint8_t> dxtData((uint8_t *) malloc(dataSize));
        if (!dxtData)
            return E_OUTOFMEMORY;

        hr = pTexStream->Read(dxtData.get(), dataSize, &bytesRead);
        if (FAILED(hr) || bytesRead != dataSize)
        {
            return E_FAIL;
        }

        UINT dstW, dstH;
        ComputeFitDims(width, height, 1024, &dstW, &dstH);

        BITMAPINFO bmi = {};
        bmi.bmiHeader.biSize = sizeof(bmi.bmiHeader);
        bmi.bmiHeader.biWidth = dstW;
        bmi.bmiHeader.biHeight = -((LONG)dstH); // top-down
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;

        void* bits;
        HBITMAP hBitmap = CreateDIBSection(NULL, &bmi, DIB_RGB_COLORS, &bits, NULL, 0);
        if (!hBitmap)
        {
            return E_OUTOFMEMORY;
        }

        if (dstW == width && dstH == height)
        {
            unsigned long* pixels = (unsigned long*) bits;

            if (header.tex_format == tex_format_dxt5) {
                BlockDecompressImageDXT5(width, height, dxtData.get(), pixels);
            }
            else {
                BlockDecompressImageDXT1(width, height, dxtData.get(), pixels);
            }

            // RGBA -> ARGB
            for (size_t i = 0; i < width * height; ++i)
            {
                unsigned long rgba = pixels[i];
                unsigned char r = (rgba >> 24) & 0xFF;
                unsigned char g = (rgba >> 16) & 0xFF;
                unsigned char b = (rgba >> 8) & 0xFF;
                unsigned char a = rgba & 0xFF;

                pixels[i] = (
                    (unsigned long)a << 24) |
                    ((unsigned long)r << 16) |
                    ((unsigned long)g << 8) |
                    ((unsigned long)b);
            }
        }
        else
        {
            BoxDownsampler ds(width, height, dstW, dstH);
            unsigned long block[16];
            for (UINT by = 0; by < blockHeight; ++by)
            {
                for (UINT bx = 0; bx < blockWidth; ++bx)
                {
                    const uint8_t* src = dxtData.get() + ((size_t)by * blockWidth + bx) * bytesPerBlock;
                    if (header.tex_format == tex_format_dxt5)
                        DecompressBlockDXT5(0, 0, 4, src, block);
                    else
                        DecompressBlockDXT1(0, 0, 4, src, block);

                    for (UINT j = 0; j < 4; ++j)
                    {
                        UINT sy = by * 4 + j;
                        if (sy >= height) continue;
                        for (UINT i = 0; i < 4; ++i)
                        {
                            UINT sx = bx * 4 + i;
                            if (sx >= width) continue;
                            unsigned long rgba = block[j * 4 + i];
                            ds.Add(sx, sy,
                                   (uint8_t)((rgba >> 8) & 0xFF),
                                   (uint8_t)((rgba >> 16) & 0xFF),
                                   (uint8_t)((rgba >> 24) & 0xFF),
                                   (uint8_t)(rgba & 0xFF));
                        }
                    }
                }
            }
            ds.WriteArgb(bits);
        }

        *phbmp = hBitmap;
        *pdwAlpha = WTSAT_ARGB;

        return S_OK;
    }

    return E_NOTIMPL;
}


// IThumbnailProvider
IFACEMETHODIMP CTexThumbProvider::GetThumbnail(UINT /* cx */, HBITMAP* phbmp, WTS_ALPHATYPE* pdwAlpha)
{
    HRESULT hr = CreateHBitmapFromTex(this->_pStream, phbmp, pdwAlpha);

    return hr;
}
