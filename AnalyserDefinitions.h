
// basic Parameters
#define OCTAVES 9
#define NOTES 12

#define VALUE_CUTOFF 15

// cutoff at B3 since the Noisefloor makes it difficult to correctly identify the peaks up to ~250Hz
#define GRAPHING_MIN_FREQ 246
#define GRAPHING_MAX_FREQ 4000

// peak definitions
#define PEAK_DIFFERENCE_PER_STEP 2
#define PEAK_DIFFERENCE_OFFSET 5
#define PEAK_DISTANCE 30
#define PEAK_MAX_SEARCH_DISTANCE 10
#define PEAK_SEARCH_DISTANCE_INCREASE 10
#define PEAK_SEARCH_INCREASE_STEPSIZE 500

// recording Parameters
#define SAMPLE_RATE 44100
#define FRAMES_PER_BUFFER 512
#define NUM_SECONDS 0.5
#define NUM_CHANNELS 2

constexpr int totalFrames = NUM_SECONDS * SAMPLE_RATE;
constexpr int numSamples = totalFrames * NUM_CHANNELS;
constexpr int resultSize = (GRAPHING_MAX_FREQ - GRAPHING_MIN_FREQ) * NUM_SECONDS;
constexpr double calcHz(int x) { return (x / NUM_SECONDS) + GRAPHING_MIN_FREQ; }

#define PA_SAMPLE_TYPE paFloat32
typedef float SAMPLE;
#define SAMPLE_SILENCE (0.0f)
#define PRINTF_S_FORMAT "%.8f"

#define DITHER_FLAG (0)

#define WRITE_TO_FILE (0)