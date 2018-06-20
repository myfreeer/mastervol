#include <windows.h>
#include <initguid.h>
#include <commctrl.h>
#include <mmdeviceapi.h>
#include <endpointvolume.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#define FLAG(n) (1U << n##U)
#define MASTERVOL_SILENT FLAG(0)
#define MASTERVOL_SET_VOL FLAG(1)
#define MASTERVOL_GET_VOL FLAG(2)
#define MASTERVOL_SET_MUTE FLAG(3)
#define MASTERVOL_GET_MUTE FLAG(4)
#define MASTERVOL_WAVEOUT FLAG(5)

#define EXIT_ON_ERROR(hr, description)                                         \
    if (FAILED(hr)) {                                                          \
        errorCode = -(hr);                                                     \
        printf("%s failed: error %d occurred\n", #description, errorCode);     \
        goto ExitEndpoint;                                                     \
    }

#define EXIT_ON_MM_ERROR(mmResult, description)                                \
    if (MMSYSERR_NOERROR != (mmResult)) {                                      \
        errorCode = (mmResult);                                                \
        printf("%s failed: error %d occurred\n", #description, errorCode);     \
        goto ExitMixer;                                                        \
    }

#define MIXER_INIT(mixerStruct)                                                \
    ZeroMemory(&(mixerStruct), sizeof(mixerStruct));                           \
    (mixerStruct).cbStruct = sizeof(mixerStruct);

#define MIXER_LI_INIT(mixerLineControls, mixerControl, mixerLine)              \
    MIXER_INIT(mixerLineControls)                                              \
    (mixerLineControls).cControls = 1;                                         \
    (mixerLineControls).cbmxctrl = sizeof(mixerControl);                       \
    (mixerLineControls).pamxctrl = &(mixerControl);                            \
    (mixerLineControls).dwLineID = (mixerLine).dwLineID;

#define MIXER_DT_INIT(mixerControlDetails, mixerControl, paStruct)             \
    MIXER_INIT(mixerControlDetails)                                            \
    (mixerControlDetails).cbDetails = sizeof(paStruct);                        \
    (mixerControlDetails).dwControlID = (mixerControl).dwControlID;            \
    (mixerControlDetails).paDetails = &(paStruct);                             \
    (mixerControlDetails).cChannels = 1;

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
    "%s [-s|-h|-d|-m|-u|-w] volume\n"                                          \
    "-s Silent mode (does not print current status)\n"                         \
    "-h Display this help message and exit\n"                                  \
    "-m Mute\n"                                                                \
    "-u Unmute\n"                                                              \
    "-d Display mute status (ignored if -s)\n"                                 \
    "-w Use wave out api on win xp\n"                                          \
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

int masterVolEndpoint(unsigned int flags, float *volume, BOOL *mute) {
    int errorCode = 0;
    IAudioEndpointVolume *pEndptVol = NULL;
    HRESULT hResult = S_OK;
    IMMDeviceEnumerator *pEnumerator = NULL;
    IMMDevice *pDevice = NULL;

    CoInitialize(NULL);

    // Get enumerator for audio endpoint devices.
    hResult = CoCreateInstance(&CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL,
                               &IID_IMMDeviceEnumerator, (void **)&pEnumerator);
    EXIT_ON_ERROR(hResult, CoCreateInstance)

    // Get default audio-rendering device.
    COM_CALL(hResult, pEnumerator, GetDefaultAudioEndpoint, eRender, eConsole,
             &pDevice);

    COM_CALL(hResult, pDevice, Activate, &IID_IAudioEndpointVolume, CLSCTX_ALL,
             NULL, (void **)&pEndptVol);

    if (flags & MASTERVOL_SET_VOL) {
        COM_CALL(hResult, pEndptVol, SetMasterVolumeLevelScalar, *volume,
                 NULL);
    }
    if (flags & MASTERVOL_SET_MUTE) {
        COM_CALL(hResult, pEndptVol, SetMute, *mute, NULL);
    }
    if (flags & MASTERVOL_GET_VOL) {
        COM_CALL(hResult, pEndptVol, GetMasterVolumeLevelScalar, volume);
    }
    if (flags & MASTERVOL_GET_MUTE) {
        COM_CALL(hResult, pEndptVol, GetMute, mute);
    }

ExitEndpoint:
    SAFE_RELEASE(pEnumerator)
    SAFE_RELEASE(pDevice)
    SAFE_RELEASE(pEndptVol)
    CoUninitialize();
    return errorCode;
}

int masterVolMixer(unsigned int flags, float *volume, BOOL *mute) {
    int errorCode = 0;
    MMRESULT mmResult = MMSYSERR_NOERROR;
    HMIXER hMixer;
    MIXERCONTROL mixerControl;
    MIXERLINE mixerLine;
    MIXERLINECONTROLS mixerLineControls;
    MIXERCONTROLDETAILS mixerControlDetails;
    MIXERCONTROLDETAILS_UNSIGNED volStruct;
    MIXERCONTROLDETAILS_BOOLEAN muteStruct;

    // open default mixer
    mmResult = mixerOpen(&hMixer, 0, 0, 0, 0);
    EXIT_ON_MM_ERROR(mmResult, mixerOpen)

    // get mixer line info
    MIXER_INIT(mixerLine)
    // speakers
    mixerLine.dwComponentType = MIXERLINE_COMPONENTTYPE_DST_SPEAKERS;
    mmResult = mixerGetLineInfo((HMIXEROBJ)hMixer, &mixerLine,
                                MIXER_GETLINEINFOF_COMPONENTTYPE);
    EXIT_ON_MM_ERROR(mmResult, mixerGetLineInfo)

    // Mixer Line Controls Volume
    MIXER_INIT(mixerControl)
    MIXER_LI_INIT(mixerLineControls, mixerControl, mixerLine)
    mixerLineControls.dwControlType = MIXERCONTROL_CONTROLTYPE_VOLUME;
    mmResult = mixerGetLineControls((HMIXEROBJ)hMixer, &mixerLineControls,
                                    MIXER_GETLINECONTROLSF_ONEBYTYPE);
    EXIT_ON_MM_ERROR(mmResult, mixerGetLineControls)

    // volume mixer control details
    MIXER_DT_INIT(mixerControlDetails, mixerControl, volStruct)

    const DWORD maxVol = mixerControl.Bounds.lMaximum;
    const DWORD minVol = mixerControl.Bounds.lMinimum;
    if (flags & MASTERVOL_SET_VOL) {
        volStruct.dwValue = ((maxVol - minVol) * (*volume)) + minVol;
        mmResult = mixerSetControlDetails((HMIXEROBJ)hMixer, &mixerControlDetails,
                                   MIXER_SETCONTROLDETAILSF_VALUE);
        EXIT_ON_MM_ERROR(mmResult, mixerSetControlDetails)
    }
    // get master volume
    if (flags & MASTERVOL_GET_VOL) {
        mmResult = mixerGetControlDetails((HMIXEROBJ)hMixer, &mixerControlDetails,
                                          MIXER_GETCONTROLDETAILSF_VALUE);
        EXIT_ON_MM_ERROR(mmResult, mixerGetControlDetails)
        *volume = ((double)volStruct.dwValue + minVol) / (maxVol - minVol);
    }

    if (flags & (MASTERVOL_SET_MUTE | MASTERVOL_GET_MUTE)) {
        // Mixer Line Controls Mute
        mixerLineControls.dwControlType = MIXERCONTROL_CONTROLTYPE_MUTE;
        mmResult = mixerGetLineControls((HMIXEROBJ)hMixer, &mixerLineControls,
                                        MIXER_GETLINECONTROLSF_ONEBYTYPE);
        EXIT_ON_MM_ERROR(mmResult, mixerGetLineControls)

        // mute mixer control details
        MIXER_DT_INIT(mixerControlDetails, mixerControl, muteStruct)
    }
    if (flags & MASTERVOL_SET_MUTE) {
        // change mute status
        muteStruct.fValue = *mute;
        mmResult =
            mixerSetControlDetails((HMIXEROBJ)hMixer, &mixerControlDetails,
                                   MIXER_SETCONTROLDETAILSF_VALUE);
        EXIT_ON_MM_ERROR(mmResult, mixerSetControlDetails)
    }
    if (flags & MASTERVOL_GET_MUTE) {
        // get mute status
        mmResult =
            mixerGetControlDetails((HMIXEROBJ)hMixer, &mixerControlDetails,
                                    MIXER_GETCONTROLDETAILSF_VALUE);
        EXIT_ON_MM_ERROR(mmResult, mixerGetControlDetails)
        *mute = muteStruct.fValue;
    }
ExitMixer:
    mixerClose(hMixer);
    return errorCode;
}

MMRESULT setVolumeWaveOut(float vol) {
    DWORD volume = (DWORD)lroundf(vol * 0xffff);
    if (volume > 0xffff)
        volume = 0xffff;
    return waveOutSetVolume(NULL, (volume << 16) | volume);
}

int getVolumeWaveOut() {
    DWORD volume = 0;
    waveOutGetVolume(NULL, &volume);
    int vol =
        (int)lroundf(100 * ((volume >> 16) + (volume & 0xffff)) / (2 * 0xffff));
    return vol;
}

int __cdecl mainCRTStartup() {
    int errorCode = 0;
    float volume = 0;
    unsigned int flags = MASTERVOL_GET_VOL;
    BOOL mute = FALSE;
    int argc;
    wchar_t **argv = CommandLineToArgvW(GetCommandLineW(), &argc);

    // arguments parsing
    for (int i = 1; i < argc; i++) {
        size_t argLen = wcslen(argv[i]);
        if (argv[i][0] == '-' || argv[i][0] == '/') {
            for (size_t j = 1; j < argLen; j++) {
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
                    flags |= MASTERVOL_SILENT;
                    break;
                case 'm':
                    mute = TRUE;
                    flags |= MASTERVOL_SET_MUTE;
                    flags |= MASTERVOL_GET_MUTE;
                    break;
                case 'd':
                    flags |= MASTERVOL_GET_MUTE;
                    break;
                case 'w':
                    flags |= MASTERVOL_WAVEOUT;
                    break;
                case 'u':
                    mute = FALSE;
                    flags |= MASTERVOL_SET_MUTE;
                    flags |= MASTERVOL_GET_MUTE;
                    break;
                }
            }
        } else {
            volume = _wtof(argv[i]);
            flags |= MASTERVOL_SET_VOL;
        }
    }
    if (flags & MASTERVOL_SET_VOL) {
        volume = volume / 100.0;
        if (volume > 1.0) {
            volume = 1.0;
        } else if (volume < 0.0) {
            volume = 0.0;
        }
    }
    if (flags & MASTERVOL_SILENT) {
        flags &= ~(MASTERVOL_GET_VOL|MASTERVOL_GET_MUTE);
    }
    if (!checkVersion(5)) {
        if (flags & MASTERVOL_WAVEOUT) {
            if (flags & MASTERVOL_SET_VOL) {
                setVolumeWaveOut(volume);
            }
            if (!(flags & MASTERVOL_SILENT)) {
                printf("Wave Out Vol: %d\n", getVolumeWaveOut());
            }
            flags &= ~MASTERVOL_SET_VOL;
            errorCode = masterVolMixer(flags, &volume, &mute);
        } else {
            errorCode = masterVolMixer(flags, &volume, &mute);
        }
    } else {
        errorCode = masterVolEndpoint(flags, &volume, &mute);
    }
    if (!(flags & MASTERVOL_SILENT)) {
        printf("%d\n", (int) lroundf(volume * 100));
        if (flags & MASTERVOL_GET_MUTE) {
            printf(mute ? "Muted\n" : "Not Muted\n");
        }
    }

Exit:
    fflush(stdout);
    LocalFree(argv);
    ExitProcess(errorCode);
    return errorCode;
}
