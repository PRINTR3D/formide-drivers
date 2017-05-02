var driver = require('../index.js');

var positive = false;
var printerID;
var tuneLine=0;
var samplecode = "testgcodes/moveAround.gcode"
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


			sleep.sleep(15);
			driver.printFile(samplecode, 6, event.port,function(err,result){
				if(err)
				{
					console.log(err);
				}
				if(result)
				{
					console.log(result);
				}
			});

			console.log("Setting interval!!!!!!")
			setInterval(alertFunc2, 15000);
		}
	}
});

 //setInterval(alertFunc, 20000);



 function alertFunc2(){

	 if(positive)
		 {
			 driver.pausePrint(printerID,function(err,result){
				 if(err)
				 {
					 console.log(err);
				 }
				 if(result)
				 {
					 console.log(result);
				 }
			 });
			 positive=false;
		 }
	 else {
		 driver.resumePrint(printerID,function(err,result){
			 if(err)
			 {
				 console.log(err);
			 }
			 if(result)
			 {
				 console.log(result);
			 }
		 });
		 positive=true;
	 }

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
