//Examples of functions to the driver

var driver = require('./build/Release/Formidriver');

var moveRight = "G1 X40";
var moveLeft = "G1 X20";
var homeCommand = "G28";

var filename = "/path/to/file.gcode";
var printjobID = 3;
var serialPortPath = "/dev/tty.usbmodem12341";

//START SERVER 
driver.start(function(err,result,event){
	if(err)
	{
		//your error handler
	}
	if(result)
	{
		//your handler
	}
	if(event)
	{
		//Your event handler
		//This is the big door that event callbacks use to enter the NodeJS world
	}
});

//SEND RAW COMMAND & RECEIVE RAW RESPONSE FROM PRINTER
//[gcode command, serial port, callback]
driver.sendGcode(homeCommand, serialPortPath, function(err,result){
	if(err)
	{
		//your error handler
	}
	if(result)
	{
		// your handler
	}
});


// PRINT GCODE FILE 
//[filename, printjobID, serial port, callback]
driver.printFile(short_filename, 6, serialPortPath,function(err,result){
	if(err)
	{
		//your error handler
	}
	if(result)
	{
		//your handler
	}
});

// GET PRINTER LIST
//[callback]
driver.getPrinterList(function(err,result){
	if(err)
	{
		//your error handler
	}
	if(result)
	{
		//your handler
	}
});


// GET PRINTER INFO
//[serial port, callback]
driver.getPrinterInfo(serialPortPath, function(err,result){
	if(err)
	{
		//your error handler
	}
	if(result)
	{
		//your handler
	}
});

// PAUSE PRINT
//[serial port, callback]
driver.pausePrint(serialPortPath, function(err,result){
	if(err)
	{
		//your error handler
	}
	if(result)
	{
		//your handler
	}
});

// RESUME PRINT
//[serial port, callback]
driver.resumePrint(serialPortPath, function(err,result){
	if(err)
	{
		//your error handler
	}
	if(result)
	{
		//your handler
	}
});

// STOP PRINT
//[serial port, stop Gcode, callback]
driver.stopPrint(serialPortPath, stopGcode, function(err,result){
	if(err)
	{
		//your error handler
	}
	if(result)
	{
		//your handler
	}
});


