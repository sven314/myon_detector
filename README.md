# myon_detector
software for a raspberry pi based myon detector system using a ublox gps module for timing.

Final goal:
A software solution that can use a Raspberry Pi mini computer and the Ublox m8 gps modules "timemark" feature together with 
a plastic scintillator + SciPm based detector system to measure myons up to a 50ns time accuracy. 
Therefore the software has to communicate with the ublox gps module through a serial interface using the ubx protocol. 
The high time accuracy is needed because it is planned to use multiple stations for measuring myons.
The software must be easy to use and run in background while synchronizing accumulated data with a server via tcp.
It also has to be remotely controllable via tcp.
