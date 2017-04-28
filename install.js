const spawn           = require('cross-spawn');
const os              = require('os');
const electron_target = '0.33.2';
const node_target     = '4.2.2';

var target_platform,
    runtime,
    target,
    target_arch,
    install = null;

if (process.env.F_TARGET_PLATFORM) target_platform = process.env.F_TARGET_PLATFORM;
if (process.env.F_RUNTIME) runtime = process.env.F_RUNTIME;
if (process.env.F_TARGET) target = process.env.F_TARGET;
if (process.env.F_TARGET_ARCH) target_arch = process.env.F_TARGET_ARCH;

if (!target_platform) target_platform = os.platform();

if (!runtime && target_platform === 'win32')
    runtime = 'electron';
else if (!runtime)
    runtime = 'node';

if (!target && runtime === 'electron')
    target = electron_target;
else if (!target && runtime === 'node')
    target = node_target;

if (!target_arch) target_arch = os.arch();

var installCommands = ['install'];
if (target_platform !== null) installCommands.push('--target_platform=' + target_platform);
if (runtime !== null) installCommands.push('--runtime=' + runtime);
if (target !== null) installCommands.push('--target=' + target);
if (target_arch !== null) installCommands.push('--target_arch=' + target_arch);

console.log('installing slicer with params', installCommands);
install = spawn('node-pre-gyp', installCommands);

install.on('error', function (err) {
    console.error('install: ' + err);
});

install.stdout.on('data', function (data) {
    console.log('install: ' + data);
});

install.stderr.on('data', function (data) {
    console.error('install: ' + data);
});

install.on('close', function (code) {
    console.log('install process exited with code ' + code);
});
