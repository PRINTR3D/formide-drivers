# Formide-Drivers
 
Formide-Drivers is the software used by Formide to connect with a 3D printer and interact with it. It's developed in C++ for Marlin and Repetier firmwares.

## Features
* Emission of events when connecting / disconnecting printers & more.
* Control many printers at the same time.
* Send gcode commands.
* Print Gcode files.
* Pause / Resume / Stop prints
* Check printer status (temperatures, progress, coordinates, current layer, etc).
* Read communication logs.

# Documentation
Please find all the documentation in this project's [wiki](https://github.com/PRINTR3D/formide-drivers/wiki).

# Installation
There are pre-built binaries of this project.
Installation can be done with `npm`:
```
npm install --save formide-drivers
```


# Build
To build from source, follow these instructions:

* Clone this repository.
* run `npm install` to install dependencies.
* Clean and build using `node-gyp rebuild` or `node-pre-gyp rebuild`.

The output is a binary called Formidriver.node located at `formide-drivers/build/Release`. You can import it and include it in your application.


# Integration
Formide-drivers is a NodeJS Addon, so integrating it in your project is very easy.
- See a minimal example of implementation [here][1].
- See a real implementation case [here][2] (formide-client-2).


# Contributing
You can contribute to `formide-drivers` by closing issues (via fork -> pull request to development), adding features or just using it and report bugs!
Please check the issue list of this repo before adding new ones to see if we're already aware of the issue that you're having.


# Credits
- [Doodle3D](https://github.com/doodle3d): [print3d](https://github.com/doodle3d/print3d)


# Licence
Please check LICENSE.md for licensing information.



[1]:https://github.com/PRINTR3D/formide-drivers/blob/master/examples/FormidriverFunctions.js
[2]:https://github.com/PRINTR3D/formide-client-2/tree/master/src/core/drivers
