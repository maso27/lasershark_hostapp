lasershark_hostapp
===================
Please note that using this is likely to be difficult.  In a perfect world, someone with _actual_ programming chops could pick this up and turn it into something beautiful.  For now, it's working well enough for me to run on my Raspberry Pi/RAMPS/Lasershark combo.

Buyer beware!

###  Notes for installation onto a Raspberry Pi are in the _install.sh_ file.

## Programs:
**fullprint** -  Application for parsing g-code as generated by Slic3r to draw each layer and advance the z-axis of the printer.  Note that some default settings can be adjusted within _fullprint.c_, so that they don't need to be re-entered at each run of the command.  In order for this program to function optimally, care must be taken to add special notes within the G-code.  See Slic3r notes below.

**lasershark_jack** - LaserShark USB ShowCard Host Application. Allows LaserShark boards to be controlled by applications that use the JACK audio backbone via ISO transfers.

**lasershark_twostep** - LaserShark TwoStep Host Application. Demonstrates control of a TwoStep board connected to a LaserShark board's UART.  This can be used to control a stepper motor for Z-axis control when SLA printing.

**lasershark_stdin** - LaserShark USB ShowCard Host Application. Piping commands to this application as described in lasershark_stdin_input_example.txt will allow a LaserShark board to be controlled via BULK transfers.

**lasershark_stdin_displayimage** - Application intended to be piped into the lasershark_stdin application.  This application will render a raster of a .png image that is less than or equal to 4096 x 4096 in size.  Useful for exposing layers of resin while SLA printing.  Note: this does not advance the Z-axis

**lasershark_stdin_gridmaker** - Application intended to be piped to the lasershark_stdin application.  This application will generate a grid pattern over the functional area of the output.  Intended to be used for calibration.

**lasershark_stdin_circlemaker** - Example application intended to be piped to the lasershark_stdin application. Commands output by this application will generate a circle.


## Example Commands:
**Printing From G-Code**
`./fullprint -f ../gcodes/ExampleFile.gcode -D 127 | ./lasershark_stdin`

**Raster-Displaying A PNG Image**
`./lasershark_stdin_displayimage -m -p 28.png -r 20000 | ./lasershark_stdin`

## Configuring Slic3r:
Slic3r is designed to extrude filament, and I am basically extruding laser beams onto the build platform any time there is a positive extrusion.  Retraction or no extrusion result in the laser being turned off.
It's sloppy, perhaps, but it's effective.

**Print Settings**
- Fill Density should be 100%.
- Infill before perimeters should be checked, so that the part is cured from inside-out.
- Speed settings are completely ignored.  They can be adjusted within fullprint with _Sweep Speed_.

**Filament Settings**
- Filament settings are largely ignored.  The laser is on or it's off, so that can't be controlled.
- If you are using an exhaust fan connected to your RAMPS, this might be a good place to control that.

**Printer Settings**
- Nozzle Diameter is the effective size of your laser beam.  This will vary depending on your degree of focus etc.  Mine is set to 0.3mm and I have a pretty well focused beam that gets hot if I leave it on too long.
- Custom G-Code -- place the following into "Before layer change G-code":
```
G91 ; set relative positioning
G1 Z5 F50 ; lift bed by 5 mm
G90 ; set absolute positioning
;[layer_z] L[layer_num] ; used by fullprint to over-expose early layers
```
This will lift the print bed away from the surface to un-stick the print, and then tell fullprint what layer you're on.  You can configure fullprint to move more slowly on the first few layers to over-cure and stick to the bed.  


Please see the following for more details:

http://macpod.net/electronics/lasershark/lasershark.php

http://macpod.net/electronics/twostep/twostep.php

This code is released under the GPL V2 
