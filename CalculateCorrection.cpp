#include <fstream>
#include <vector>

using namespace std;

struct mapping_t {
    vector<int> frequencies {33, 311, 880, 2093, 3729, 5274, 5588, 7902};
    vector<double> volume {0.0, 48.5475, 588.928, 957.485, 2814.41, 1386.78, 433.613, 62.1036};
} mappingStruct;

// Calculate correction with one value per note
void calculateCorrection1() {
    ifstream infile("frequencyScale");
    ofstream outfile("frequncyCorrection");
    int frequency;
    double value;
    float level = 200.0;
    float counter = 0.0;
    float stepSize = 1.5;

    while(infile >> frequency >> value) {
        // Ignore frequencies below 180hz, values are getting to big
        if(frequency > 180) {
            double correction = (level - counter) / value;
            outfile << frequency << " " << correction << endl;
            counter += stepSize;
        }
    }

    infile.close();
    outfile.close();
}

// Calculate correction with approximate values
void calculateCorrection2() {
    ofstream outfile("frequncyCorrectionAlt");
    float level = 200.0;
    float counter = 0.0;
    float stepSize = 8;

    for(int i = 0; i < mappingStruct.frequencies.size(); i++) {
        float val = mappingStruct.volume[i];
        int freq = mappingStruct.frequencies[i];

        if(val != 0) {
            double correction = (level - counter) / val;
            outfile << freq << " " << correction << endl;
            counter += stepSize;
        } else {
            outfile << freq << " " << val << endl;
        }
    }

    outfile.close();
}

int main(int argc, char *argv[]) {
    calculateCorrection1();
    calculateCorrection2();
}