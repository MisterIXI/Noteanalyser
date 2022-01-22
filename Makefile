NoteAnalyzer:
	g++ NoteAnalyser.cpp i2cLEDScreen.cpp -l portaudio -l fftw3 -l ws2811 -l wiringPi -o NoteAnalyser

LedExample:
	g++ ledExample.cpp -l ws2811 -o ledExample
