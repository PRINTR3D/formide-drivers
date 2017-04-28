{
  "targets": [
    {
      "target_name": "<(module_name)",
      "sources": [
      "src/handler/Buffer/GCodeBuffer.cpp",
      "src/utils/find_devices.hpp",
      "src/utils/Logger/Logger.cpp",
      "src/handler/CommunicationLogger/CommLogger.cpp",
      "src/handler/DeviceCenter/DeviceCenter.cpp",
      "src/handler/PrinterData/PrinterData.cpp",
      "src/handler/Blocker/Blocker.cpp",
      "src/handler/Checksum/ChecksumControl.cpp",
      "src/driver/MarlinDriver/MarlinDriver.cpp",
      "src/utils/Timer/Timer.cpp",
      "src/driver/utilities/utilities.c",
      "src/FormiThread.cpp",
      "src/Formidriver.cpp"
      ],
      "include_dirs": [
      "<!(node -e \"require('nan')\")",
          '-lpthread', '-lcurl'
      ],
      "libraries": [
          "-L/opt/local/lib", "-L/usr/include"
      ],
      "cflags!": [ "-fno-exceptions" ],
      "cflags": [],
      "cflags_cc!": [ "-fno-exceptions" ],
      "conditions": [
                [ 'OS!="win"', {
                  "cflags+": [ "-std=c++11" ],
                  "cflags_c+": [ "-std=c++11" ],
                  "cflags_cc+": [ "-std=c++11" ],
                  "sources": [
                    'src/driver/Serial/Serial.cpp'
                    ],
                }],
                [ 'OS=="mac"', {
                  "xcode_settings": {
                      'GCC_ENABLE_CPP_EXCEPTIONS': 'YES',
                      "OTHER_CPLUSPLUSFLAGS" : [ "-std=c++11", "-stdlib=libc++" ],
                      "OTHER_LDFLAGS": [ "-stdlib=libc++" ],
                      "MACOSX_DEPLOYMENT_TARGET": "10.7"
                  },
                }],
        ]
    },
    {
	    "target_name": "action_after_build",
		"type": "none",
		"dependencies": [ "<(module_name)" ],
	    "copies": [
        {
          "files": [ "build/Release/<(module_name).node" ],
          "destination": "<(module_path)"
        }
      ]
    }
  ]
}
