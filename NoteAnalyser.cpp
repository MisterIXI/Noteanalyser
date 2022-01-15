#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include <portaudio.h>
#include <fftw3.h>

#include <string>

/* #define SAMPLE_RATE  (17932) // Test failure to open with this value. */
#define SAMPLE_RATE (44100)
#define FRAMES_PER_BUFFER (512)
#define NUM_SECONDS (0.1f)
#define NUM_CHANNELS (2)
/* #define DITHER_FLAG     (paDitherOff) */
#define DITHER_FLAG (0)

#define WRITE_TO_FILE (0)

/* Select sample format. */
#if 1
#define PA_SAMPLE_TYPE paFloat32
typedef float SAMPLE;
#define SAMPLE_SILENCE (0.0f)
#define PRINTF_S_FORMAT "%.8f"
#elif 1
#define PA_SAMPLE_TYPE paInt16
typedef short SAMPLE;
#define SAMPLE_SILENCE (0)
#define PRINTF_S_FORMAT "%d"
#elif 0
#define PA_SAMPLE_TYPE paInt8
typedef char SAMPLE;
#define SAMPLE_SILENCE (0)
#define PRINTF_S_FORMAT "%d"
#else
#define PA_SAMPLE_TYPE paUInt8
typedef unsigned char SAMPLE;
#define SAMPLE_SILENCE (128)
#define PRINTF_S_FORMAT "%d"
#endif

#define OCTAVES 9
#define NOTES 12

float noteFrequencies[OCTAVES * NOTES] = {0};
std::string noteNames[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "H"};

typedef struct
{
    int frameIndex; /* Index into sample array. */
    int maxFrameIndex;
    SAMPLE *recordedSamples;
} paTestData;

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

/* This routine will be called by the PortAudio engine when audio is needed.
** It may be called at interrupt level on some machines so don't do anything
** that could mess up the system like calling malloc() or free().
*/
static int playCallback(const void *inputBuffer, void *outputBuffer, unsigned long framesPerBuffer, const PaStreamCallbackTimeInfo *timeInfo, PaStreamCallbackFlags statusFlags, void *userData)
{
    paTestData *data = (paTestData *)userData;
    SAMPLE *rptr = &data->recordedSamples[data->frameIndex * NUM_CHANNELS];
    SAMPLE *wptr = (SAMPLE *)outputBuffer;
    unsigned int i;
    int finished;
    unsigned int framesLeft = data->maxFrameIndex - data->frameIndex;

    (void)inputBuffer; /* Prevent unused variable warnings. */
    (void)timeInfo;
    (void)statusFlags;
    (void)userData;

    if (framesLeft < framesPerBuffer)
    {
        /* final buffer... */
        for (i = 0; i < framesLeft; i++)
        {
            *wptr++ = *rptr++; /* left */

            if (NUM_CHANNELS == 2)
                *wptr++ = *rptr++; /* right */
        }

        for (; i < framesPerBuffer; i++)
        {
            *wptr++ = 0; /* left */

            if (NUM_CHANNELS == 2)
                *wptr++ = 0; /* right */
        }

        data->frameIndex += framesLeft;
        finished = paComplete;
    }
    else
    {
        for (i = 0; i < framesPerBuffer; i++)
        {
            *wptr++ = *rptr++; /* left */

            if (NUM_CHANNELS == 2)
                *wptr++ = *rptr++; /* right */
        }

        data->frameIndex += framesPerBuffer;
        finished = paContinue;
    }

    return finished;
}

int calculateNote(int frequency)
{
    if (noteFrequencies[0] == 0) //check if array is already initialized
    {
        //calculate NoteFrequencies
        float currFreq = 16.35f;
        for (size_t i = 0; i < OCTAVES; i++)
        {
            for (size_t j = 0; j < NOTES; j++)
            {
                noteFrequencies[j + i * NOTES] = currFreq;
                //printf("%f  %d  %f\n", currFreq, j+i*NOTES, pow(2.f,1.f/12.f));
                currFreq = currFreq * pow(2.f, 1.f / 12.f);
            }
        }
    }

    if (frequency < 15.f || frequency > 8000)
        return -1;
    int result = 0;

    float shortestDistance = abs(noteFrequencies[0] - frequency);
    float currDistance = 0;
    for (size_t i = 0; i < OCTAVES * NOTES; i++)
    {
        currDistance = abs(noteFrequencies[i] - frequency);
        if (currDistance < shortestDistance)
        {
            shortestDistance = currDistance;
            result = i;
        }
    }

    return result;
}

void printNote(int note)
{
    if (note != -1)
        printf("The Note detected was: %s%d", noteNames[note % 12].c_str(), note / 12);
    else
        printf("No Note recognized!");
}

/*******************************************************************/
int main(void);
int main(void)
{
    PaStreamParameters inputParameters, outputParameters;
    PaStream *stream;
    PaError err = paNoError;
    paTestData data;
    int i;
    int totalFrames;
    int numSamples;
    int numBytes;
    SAMPLE max, val;
    double average;
    double analyzeMax = 0;
    int highestFrequency = 0;

    printf("patest_record.c\n");
    fflush(stdout);

    data.maxFrameIndex = totalFrames = NUM_SECONDS * SAMPLE_RATE; /* Record for a few seconds. */
    data.frameIndex = 0;
    numSamples = totalFrames * NUM_CHANNELS;
    numBytes = numSamples * sizeof(SAMPLE);
    data.recordedSamples = (SAMPLE *)malloc(numBytes); /* From now on, recordedSamples is initialised. */
    fftw_complex in[numSamples];
    fftw_complex out[numSamples];
    printf("Hallo\n");
    fflush(stdout);
    fftw_plan plan = fftw_plan_dft_1d(numSamples, in, out, FFTW_FORWARD, FFTW_ESTIMATE);

    if (data.recordedSamples == NULL)
    {
        printf("Could not allocate record array.\n");
        goto done;
    }

    for (i = 0; i < numSamples; i++)
        data.recordedSamples[i] = 0;

    err = Pa_Initialize();

    if (err != paNoError)
        goto done;

    inputParameters.device = Pa_GetDefaultInputDevice(); /* default input device */

    if (inputParameters.device == paNoDevice)
    {
        fprintf(stderr, "Error: No default input device.\n");
        goto done;
    }

    inputParameters.channelCount = 2; /* stereo input */
    inputParameters.sampleFormat = PA_SAMPLE_TYPE;
    inputParameters.suggestedLatency = Pa_GetDeviceInfo(inputParameters.device)->defaultLowInputLatency;
    inputParameters.hostApiSpecificStreamInfo = NULL;

    /* Record some audio. -------------------------------------------- */
    err = Pa_OpenStream(&stream, &inputParameters, NULL, SAMPLE_RATE, FRAMES_PER_BUFFER, paClipOff, recordCallback, &data);

    if (err != paNoError)
        goto done;

    err = Pa_StartStream(stream);
    if (err != paNoError)
        goto done;

    printf("\n=== Now recording!! Please speak into the microphone. ===\n");
    fflush(stdout);

    while ((err = Pa_IsStreamActive(stream)) == 1)
    {
        Pa_Sleep(1000);
        printf("index = %d\n", data.frameIndex);
        fflush(stdout);
    }

    if (err < 0)
        goto done;

    err = Pa_CloseStream(stream);

    if (err != paNoError)
        goto done;

    /* Measure maximum peak amplitude. */
    max = 0;
    average = 0.0;

    for (i = 0; i < numSamples; i++)
    {
        val = data.recordedSamples[i];

        if (val < 0)
            val = -val; /* ABS */

        if (val > max)
        {
            max = val;
        }

        average += val;

        in[i][0] = data.recordedSamples[i];
        in[i][1] = 0;
    }

    fftw_execute(plan);

    printf("=================================");
    printf("fftw");
    for (i = 0; i < numSamples/2; i++)
    {
        double mag = sqrt(out[i][0] * out[i][0] + out[i][1] * out[i][1]);

        if (analyzeMax < mag)
        {
            analyzeMax = mag;
            highestFrequency = i;
        }
        printf("%g\n", mag);
    }

    average = average / (double)numSamples;

    printf("=================================");

    printf("sample max amplitude = %d\n", max);
    printf("sample average = %lf\n", average);
    printf("\n\n");
    if (NUM_SECONDS < 1)
        highestFrequency = highestFrequency / NUM_SECONDS;
    printNote(calculateNote(highestFrequency));

done:
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

    return err;
}