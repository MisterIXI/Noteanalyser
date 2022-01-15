CompileNoteAnalyser:
	g++ NoteAnalyser.cpp i2cLEDScreen.cpp -l portaudio -l fftw3 -l ws2811 -l wiringPi -o NoteAnalyser