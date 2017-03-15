/*
lasershark_stdin_gridmaker.c - Application that draws a grid pattern intended
for consumption by lasershark_stdin.

2017 Mason Stone

This file is part of Lasershark's USB Host App.

Lasershark is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

Lasershark is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Lasershark. If not, see <http://www.gnu.org/licenses/>.
*/
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>
#include "./getopt_portable.h"

#define MAX_SIZE 4096

void print_help(const char* prog_name, FILE* stream)
{
    fprintf(stream, "%s [OPTIONS] - Displays a grid via LaserShark\n", prog_name);
    fprintf(stream, "\t-h");
    fprintf(stream, "\tPrint this help text\n");
    fprintf(stream, "\t-n");
    fprintf(stream, "\tNumber of lines to display on each axis\n");
    fprintf(stream, "\t-x");
    fprintf(stream, "\tWidth of grid (max 4096). Default: 4096\n");
    fprintf(stream, "\t-y");
    fprintf(stream, "\tHeight of grid (max 4096). Default: 4096\n");
    fprintf(stream, "\t-r");
    fprintf(stream, "\tRate to display samples at (1 to 30,000). Default: 1000\n");
}

static uint16_t float_to_lasershark_xy(float var, unsigned int size)
{
    unsigned int offset = (MAX_SIZE-size)/2;
    uint16_t val = ((size - 1) * (var + 1.0)/2.0);
    return val + offset;
}


int main (int argc, char *argv[])
{
    double x_f = -1, y_f = -1;

    unsigned int numLines = 6;
    unsigned int x_size = MAX_SIZE;
    unsigned int y_size = MAX_SIZE;
    unsigned int refreshRate = 1000;

    int c;

    int nflag = 0;
    int xflag = 0;
    int yflag = 0;
    int rflag = 0;


    // parsing the flags
    opterr_portable = 1;
    while (-1 != (c = getopt_portable(argc, argv, "h:n:x:y:r:"))) {
        switch(c) {
        case 'h': // help
            print_help(argv[0], stdout);
            return 1;
        case 'n': // set number of grid lines
            nflag++;
            numLines = atoi(optarg_portable);
            break;
        case 'x': // set width of grid
            xflag++;
            x_size = atoi(optarg_portable);
            break;
        case 'y': // set height of grid
            yflag++;
            y_size = atoi(optarg_portable);
            break;
        case 'r': // set refresh rate
            rflag++;
            refreshRate = atoi(optarg_portable);
            break;
        default: // entered a non-sanctioned flag
            print_help(argv[0], stderr);
            return 1;
        }
    }


    // error handling
    if (nflag > 1 || xflag > 1 || yflag > 1 || rflag > 1) {
        fprintf(stderr, "Cannot specify flags more than once.\n");
        print_help(argv[0], stderr);
        return 1;
    }

    if (x_size < 0 || x_size > MAX_SIZE) {
        fprintf(stderr, "Width must be between 0 and %i\n", MAX_SIZE);
        print_help(argv[0], stderr);
        return 1;
    }

    if (y_size < 0 || y_size > MAX_SIZE) {
        fprintf(stderr, "Height must be between 0 and %i\n", MAX_SIZE);
        print_help(argv[0], stderr);
        return 1;
    }

    if (refreshRate < 0 || refreshRate > 30000) {
        fprintf(stderr, "Rate must be between 1 and 30,000\n");
        print_help(argv[0], stderr);
        return 1;
    }


//////////////////////////////////////////////////////////////////////////
// running the code
//////////////////////////////////////////////////////////////////////////

    float step = 2.0/(numLines-1); // step size for each line (go from -1 to +1)

    printf("r=%i\n", refreshRate); // set refresh rate
    printf("e=1\n"); // start the stream

    while(1) {  // main loop
        while(x_f <= (1+step)) { // display vertical lines
            printf("s=%u,%u,%u,%u,%u,%u\n", 
                float_to_lasershark_xy(x_f, x_size), float_to_lasershark_xy(y_f, y_size), 0,0,0,0); // x, y, a, b, c, intl_a
            y_f = -y_f; // span across
            printf("s=%u,%u,%u,%u,%u,%u\n",
                float_to_lasershark_xy(x_f, x_size), float_to_lasershark_xy(y_f, y_size), 4095,4095,1,1); // x, y, a, b, c, intl_a
            x_f += step; // next line
        }

        x_f = 1; // because it will already be over there
        y_f = -1; // reset for vertical lines

        while(y_f <= (1+step)) { // display horizontal lines
            printf("s=%u,%u,%u,%u,%u,%u\n",
                float_to_lasershark_xy(x_f, x_size), float_to_lasershark_xy(y_f, y_size), 0,0,0,0); // x, y, a, b, c, intl_a
            x_f = -x_f; // span across
            printf("s=%u,%u,%u,%u,%u,%u\n",
                float_to_lasershark_xy(x_f, x_size), float_to_lasershark_xy(y_f, y_size), 4095,4095,1,1); // x, y, a, b, c, intl_a
            y_f += step; // next line
        }

        y_f = -1; // reinitialize
        x_f = -1; // reinitialize
    }

    // These should really be stuck at the end of the output.. but this is a loop.
    //printf("f=1\n");
    //printf("e=0\n");
    return 0;
}
