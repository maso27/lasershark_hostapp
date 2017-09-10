/*
fullprint.c - G-code interpreter.
Uses Lasershark to draw each layer - using lasershark_stdin.
Then drives a connected 3D printer board to move the z-axis.

2017 Mason Stone

*/

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <ctype.h>
#include <math.h>
#include <unistd.h>
#include "getopt_portable.h"

#define MIN_VAL 0
#define MAX_VAL 4095

#define RES_MAX 4096

#define PORTNAME "/dev/ttyUSB0" // name of connected 3D printer board for Z control
#define BAUDRATE B115200
#define FILENAME "gcode.gcode"  // default gcode file to parse

#define Z_DELAY 10              // amount of time to delay during any Z-moves
#define HOME_DELAY 5            // amount of time to delay while homing Z

#define LAYER_STICK 3           // number of layers to overexpose for stickiness
#define OVER_EXPOSE 3 // 4          // amount to slow the laser while overexposing early layers

int nowX = 0;                   // current x-location of laser
int nowY = 0;                   // current y-location of laser
float nowE = 0;                 // current extruder setting (used for tracking laser on/off)
float x_scalar = 1;             // how much to scale the x-axis -- should probably stay at 1
float y_scalar = 0.972;             // how much to scale the y-axis -- should probably stay at 1
double e_factor = 2;            // exponent of distortion correction -- should probably stay at 2
double m_factor = 0;            // how much distortion correction to apply
unsigned int a_min = MIN_VAL;   // minimum value for analog drive a-port on lasershark
unsigned int a_max = MAX_VAL;   // maximum value for analog drive a-port on lasershark
unsigned int b_min = MIN_VAL;   // minimum value for analog drive b-port on lasershark
unsigned int b_max = MAX_VAL;   // maximum value for analog drive b-port on lasershark
float dimension = 110.485;          // size of full-scale x and y dimensions on print bed
int abs_pos = 1;                // absolute positioning by default
int fd;                         // the serial buffer to be output
int lineNum = 0;                // the rolling line number for the serial output
int superStick = 0;             // tracking whether we are overexposing right now or not

void print_help(const char* prog_name, FILE* stream)
{
    fprintf(stream, "%s [OPTIONS] - Draws a G-Code file via LaserShark\n", prog_name);

    fprintf(stream, "Options:\n");
    fprintf(stream, "\t-h");
    fprintf(stream, "\tPrint this help text.\n");

    fprintf(stream, "Amplitudes of analog outputs:\n (default is max scale from 0 to 4095)\n");
    fprintf(stream, "\t-a\tA-channel minimum value.\n");
    fprintf(stream, "\t-A\tA-channel maximum value.\n");
    fprintf(stream, "\t-b\tB-channel minimum value.\n");
    fprintf(stream, "\t-B\tB-channel maximum value.\n");

    fprintf(stream, "Dimensions:\n (default is 127mm)\n");
    fprintf(stream, "\t-D\t Square dimensions of full-scale print area in mm.\n");

    fprintf(stream, "Scaling:\n (default is 1)\n");
    fprintf(stream, "\t-X\tAmount to scale x-axis.\n");
    fprintf(stream, "\t-Y\tAmount to scale y-axis.\n");

    fprintf(stream, "Distortion Correction:\n (do not use: broken for now)\n");
    fprintf(stream, "\t-E\tE-Factor correction (default is 2).\n");
    fprintf(stream, "\t-M\tM-Factor correction (default is 0).\n");

    fprintf(stream, "File input:\n (default is ./gcode.gcode)\n");
    fprintf(stream, "\t-f\tG-Code file to draw.\n");

    fprintf(stream, "Sweep speed:\n (default is 20000)\n");
    fprintf(stream, "\t-r\tRate to display samples at. Must be between 1 and 30,000\n");
}

// used for serial connection to motor drivers
int set_interface_attribs(int fd, int speed)
{
    struct termios tty;

    if (tcgetattr(fd, &tty) < 0) {
        printf("Error from tcgetattr: %s\n", strerror(errno));
        return -1;
    }

    cfsetospeed(&tty, (speed_t)speed);
    cfsetispeed(&tty, (speed_t)speed);

    tty.c_cflag |= (CLOCAL | CREAD);    // ignore modem controls
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;         // 8-bit characters
    tty.c_cflag &= ~PARENB;     // no parity bit
    tty.c_cflag &= ~CSTOPB;     // only need 1 stop bit
    tty.c_cflag &= ~CRTSCTS;    // no hardware flowcontrol

    // setup for non-canonical mode
    tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
    tty.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
    tty.c_oflag &= ~OPOST;

    // fetch bytes as they become available
    tty.c_cc[VMIN] = 1;
    tty.c_cc[VTIME] = 1;

    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        printf("Error from tcsetattr: %s\n", strerror(errno));
        return -1;
    }
    return 0;
}

// used for serial connection to motor drivers
void set_mincount(int fd, int mcount)
{
    struct termios tty;

    if (tcgetattr(fd, &tty) < 0) {
        printf("Error tcgetattr: %s\n", strerror(errno));
        return;
    }

    tty.c_cc[VMIN] = mcount ? 1 : 0;
    tty.c_cc[VTIME] = 5;        /* half second timer */

    if (tcsetattr(fd, TCSANOW, &tty) < 0)
        printf("Error tcsetattr: %s\n", strerror(errno));
}

// return the nearest integer, rounding up or down
int roundNum(double num)
{
    return num < 0 ? num - 0.5 : num + 0.5;
}

// apply scalar factors to values and print them out
// MSS -- e-factor and m-factor processing doesn't seem to work yet
void sendScaled(int x, int y, int a, int b, int c, int intl_a)
{
    int new_x, new_y;
    int i;


    if ((m_factor != 0.0) || (x_scalar != 1) || (y_scalar != 1)) { // if distortion correction has been requested
        int shift_amt = roundNum(RES_MAX / 2); // midway between MIN_VAL and MAX_VAL
        int x_shifted = x - shift_amt;
        int y_shifted = y - shift_amt;

        if (m_factor != 0.0) { // apply distortion correction
            x_scalar *= 1 - (m_factor * pow(abs(y_shifted),e_factor));  // x = M * y^e parabola , removed from the scalar
//            y_scalar *= 1 - (m_factor * pow(abs(x_shifted),e_factor));  //  (Note that M should be tiny)
        }

        new_x = roundNum(x_shifted * x_scalar) + shift_amt; // have to center it before multiplying, then shift back
        new_y = roundNum(y_shifted * y_scalar) + shift_amt;
    }
    else {
        new_x = x;
        new_y = y;
    }

    if ((new_x >= 0) && (new_x <= RES_MAX) && (new_y >= 0) && (new_y <= RES_MAX)) {
        if (superStick) // early layers want more exposure to stick better
            for (i=1; i < OVER_EXPOSE; i++) // make the laser stay at its current spot longer to overexpose the layer
                printf("s=%u,%u,%u,%u,%u,%u\n",
                    new_x, new_y, a, b, c, intl_a); // x, y, a, b, c, intl_a
        printf("s=%u,%u,%u,%u,%u,%u\n",
            new_x, new_y, a, b, c, intl_a); // x, y, a, b, c, intl_a
    }

    return;
}

// convert mm to appropriate values within the print surface (uses dimension)
int pixelize(float rawval)
{
    float output;
    output = (rawval * RES_MAX / dimension) + RES_MAX/2; // scale it to the new number system
    return roundNum(output);
}

// the g-code parser
void moveLaser(int newX, int newY, int laserOn)
{
    int deltaX = newX - nowX;
    int deltaY = newY - nowY;
    int dirX = (deltaX > 0) ? 1 : -1;
    int dirY = (deltaY > 0) ? 1 : -1;
    int a = (laserOn) ? a_max : a_min;
    int b = (laserOn) ? b_max : b_min;
    int c = laserOn;

    deltaX = abs(deltaX);
    deltaY = abs(deltaY);

    int i;
    int over = 0;

    if(deltaX > deltaY) {
        for(i=0;i < deltaX;++i) {
            nowX += dirX;

            sendScaled(//"s=%u,%u,%u,%u,%u,%u\n",
             nowX, nowY, a, b, c, 1); // x, y, a, b, c, intl_a

            over+=deltaY;
            if(over>=deltaX) {
                over-=deltaX;
                nowY += dirY;

                sendScaled(//"s=%u,%u,%u,%u,%u,%u\n",
                 nowX, nowY, a, b, c, 1); // x, y, a, b, c, intl_a

            }
            // pause(step_delay); // step_delay is a global connected to feed rate.
            // test limits and/or e-stop here
        }
    } else {
        for(i=0;i < deltaY;++i) {
            nowY += dirY;

            sendScaled(//"s=%u,%u,%u,%u,%u,%u\n",
             nowX, nowY, a, b, c, 1); // x, y, a, b, c, intl_a

            over+=deltaX;
            if(over>=deltaY) {
                over-=deltaY;
                nowX += dirX;

                sendScaled(//"s=%u,%u,%u,%u,%u,%u\n",
                 nowX, nowY, a, b, c, 1); // x, y, a, b, c, intl_a

             }
         }
    }
}

// send the z information out the serial port and wait for acknowledgment
void sendSerial(char *lineOut, int waitTime) { // (float zVal, float feedrate) {
    unsigned char buf[80];
    int rdlen;
    int wlen;

    sendScaled(nowX, nowY, a_min, b_min, 0, 1); // turn off the laser

    lineNum++;

    wlen = write(fd, lineOut, strlen(lineOut));
    if (wlen != strlen(lineOut)) {
        printf("Error from write: %d, %d\n", wlen, errno);
        exit(1);
    }
    tcdrain(fd); // delay for output

    do { // wait for "ok xxx"
        rdlen = read(fd, buf, sizeof(buf) - 1);
        if (rdlen > 0) {
            buf[rdlen] = 0;
        } else if (rdlen < 0) {
            printf("Error from read: %d: %s\n", rdlen, strerror(errno));
        }
        if (strstr(buf, "Resend")) {
            write(fd, lineOut, strlen(lineOut));
            tcdrain(fd);
        }
    } while ((strstr(buf, "ok") == NULL) && (strstr(buf, "wait") == NULL)); //  || (strstr(buf, itoa(lineNum)) == NULL));

    sleep(waitTime);
}

// interpret the latest line of g-code
void parseLine(char *thisLine)
{
    char command[5];
    float commandVal[5];
    int newX = nowX;
    int newY = nowY;
    float newE = nowE;
    int laserOn = 0;
    float zVal;
    float feedrate = 0;
    int sendZMove = 0;
    int i;

    // initialize to zero
    for (i=0; i < 5; i++) {
        command[i] = 0;
        commandVal[i] = 0;
    }

    sscanf(thisLine, "%c%f %c%f %c%f %c%f %c%f %c%f", &command[0], &commandVal[0], &command[1], &commandVal[1], &command[2], &commandVal[2], &command[3], &commandVal[3], &command[4], &commandVal[4], &command[5], &commandVal[5]);

    if (toupper(command[0]) == 'G') // a G command has been sent
    {
        if (commandVal[0] <= 1)     // G0 or G1 means "move"
        {
            for (i=1; (i < 5) && (command[i] != ';'); i++) // get out if there's a comment
            {
                switch(toupper(command[i])) {
                case 'X': // new X location
                    newX = pixelize(commandVal[i]);
                    if (!abs_pos) newX += nowX;
                    break;
                case 'Y': // new Y location
                    newY = pixelize(-commandVal[i]);
                    if (!abs_pos) newY += nowY;
                    break;
                case 'Z': // there is a z-move.  Store the z-value and flag to send it over serial
                    zVal = commandVal[i];
                    sendZMove = 1;
                    break;
                case 'F': // feedrate.  Store this in case of z-move (not used with laser)
                    feedrate = commandVal[i];
                    break;
                case 'E': // extrusion setting. Turn on the laser if this is a forward extrusion
                            // note - only absolute mode is currently supported. Need a plan for relative.
                    newE = commandVal[i];
                    if (newE > nowE) laserOn = 1;
                    nowE = newE;
                    break;
                default: // Nothing of use.
                    break;
                }
            }

            if (sendZMove) { // if a z-move was specified
                if (superStick)
                    sendSerial(thisLine, Z_DELAY * 3); // send the new Z value and wait (longer for first layers)
                else
                    sendSerial(thisLine, Z_DELAY); // send the new z value and the feedrate out serially
            }

            else { // the actual "move the laser" command
                moveLaser(newX, newY, laserOn);
            }
        }

        else if (commandVal[0] == 28) { // home command
            sendScaled(nowX, nowY, a_min, b_min, 0, 1); // turn off the laser
            sendSerial(thisLine, HOME_DELAY);
        }

        else if (commandVal[0] == 90) { // set to absolute positioning
            abs_pos = 1;
            sendSerial(thisLine, 0);
        }

        else if (commandVal[0] == 91) { // set to relative positioning
            abs_pos = 0;
            sendSerial(thisLine, 0);
        }

        else if (commandVal[0] == 92) { // resetting the extruder position
            for (i=1; (i < 5) && (command[i] != ';'); i++) // get out if there's a comment
            {
                if (toupper(command[i]) == 'E')
                    nowE = commandVal[i];
            }
        }
    }
    else if (toupper(command[0]) == 'M') // an M command has been sent
//    {
//        if (commandVal[0] == 18) // M18 means turn off power to motors
            sendSerial(thisLine, 0); // send it to the RAMPS board
//    }

    else if (toupper(command[0]) == ';') // catch first layer (must be set up in slic3r)
    {
	if ((toupper(command[1]) == 'L') && (commandVal[1] < LAYER_STICK)) superStick = 1; // look for first LAYER_STICK layers
        else if (toupper(command[1]) == 'L') superStick = 0;
    }
    return;
}

int main (int argc, char *argv[])
{
    int c; // options sent in at command prompt
    int rate = 20000;

    char * path = FILENAME; // the path of the g-code file passed in at the command prompt
    FILE * file; // the contents of the g-code file passed in at command prompt
    char thisLine[60]; // the current line we are parsing
    unsigned char lineOut[80]; // the string to be sent out the serial port
    char * portname = PORTNAME;
    unsigned char buf[80];
    int wlen;
    int rdlen;

    int aflag = 0;
    int Aflag = 0;
    int bflag = 0;
    int Bflag = 0;
    int Dflag = 0;
    int Xflag = 0;
    int Yflag = 0;
    int Mflag = 0;
    int Eflag = 0;
    int fflag = 0;
    int rflag = 0;
    int hflag = 0;
    int pflag = 0;

    opterr_portable = 1;
    while (-1 != (c = getopt_portable(argc, argv, "a:A:b:B:hD:X:Y:M:E:f:p:r:"))) { // parsing the command line variables
        switch(c) {
        case 'a':
            aflag++;
            a_min = atoi(optarg_portable);
            break;
        case 'A':
            Aflag++;
            a_max = atoi(optarg_portable);
            break;
        case 'b':
            bflag++;
            b_min = atoi(optarg_portable);
            break;
        case 'B':
            Bflag++;
            b_max = atoi(optarg_portable);
            break;
        case 'h':
            hflag++;
            break;
        case 'D':
            Dflag++;
            dimension = atof(optarg_portable);
            break;
        case 'X':
            Xflag++;
            x_scalar = atof(optarg_portable);
            break;
        case 'Y':
            Yflag++;
            y_scalar = atof(optarg_portable);
            break;
        case 'M':
            Mflag++;
            m_factor = atof(optarg_portable)/100000000;
            break;
        case 'E':
            Eflag++;
            e_factor = atof(optarg_portable);
            break;
        case 'f':
            fflag++;
            path = optarg_portable;
            break;
        case 'p':
            pflag++;
            portname = optarg_portable;
            break;
        case 'r':
            rflag++;
            rate = atoi(optarg_portable);
            break;
        default:
            print_help(argv[0], stderr);
            exit(1);
        }
    }

    // error handling
    if (aflag > 1 || Aflag > 1 || bflag > 1 || Bflag > 1 ||
            hflag > 1 || Dflag > 1 || Xflag > 1 || Yflag > 1
            || Mflag > 1 || Eflag > 1 || rflag > 1 || fflag > 1 || pflag > 1) {
        fprintf(stderr, "Cannot specify flags more than once.\n");
        print_help(argv[0], stderr);
        exit(1);
    }

/*    if (!fflag) {
        fprintf(stderr, "Must specify file to draw.\n");
        print_help(argv[0], stderr);
        exit(1);
    }*/

    if (a_min < MIN_VAL || a_min > MAX_VAL || a_max < MIN_VAL || a_max > MAX_VAL) {
        fprintf(stderr, "A-channel min and max must be between %d and %d\n", MIN_VAL, MAX_VAL);
        print_help(argv[0], stderr);
        exit(1);
    }

    if (a_min > a_max) {
        fprintf(stderr, "A-channel min cannot be greater than the A-channel max\n");
        print_help(argv[0], stderr);
        exit(1);
    }

    if (b_min < MIN_VAL || b_min > MAX_VAL || b_max < MIN_VAL || b_max > MAX_VAL) {
        fprintf(stderr, "B-channel min and max must be between %d and %d\n", MIN_VAL, MAX_VAL);
        print_help(argv[0], stderr);
        exit(1);
    }

    if (b_min > b_max) {
        fprintf(stderr, "B-channel min cannot be greater than the B-channel max\n");
        print_help(argv[0], stderr);
        exit(1);
    }

    if (Mflag || Eflag) {
        if (m_factor * pow(abs(MAX_VAL/2),e_factor) > 1) {
            fprintf(stderr, "M and E factors will result in failure. Try Different Values.\n");
            exit(1);
        }
    }

    if (rate < 0 || rate > 30000) {
        fprintf(stderr, "Rate must be between 1 and 30,000\n");
        print_help(argv[0], stderr);
        exit(1);
    }

    if (hflag) {
        print_help(argv[0], stdout);
        exit(1);
    }


    // open the serial port
    fd = open(portname, O_RDWR | O_NOCTTY | O_SYNC);
    if (fd < 0) {
        printf("Error opening %s: %s\n", portname, strerror(errno));
        return -1;
    }
    //baudrate 115200, 8 bits, no parity, 1 stop bit
    set_interface_attribs(fd, BAUDRATE);

    // initialize the serial port
    sprintf(lineOut, "N%d M110 N%d\n", lineNum, lineNum);
    wlen = write(fd, lineOut, strlen(lineOut));
    if (wlen != strlen(lineOut)) {
        printf("Error from write: %d, %d\n", wlen, errno);
        return -1;
    }
    tcdrain(fd);  // delay for output
// printf("Sent: %s", lineOut);

    do { // wait for "wait"
        rdlen = read(fd, buf, sizeof(buf) - 1);
        if (rdlen > 0) {
            buf[rdlen] = 0;
// printf("Read: %s\n", buf);
        } else if (rdlen < 0) {
            printf("Error from read: %d: %s\n", rdlen, strerror(errno));
        }
    } while (strstr(buf, "wait") == NULL);

    do { // wait for second "wait"
        rdlen = read(fd, buf, sizeof(buf) - 1);
        if (rdlen > 0) {
            buf[rdlen] = 0;
// printf("Read: %s\n", buf);
        } else if (rdlen < 0) {
            printf("Error from read: %d: %s\n", rdlen, strerror(errno));
        }
    } while (strstr(buf, "wait") == NULL);

    wlen = write(fd, lineOut, strlen(lineOut)); // set initial line number to 0 again
    if (wlen != strlen(lineOut)) {
        printf("Error from write: %d, %d\n", wlen, errno);
        return -1;
    }
    tcdrain(fd);  // delay for output
// printf("Sent: %s", lineOut);

    do { // wait for "ok"
        rdlen = read(fd, buf, sizeof(buf) - 1);
        if (rdlen > 0) {
            buf[rdlen] = 0;
// printf("Read: %s\n", buf);
        } else if (rdlen < 0) {
            printf("Error from read: %d: %s\n", rdlen, strerror(errno));
        }
        if (strstr(buf, "Resend")) {
            write(fd, lineOut, strlen(lineOut)); // resend
            tcdrain(fd);
// printf("Sent: %s", lineOut);
        }
    } while (strstr(buf, "ok") == NULL);

    // open the g-code file
    file = fopen (path, "r");
    if (file == NULL) {
        fprintf(stderr, "Error: can't open file.\n");
        return 1;
    }
    
    // setup
    printf("r=%d\n", rate);
    printf("e=1\n");

    // loop through, looking for parse-able commands
    while (fgets(thisLine, 60, file)!=NULL) {
        parseLine(thisLine);
    }

    fclose(file);
    close(fd);

    // closing
    printf("f=1\n");
    printf("e=0\n");

    popen("wall Print Done.", "r"); // send message to everyone that the print is complete.

    return 0;
}

