The Grid

Please see the grid-howto file too

The boggy that does not let you sleep at night.

Order and organization make life easier, most people know the Cartesian grid.
The grid structure is an essential tool in mapping. There are a lot of ways to
draw a grid, most need a database or the use of sophisticated programs and most
importantly the process is time consuming.

This repository aims to make drawing a grid easier, it is not a 
point and click solution, it does take some effort from the user.

In QGIS you can add a layer from a "Well Known Text" formated file.
This program output is a text file in "Well Known Text" format that can be
used to draw a grid on top of OSM quicly using QGIS application and maybe 
other programs as well. This is NOT a point and click slolution, you have to
do some work.

A "Well Known Text" file with three polygons (rectangles):

wkt;
"POLYGON ((-112.0739105 33.5597757, -112.0652852 33.5605235, -112.0652213 33.5527166, -112.0739008 33.5528019, -112.0739105 33.5597757))"
"POLYGON ((-112.0739008 33.5528019, -112.0652213 33.5527166, -112.0651409 33.5455279, -112.0738334 33.5455559, -112.0739008 33.5528019))"
"POLYGON ((-112.0738334 33.5455559, -112.0651409 33.5455279, -112.0650791 33.5382827, -112.0737604 33.5383198, -112.0738334 33.5455559))"
           
We can use the above file to add a layer in QGIS program on top of open street
map as follows:

From top menu: Layer --> Add Layer --> Add Delimited Text Layer
Then in the dialog choose your file as CSV format, change layer name if you
do not want to use your file name then click on the Geometry drop box and 
choose "Polygon".

You can then configure your "Polygon" layer by making it transparent and set it
as the top layer on top of openstreetmap layer. See QGIS documentations please.


So we want to produce the above wkt / csv file.

Details about program:

1) Program uses GNU getopt_long() function to parse the command line arguments.
   Program output to stdout (screen) unless output option is used.
   
2) Program has one required argument which is the input file or a list of input
files separated by space.

3) program takes the following optional arguments:
    - help to show help text.
    - verbose to provide more information about its progress. (Not implemented)
    - output option to specify output file name.
    - force to force use existing file as an output file.
    
Program accepts and processes two text file formats, not mixing them.
The first is a list of four intersection for each polygon, the second is row
and column list where intersections are formed by the program.

Program uses curlGetXrdsGPS() function to query overpass server for GPS.

Two dimenssional arrays are used to process points and polygons. C pointers
usage with 2D array is illustrated.

This tool is not just to draw grid. I will use this same tool to sort out and
correct street names we get from overpass before they go into our database.

The program starts in the main() function in file mk-grid.c, the file has a lot
of white space and comments, it is really short and not as long as it seems, the 
main() function is simple and straight forward.

On entry; main() function sets the "prog_name" global variable to argv[0], the
first argument on the command line, "prog_name" is global so it is available to
functions without having to pass it as an argument. Please note the "extern"
declaration for "prog_name" in the "mk-grid.h" header file which is included in
files where it used and needed. Then main() parses the defined server to check
connection to it,

The main() function uses the GNU getopt_long() function to parse the command
line arguments, it decleares two arrays for that; character array for the short 
options: "shortOptions", and a structure array for long options in "longOptions"
main() does not use any advanced features for getopt_long().
checks server URL for correct format and also whether we
can or cannot connect to it. ... uses getopt_long() ... sets verbose, check all
input files, output is set to output file if specified, we do not over write
existing file unless told to do so with force ....

structured programming; main structures are POINT, RECTANGLE, XROADS (cross roads)
others like DIMENSION, DL_LIST and DL_ELEM. The list is doubly liked list, note 
the list is the pointer to data member is just a pointer, as you use this list
structure stay aware of the pointer as long as you need it, do not over write or
delete it when you still need it. This pointer can be to any thing, know what it
points to all the time, do not mix pointers to defferint  types. Another is the
MATRIX, two dimensional array with an array pointer to some structure and 
dimension, XRDS_MTRX is cross roads matrix with two dimensional array of XROADS
and another member for dimension. XROADS and RECTANGLE structures are the work
horses for this program.

