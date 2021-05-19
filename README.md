The Grid

This program produces a text file in "Well Known Text" format for polygons to
draw grids with graphical GIS applications such as QGIS.

For program usage, please see "grid-howto" file in this repository.

Well Known Text (WKT) format is a standard format originally defined by the Open
Geospatial Consortium (OGC), it is a text markup language for representing vector
geometry objects like "Point" and "Polygon". See the Wikipedia page:
https://en.wikipedia.org/wiki/Well-known_text_representation_of_geometry

Order and organization make life easier, most people know the Cartesian grid.
The grid structure is an essential tool in mapping. There are a lot of ways to
draw a grid, most need a database or the use of sophisticated programs and most
importantly the process is time consuming. This repository aims to make drawing
a grid a little easier, it is not a point and click solution, it does take some
effort from the user. 

This program uses Open Street Map (OSM) data retrieved with queries to Overpass
server utilizing code from my overpass-c-queries repository particularly function
curlGetXrdsGPS(). https://github.com/waelhammoudeh/overpass-c-queries.

In QGIS "Well Known Text" format is called "Delimited Text"! QGIS allows users
to add layers from "Delimited Text". You can reach that from the top menu with:

 * Layer
    * Add Layer
       * Add Delimited Text Layer... Ctrl+Shift+T
       
By default QGIS draws polygons with a "Solid Color Fill". This setting can be
changed from the layer "Layer Properties" sheet/page, use "Transparent Fill" to
see your other layers.

Program output example in "Well Known Text" format with three polygons:

wkt;
"POLYGON ((-112.0739105 33.5597757, -112.0652852 33.5605235, -112.0652213 33.5527166, -112.0739008 33.5528019, -112.0739105 33.5597757))"
"POLYGON ((-112.0739008 33.5528019, -112.0652213 33.5527166, -112.0651409 33.5455279, -112.0738334 33.5455559, -112.0739008 33.5528019))"
"POLYGON ((-112.0738334 33.5455559, -112.0651409 33.5455279, -112.0650791 33.5382827, -112.0737604 33.5383198, -112.0738334 33.5455559))"

About the program c code:

1) Program uses GNU getopt_long() function to parse the command line arguments.
   Program output to stdout (screen) unless output option is used.
   
2) Program has one required argument which is the input file or a list of input
files separated by space.

3) program takes the following optional arguments:
    - help to show help text.
    - verbose to provide more information about its progress. (Not implemented)
    - output option to specify output file name.
    - force to force use existing file as an output file.
    
Program accepts and processes two text file formats, not mixing them. For that
an enumerated data type is defined in file mkWKT.h with:

typedef enum INFORMAT_ { // input file format

    UNKNOWN_FORMAT = 0,
    FOUR_CORNERS,
    ROWxCOL

}INFORMAT;

Two dimensional arrays are used to process points and polygons. C pointers usage
with 2D array is illustrated.

The program starts in the main() function in file mk-grid.c, the file has a lot
of white space and comments, it is really short and not as long as it seems, the 
main() function is simple and straight forward.

See README.structures file (incomplete) please.
