#include <windows.h>
#include <initguid.h>
#include <commctrl.h>
#include <mmdeviceapi.h>
#include <endpointvolume.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#define EXIT_ON_ERROR(hr, description)                                         \
    if (FAILED(hr)) {                                                          \
        errorCode = -hr;                                                       \
        printf("%s failed: error %d occurred\n", #description, errorCode);     \
        goto Exit;                                                             \
    }

#define COM_CALL(hr, pointer, function, ...)                                   \
    hr = (pointer)->lpVtbl->function((pointer), ##__VA_ARGS__);                \
    EXIT_ON_ERROR(hr, function)

#define SAFE_RELEASE(punk)                                                     \
    if ((punk) != NULL) {                                                      \
        (punk)->lpVtbl->Release(punk);                                         \
        (punk) = NULL;                                                         \
    }

#define HELP_TEXT                                                              \
    "Change and show current master volume level\n"                            \
    "%s [-s|-h|-d|-m|-u] volume\n"                                             \
    "-s Silent mode (does not print current status)\n"                         \
    "-h Display this help message and exit\n"                                  \
    "-m Mute [vista+ only]\n"                                                  \
    "-u Unmute [vista+ only]\n"                                                \
    "-d Display mute status (ignored if -s) [vista+ only]\n"                   \
    "volume: float in 0 to 100\n"

#define PRINT_HELP                                                             \
    {                                                                          \
        printHelp();                                                           \
        goto Exit;                                                             \
    }

void printHelp() {
    // get self path
    char selfName[MAX_PATH];
    GetModuleFileName(NULL, selfName, MAX_PATH);

    // get file name from path
    const char *ptr = strrchr(selfName, '\\');
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

MMRESULT setMasterAudioWaveOut(float vol) {
    DWORD volume = (DWORD)round(vol * 0xffff / 100);
    if (volume > 0xffff)
        volume = 0xffff;
    return waveOutSetVolume(NULL, (volume << 16) | volume);
}
int getMasterAudioWaveOut() {
    DWORD volume = 0;
    waveOutGetVolume(NULL, &volume);
    int vol =
        (int)round(100 * ((volume >> 16) + (volume & 0xffff)) / (2 * 0xffff));
    return vol;
}

int __cdecl mainCRTStartup() {
    IAudioEndpointVolume *g_pEndptVol = NULL;
    HRESULT hr = S_OK;
    IMMDeviceEnumerator *pEnumerator = NULL;
    IMMDevice *pDevice = NULL;
    BOOL silent = FALSE;
    int errorCode = 0;
    float currentVal, nextVal = __INT32_MAX__;
    BOOL setMute = FALSE;
    BOOL showMute = FALSE;
    BOOL mute = FALSE;
    int argc;
    wchar_t **argv = CommandLineToArgvW(GetCommandLineW(), &argc);

    // arguments parsing
    for (int i = 1; i < argc; i++) {
        int argLen = wcslen(argv[i]);
        if (argv[i][0] == '-' || argv[i][0] == '/') {
            for (int j = 1; j < argLen; j++) {
                switch (argv[i][j]) {
                case '-':
                    if (argLen == 6 && _wcsicmp(argv[i], L"--help") == 0) {
                        PRINT_HELP
                    }
                    break;
                case 'h':
                case '?':
                    PRINT_HELP
                    break;
                case 's':
                    silent = TRUE;
                    break;
                case 'm':
                    setMute = TRUE;
                    mute = TRUE;
                    showMute = TRUE;
                    break;
                case 'd':
                    showMute = TRUE;
                    break;
                case 'u':
                    setMute = TRUE;
                    mute = FALSE;
                    showMute = TRUE;
                    break;
                }
            }
        } else {
            nextVal = _wtof(argv[i]);
        }
    }
    if (!checkVersion(5)) {
        if (nextVal != __INT32_MAX__)
            setMasterAudioWaveOut(nextVal);
        if (!silent)
            printf("%d\n", getMasterAudioWaveOut());
        goto Exit;
    }
    CoInitialize(NULL);

    // Get enumerator for audio endpoint devices.
    hr = CoCreateInstance(&CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL,
                          &IID_IMMDeviceEnumerator, (void **)&pEnumerator);
    EXIT_ON_ERROR(hr, CoCreateInstance)

    // Get default audio-rendering device.
    COM_CALL(hr, pEnumerator, GetDefaultAudioEndpoint, eRender, eConsole,
             &pDevice);

    COM_CALL(hr, pDevice, Activate, &IID_IAudioEndpointVolume, CLSCTX_ALL, NULL,
             (void **)&g_pEndptVol);

    if (nextVal != __INT32_MAX__) {
        nextVal = nextVal / 100.0;
        if (nextVal > 1.0) {
            nextVal = 1.0;
        } else if (nextVal < 0.0) {
            nextVal = 0.0;
        }
        COM_CALL(hr, g_pEndptVol, SetMasterVolumeLevelScalar, nextVal, NULL);
    }
    if (setMute) {
        COM_CALL(hr, g_pEndptVol, SetMute, mute, NULL);
    }
    if (!silent) {
        COM_CALL(hr, g_pEndptVol, GetMasterVolumeLevelScalar, &currentVal);
        printf("%d\n", (int)round(100 * currentVal)); // 0.839999 to 84 :)
        if (showMute) {
            COM_CALL(hr, g_pEndptVol, GetMute, &mute);
            if (mute) {
                printf("Muted\n");
            } else {
                printf("Not Muted\n");
            }
        }
    }

Exit:
    fflush(stdout);
    SAFE_RELEASE(pEnumerator)
    SAFE_RELEASE(pDevice)
    SAFE_RELEASE(g_pEndptVol)
    CoUninitialize();
    LocalFree(argv);
    ExitProcess(errorCode);
    return errorCode;
}
