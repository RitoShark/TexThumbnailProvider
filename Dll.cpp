// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved

#include <objbase.h>
#include <shlwapi.h>
#include <thumbcache.h> // For IThumbnailProvider.
#include <shlobj.h>     // For SHChangeNotify
#include <strsafe.h>    // For StringCchPrintfW
#include <new>


extern HRESULT CTexThumbProvider_CreateInstance(REFIID riid, void **ppv);

#define SZ_CLSID_TexTHUMBHANDLER     L"{243B3EEC-8FD0-44CD-95AD-BEAFDCE52CBF}"
#define SZ_TexTHUMBHANDLER           L"Tex Thumbnail Handler"

// The shell's IThumbnailProvider handler subkey GUID.
#define SZ_THUMBNAIL_SHELLEX         L"{e357fccd-a995-4576-b01f-234630154e96}"

const CLSID CLSID_TexThumbHandler    = { 0x243b3eec, 0x8fd0, 0x44cd, { 0x95, 0xad, 0xbe, 0xaf, 0xdc, 0xe5, 0x2c, 0xbf } };

typedef HRESULT (*PFNCREATEINSTANCE)(REFIID riid, void **ppvObject);
struct CLASS_OBJECT_INIT
{
    const CLSID *pClsid;
    PFNCREATEINSTANCE pfnCreate;
};

// add classes supported by this module here
const CLASS_OBJECT_INIT c_rgClassObjectInit[] =
{
    { &CLSID_TexThumbHandler, CTexThumbProvider_CreateInstance }
};


long g_cRefModule = 0;

// Handle the the DLL's module
HINSTANCE g_hInst = NULL;

// Standard DLL functions
STDAPI_(BOOL) DllMain(HINSTANCE hInstance, DWORD dwReason, void *)
{
    if (dwReason == DLL_PROCESS_ATTACH)
    {
        g_hInst = hInstance;
        DisableThreadLibraryCalls(hInstance);
    }
    return TRUE;
}

STDAPI DllCanUnloadNow()
{
    // Only allow the DLL to be unloaded after all outstanding references have been released
    return (g_cRefModule == 0) ? S_OK : S_FALSE;
}

void DllAddRef()
{
    InterlockedIncrement(&g_cRefModule);
}

void DllRelease()
{
    InterlockedDecrement(&g_cRefModule);
}

class CClassFactory : public IClassFactory
{
public:
    static HRESULT CreateInstance(REFCLSID clsid, const CLASS_OBJECT_INIT *pClassObjectInits, size_t cClassObjectInits, REFIID riid, void **ppv)
    {
        *ppv = NULL;
        HRESULT hr = CLASS_E_CLASSNOTAVAILABLE;
        for (size_t i = 0; i < cClassObjectInits; i++)
        {
            if (clsid == *pClassObjectInits[i].pClsid)
            {
                IClassFactory *pClassFactory = new (std::nothrow) CClassFactory(pClassObjectInits[i].pfnCreate);
                hr = pClassFactory ? S_OK : E_OUTOFMEMORY;
                if (SUCCEEDED(hr))
                {
                    hr = pClassFactory->QueryInterface(riid, ppv);
                    pClassFactory->Release();
                }
                break; // match found
            }
        }
        return hr;
    }

    CClassFactory(PFNCREATEINSTANCE pfnCreate) : _cRef(1), _pfnCreate(pfnCreate)
    {
        DllAddRef();
    }

    // IUnknown
    IFACEMETHODIMP QueryInterface(REFIID riid, void ** ppv)
    {
        static const QITAB qit[] =
        {
            QITABENT(CClassFactory, IClassFactory),
            { 0 }
        };
        return QISearch(this, qit, riid, ppv);
    }

    IFACEMETHODIMP_(ULONG) AddRef()
    {
        return InterlockedIncrement(&_cRef);
    }

    IFACEMETHODIMP_(ULONG) Release()
    {
        long cRef = InterlockedDecrement(&_cRef);
        if (cRef == 0)
        {
            delete this;
        }
        return cRef;
    }

    // IClassFactory
    IFACEMETHODIMP CreateInstance(IUnknown *punkOuter, REFIID riid, void **ppv)
    {
        return punkOuter ? CLASS_E_NOAGGREGATION : _pfnCreate(riid, ppv);
    }

    IFACEMETHODIMP LockServer(BOOL fLock)
    {
        if (fLock)
        {
            DllAddRef();
        }
        else
        {
            DllRelease();
        }
        return S_OK;
    }

private:
    ~CClassFactory()
    {
        DllRelease();
    }

    long _cRef;
    PFNCREATEINSTANCE _pfnCreate;
};

STDAPI DllGetClassObject(REFCLSID clsid, REFIID riid, void **ppv)
{
    return CClassFactory::CreateInstance(clsid, c_rgClassObjectInit, ARRAYSIZE(c_rgClassObjectInit), riid, ppv);
}

// A struct to hold the information required for a registry entry

struct REGISTRY_ENTRY
{
    HKEY   hkeyRoot;
    PCWSTR pszKeyName;
    PCWSTR pszValueName;
    PCWSTR pszData;
};

// Creates a registry key (if needed) and sets the default value of the key

HRESULT CreateRegKeyAndSetValue(const REGISTRY_ENTRY *pRegistryEntry)
{
    HKEY hKey;
    HRESULT hr = HRESULT_FROM_WIN32(RegCreateKeyExW(pRegistryEntry->hkeyRoot, pRegistryEntry->pszKeyName,
                                0, NULL, REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, NULL, &hKey, NULL));
    if (SUCCEEDED(hr))
    {
        hr = HRESULT_FROM_WIN32(RegSetValueExW(hKey, pRegistryEntry->pszValueName, 0, REG_SZ,
                            (LPBYTE) pRegistryEntry->pszData,
                            ((DWORD) wcslen(pRegistryEntry->pszData) + 1) * sizeof(WCHAR)));
        RegCloseKey(hKey);
    }
    return hr;
}

// Read the default value (a ProgID string) of a key under Software\Classes.
// Tries HKCU first, then HKLM, mirroring how the shell merges them into HKCR.
// Returns true and fills pszOut on success.
static bool ReadClassesDefault(PCWSTR pszSubKey, PWSTR pszOut, DWORD cchOut)
{
    const HKEY roots[] = { HKEY_CURRENT_USER, HKEY_LOCAL_MACHINE };
    WCHAR szKey[MAX_PATH];
    if (FAILED(StringCchPrintfW(szKey, ARRAYSIZE(szKey), L"Software\\Classes\\%s", pszSubKey)))
        return false;

    for (int i = 0; i < ARRAYSIZE(roots); i++)
    {
        HKEY hKey;
        if (RegOpenKeyExW(roots[i], szKey, 0, KEY_QUERY_VALUE, &hKey) == ERROR_SUCCESS)
        {
            DWORD cb = cchOut * sizeof(WCHAR);
            DWORD type = REG_SZ;
            LONG r = RegQueryValueExW(hKey, NULL, NULL, &type, (LPBYTE)pszOut, &cb);
            RegCloseKey(hKey);
            if (r == ERROR_SUCCESS && type == REG_SZ && pszOut[0] != L'\0')
            {
                pszOut[cchOut - 1] = L'\0';
                return true;
            }
        }
    }
    return false;
}

// Point a "<class>\ShellEx\{thumbnail-guid}" subkey at our handler CLSID, in HKCU
// (HKCU\Software\Classes wins over HKLM in the merged HKCR view, so this overrides
// a thumbnail handler an app like Photoshop registered for the same class).
static HRESULT RegisterThumbnailFor(PCWSTR pszClass)
{
    WCHAR szKey[MAX_PATH];
    if (FAILED(StringCchPrintfW(szKey, ARRAYSIZE(szKey),
        L"Software\\Classes\\%s\\ShellEx\\" SZ_THUMBNAIL_SHELLEX, pszClass)))
        return E_FAIL;

    REGISTRY_ENTRY entry = { HKEY_CURRENT_USER, szKey, NULL, SZ_CLSID_TexTHUMBHANDLER };
    return CreateRegKeyAndSetValue(&entry);
}

//
// Registers this COM server
//
STDAPI DllRegisterServer()
{
    HRESULT hr;

    WCHAR szModuleName[MAX_PATH];

    if (!GetModuleFileNameW(g_hInst, szModuleName, ARRAYSIZE(szModuleName)))
    {
        hr = HRESULT_FROM_WIN32(GetLastError());
    }
    else
    {
        // List of registry entries we want to create
        const REGISTRY_ENTRY rgRegistryEntries[] =
        {
            // RootKey            KeyName                                                                ValueName                     Data
            {HKEY_CURRENT_USER,   L"Software\\Classes\\CLSID\\" SZ_CLSID_TexTHUMBHANDLER,                                 NULL,                           SZ_TexTHUMBHANDLER},
            {HKEY_CURRENT_USER,   L"Software\\Classes\\CLSID\\" SZ_CLSID_TexTHUMBHANDLER L"\\InProcServer32",             NULL,                           szModuleName},
            {HKEY_CURRENT_USER,   L"Software\\Classes\\CLSID\\" SZ_CLSID_TexTHUMBHANDLER L"\\InProcServer32",             L"ThreadingModel",              L"Apartment"},
            {HKEY_CURRENT_USER,   L"Software\\Classes\\.tex\\ShellEx\\" SZ_THUMBNAIL_SHELLEX,                             NULL,                           SZ_CLSID_TexTHUMBHANDLER},
        };

        hr = S_OK;
        for (int i = 0; i < ARRAYSIZE(rgRegistryEntries) && SUCCEEDED(hr); i++)
        {
            hr = CreateRegKeyAndSetValue(&rgRegistryEntries[i]);
        }

        // Give our handler priority over Photoshop's. The shell resolves a file's
        // thumbnail handler by precedence:  ProgID(.tex)  >  SystemFileAssociations
        // >  the bare .tex\ShellEx entry above. Photoshop wins today because it owns
        // the .tex ProgID's handler (in HKLM). By writing our CLSID under that same
        // ProgID and under SystemFileAssociations in HKCU (which overrides HKLM in
        // the merged HKCR), our handler is the one the shell picks.
        if (SUCCEEDED(hr))
        {
            WCHAR szProgID[MAX_PATH];
            if (ReadClassesDefault(L".tex", szProgID, ARRAYSIZE(szProgID)))
            {
                // Don't clobber our own ProgID if .tex already points at us.
                if (_wcsicmp(szProgID, SZ_CLSID_TexTHUMBHANDLER) != 0)
                    hr = RegisterThumbnailFor(szProgID);
            }
        }

        if (SUCCEEDED(hr))
            hr = RegisterThumbnailFor(L"SystemFileAssociations\\.tex");
    }
    if (SUCCEEDED(hr))
    {
        // This tells the shell to invalidate the thumbnail cache.  This is important because any .Tex files
        // viewed before registering this handler would otherwise show cached blank thumbnails.
        SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);
    }
    return hr;
}

//
// Unregisters this COM server
//
STDAPI DllUnregisterServer()
{
    HRESULT hr = S_OK;

    // Also tear down the priority entries we added in DllRegisterServer (ProgID +
    // SystemFileAssociations). Re-query the current .tex ProgID to find it.
    WCHAR szProgIDShellEx[MAX_PATH] = L"";
    WCHAR szProgID[MAX_PATH];
    if (ReadClassesDefault(L".tex", szProgID, ARRAYSIZE(szProgID)) &&
        _wcsicmp(szProgID, SZ_CLSID_TexTHUMBHANDLER) != 0)
    {
        StringCchPrintfW(szProgIDShellEx, ARRAYSIZE(szProgIDShellEx),
            L"Software\\Classes\\%s\\ShellEx\\" SZ_THUMBNAIL_SHELLEX, szProgID);
    }

    const PCWSTR rgpszKeys[] =
    {
        L"Software\\Classes\\CLSID\\" SZ_CLSID_TexTHUMBHANDLER,
        L"Software\\Classes\\.tex\\ShellEx\\" SZ_THUMBNAIL_SHELLEX,
        L"Software\\Classes\\SystemFileAssociations\\.tex\\ShellEx\\" SZ_THUMBNAIL_SHELLEX,
        szProgIDShellEx
    };

    // Delete the registry entries
    for (int i = 0; i < ARRAYSIZE(rgpszKeys) && SUCCEEDED(hr); i++)
    {
        if (rgpszKeys[i][0] == L'\0')
            continue; // no .tex ProgID was found

        hr = HRESULT_FROM_WIN32(RegDeleteTreeW(HKEY_CURRENT_USER, rgpszKeys[i]));
        if (hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
        {
            // If the registry entry has already been deleted, say S_OK.
            hr = S_OK;
        }
    }
    return hr;
}
