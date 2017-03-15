lasershark_hostapp
===================

###  To install this onto a Raspberry Pi, run _install.sh_ from a terminal.

## Programs:
**lasershark_jack**
LaserShark USB ShowCard Host Application. Allows LaserShark boards to be controlled by applications that use the JACK audio backbone via ISO transfers.

**lasershark_twostep**
LaserShark TwoStep Host Application. Demonstrates control of a TwoStep board connected to a LaserShark board's UART.  This can be used to control a stepper motor for Z-axis control when SLA printing.

**lasershark_stdin**
LaserShark USB ShowCard Host Application. Piping commands to this application as described in lasershark_stdin_input_example.txt will allow a LaserShark board to be controlled via BULK transfers.

**lasershark_stdin_displayimage**
Application intended to be piped into the lasershark_stdin application.  This application will render a raster of a .png image that is less than or equal to 4096 x 4096 in size.  Useful for exposing layers of resin while SLA printing.

**lasershark_stdin_gridmaker**
Application intended to be piped to the lasershark_stdin application.  This application will generate a grid pattern over the functional area of the output.  Intended to be used for calibration.

**lasershark_stdin_circlemaker**
Example application intended to be piped to the lasershark_stdin application. Commands output by this application will generate a circle.



An example command:
`./lasershark_stdin_displayimage -m -p 28.png -r 20000 | ./lasershark_stdin`


Please see the following for more details:

http://macpod.net/electronics/lasershark/lasershark.php
http://macpod.net/electronics/twostep/twostep.php

This code is released under the GPL V2 
