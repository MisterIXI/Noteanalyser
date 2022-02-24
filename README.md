# Summary
This program takes live input from the default microphone, and performs a frequency analysis to recognize the played note on a piano. (Or a direct generated sine wave)
This is a university project for "MUME" under Maximilian Mengel.

[Click here](https://misterixi.github.io/NoteanalyserPreview/) for a rendered Preview of the 3D Model. Alternatively you can visit the [Fusion 360 shared Link](https://a360.co/3uYjknz) to get a manual view of the model file. :)

# Installation
Required Programms:
please install the following programms with your package manager (sudo apt-get install [...])
- make
- cmake
- g++
- gnuplot (optional if you never activate plotting)  

You can install everything at once like so on debian (and raspbian)  
```sudo apt-get install make cmake g++ gnuplot```

After this you can simply run the provided installation script with:  
```sudo ./install_dependencies```  

Note that this script will attempt to download and install every depencies it finds as missing in the system including compiling. It might take a few minutes to run, but afterwards everything should be ready.  
It will create the directory **lib/** which you can delete after running the script since it is only a working directory. The dependencies themselves will be installed to the system accordingly.

