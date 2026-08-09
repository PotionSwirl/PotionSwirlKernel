#pragma once

#include <stdint.h>
#include "PCI.h"

// Global capabilities, status and control register offsets
#define IHDA_GCAP_REGISTER_OFFSET       0x00
#define IHDA_GCTL_REGISTER_OFFSET       0x08
#define IHDA_STATESTS_REGISTER_OFFSET   0x0E
#define IHDA_INTCTL_REGISTER_OFFSET     0x20
#define IHDA_INTSTS_REGISTER_OFFSET     0x24
#define IHDA_CORBLBASE_REGISTER_OFFSET  0x40
#define IHDA_CORBUBASE_REGISTER_OFFSET  0x44
#define IHDA_CORBWP_REGISTER_OFFSET     0x48
#define IHDA_CORBRP_REGISTER_OFFSET     0x4A
#define IHDA_CORBCTL_REGISTER_OFFSET    0x4C
#define IHDA_CORBSIZE_REGISTER_OFFSET   0x4E
#define IHDA_RIRBLBASE_REGISTER_OFFSET  0x50
#define IHDA_RIRBUBASE_REGISTER_OFFSET  0x54
#define IHDA_RIRBWP_REGISTER_OFFSET     0x58
#define IHDA_RIRBCTL_REGISTER_OFFSET    0x5C
#define IHDA_RIRBSIZE_REGISTER_OFFSET   0x5E

// Stream descriptor register offsets
#define IHDA_SDNCTL_REGISTER_OFFSET     0x00
#define IHDA_SDNSTS_REGISTER_OFFSET     0x03
#define IHDA_SDNCBL_REGISTER_OFFSET     0x08
#define IHDA_SDNLVI_REGISTER_OFFSET     0x0C
#define IHDA_SDNFMT_REGISTER_OFFSET     0x12
#define IHDA_SDNBDPL_REGISTER_OFFSET    0x18
#define IHDA_SDNBDPU_REGISTER_OFFSET    0x1C

// Verb IDs
#define IHDA_SET_CONVERTER_FORMAT_VERB_ID                 0x2
#define IHDA_SET_AMPLIFIER_GAIN_MUTE_VERB_ID              0x3
#define IHDA_SET_CONNECTION_SELECT_CONTROL_VERB_ID        0x701
#define IHDA_SET_POWER_STATE_VERB_ID                      0x705
#define IHDA_SET_CONVERTER_STREAM_CHANNEL_VERB_ID         0x706
#define IHDA_SET_PIN_WIDGET_CONTROL_VERB_ID               0x707
#define IHDA_SET_EAPD_BTL_ENABLE_VERB_ID                  0x70C
#define IHDA_SET_CONVERTER_CHANNEL_COUNT_VERB_ID          0x72D
#define IHDA_GET_PARAMETER_VERB_ID                        0xF00
#define IHDA_GET_CONNECTION_LIST_ENTRY_VERB_ID            0xF02
#define IHDA_GET_CONFIGURATION_DEFAULT_VERB_ID            0xF1C

// Parameter IDs
#define IHDA_SUBORDINATE_NODE_COUNT_PARAMETER_ID          0x04
#define IHDA_FUNCTION_GROUP_TYPE_PARAMETER_ID             0x05
#define IHDA_AUDIO_WIDGET_CAPABILITIES_PARAMETER_ID       0x09
#define IHDA_OUTPUT_AMPLIFIER_CAPABILITIES_PARAMETER_ID   0x12
#define IHDA_SUPPORTED_PCM_SIZE_AND_RATES_PARAMETER_ID    0x0A
#define IHDA_SUPPORTED_STREAM_FORMATS_PARAMETER_ID        0x0B
#define IHDA_PIN_CAPABILITIES_PARAMETER_ID                0x0C
#define IHDA_INPUT_AMPLIFIER_CAPABILITIES_PARAMETER_ID    0x0D
#define IHDA_CONNECTION_LIST_LENGTH_PARAMETER_ID          0x0E

// Node types
#define IHDA_DAC_TYPE                                     0x00
#define IHDA_AUDIO_FUNCTION_GROUP_TYPE                    0x01
#define IHDA_MIXER_TYPE                                   0x02
#define IHDA_SELECTOR_TYPE                                0x03
#define IHDA_PIN_TYPE                                     0x04

typedef enum {
    IHDA_8_BIT_REGISTER,
    IHDA_16_BIT_REGISTER,
    IHDA_24_BIT_REGISTER,
    IHDA_32_BIT_REGISTER
} IHDA_RegisterSize_t;

// BDL Entry
typedef struct __attribute__((packed)) {
    uint64_t Address;
    uint32_t Length;
    uint32_t IOCAndReserved;
} IHDA_BDLE_t;

// WAV file header
typedef struct __attribute__((packed)) {
    char RiffID[4];         // "RIFF"
    uint32_t ChunkSize;     // File size - 8 bytes
    char RiffFormat[4];     // "WAVE"
    char FmtID[4];          // "fmt "
    uint32_t FmtSize;       // Size of the fmt chunk (16 for PCM)
    uint16_t AudioFormat;   // Audio format (1 for PCM)
    uint16_t NumChannels;   // Number of channels (1 for mono, 2 for stereo)
    uint32_t SampleRate;    // Sampling frequency (e.g., 44100)
    uint32_t ByteRate;      // SampleRate * NumChannels * BitsPerSample / 8
    uint16_t BlockAlign;    // NumChannels * BitsPerSample / 8
    uint16_t BitsPerSample; // Bits per sample (e.g., 16)
    char DataID[4];         // "data"
    uint32_t DataSize;      // Size of the audio data
} IHDA_WAVHeader_t;

// Functions
void IHDA_Initialise(volatile uint8_t* IHDABAR0Argument, PCI_BusDeviceFunction_t* IHDABusDeviceFunctionArgument);
void IHDA_TakeControllerOutOfReset();
void IHDA_FindCodec();
void IHDA_InitialiseCORB();
void IHDA_InitialiseRIRB();
void IHDA_InitialiseBDLs();
void IHDA_InitialiseOutputStreamBuffer();
void IHDA_InitialiseOutputStream1Interrupt();
void IHDA_CreateSpeakerPinAndDACAudioPath();
void IHDA_InitialiseSpeakerPinAndDAC();
void IHDA_UnmuteNode(uint8_t NID, uint8_t IndexToUnmute);
void IHDA_ConfigureOutputStream1();
uint32_t IHDA_OutputStreamDescriptorRead(IHDA_RegisterSize_t RegisterSize, uint8_t StreamID, uint32_t Offset);
void IHDA_OutputStreamDescriptorWrite(IHDA_RegisterSize_t RegisterSize, uint8_t StreamID, uint64_t Offset, uint32_t Value);
uint32_t IHDA_Submit4BitIDVerb(uint8_t CAd, uint8_t NID, uint8_t VerbID, uint16_t Payload);
uint32_t IHDA_Submit12BitIDVerb(uint8_t CAd, uint8_t NID, uint16_t VerbID, uint8_t Payload);
uint32_t IHDA_SubmitVerbStub(uint32_t Verb);
void IHDA_PlayWAV(const char* FilePath);
void IHDA_Vector64InterruptHandlePassover();
