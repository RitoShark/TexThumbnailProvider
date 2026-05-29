#include <shlwapi.h>
#include <thumbcache.h> // For IThumbnailProvider.
#include <wincodec.h>   // Windows Imaging Codecs
#include <msxml6.h>
#include <new>

#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "Crypt32.lib")

#include <memory> // Smart Pointers >w<
#include "s3tc.h"


#if defined(_MSC_VER)
#include <intrin.h> // _BitScanReverse64
static inline unsigned int LEADING_ZEROS(unsigned int x) {
    unsigned long idx;
    if (_BitScanReverse64(&idx, x))
        return 31 - idx;
    else
        return 32;
}
#endif

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


void swapUInt8(uint8_t* a, uint8_t* b) {
    uint8_t temp = *a;
    *a = *b;
    *b = temp;
}

int get_num_mipmaps(int width, int height) {
    int num = 0;
    while (width > 1 || height > 1) {
        if (width > 1) width >>= 1;
        if (height > 1) height >>= 1;
        ++num;
    }
    return num;
}

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

        const UINT imageSize = stride * height;

        BYTE* pixelData = (BYTE*)malloc(imageSize);
        if (!pixelData)
            return E_OUTOFMEMORY;

        hr = pTexStream->Read(pixelData, imageSize, &bytesRead);
        if (FAILED(hr) || bytesRead != imageSize)
        {
            free(pixelData);
            return E_FAIL;
        }

        BITMAPINFO bmi = {};
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = width;
        bmi.bmiHeader.biHeight = -((LONG)height);
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;

        void* bits;
        HBITMAP hBitmap = CreateDIBSection(NULL, &bmi, DIB_RGB_COLORS, &bits, NULL, 0);
        if (!hBitmap)
        {
            free(pixelData);
            return E_OUTOFMEMORY;
        }

        memcpy(bits, pixelData, imageSize);
        *phbmp = hBitmap;
        *pdwAlpha = WTSAT_ARGB;

        free(pixelData);
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

        UINT stride = width * 4;

        auto dxtData = std::make_unique<uint8_t*>((uint8_t *) malloc(dataSize));
        if (!dxtData)
            return E_OUTOFMEMORY;

        hr = pTexStream->Read(*dxtData, dataSize, &bytesRead);
        if (FAILED(hr) || bytesRead != dataSize)
        {
            return E_FAIL;
        }

        auto imageData = std::make_unique<unsigned long*>((unsigned long*) malloc(stride * height));
        if (!imageData)
        {
            return E_OUTOFMEMORY;
        }


        if (header.tex_format == tex_format_dxt5) {
            BlockDecompressImageDXT5(width, height, *dxtData, *imageData);
        }
        else {
            BlockDecompressImageDXT1(width, height, *dxtData, *imageData);
        }

        BITMAPINFO bmi = {};
        bmi.bmiHeader.biSize = sizeof(bmi.bmiHeader);
        bmi.bmiHeader.biWidth = width;
        bmi.bmiHeader.biHeight = -((LONG)height); // top-down
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;

        void* bits;
        
        auto finalImage = std::make_unique<unsigned long*>((unsigned long*) malloc(stride * height));
        if (!finalImage)
        {
            return E_OUTOFMEMORY;
        }
        
        // RGBA -> ARGB
        for (size_t i = 0; i < width * height; ++i)
        {
            unsigned long rgba = (*imageData)[i];
            unsigned char r = (rgba >> 24) & 0xFF;
            unsigned char g = (rgba >> 16) & 0xFF;
            unsigned char b = (rgba >> 8) & 0xFF;
            unsigned char a = rgba & 0xFF;

            (*finalImage)[i] = (
                (unsigned long)a << 24) |
                ((unsigned long)r << 16) |
                ((unsigned long)g << 8) |
                ((unsigned long)b);
        }
        
        imageData = std::move(finalImage);

        HBITMAP hBitmap = CreateDIBSection(NULL, &bmi, DIB_RGB_COLORS, &bits, NULL, 0);
        if (!hBitmap)
        {
            return E_OUTOFMEMORY;
        }

        memcpy(bits, *imageData, stride * height);
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
