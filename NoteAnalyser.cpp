#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <sstream>
#include <signal.h>
#include <algorithm>
#include <fstream>
#include <vector>

#include <portaudio.h>
#include <fftw3.h>
#include <string>

#include "i2cLEDScreen.h"

/* #define SAMPLE_RATE  (17932) // Test failure to open with this value. */
#define SAMPLE_RATE (44100)
#define FRAMES_PER_BUFFER (512)
#define NUM_SECONDS (1)
#define NUM_CHANNELS (2)
/* #define DITHER_FLAG     (paDitherOff) */
#define DITHER_FLAG (0)

#define ITERATION_SIZE (1)

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

// default without LCD
bool useScreen = false;
bool multipleNotes = false;
bool graphOutputs = true;

#define GRAPHING_MIN_FREQ 32
#define GRAPHING_MAX_FREQ 8000

double noteFrequencies[OCTAVES * NOTES] = {0};
double notePeaks[OCTAVES * NOTES] = {0};
bool noteHits[OCTAVES * NOTES] = {0};
std::string noteNames[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "H"};

using namespace std;
typedef struct
{
    int frameIndex; /* Index into sample array. */
    int maxFrameIndex;
    SAMPLE *recordedSamples;
} paTestData;

struct mapping_t {
    vector<int> frequencies;
    vector<double> volume;
} mappingStruct;

volatile sig_atomic_t stop;

void inthand(int signum)
{
    stop = 1;
    printf("Programm is beeing shutdown, please await the last iteration...\n");
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

void initializeNoteFrequencies()
{
    // calculate NoteFrequencies
    float currFreq = 16.35f;
    for (size_t i = 0; i < OCTAVES; i++)
    {
        for (size_t j = 0; j < NOTES; j++)
        {
            noteFrequencies[j + i * NOTES] = currFreq;
            // printf("%f  %d  %f\n", currFreq, j+i*NOTES, pow(2.f,1.f/12.f));
            currFreq = currFreq * pow(2.f, 1.f / 12.f);
        }
    }
}

int calculateNote(double frequency)
{
    // filter out everything below 55, because it's the basic noise
    if (frequency < 55. || frequency > 8000)
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
    std::stringstream output;
    if (note != -1)
        output << "The Note detected was: " << noteNames[note % 12].c_str() << note / 12;
    else
        output << "No Note recognized!";

    printf("%s\n", output.str().c_str());
    if (useScreen)
        printToScreen(output.str(), 1);
}

// todo: implement multiple note calculation
void calculateNotes()
{
}

void printNotes(bool notesToPrint[])
{
    bool recognizedSomething = false;
    std::stringstream output;
    for (int i = 0; i < NOTES * OCTAVES; i++)
    {
        if (notesToPrint[i])
        {
            recognizedSomething = true;
            output << noteNames[i % 12].c_str() << i / 12 << " ";
        }
    }
    if (recognizedSomething)
        printf("Notes recognized: %s\n", output.str().c_str());
    else
        printf("No Notes recognized!");
}
/**
 * @brief Filter input to only peaks to output by comparing left and right of values.
 *
 * @param toFilter input array to check
 * @param output output (same size as input) for peaks only, rest is filled with 0
 * @param arraySize size of both arrays
 */
void filterPeaks(double *toFilter, double *output, int arraySize)
{
    int VALUE_CUTOFF = 3;
    int cooldown = 0;
    double oldValue = 0;
    double lastValue = toFilter[0];
    // go through the array
    for (int i = 1; i < arraySize; i++)
    {
        // throw away everything under constant
        if (toFilter[i] > VALUE_CUTOFF)
        {
            // check if last value was lokal peak
            if (lastValue > oldValue && lastValue > toFilter[i])
            {
                // try to look ahead one more to try and avoid noise || let second last value pass for free
                if ((i + 1 < arraySize && lastValue > toFilter[i + 1]) || i + 1 == arraySize - 1)
                {
                    // remember spike
                    output[i - 1] = lastValue;
                }
            }
        }
        // advance comparison values
        oldValue = lastValue;
        lastValue = toFilter[i];
    }
}

bool cmdOptionExists(char **begin, char **end, const std::string &option)
{
    return std::find(begin, end, option) != end;
}

// Returns correction values from file
void readCorrectionValues() {
    ifstream correctionFile("frequncyCorrection");
    // Alternative values (approximnation)
    // ifstream correctionFile("frequncyCorrectionAlt");
    int freq;
    double val;

    while(correctionFile >> freq >> val) {
        mappingStruct.frequencies.push_back(freq);
        mappingStruct.volume.push_back(val);
    }
}

// Calculates the corrected value, louder values are pitched down, quieter pitched up
double correctValue(int frequency, double value) {
    double shortestDistance = 100.0;
    double currentDistance = 0.0;
    double correction = 0.0;

    for(int i = 0; i < mappingStruct.frequencies.size(); i++) {
        currentDistance = abs(frequency - mappingStruct.frequencies[i]);

        if(currentDistance < shortestDistance) {
            correction = mappingStruct.volume[i];
            shortestDistance = currentDistance;
        }
    }

    return correction * value;
}

/*******************************************************************/
int main(int argc, char *argv[]);
int main(int argc, char *argv[])
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
    double highestPeak = 0;
    int highestFrequencyIndex = 0;
    int stepSize;
    data.maxFrameIndex = totalFrames = NUM_SECONDS * SAMPLE_RATE; /* Record for a few seconds. */
    numSamples = totalFrames * NUM_CHANNELS;
    numBytes = numSamples * sizeof(SAMPLE);
    data.recordedSamples = (SAMPLE *)malloc(numBytes); /* From now on, recordedSamples is initialised. */
    stepSize = numSamples / ITERATION_SIZE;
    double results[stepSize] = {0};
    bool firstRun = true;
    ofstream plotFile;
    ofstream analysisFile;
    initializeNoteFrequencies();
    readCorrectionValues();
    if (cmdOptionExists(argv, argv + argc, "-L"))
    {
        useScreen = 1;
        init_i2c_screen();
    }
    // else
    // printf("Call with \"-L\" to use i2cLCD\n");
    fflush(stdout);

    fftw_complex in[stepSize];
    fftw_complex out[stepSize];
    fflush(stdout);
    fftw_plan plan = fftw_plan_dft_1d(stepSize, in, out, FFTW_FORWARD, FFTW_ESTIMATE);

    if (data.recordedSamples == NULL)
    {
        printf("Could not allocate record array.\n");
        goto done;
    }

    // while loop until ctrl+c is pressed
    signal(SIGINT, inthand);
    while (!stop)
    {
        // reset arrays and variables
        fill(notePeaks, notePeaks + (OCTAVES + NOTES), 0);
        data.frameIndex = 0;
        fill(data.recordedSamples, data.recordedSamples + numSamples, 0);

        // re route stderr to hide ALSA errors that cannot easily be disabled
        // see: https://stackoverflow.com/questions/24778998/how-to-disable-or-re-route-alsa-lib-logging
        freopen("/dev/null", "w", stderr);
        err = Pa_Initialize();
        freopen("/dev/tty", "w", stderr);

        if (err != paNoError)
            goto done;

        inputParameters.device = Pa_GetDefaultInputDevice(); /* default input device */

        if (inputParameters.device == paNoDevice)
        {
            fprintf(stderr, "Error: No default input device.\n");
            goto done;
        }

        inputParameters.channelCount = NUM_CHANNELS; /* stereo input */
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

        // printf("=== Now recording!! ...");
        fflush(stdout);

        while ((err = Pa_IsStreamActive(stream)) == 1)
        {
            Pa_Sleep(1000 * NUM_SECONDS);
            // printf("index = %d\n", data.frameIndex);
            fflush(stdout);
        }

        if (err < 0)
            goto done;

        err = Pa_CloseStream(stream);
        // printf("  Done recording!! ===\n");
        if (err != paNoError)
            goto done;

        /* Measure maximum peak amplitude. */
        max = 0;
        average = 0.0;
        // for (int i = 0; i < numSamples; i++)
        // {
        //    printf("%g\n", data.recordedSamples[i]);
        // }

        for (int n = 1; n <= ITERATION_SIZE; n++)
        {
            for (i = stepSize * (n - 1); i < stepSize * n; i++)
            {
                val = data.recordedSamples[i];
                if (val < 0)
                    val = -val; /* ABS */

                if (val > max)
                {
                    max = val;
                }

                average += val;

                in[i - (stepSize * (n - 1))][0] = data.recordedSamples[i];
                in[i - (stepSize * (n - 1))][1] = 0;
            }

            fftw_execute(plan);

            for (i = 0; i < stepSize; i++)
            {
                double mag = sqrt(out[i][0] * out[i][0] + out[i][1] * out[i][1]);
                results[i] += mag;
            }
        }
        // printf("\n");

        // printf("=================================\n");
        // printf("fftw\n");
        highestFrequency = 0;
        highestFrequencyIndex = 0;
        highestPeak = 0;
        plotFile.open("plotData");

        // reset noteHits
        for (int i = 0; i < OCTAVES * NOTES; i++)
        {
            noteHits[i] = false;
        }

        // process results
        for (i = 0; i < stepSize / NUM_CHANNELS; i++)
        {
            int currFrequency = i / NUM_SECONDS * ITERATION_SIZE;
            results[i] = results[i] / ITERATION_SIZE;
            results[i] = correctValue(i, results[i]);
            if (currFrequency > GRAPHING_MIN_FREQ && currFrequency < GRAPHING_MAX_FREQ)
                if (multipleNotes)
                {
                    if (results[i] > 3)
                        noteHits[calculateNote(i)] = true;
                }
                else
                {
                    if (results[i] > highestPeak)
                    {
                        highestPeak = results[i];
                        highestFrequencyIndex = i;
                    }
                }
            if (graphOutputs && currFrequency > GRAPHING_MIN_FREQ && currFrequency < GRAPHING_MAX_FREQ)
            {
                plotFile << currFrequency << " " << results[i] << "\n";
            }
            // printf("%g\n", results[i]);
        }
        // printf("=================================\n");
        plotFile.close();
        if (!firstRun && graphOutputs)
            system("gnuplot oneTimeGnuPlot");
        highestFrequency = highestFrequencyIndex / NUM_SECONDS * ITERATION_SIZE;
        printf("Frequency peak at: %d\n", highestFrequency);
        if (multipleNotes)
        {
            calculateNotes();
            printNotes(noteHits);
        }
        else
        {
            if (highestPeak > 1)
                printNote(calculateNote(highestFrequency));
            else
                printNote(-1);
        }
        printf("FrequencyValue: %g\n", highestPeak);
        analysisFile.open("frequencyScale", std::ofstream::app);
        int freq = 0;
        // freq == Freqency ID according to the notes from noteFrequencies (C0(0) - B8(108))
        if (sscanf(argv[1], "%i", &freq) != 1)
        {
            fprintf(stderr, "error - not an integer");
        }
        // freq = 60;
        freq = round(noteFrequencies[freq]);
        if((results[freq] +3) < results[highestFrequencyIndex])
            freq = highestFrequencyIndex;
        printf("freq saved: %d\n", freq);
        analysisFile << freq << " " << results[freq] << endl;
        analysisFile.close();
        for (int i = 0; i < stepSize; i++)
        {
            results[i] = 0;
        }
        firstRun = false;
        stop = true;
    }
done:
    Pa_Terminate();
    if (plotFile.is_open())
        plotFile.close();
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
