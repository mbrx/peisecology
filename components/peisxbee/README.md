# peisxbee Installation Guide
The following installation guide has been tested on Ubuntu 12.04.5 LTS.

*peisxbee requires PEIS middleware to be installed.*

1. Download the WearAmI package from the GitHub repository **bbbruno/WearAmI**
2. Move it to the desired folder (e.g. 'Documents/') and extract it
3. Open a terminal window and issue the following commands:
4. `sudo apt-get install cmake`
5. `cd [extraction_folder]/WearAmI/PEIS_tools/peisxbee/libxbee`
6. `cmake .`
7. `make`
8. `sudo make install`
9. `cd ..`
10. `cmake .`
11. `make`

To run, open a terminal window and issue the following commands:

1. `cd [extraction_folder]/peisxbee`
2. `./peisxbee`

