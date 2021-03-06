#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <sstream>
#include <signal.h>
#include <algorithm>
#include <fstream>
#include <vector>

#include <cstring>

#include <chrono>

#include <portaudio.h>
#include <fftw3.h>
#include <string>
#include <ws2811/ws2811.h>

#include "i2cLEDScreen.h"

#include "AnalyserDefinitions.h"

#include "PortaudioRecording.h"
using namespace std;

/* #define DITHER_FLAG     (paDitherOff) */

// default without LCD
bool useScreen = FLAGS_USE_SCREEN;
bool multipleNotes = FLAGS_MULTIPLE_NOTES;
bool graphOutputs = FLAGS_GRAPH_OUTPUTS;
bool useLEDs = FLAGS_USE_LEDS;

double noteFrequencies[OCTAVES * NOTES] = {0};
bool noteHits[OCTAVES * NOTES] = {0};
double noteStrengths[OCTAVES * NOTES] = {0};
std::string noteNames[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "H"};

struct mapping_t
{
    vector<int> frequencies;
    vector<double> volume;
} mappingStruct;

volatile sig_atomic_t stop;

void intHandler(int signum)
{
    stop = true;
    printf("\nProgramm is beeing shutdown, please await the last iteration...\n");
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

void renderLEDs(int note, double strength);

void printNote(int note, double strength)
{
    std::stringstream output;
    if (note != -1)
        output << "Note detected: " << noteNames[note % 12].c_str() << note / 12;
    else
        output << "No Note recognized!";

    if (useScreen)
        printToScreen(output.str(), 1);
    else
        printf("%s\n", output.str().c_str());

    if (note != -1 && renderLEDs)
        renderLEDs(note, strength);
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
            //<< "|" << noteFrequencies[i] << "Hz"
            if (useLEDs)
                renderLEDs(i, noteStrengths[i]);
        }
    }
    if (useScreen)
        ClrLcd();
    if (recognizedSomething)
    {
        if (useScreen)
        {
            printToScreen("Notes recognized: ", 1);
            printToScreen(output.str(), 2);
        }
        else
        {
            printf("Notes recognized: \n%s\n", output.str().c_str());
        }
    }
    else
    {
        if (useScreen)
            printToScreen("No Notes recognized!", 1);
        else
            printf("No Notes recognized!\n");
    }
}

ws2811_t ledstrip =
    {
        .freq = WS2811_TARGET_FREQ,
        .dmanum = 10,
        .channel =
            {
                [0] =
                    {
                        .gpionum = 12,
                        .invert = 0,
                        .count = 96,
                        .strip_type = WS2811_STRIP_GBR,
                        .brightness = 255,
                    },
                [1] =
                    {
                        .gpionum = 0,
                        .invert = 0,
                        .count = 0,
                        .brightness = 0,
                    },
            },
};
ws2811_led_t dotcolors[] =
    {
        0x00111111, // will probably never happen --> white
        0x00111111, // will probably never happen --> white
        0x00111100, // turquoise
        0x00110000, // blue
        0x00000011, // red
        0x00001100, // green
        0x00111111, // white
        0x00110011, // purple
};
/**
 * @brief Accepts a note and its corresponding strength, then renders said note using the LED-strip.
 *
 * @param note the note played (0-108 -> 12 notes in 9 octaves)
 * @param strength strength of the played note (10-600)
 */
void renderLEDs(int note, double strength)
{
    int ledColumn = note % 12; // ideally 0 to 11
    if (strength > 600)
        strength = 600;
    int numLEDs = int((strength / 600) * 8); // calculates number of LEDs according to strength

    // activate the right LEDs
    for (int i = (ledColumn * 8) + (8 - numLEDs); i < (ledColumn + 1) * 8; i++)
    {
        ledstrip.channel[0].leds[i] += dotcolors[note / 12];
    }
}

void clearLEDS()
{
    // clear LEDs
    for (int i = 0; i < 96; i++)
    {
        ledstrip.channel[0].leds[i] = 0;
    }
}

/**
 * @brief Finds the nearest not 0 entry in the given array.
 *
 * @param array Array to search the new entry in
 * @param findIndex The start index from where to start the search
 * @param arraySize The size of the given array to avoid OOB Errors
 * @return The index of the nearest not 0 entry of the Array. -1 if nothing is found.
 */
int findPeak(double *array, int findIndex, int arraySize)
{
    // increase searchDistance if Point is in higher Hz territories to account for increasing variability
    int maxDistance = PEAK_MAX_SEARCH_DISTANCE + PEAK_SEARCH_DISTANCE_INCREASE * (findIndex / PEAK_SEARCH_INCREASE_STEPSIZE);
    if (array[findIndex] != 0)
        return findIndex;
    // search for the nearest not null entry in the filtered peakArray
    for (int i = 0; i < maxDistance; i++)
    {
        if (findIndex + i < arraySize)
            if (array[findIndex + i] != 0)
                return findIndex + i;
        if (findIndex - i >= 0)
            if (array[findIndex - i] != 0)
                return findIndex - i;
    }
    return -1;
}

/**
 * @brief Tries to remove the Overtones in the given array. Note that the Values should already filtered to avoid bad removals.
 * This function searches for the nearest Peak if it isn't at the correct frequency.
 *
 * @param array The pre-filtered Resultarray to remove the overtones in
 * @param startIndex
 */
void removeOvertones(double *array, int startIndex)
{
    int offset = GRAPHING_MIN_FREQ * NUM_SECONDS;
    double currStrength = array[startIndex] * 0.7;
    int i = startIndex + (startIndex + offset);
    while (i < resultSize)
    {
        // use findPeak() to account for slight variations in Peaks to not miss the overtone
        int actualPos = findPeak(array, i, resultSize);
        if (actualPos != -1)
        {
            double result = array[actualPos] - currStrength;
            if (result < VALUE_CUTOFF)
                result = 0;
            if (DEBUG_OVERTONE_VERBOSE)
                printf("i: %d | actualPos: %d | OT removed at %dHz: %g -> %g\n", i, actualPos, actualPos * 2, array[actualPos], result);
            array[actualPos] = result;
        }
        else
        {
            if (DEBUG_OVERTONE_VERBOSE)
                printf("skipped an OT at %gHz\n", calcHz(i));
        }

        i = i + startIndex + offset;
        currStrength = currStrength * 0.7;
        if (currStrength < 30)
            currStrength = 30;
    }
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
    double lastValue = 0;
    double currentValue = 0;
    double nextValue = 0;
    // go through the array (excluding first and last place for comparison reasons)
    for (int i = 1; i < arraySize - 1; i++)
    {
        currentValue = toFilter[i];
        // throw away everything under constant
        if (currentValue > VALUE_CUTOFF)
        {
            lastValue = toFilter[i - 1];
            nextValue = toFilter[i + 1];
            // check if last value was lokal peak
            if (currentValue > lastValue && currentValue > nextValue)
            {
                // remember spike
                output[i] = currentValue;
            }
        }
    }

    bool filtered = true;
    while (filtered)
    {
        filtered = false;
        int lastPeak = -1;
        int currentPeak = -1;
        int nextPeak = -1;
        // go through the output and kick potential noise peaks
        for (int i = 0; i < arraySize; i++)
        {
            if (output[i] > 0)
            {
                lastPeak = currentPeak;
                currentPeak = nextPeak;
                nextPeak = i;
                // check if at least two values are set and if currentPeak was deleted
                if (currentPeak != -1 && output[currentPeak] > 0)
                {
                    // check behind (only if all three values have been set)
                    if (lastPeak != -1)
                    {
                        // check if lastPeak was deleted
                        if (output[lastPeak] > 0)
                        {
                            // check behind only if difference in peakHeight ist > PEAK_DIFFERENCE_PER_STEP*distance and both indices are closer together than PEAK_DISTANCE
                            if (output[currentPeak] - output[lastPeak] > PEAK_DIFFERENCE_PER_STEP * (currentPeak - lastPeak) + PEAK_DIFFERENCE_OFFSET && currentPeak - lastPeak < PEAK_DISTANCE)
                            {
                                output[lastPeak] = 0;
                                filtered = true;
                                // printf("%d - %d = %d\n", currentPeak, lastPeak, currentPeak - lastPeak);
                            }
                        }
                    }
                    // check forward
                    if (output[currentPeak] - output[nextPeak] > PEAK_DIFFERENCE_PER_STEP * (nextPeak - currentPeak) + PEAK_DIFFERENCE_OFFSET && nextPeak - currentPeak < PEAK_DISTANCE)
                    {
                        output[nextPeak] = 0;
                        filtered = true;
                    }
                }
            }
        }
    }
    for (int i = 0; i < resultSize; i++)
    {
        if (output[i] > 0)
        {
            removeOvertones(output, i);
        }
    }
}

/**
 * @brief Checks if command options or arguments are set
 *
 * @param begin Startpointer of the array (usually argv)
 * @param end Endpointer of the array (usually argv+argc)
 * @param option The Option to search for as a string
 * @return True if the given string was found
 * @return False if no match was found
 */
bool cmdOptionExists(char **begin, char **end, const std::string &option)
{
    return std::find(begin, end, option) != end;
}

/**
 * @brief Retrieves the CorrectionValues from the File
 *
 */
void readCorrectionValues()
{
    ifstream correctionFile("frequencyCorrection");
    // Alternative values (approximnation)
    // ifstream correctionFile("frequencyCorrectionAlt");
    int freq;
    double val;

    while (correctionFile >> freq >> val)
    {
        mappingStruct.frequencies.push_back(freq);
        mappingStruct.volume.push_back(val);
    }
}

/**
 * @brief Calculates the corrected value, louder values are pitched down, quieter pitched up
 *
 * @param frequency The frequency at what the value occurs
 * @param value The value to be corrected
 * @return The corrected value as a double
 */
double correctValue(int frequency, double value)
{
    double shortestDistance = 100.0;
    double currentDistance = 0.0;
    double correction = 0.0;

    for (int i = 0; i < mappingStruct.frequencies.size(); i++)
    {
        currentDistance = abs(frequency - mappingStruct.frequencies[i]);

        if (currentDistance < shortestDistance)
        {
            correction = mappingStruct.volume[i];
            shortestDistance = currentDistance;
        }
    }

    return correction * value;
}

/**
 * @brief Outputs the given Resultarray into the file with the given filename and calls the given gnuplot to generate the png of the graph.
 *
 * @param fileName Name of the plotFile (likely "plotData" or "plotDataFiltered");
 * @param gnuplotName Name of the gnuplot file (likely "oneTimeGnuPlot" or "oneTimeGnuPlotFiltered")
 * @param resultArr Pointer to the result array (double[])
 */
void printPlotData(string fileName, string gnuplotName, double *resultArr)
{
    ofstream plotFile;
    plotFile.open(fileName);
    for (int i = 0; i < resultSize; i++)
    {
        int currFrequency = calcHz(i);
        if (currFrequency < GRAPHING_MAX_FREQ)
        {
            plotFile << currFrequency << " " << resultArr[i] << "\n";
        }
    }
    plotFile.close();
    string call = "gnuplot " + gnuplotName;
    system(call.c_str());
}

/*******************************************************************/
int main(int argc, char *argv[])
{

    int i;

    double average;
    double analyzeMax = 0;
    int highestFrequency = 0;
    double highestPeak = 0;
    int highestFrequencyIndex = 0;

    // results only need half the samples since we only look at one channel
    double results[resultSize] = {0};
    double filteredResults[resultSize] = {0};
    bool firstRun = true;
    printf("Starting Noteanalyser...\n");
    initializeNoteFrequencies();
    readCorrectionValues();
    // check for flags
    if (cmdOptionExists(argv, argv + argc, "-S") || FLAGS_USE_SCREEN)
    {
        useScreen = true;
        init_i2c_screen();
    }
    else
        printf("Call with \"-S\" to use i2cLCD screen\n");
    if (cmdOptionExists(argv, argv + argc, "-L") || FLAGS_USE_LEDS)
    {
        useLEDs = true;
        ws2811_return_t ret;
        if ((ret = ws2811_init(&ledstrip)) != WS2811_SUCCESS)
        {
            fprintf(stderr, "ws2811_init failed: %s\n", ws2811_get_return_t_str(ret));
            return ret;
        }
    }
    else
        printf("Call with \"-L\" to use LED rendering\n");
    if (cmdOptionExists(argv, argv + argc, "-G") || FLAGS_GRAPH_OUTPUTS)
        graphOutputs = true;
    else
        printf("Call with \"-G\" to graph outputs to png files\n");
    if (cmdOptionExists(argv, argv + argc, "-M") || FLAGS_MULTIPLE_NOTES)
        multipleNotes = true;
    else
        printf("Call with \"-M\" to recognize multiple Notes instead of a single one\n");
    fflush(stdout);
    fftw_complex in[numSamples];
    fftw_complex out[numSamples];
    fftw_plan plan = fftw_plan_dft_1d(numSamples, in, out, FFTW_FORWARD, FFTW_ESTIMATE);

    initializePA();
    startRecording();

    // while loop until ctrl+c is pressed
    struct sigaction act;
    act.sa_handler = intHandler;
    sigaction(SIGINT, &act, NULL);
    while (!stop)
    {
        using std::chrono::duration;
        using std::chrono::duration_cast;
        using std::chrono::high_resolution_clock;
        using std::chrono::milliseconds;
        auto t1 = high_resolution_clock::now();

        for (int i = 0; i < numSamples; i++)
        {
            in[i][0] = 0;
            out[i][0] = 0;
        }

        // blocking until  it's done recording
        SAMPLE *paData = retrieveResults();
        // copy results in working array for FFTW
        for (int i = 0; i < numSamples; i++)
        {
            in[i][0] = paData[i];
            in[i][1] = 0;
        }
        // let PA record while the calculations are run
        startRecording();

        // execute FFTW on data
        fftw_execute(plan);
        int minFreqOffset = GRAPHING_MIN_FREQ * NUM_SECONDS;
        for (i = 0; i < resultSize; i++)
        {
            double mag = sqrt(out[i + minFreqOffset][0] * out[i + minFreqOffset][0] + out[i + minFreqOffset][1] * out[i + minFreqOffset][1]);
            results[i] = mag;
        }

        highestFrequency = 0;
        highestFrequencyIndex = 0;
        highestPeak = 0;

        // process results only to half since it's mirrored at the middle from the 2 Channels
        for (i = 0; i < resultSize; i++)
        {
            results[i] = correctValue(i + minFreqOffset, results[i]);
        }
        filterPeaks(results, filteredResults, resultSize);
        for (i = 0; i < resultSize; i++)
        {
            int currFrequency = calcHz(i);
            // find note
            if (currFrequency > GRAPHING_MIN_FREQ && currFrequency < GRAPHING_MAX_FREQ)
                if (multipleNotes)
                {
                    if (filteredResults[i] > VALUE_CUTOFF)
                    {
                        noteHits[calculateNote(currFrequency)] = true;
                        noteStrengths[calculateNote(currFrequency)] = filteredResults[i];
                    }
                }
                else
                {
                    if (filteredResults[i] > highestPeak)
                    {
                        highestPeak = filteredResults[i];
                        highestFrequencyIndex = i;
                    }
                }
        }
        // output data to plotfiles if flag was set
        if (graphOutputs)
        {
            printPlotData("plotData", "oneTimeGnuPlot", results);
            printPlotData("plotDataFiltered", "oneTimeGnuPlotFiltered", filteredResults);
        }

        highestFrequency = calcHz(highestFrequencyIndex);
        if (!useScreen)
            printf("Frequency peak at: %d\n", highestFrequency);
        if (useLEDs)
            clearLEDS();
        if (multipleNotes)
        {
            // calculateNotes(filteredResults);
            printNotes(noteHits);
        }
        else
        {
            if (highestPeak > VALUE_CUTOFF)
            {

                printNote(calculateNote(highestFrequency), filteredResults[highestFrequencyIndex]);
                if (!useScreen)
                    printf("With a strength of: %f\n", results[highestFrequencyIndex]);
            }
            else
                printNote(-1, -1);
        }
        if (useLEDs)
            ws2811_render(&ledstrip);

        if (DEBUG_STOP_AFTER_HIT)
        {
            int peakCount = 0;
            for (int i = 0; i < resultSize; i++)
            {
                if (filteredResults[i] > 0)
                    peakCount++;
            }
            if (peakCount >= 1)
                stop = 1;
        }

        firstRun = false;

        // reset arrays and variables
        fill(noteHits, noteHits + (OCTAVES * NOTES), 0);
        fill(noteStrengths, noteStrengths + (OCTAVES * NOTES), 0);
        fill(results, results + resultSize, 0);
        fill(filteredResults, filteredResults + resultSize, 0);

        auto t2 = high_resolution_clock::now();
        if (DEBUG_MEASURE_TIME && !useScreen)
        {
            duration<double, std::milli> ms_double = t2 - t1;
            printf("Calculated for: %fms\n", ms_double.count());
        }
        if (DEBUG_CLEAR_TERMINAL)
            if (!stop)
                system("clear");
    }
    if (useLEDs)
    {
        // clearLEDS();
        ws2811_render(&ledstrip);
    }
}
