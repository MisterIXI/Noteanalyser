
NoteAnalyser:
	g++ NoteAnalyser.cpp i2cLEDScreen.cpp PortaudioRecording.cpp -l portaudio -l fftw3 -l ws2811 -l wiringPi -o NoteAnalyser.out -Wno-psabi

LedExample:
	g++ ledExample.cpp -l ws2811 -o ledExample.out

CalculationCorrection:
	g++ CalculateCorrection.cpp -o CalculateCorrection.out

