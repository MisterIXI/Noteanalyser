using namespace std;

#include <portaudio.h>
#include "AnalyserDefinitions.h"
#include <string>

typedef struct
{
    int frameIndex; /* Index into sample array. */
    int maxFrameIndex;
    SAMPLE *recordedSamples;
} paTestData;

PaStreamParameters inputParameters, outputParameters;
PaStream *stream;
PaError err = paNoError;
paTestData data;

//forward Declarations
void cleanupPAAndExit(string message);
void cleanupPA();
void cleanupPA(string message);
static int recordCallback(const void *inputBuffer, void *outputBuffer, unsigned long framesPerBuffer, const PaStreamCallbackTimeInfo *timeInfo, PaStreamCallbackFlags statusFlags, void *userData);

SAMPLE *retrieveResults()
{
    while ((err = Pa_IsStreamActive(stream)) == 1)
        Pa_Sleep(100);
    if (err < 0)
        cleanupPAAndExit("Error while checking if the stream is active!");

    err = Pa_CloseStream(stream);
    if (err != paNoError)
        cleanupPAAndExit("Error with closing the stream!");
    return data.recordedSamples;
}
void resetPA()
{
    data.frameIndex = 0;
    fill(data.recordedSamples, data.recordedSamples + numSamples, 0);
}
void startRecording()
{
    resetPA();
    // SAMPLE max, val;
    err = Pa_OpenStream(&stream, &inputParameters, NULL, SAMPLE_RATE, FRAMES_PER_BUFFER, paClipOff, recordCallback, &data);
    if (err != paNoError)
        cleanupPAAndExit("");

    err = Pa_StartStream(stream);
    if (err != paNoError)
        cleanupPAAndExit("");
}

void initializePA()
{
    data.maxFrameIndex = totalFrames;

    data.recordedSamples = (SAMPLE *)malloc(numSamples * sizeof(SAMPLE)); /* From now on, recordedSamples is initialised. */

    if (data.recordedSamples == NULL)
        cleanupPAAndExit("Could not allocate record array.\n");

    // re route stderr to hide ALSA errors that cannot easily be disabled
    // see: https://stackoverflow.com/questions/24778998/how-to-disable-or-re-route-alsa-lib-logging
    freopen("/dev/null", "w", stderr);
    err = Pa_Initialize();
    freopen("/dev/tty", "w", stderr);

    if (err != paNoError)
        cleanupPAAndExit("");

    inputParameters.device = Pa_GetDefaultInputDevice(); /* default input device */
    if (inputParameters.device == paNoDevice)
        cleanupPAAndExit("Error: No default input device.\n");

    inputParameters.channelCount = NUM_CHANNELS; /* stereo input */
    inputParameters.sampleFormat = PA_SAMPLE_TYPE;
    inputParameters.suggestedLatency = Pa_GetDeviceInfo(inputParameters.device)->defaultLowInputLatency;
    inputParameters.hostApiSpecificStreamInfo = NULL;

    data.frameIndex = 0;
}

void cleanupPAAndExit(string message)
{
    cleanupPA(message);
    exit(1);
}

void cleanupPA()
{
    cleanupPA("");
}

void cleanupPA(string message)
{
    if (message != "")
        printf(message.c_str());
    Pa_Terminate();
    if (data.recordedSamples)
        free(data.recordedSamples);

    if (err != paNoError)
    {
        fprintf(stderr, "An error occured while using the portaudio stream\n");
        fprintf(stderr, "Error number: %d\n", err);
        fprintf(stderr, "Error message: %s\n", Pa_GetErrorText(err));
        err = 1; /* Always return 0 or 1, but no other return codes. */
    }
}

/* This routine will be called by the PortAudio engine when audio is needed.
** It may be called at interrupt level on some machines so don't do anything
** that could mess up the system like calling malloc() or free().
*/
static int recordCallback(const void *inputBuffer, void *outputBuffer, unsigned long framesPerBuffer, const PaStreamCallbackTimeInfo *timeInfo, PaStreamCallbackFlags statusFlags, void *userData)
{
    paTestData *data = (paTestData *)userData;
    const SAMPLE *rptr = (const SAMPLE *)inputBuffer;
    SAMPLE *wptr = &data->recordedSamples[data->frameIndex * NUM_CHANNELS];
    long framesToCalc;
    long i;
    int finished;
    unsigned long framesLeft = data->maxFrameIndex - data->frameIndex;

    (void)outputBuffer; /* Prevent unused variable warnings. */
    (void)timeInfo;
    (void)statusFlags;
    (void)userData;

    if (framesLeft < framesPerBuffer)
    {
        framesToCalc = framesLeft;
        finished = paComplete;
    }
    else
    {
        framesToCalc = framesPerBuffer;
        finished = paContinue;
    }

    if (inputBuffer == NULL)
    {
        for (i = 0; i < framesToCalc; i++)
        {
            *wptr++ = SAMPLE_SILENCE; /* left */

            if (NUM_CHANNELS == 2)
                *wptr++ = SAMPLE_SILENCE; /* right */
        }
    }
    else
    {
        for (i = 0; i < framesToCalc; i++)
        {
            *wptr++ = *rptr++; /* left */

            if (NUM_CHANNELS == 2)
                *wptr++ = *rptr++; /* right */
        }
    }

    data->frameIndex += framesToCalc;

    return finished;
}