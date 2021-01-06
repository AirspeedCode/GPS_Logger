# GPS Logger

An Arduino based GPS data logger using the U-Blox NEO-M8N GPS receiver module.
Saves the data to a micro SD card in a .csv file format which can be opened in Excel, etc. <br>
Convert data to .KMZ format for viewing in Google Earth using GPS Visualiser, here: <br>
https://www.gpsvisualizer.com/map_input?form=googleearth

![Image of GPS Logger prototype](https://github.com/AirspeedCode/GPS_Logger/blob/master/gps_proto.jpg)

# CSV Data File Format
![Image of CSV file format](https://github.com/AirspeedCode/GPS_Logger/blob/master/csv_format.PNG)

# Problems
1. SD Card error sometimes triggered when creating a new folder for the day. This only seems to happen if there is already a folder in existence. If the SD card is previously empty, the folder is ceated without any issues.

# TO DO
1. Fix SD card error
2. Put it in a box
