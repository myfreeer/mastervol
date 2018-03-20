#include <windows.h>
#include <initguid.h>
#include <commctrl.h>
#include <mmdeviceapi.h>
#include <endpointvolume.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#define EXIT_ON_ERROR(hr)  \
    if (FAILED(hr)) \
        { printf("error %ld occurred\n", -hr); goto Exit; }

#define COM_CALL(pointer,function,...) \
    (pointer)->lpVtbl->function((pointer), ##__VA_ARGS__)

#define SAFE_RELEASE(punk)  \
    if ((punk) != NULL)  \
        { COM_CALL((punk),Release); (punk) = NULL; }

#define HELP_TEXT \
    "Change and show current master volume level [vista+]\n" \
    "%s [-s|-h] volume\n" \
    "-s Silent mode (does not print current volume)\n" \
    "-h Display this help message and exit\n" \
    "volume: float in 0 to 100\n" \

#define PRINT_HELP(cond) \
    if (cond) {printHelp(); goto Exit;}

void printHelp () {
    //get self path
    char selfName[MAX_PATH];
    GetModuleFileName(NULL, selfName, MAX_PATH);

    //get file name from path
    const char * ptr = strrchr(selfName, '\\');
    if (ptr != NULL)
        strcpy(selfName, ptr + 1);

    printf(HELP_TEXT, selfName);
}
BOOL checkVersion(unsigned int minVersion) {
    OSVERSIONINFO VersionInfo;
    ZeroMemory(&VersionInfo, sizeof(OSVERSIONINFO));
    VersionInfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
    GetVersionEx(&VersionInfo);
    if (VersionInfo.dwMajorVersion <= minVersion)
        return FALSE;
    return TRUE;
}

int main(int argc, char const *argv[]) {
    IAudioEndpointVolume *g_pEndptVol = NULL;
    HRESULT hr = S_OK;
    IMMDeviceEnumerator *pEnumerator = NULL;
    IMMDevice *pDevice = NULL;
    BOOL silent = FALSE;

    if (!checkVersion(5))
        return -1;

    CoInitialize(NULL);

    // Get enumerator for audio endpoint devices.
    hr = CoCreateInstance(
        &CLSID_MMDeviceEnumerator,
        NULL,
        CLSCTX_ALL,
        &IID_IMMDeviceEnumerator,
        (void**)&pEnumerator
    );
    EXIT_ON_ERROR(hr)

    // Get default audio-rendering device.
    hr = COM_CALL(pEnumerator, GetDefaultAudioEndpoint, eRender, eConsole, &pDevice);
    EXIT_ON_ERROR(hr)

    hr = COM_CALL(pDevice, Activate, &IID_IAudioEndpointVolume,
                           CLSCTX_ALL, NULL, (void**)&g_pEndptVol);
    EXIT_ON_ERROR(hr)
    float currentVal, nextVal=__INT32_MAX__;
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-' || argv[i][0] == '/') {
            if (argv[i][1] == '-')
                PRINT_HELP(strcmp(argv[i], "--help") == 0)
            switch (argv[i][1]) {
                case 'h':
                case '?':
                    PRINT_HELP(TRUE)
                    break;
                case 's':
                    silent = TRUE;
                    break;
            }
        }
        else {
            nextVal = atof(argv[i]);
        }
    }
    if(nextVal != __INT32_MAX__) {
        nextVal = nextVal / 100.0;
       if (nextVal > 1.0) {
           nextVal = 1.0;
       } else if (nextVal < 0.0) {
           nextVal = 0.0;
       }
       hr = COM_CALL(g_pEndptVol,SetMasterVolumeLevelScalar,nextVal, NULL);
       EXIT_ON_ERROR(hr)
    }
    if (!silent) {
        hr = COM_CALL(g_pEndptVol,GetMasterVolumeLevelScalar,&currentVal);
        EXIT_ON_ERROR(hr)
        printf("%d\n", (int) round(100 * currentVal));// 0.839999 to 84 :)
    }

    Exit:
    fflush(stdout);
    SAFE_RELEASE(pEnumerator)
    SAFE_RELEASE(pDevice)
    SAFE_RELEASE(g_pEndptVol)
    CoUninitialize();
    return 0;
}
