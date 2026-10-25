#define COBJMACROS
#include <windows.h>
#include <d3d9.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    freopen("fnvvr-adapter.txt", "w", stdout);
    setvbuf(stdout, NULL, _IONBF, 0);
    puts("probe_started");
    char path[MAX_PATH];
    IDirect3D9 *d = Direct3DCreate9(D3D_SDK_VERSION);
    if (!d) {
        puts("Direct3DCreate9 failed");
        return 1;
    }
    GetModuleFileNameA(GetModuleHandleA("d3d9.dll"), path, sizeof(path));
    printf("module=%s\n", path);
    D3DADAPTER_IDENTIFIER9 id;
    HRESULT r = IDirect3D9_GetAdapterIdentifier(d, 0, 0, &id);
    if (FAILED(r)) {
        printf("identifier_failed=%lx\n", r);
        return 2;
    }
    DWORD part[4];
    memcpy(part, &id.DeviceIdentifier, sizeof(part));
    printf("description=%s\nvendor=%04lx\ndevice=%04lx\n",
           id.Description, id.VendorId, id.DeviceId);
    for (int i = 0; i < 4; i++)
        printf("uVideoDeviceIdentifierPart%d=%lu\n", i + 1, part[i]);
    printf("mode_count=%u\n", IDirect3D9_GetAdapterModeCount(d, 0, D3DFMT_X8R8G8B8));
    IDirect3D9_Release(d);
    return 0;
}
