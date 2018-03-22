#include <windows.h>
#include <initguid.h>
#include <commctrl.h>
#include <mmdeviceapi.h>
#include <endpointvolume.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#define EXIT_ON_ERROR(hr, description)  \
    if (FAILED(hr)) \
        { errorCode = -hr; printf("%s failed: error %d occurred\n", #description, errorCode); goto Exit; }

#define COM_CALL(pointer,function,...) \
    (pointer)->lpVtbl->function((pointer), ##__VA_ARGS__)

#define SAFE_RELEASE(punk)  \
    if ((punk) != NULL)  \
        { COM_CALL((punk),Release); (punk) = NULL; }

#define HELP_TEXT \
    "Change and show current master volume level\n" \
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

MMRESULT setMasterAudioWaveOut(float vol) {
	DWORD volume = (DWORD) round(vol * 0xffff / 100);
	if (volume > 0xffff) volume = 0xffff;
	return waveOutSetVolume(NULL, (volume << 16) | volume);
}
int getMasterAudioWaveOut() {
	DWORD volume = 0;
	waveOutGetVolume(NULL, &volume);
	int vol = (int) round(100 * ((volume>>16) + (volume & 0xffff)) / (2*0xffff));
	return vol;
}
int main(int argc, char const *argv[]) {
    IAudioEndpointVolume *g_pEndptVol = NULL;
    HRESULT hr = S_OK;
    IMMDeviceEnumerator *pEnumerator = NULL;
    IMMDevice *pDevice = NULL;
    BOOL silent = FALSE;
    int errorCode = 0;
    float currentVal, nextVal=__INT32_MAX__;


   // arguments parsing
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
    if (!checkVersion(5)) {
        if (nextVal != __INT32_MAX__)
            setMasterAudioWaveOut(nextVal);
        if (!silent)
            printf("%d\n", getMasterAudioWaveOut());
        goto Exit;
    }
    CoInitialize(NULL);

    // Get enumerator for audio endpoint devices.
    hr = CoCreateInstance(
        &CLSID_MMDeviceEnumerator,
        NULL,
        CLSCTX_ALL,
        &IID_IMMDeviceEnumerator,
        (void**)&pEnumerator
    );
    EXIT_ON_ERROR(hr, CoCreateInstance)

    // Get default audio-rendering device.
    hr = COM_CALL(pEnumerator, GetDefaultAudioEndpoint, eRender, eConsole, &pDevice);
    EXIT_ON_ERROR(hr, GetDefaultAudioEndpoint)

    hr = COM_CALL(pDevice, Activate, &IID_IAudioEndpointVolume,
                           CLSCTX_ALL, NULL, (void**)&g_pEndptVol);
    EXIT_ON_ERROR(hr, IMMDevice Activate)

    if (nextVal != __INT32_MAX__) {
        nextVal = nextVal / 100.0;
       if (nextVal > 1.0) {
           nextVal = 1.0;
       } else if (nextVal < 0.0) {
           nextVal = 0.0;
       }
       hr = COM_CALL(g_pEndptVol,SetMasterVolumeLevelScalar,nextVal, NULL);
       EXIT_ON_ERROR(hr, SetMasterVolumeLevelScalar)
    }
    if (!silent) {
        hr = COM_CALL(g_pEndptVol,GetMasterVolumeLevelScalar,&currentVal);
        EXIT_ON_ERROR(hr, GetMasterVolumeLevelScalar)
        printf("%d\n", (int) round(100 * currentVal));// 0.839999 to 84 :)
    }

    Exit:
    fflush(stdout);
    SAFE_RELEASE(pEnumerator)
    SAFE_RELEASE(pDevice)
    SAFE_RELEASE(g_pEndptVol)
    CoUninitialize();
    return errorCode;
}
