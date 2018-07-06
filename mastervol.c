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
#define MASTERVOL_IN FLAG(5)
#define MASTERVOL_WAVEOUT FLAG(6)
#define MASTERVOL_CD FLAG(7)
#define MASTERVOL_MIDI FLAG(8)
#define MASTERVOL_LINE FLAG(9)
#define MASTERVOL_MUTE ( MASTERVOL_SET_MUTE | MASTERVOL_GET_MUTE )
#define MASTERVOL_SWITCHES ( MASTERVOL_IN | MASTERVOL_WAVEOUT |                \
    MASTERVOL_CD | MASTERVOL_MIDI | MASTERVOL_LINE )

#define EXIT_ON_MM_ERROR(mmResult, description)                                \
    if (MMSYSERR_NOERROR != (mmResult)) {                                      \
        errorCode = (mmResult);                                                \
        printf("%s failed: error %d occurred\n", #description, errorCode);     \
        goto ExitMixer;                                                        \
    }

#define MIXER_INIT(mixerStruct) mixerStruct = {.cbStruct = sizeof(mixerStruct)}

#define MIXER_LI_INIT(mixerLineControls, mixerControl, mixerLine)              \
    mixerLineControls = {.cbStruct = sizeof(mixerLineControls),                \
                         .cControls = 1,                                       \
                         .cbmxctrl = sizeof(mixerControl),                     \
                         .pamxctrl = &(mixerControl),                          \
                         .dwLineID = (mixerLine).dwLineID}

#define MIXER_DT_INIT(mixerControlDetails, mixerControl, paStruct)             \
    mixerControlDetails = {                                                    \
        .cbStruct = sizeof(mixerControlDetails),                               \
        .cbDetails = sizeof(paStruct),                                         \
        .dwControlID = (mixerControl).dwControlID,                             \
        .paDetails = &(paStruct),                                              \
        .cChannels = 1,                                                        \
    }

#define COM_CALL(hr, pointer, function, ...)                                   \
    hr = (pointer)->lpVtbl->function((pointer), ##__VA_ARGS__);                \
    EXIT_ON_ERROR(hr, function)

#define EXIT_ON_ERROR(hr, description)                                         \
    if (FAILED(hr)) {                                                          \
        errorCode = -(hr);                                                     \
        printf("%s failed: error %d occurred\n", #description, errorCode);     \
        goto ExitEndpoint;                                                     \
    }

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
    "-i Set/get default audio input device status\n"

#define HELP_TEXT_XP                                                           \
    "-w Set/get Wave Out status (win xp)\n"                                    \
    "-c Set/get CD status (win xp)\n"                                          \
    "-n Set/get Midi status (win xp)\n"                                        \
    "-l Set/get Line in status (win xp)\n"                                     \
    "volume: float in 0 to 100\n"

#define PRINT_HELP(version)                                                    \
    {                                                                          \
        printHelp(version);                                                    \
        goto Exit;                                                             \
    }

void printHelp(ULONG version) {
    // get self path
    char selfName[MAX_PATH];
    GetModuleFileName(NULL, selfName, MAX_PATH);

    // get file name from path
    const char *ptr = strrchr(selfName, '\\');
    if (ptr != NULL)
        strcpy(selfName, ptr + 1);

    printf(HELP_TEXT, selfName);
    if (version < 6) {
        puts(HELP_TEXT_XP);
    }
}
ULONG getMajorVersion(void) {
    OSVERSIONINFO VersionInfo;
    ZeroMemory(&VersionInfo, sizeof(OSVERSIONINFO));
    VersionInfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
    GetVersionEx(&VersionInfo);
    return VersionInfo.dwMajorVersion;
}

// typedef int (* pMasterVolFunc)(unsigned int flags, float *volume, BOOL *mute);

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

    EDataFlow eDataFlow = eRender;
    if (flags & MASTERVOL_IN) {
        eDataFlow = eCapture;
    }
    // Get default audio-rendering device.
    COM_CALL(hResult, pEnumerator, GetDefaultAudioEndpoint, eDataFlow, eConsole,
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

    // open default mixer
    mmResult = mixerOpen(&hMixer, 0, 0, 0, 0);
    EXIT_ON_MM_ERROR(mmResult, mixerOpen)

    // get mixer line info
    MIXERLINE MIXER_INIT(mixerLine);
    // dwComponentType
    DWORD dwComponentType = MIXERLINE_COMPONENTTYPE_DST_SPEAKERS;
    if (flags & MASTERVOL_IN) {
        dwComponentType = MIXERLINE_COMPONENTTYPE_SRC_MICROPHONE;
    } else if (flags & MASTERVOL_WAVEOUT) {
        dwComponentType = MIXERLINE_COMPONENTTYPE_SRC_WAVEOUT;
    } else if (flags & MASTERVOL_LINE) {
        dwComponentType = MIXERLINE_COMPONENTTYPE_SRC_LINE;
    } else if (flags & MASTERVOL_MIDI) {
        dwComponentType = MIXERLINE_COMPONENTTYPE_SRC_SYNTHESIZER;
    } else if (flags & MASTERVOL_CD) {
        dwComponentType = MIXERLINE_COMPONENTTYPE_SRC_COMPACTDISC;
    }

    mixerLine.dwComponentType = dwComponentType;
    mmResult = mixerGetLineInfo((HMIXEROBJ)hMixer, &mixerLine,
                                MIXER_GETLINEINFOF_COMPONENTTYPE);

    // workaround
    if (mmResult == MIXERR_INVALCONTROL &&
        dwComponentType == MIXERLINE_COMPONENTTYPE_DST_WAVEIN) {
        mixerLine.dwComponentType = dwComponentType;
        mmResult = mixerGetLineInfo((HMIXEROBJ)hMixer, &mixerLine,
                                    MIXER_GETLINEINFOF_COMPONENTTYPE);
    }
    EXIT_ON_MM_ERROR(mmResult, mixerGetLineInfo)

    // Mixer Line Controls
    MIXERCONTROL MIXER_INIT(mixerControl);
    MIXERLINECONTROLS MIXER_LI_INIT(mixerLineControls, mixerControl, mixerLine);

    mixerLineControls.dwControlType = MIXERCONTROL_CONTROLTYPE_VOLUME;
    mmResult = mixerGetLineControls((HMIXEROBJ)hMixer, &mixerLineControls,
                                    MIXER_GETLINECONTROLSF_ONEBYTYPE);
    EXIT_ON_MM_ERROR(mmResult, mixerGetLineControls)

    // volume mixer control details
    MIXERCONTROLDETAILS_UNSIGNED volStruct = {0};
    MIXERCONTROLDETAILS MIXER_DT_INIT(mixerControlDetails, mixerControl, volStruct);

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

    MIXERCONTROLDETAILS_BOOLEAN muteStruct = {0};
    if (flags & MASTERVOL_MUTE) {
        // Mixer Line Controls Mute
        mixerLineControls.dwControlType = MIXERCONTROL_CONTROLTYPE_MUTE;
        mmResult = mixerGetLineControls((HMIXEROBJ)hMixer, &mixerLineControls,
                                        MIXER_GETLINECONTROLSF_ONEBYTYPE);
        EXIT_ON_MM_ERROR(mmResult, mixerGetLineControls)

        // mute mixer control details
        mixerControlDetails.cbDetails = sizeof(muteStruct);
        mixerControlDetails.paDetails = &muteStruct;
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

int __cdecl mainCRTStartup() {
    int errorCode = 0;
    float volume = 0;
    unsigned int flags = MASTERVOL_GET_VOL;
    BOOL mute = FALSE;
    int argc;
    wchar_t **argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    ULONG version = getMajorVersion();

    // arguments parsing
    for (int i = 1; i < argc; i++) {
        size_t argLen = wcslen(argv[i]);
        if (argLen < 1)
            continue;
        if (argv[i][0] == '-' || argv[i][0] == '/') {
            for (size_t j = 1; j < argLen; j++) {
                switch (argv[i][j]) {
                case '-':
                    if (argLen == 6 && _wcsicmp(argv[i], L"--help") == 0) {
                        PRINT_HELP(version);
                    }
                    break;
                case 'h':
                case '?':
                    PRINT_HELP(version);
                    break;
                case 's':
                    flags &= ~(MASTERVOL_GET_VOL | MASTERVOL_GET_MUTE);
                    flags |= MASTERVOL_SILENT;
                    break;
                case 'm':
                    mute = TRUE;
                    flags |= MASTERVOL_MUTE;
                    break;
                case 'd':
                    flags |= MASTERVOL_GET_MUTE;
                    break;
                case 'u':
                    mute = FALSE;
                    flags |= MASTERVOL_MUTE;
                    break;
                case 'i':
                    flags &= ~MASTERVOL_SWITCHES;
                    flags |= MASTERVOL_IN;
                    break;
                case 'w':
                    flags &= ~MASTERVOL_SWITCHES;
                    flags |= MASTERVOL_WAVEOUT;
                    break;
                case 'c':
                    flags &= ~MASTERVOL_SWITCHES;
                    flags |= MASTERVOL_CD;
                    break;
                case 'n':
                    flags &= ~MASTERVOL_SWITCHES;
                    flags |= MASTERVOL_MIDI;
                    break;
                case 'l':
                    flags &= ~MASTERVOL_SWITCHES;
                    flags |= MASTERVOL_LINE;
                    break;
                }
            }
        } else if ((argv[i][0] >= '0' && argv[i][0] <= '9') || argv[i][0] == '.') {
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

    if (version < 6) {
        errorCode = masterVolMixer(flags, &volume, &mute);
    } else {
        errorCode = masterVolEndpoint(flags, &volume, &mute);
    }

    if (!(flags & MASTERVOL_SILENT)) {
        printf("%d\n", (int)lroundf(volume * 100));
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
