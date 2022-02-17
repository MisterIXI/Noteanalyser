
//basic Parameters
#define OCTAVES 9
#define NOTES 12

#define VALUE_CUTOFF 15

#define GRAPHING_MIN_FREQ 170
#define GRAPHING_MAX_FREQ 4000

//recording Parameters
#define SAMPLE_RATE 44100
#define FRAMES_PER_BUFFER 512
#define NUM_SECONDS 0.5
#define NUM_CHANNELS 2

constexpr int totalFrames = NUM_SECONDS * SAMPLE_RATE;
constexpr int numSamples = totalFrames * NUM_CHANNELS;
constexpr int resultSize = totalFrames;

#define PA_SAMPLE_TYPE paFloat32
typedef float SAMPLE;
#define SAMPLE_SILENCE (0.0f)
#define PRINTF_S_FORMAT "%.8f"

#define DITHER_FLAG (0)

#define WRITE_TO_FILE (0)