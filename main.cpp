/*!
 *   \file  main.c
 *   \brief  
 *  
 *  <+DETAILED+>
 *  
 *  \author  Bertrand.F (), 
 *  
 *  \internal
 *       Created:  25/06/2014
 *      Revision:  none
 *      Compiler:  gcc
 *  Organization:  
 *     Copyright:  Copyright (C), 2014, Bertrand.F
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <iostream>
#include <string>
#include <sstream>
#include <unistd.h>
#include <getopt.h>

#include <opencv2/opencv.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <SDL/SDL.h>

using namespace std;
using namespace cv;

#define VERSION_MAJOR   (0)
#define VERSION_MINOR   (1)

#define OPT_STRING      "hd:l:o:r:"
static struct option long_options[] = {
    {"help",        no_argument,        NULL,   'h'},
    {"device",      required_argument,  NULL,   'd'},
    {"loops",       required_argument,  NULL,   'l'},
    {"output",      required_argument,  NULL,   'o'},
    {"framerate",   required_argument,  NULL,   'r'},
    {0,             0,                  NULL,    0 }
};

#define WINDOW_NAME         "WEBCAM"

#define DEFAULT_DEVICE      (1)
#define DEFAULT_FRAMERATE   (10)
#define DEFAULT_FRAMEWIDTH  (352)
#define DEFAULT_FRAMEHEIGHT (288)
#define DEFAULT_LOOPS       (50)
#define DEFAULT_OUTFCC      (CV_FOURCC('M', 'P', '4', 'V'))

static int  device      = DEFAULT_DEVICE;
static int  frameRate   = DEFAULT_FRAMERATE;
static Size inSize      = Size(DEFAULT_FRAMEWIDTH, DEFAULT_FRAMEHEIGHT);
static int  loops       = DEFAULT_LOOPS;
static char *outfile    = NULL;
static int  sleepTime   = 1000000 / DEFAULT_FRAMERATE;
static char *infile     = NULL;

void
usage(char* name)
{
    cout << name << " version " << VERSION_MAJOR << "." << VERSION_MINOR                        << endl;
    cout << "Reads video from input file and display it in a SDL window."                       << endl;
    cout << "Usage: " << name << " [options] <input_file>"                                      << endl;
    cout << ""                                                                                  << endl;
    cout << "Options:"                                                                          << endl;
    cout << "  -h, --help            Prints this help"                                          << endl;
    cout << ""                                                                                  << endl;
    cout << "  -d, --device <n>      Index of video device to use (defaults to " << DEFAULT_DEVICE 
        << ")" << endl;
    cout << "  -l, --loops <n>       Number of loops (defaults to " << DEFAULT_LOOPS << ")"     << endl;
    cout << "  -o, --output <file>   Save output to file\n"                                     << endl;
    cout << "  -r, --framerate <n>   Set framerate (defaults to " << DEFAULT_FRAMERATE << ")"   << endl;
    cout << ""                                                                                  << endl;
}

SDL_Surface*
convert_to_SDLSurface(const Mat& frame) {
    IplImage img = (IplImage)frame;
    return SDL_CreateRGBSurfaceFrom((void*)img.imageData, img.width, img.height, 
            img.depth*img.nChannels, img.widthStep, 0xff0000, 0x00ff00, 0x0000ff, 0);
}

int
main(int argc, char** argv )
{
    char opt;
    uint32_t start;
    bool end = false;
    string outfileName;
    SDL_Event event;
    SDL_Surface *screen;
    SDL_Surface *img;

    // Parse args
    while( (opt=getopt_long(argc, argv, OPT_STRING, long_options, NULL))>0 ) {
        switch(opt)  {
            case 'h':
                usage(argv[1]);
                return 0;
                break;
            case 'd':
                istringstream(optarg) >> device;
                break;
            case 'l':
                istringstream(optarg) >> loops;
                break;
            case 'o':
                outfile = optarg;
                break;
            case 'r':
                istringstream(optarg) >> frameRate;
                sleepTime = 1000000 / frameRate;
                break;
            default:
                break;
        }
    }
    // Get input file
    if(optind>0 && optind<argc) {
         if( strcmp(argv[optind], "-")==0 ) {
            if( (infile=(char*)malloc(16))==NULL ) {
                cerr << "malloc(): error" << endl;
                return -1;
            }
            strcpy(infile, "/dev/stdin");
         } else {
            infile = argv[optind];
         }
    } else {
        cerr << "Please specify input file." << endl;
        return -1;
    }
    
    // Dump configuration
    cout << "inputfile="    << infile       << endl;
    cout << "framerate="    << frameRate    << endl;
    cout << "loops="        << loops        << endl;
    cout << "sleeptime="    << sleepTime    << endl;
    cout << "device="       << device       << endl;

    // Open device
    VideoCapture cap(infile);
    cap.set(CV_CAP_PROP_FPS,            (double)frameRate);
    cap.set(CV_CAP_PROP_FRAME_WIDTH,    (double)inSize.width);
    cap.set(CV_CAP_PROP_FRAME_HEIGHT,   (double)inSize.height);
    if(!cap.isOpened()) {
        cerr << "Cannot open input file" << endl;
        return -1;
    }

    // Open output file
    // WTF !!!! OPENCV NE SUPPORTE PAS LES PIPES !!!!!
    if( outfile ) {
        VideoWriter outVideo(outfileName, DEFAULT_OUTFCC, frameRate, inSize, true);
        if(!outVideo.isOpened()) {
            cerr << "Cannot open output file." << endl;
            return -1;
        }
    }

    // Configure SDL
    if( SDL_Init(SDL_INIT_VIDEO)==-1 ) {
        cerr << "Cannot init SDL." << endl;
        return -1;
    }
    atexit(SDL_Quit);
    screen = SDL_SetVideoMode(inSize.width, inSize.height, 16, SDL_HWSURFACE);
    if(screen==NULL) {
        cerr << "Cannot create SDL surface." << endl;
        return -1;
    }

    // Get frames and write them to output file
    Mat frame;
    //namedWindow(WINDOW_NAME, CV_WINDOW_AUTOSIZE);
    for(int i=0 ; i<loops && !end ; ++i) {
        start = SDL_GetTicks();
        while( SDL_PollEvent(&event) ) {
            switch(event.type) {
                case SDL_QUIT:
                    end=true;
                    break;
                case SDL_KEYDOWN:
                    switch(event.key.keysym.sym) {
                        case SDLK_ESCAPE:
                            end=true;
                            break;
                        default:
                            break;
                    }
                    break;
                default:
                    break;
            }
        }

        cap >> frame;
        if(frame.empty()) {
            cerr << "Error reading frame" << endl;
            continue;
        }
        img = convert_to_SDLSurface(frame);
        SDL_BlitSurface(img, NULL, screen, NULL);
        SDL_FreeSurface(img);
        SDL_Flip(screen);

        cout << "\rframes=" << i; 
        cout.flush();
        //imshow(WINDOW_NAME, frame);
        //updateWindow(WINDOW_NAME);
        //waitKey(10); // MANDATORY when displaying image with openCV => used to update window

        // Regulate FPS
        if(1000.0/frameRate > SDL_GetTicks()-start) {
            SDL_Delay(1000.0/frameRate - (SDL_GetTicks()-start));
        }
    }

    return 0;
}









