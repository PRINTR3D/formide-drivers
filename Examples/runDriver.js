var driver = require('../index.js');

var positive = false;
var printerID;
var tuneLine=0;
var samplecode = "testgcodes/none_feedrate.gcode"
var sleep = require('sleep');


console.log("start server");
driver.start(function(err,result,event){
	if(err)
	{
		console.log("error",err);
	}
	if(result)
	{
		console.log("result",result);
	}
	if(event)
	{
		console.log("event",event);

		if(event.type=="printerOnline"){

			printerID=event.port;

			// setInterval(alertFunc2, 10000);
			// printerID=event.port;
			//
			sleep.sleep(3);
			// driver.printFile(samplecode, 6, event.port,function(err,result){
			// 	if(err)
			// 	{
			// 		console.log(err);
			// 	}
			// 	if(result)
			// 	{
			// 		console.log(result);
			// 	}
			// });
		}
	}
});

 //setInterval(alertFunc, 20000);



 function alertFunc2(){

	 // Arguments: PrinterId, lineCount, skipCount, callback
	 driver.getCommunicationLogs(printerID, 10, 10, function(err,result){
 		if(err)
 		{
			console.log("Error log",err)
 		}
 		if(result)
 		{
 			console.log("log data",result)
 		}
	});
}


function alertFunc(){


		if(positive)
			{
				driver.sendTuneGcode("M220 S200", printerID,function(err,result){
					if(err)
					{
						console.log(err);
					}
					if(result)
					{
						console.log(result);
					}
				});
				tuneLine++;
				positive=false;
			}
		else {
			driver.sendTuneGcode("M220 S100", printerID,function(err,result){
				if(err)
				{
					console.log(err);
				}
				if(result)
				{
					console.log(result);
				}
			});
			tuneLine++;
			positive=true;
		}
	}
