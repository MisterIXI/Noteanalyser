#include <fstream>

using namespace std;

int main(int argc, char *argv[]) {
    ifstream infile("frequencyScale");
    ofstream outfile("frequncyCorrection");
    int frequency;
    double value;
    float level = 200.0;
    float counter = 0.0;
    float stepSize = 1.5;

    while(infile >> frequency >> value) {
        if(frequency > 180) {
            double correction = (level - counter) / value;
            printf("%d - %f - %f\n", frequency, value, correction);
            outfile << frequency << " " << correction << endl;
            counter += stepSize;
        }
    }

    infile.close();
    outfile.close();
}