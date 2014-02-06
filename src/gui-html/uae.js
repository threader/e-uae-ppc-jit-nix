// Global application objects.
var module = null;
var moduleSuccessfullyLoaded = false;
var moduleInitTasks = [];

// Track whether the emulator is paused by the user (as opposed by
// Chrome pausing graphics processing).
var isManuallyPaused = false;

// Upbeat status messages during the looong start-up.
var statusMessages = [
  "reticulating splines",
  "calling the 80s",
  "polishing floppy disks",
  "yes, it'll load faster the second time",
  "finding Chuck Norris",
  "debugging guru meditations",
  "preparing garden gnomes",
  "warming up Rick Astley's voice",
  "assembling floppy drive",
  "waking up Agnus, Denise, and Paula",
  "loading up Portable Native Client",
  "unpacking Amiga 500",
  "calibrating non-interlaced mode",
  "powering up the Motorola 68000"
]
var currentMessage = 0;


// Helper functions used throughout.
function $(element) {
  return document.getElementById(element);
}

function mimeHandlerExists (mimeType) {
  // Though it's possible to enable PNaCl in Chrome 30, the
  // emulator doesn't run properly on it, so we disable it.
  // (Don't do this at home. The capability check below is typically
  // sufficient, and it's better practice.)
  var chromeVersion = window.navigator.appVersion.match(/Chrome\/(\d+)\./);
  if (!chromeVersion) {
    return false;
  }
  var majorVersion = parseInt(chromeVersion[1], 10);
  if (majorVersion <= 30) {
    return false;
  }

  // Capability check: verify that the browser is able to handle PNaCl content.
  supportedTypes = navigator.mimeTypes;
  for (var i = 0; i < supportedTypes.length; i++) {
    if (supportedTypes[i].type == mimeType) {
      return true;
    }
  }

  return false;
}

function updateStatus(opt_message) {
  var statusField = document.getElementById('status_field');
  if (statusField) {
    statusField.innerHTML = opt_message;
  }
}

function logMessage(message) {
  console.log(message);
}


// These event handling functions are were taken from the NaCl SDK
// examples and slightly adjusted.

function moduleDidStartLoad() {
  logMessage('loadstart');
}

var intervalUpdate = setInterval(
  function() {
    currentMessage = (currentMessage + 1) % statusMessages.length;
    document.getElementById('progress_text').innerHTML =
      'Loading... (' + statusMessages[currentMessage] + ')';
  }, 2000);

var lastLoadPercent = 0;

function moduleLoadProgress(event) {
  var loadPercent = 0.0;
  var loadPercentString;
  var loadStatus;

  if (event.lengthComputable && event.total > 0) {
    clearInterval(intervalUpdate);
    loadPercent = (event.loaded / event.total * 100.0).toFixed(0);
    if (loadPercent == lastLoadPercent) return;
    lastLoadPercent = loadPercent;
    loadPercentString = loadPercent + '%';
    document.getElementById('progress_text').innerHTML =
      'Loading... (' + loadPercentString + ')';
  } else {
    // The total length is not yet known.
  }
}

// Handler that gets called if an error occurred while loading the NaCl
// module.  Note that the event does not carry any meaningful data about
// the error, you have to check lastError on the <EMBED> element to find
// out what happened.
function moduleLoadError() {
  if (module) {
    logMessage('error: ' + module.lastError);
    updateStatus('Error: ' + module.lastError);
  } else {
    logMessage('Module load error. Field "lastError" not available.');
  }
}

// Handler that gets called if the NaCl module load is aborted.
function moduleLoadAbort() {
  logMessage('abort');
}

// Indicate success when the NaCl module has loaded.
function moduleDidLoad() {
  module = document.getElementById('uae');
  // Set up general postMessage handler
  module.addEventListener('message', handleMessage, true);

  // Set up handler for kickstart ROM upload field
  document.getElementById('rom').addEventListener(
  'change', handleFileSelect, false);
  // Set up handler for disk drive 0 (df0:) upload field
  document.getElementById('df0').addEventListener(
  'change', handleFileSelect, false);
  // Set up handler for disk drive 1 (df1:) upload field
  document.getElementById('df1').addEventListener(
  'change', handleFileSelect, false);

  document.getElementById('resetBtn')
    .addEventListener('click', resetAmiga, true);
  document.getElementById('pauseBtn')
    .addEventListener('click', manuallyPauseAmiga, true);
  document.getElementById('eject0Btn')
    .addEventListener('click', function(e) { ejectDisk('df0'); }, true);
  document.getElementById('eject1Btn')
    .addEventListener('click', function(e) { ejectDisk('df1'); }, true);

  document.getElementById('port0').addEventListener(
    'change',
    function(e) {
      connectJoyPort('port0',
	             e.target.options[e.target.selectedIndex].value); },
    true);
  document.getElementById('port1').addEventListener(
    'change',
    function(e) {
      connectJoyPort('port1',
	             e.target.options[e.target.selectedIndex].value); },
    true);

  document.plugins.uae.focus();

  moduleSuccessfullyLoaded = true;
  for (var i = 0; i < moduleInitTasks.length; ++i) {
    moduleInitTasks[i]();
  }
  logMessage("Module loaded.");
}

function moduleDidEndLoad() {
  clearInterval(intervalUpdate);
  document.getElementById('overlay').style.visibility='hidden'
  logMessage('loadend');
  var lastError = event.target.lastError;
  if (lastError == undefined || lastError.length == 0) {
    lastError = '<none>';

    window.onbeforeunload = function () {
      return 'If you leave this page, the state of the Amiga emulator will ' +
        'be lost.';
    }
    if (document.webkitVisibilityState) {
      document.addEventListener("webkitvisibilitychange",
                                handleVisibilityChange,
                                false);
    }

    // If we loaded without errors, we can activate the buttons.
    // Would have been logical to do this in moduleDidLoad, but in Chrome
    // 32, there is an initial moduleDidLoad event when PNaCl starts loading
    // itself, which is way before the emulator gets started.

    $('rom').disabled = false;
    $('resetBtn').disabled = false;
    $('pauseBtn').disabled = false;
    $('port0').disabled = false;
    $('port1').disabled = false;
  } else {
    updateStatus('Error: ' + lastError);
  }
  logMessage('lastError: ' + lastError);
}

function moduleCrashed() {
  logMessage('PNaCl module crashed: ' + module.lastError);
  updateStatus('PNaCl module crashed: ' + module.lastError);
}

// TODO(cstefansen): Connect module's stdout/stderr to this.
function handleMessage(message_event) {
  logMessage(message_event.data);
}

// Functions to operate the Amiga.
function resetAmiga() {
  if (isManuallyPaused) {
    manuallyResumeAmiga();
  }
  module.postMessage('reset');
}

function manuallyPauseAmiga() {
  pauseAmiga();
  isManuallyPaused = true;
}

function pauseAmiga() {
  var pauseBtn = $('pauseBtn');
  pauseBtn.removeEventListener('click', manuallyPauseAmiga);
  pauseBtn.addEventListener('click', manuallyResumeAmiga);
  pauseBtn.innerText = "Resume Amiga";
  module.postMessage('pause');
}

function manuallyResumeAmiga() {
  resumeAmiga();
  isManuallyPaused = false;
}

function resumeAmiga() {
  var pauseBtn = $('pauseBtn');
  pauseBtn.removeEventListener('click', manuallyResumeAmiga);
  pauseBtn.addEventListener('click', manuallyPauseAmiga);
  pauseBtn.innerText = "Pause Amiga";
  module.postMessage('resume');
}

// Ejects the disk in the current drive (e.g. 'df0', 'df1'). If there
// is no disk in the drive, no action is taken.
function ejectDisk(driveDescriptor) {
  switch (driveDescriptor) {
  case 'df0':
  case 'df1':
    module.postMessage('eject ' + driveDescriptor);
    document.getElementById(driveDescriptor).value = "";
    break;
  default:
    alert('Internal page error. Try to reload the page. If the ' +
	  'problem persists, please report the issue.');
  }
}

function connectJoyPort(portId, inputDevice) {
  module.postMessage('connect ' + portId + ' ' + inputDevice);
}

function enableFloppyDrives() {
    $('df0').disabled = false;
    $('eject0Btn').disabled = false;
    $('df1').disabled = false;
    $('eject1Btn').disabled = false;
}

// Load a file to targetDevice, a pseudo-device, which can be 'rom',
// 'df0', or 'df1'.
function loadFromLocal(targetDevice, file) {
  var fileURL = window.webkitURL.createObjectURL(file);
  switch (targetDevice) {
  case 'df0':
  case 'df1':
    module.postMessage('insert ' + targetDevice + ' ' + fileURL);
    break;
  case 'rom':
    module.postMessage('rom ' + fileURL);
    // User brought own ROM; enable floppy drives
    enableFloppyDrives();
    break;
  default:
    alert('Internal page error. Try to reload the page. If the ' +
	  'problem persists, please report the issue.');
  }
}

// This functions is called when a kickstart ROM or a disk image is
// uploaded via the input fields on the page.
function handleFileSelect(evt) {
  loadFromLocal(evt.target.id, evt.target.files[0]);
}


// Pause the emulator (and thus stop playing audio) when not visible.
// Documentation:
// https://developers.google.com/chrome/whitepapers/pagevisibility?csw=1
// TODO(cstefansen): This should just pause the gfx rendering.
function handleVisibilityChange() {
  // Only mess with things if we're not already paused.
  if (!isManuallyPaused) {
    if (document.webkitHidden) {
      pauseAmiga();
    } else {
      resumeAmiga();
    }
  }
}


// Check for Amiga Forever Essentials.
// The callback will be called if and only if the ROM and key files were
// found. The argument to the callback will be the blob URL of the ROM file.
//
// It is said that if you weren't embarrassed by the first launched version of
// your product, you didn't launch early enough. The function below
// is ample cause for embarrassment.
//
// TODO(cstefansen): Refactor and clean up checkForAmigaForeverEssentials.
function checkForAmigaForeverEssentials(callback) {
  var amiga13RomFile = null;
  var amiga13KeyFile = null;
  // Development app_id:
  // var afeExtensionId = "jpbmmmmndcncdnbegeofdbakhdhnpcnh";
  var afeExtensionId = "aemohkgcfhdkjpoonndfpaikgbpgcogm";
  var afeUrlBase = "chrome-extension://" + afeExtensionId + "/shared/";
  var afeIndex = afeUrlBase + "index.json";

  console.log('Checking for Amiga Forever Essentials.');

  var xhr = new XMLHttpRequest();
  // Preferring 'loadend' to 'readystatechange' due to issues like this one:
  //   https://code.google.com/p/chromium/issues/detail?id=159827.
  // 'loadend' is guaranteed to be called exactly once regardless of outcome.
  xhr.addEventListener('loadend', function() {
    if (xhr.readyState == 4 && xhr.status == 200) {
      console.log('Found Amiga Forever Essentials index.json.');
      // Find URLs for Amiga 1.3 ROM and key.
      var index = JSON.parse(xhr.responseText);
      var romCount = index.files.rom.length;
      for (var n = 0; n < romCount; n++) {
        if (index.files.rom[n].name === "Amiga 1.3") {
          amiga13RomFile = afeUrlBase + index.files.rom[n].romFile;
          amiga13KeyFile = afeUrlBase + index.files.rom[n].keyFile;
          break;
        }
      }
      if (amiga13RomFile == '' || amiga13KeyFile == '') {
        console.log('Amiga 1.3 data not found in index.json.');
        return;
      }
      console.log('Amiga 1.3 data:\nROM File: ' + amiga13RomFile +
                  '\nKey File: ' + amiga13KeyFile);

      // Native Client doesn't like chrome-extension:// URLs, so we fetch the
      // ROM/key files and make a blob URL out of it.
      var romReq = new XMLHttpRequest();
      romReq.responseType = 'arraybuffer';
      romReq.addEventListener('loadend', function() {
        if (romReq.readyState == 4 && romReq.status == 200 && romReq.response) {
          // Now get the key file.
          var keyReq = new XMLHttpRequest();
          keyReq.responseType = 'arraybuffer';
          keyReq.addEventListener('loadend', function() {
            if (keyReq.readyState == 4 && keyReq.status == 200 && keyReq.response) {
              console.log('ROM and key files found - yay!');

              var romData = new Int8Array(romReq.response.slice(11));
              var keyData = new Int8Array(keyReq.response);
              var keyLength = keyData.length;
              for (var i = 0; i < romData.length; ++i) {
                romData[i] ^= keyData[i % keyLength];
              }
              romBlob = new Blob([romData], {type: 'application/octet-binary'});
              callback(romBlob);
              $('afe_message').innerHTML =
                'Amiga Forever Essentials detected &ndash; enjoy!';
            } else {
              console.log('Amiga Forever Essentials key file not found.');
            }
          }, false);
          keyReq.open('GET', amiga13KeyFile, true);
          keyReq.send(null);
        } else {
          console.log('Amiga Forever Essentials ROM file not found.');
        }
      }, false);
      romReq.open("GET", amiga13RomFile, true);
      romReq.send(null);
    } else {
      console.log('Amiga Forever Essentials not found.');
    }
  }, false);

  xhr.open("GET", afeIndex, true);
  xhr.send();
}


// Start-up tasks on document loading
if (!mimeHandlerExists('application/x-pnacl')) {
  updateStatus('This page uses <a href=' +
    '"https://developers.google.com/native-client/pnacl-preview/">Portable ' +
    'Native Client</a>, a technology currently only supported in Google ' +
    'Chrome (version 31 or higher; Android and iOS not yet supported).');
  document.getElementById('progress')
    .style.visibility='hidden';
} else {
  // The callback is called if and only if the Amiga Forever Essentials files
  // are found.
  checkForAmigaForeverEssentials(function(romBlob) {
    var loadAmigaForeverEssentialsROMAndKey = function() {
      setTimeout(function() {
        loadFromLocal('rom', romBlob);
      }, 3000 /* Work-around for a bug in UAE that causes it too crash if GUI
                 messages are received too quickly after start-up. */);
    };

    if (moduleSuccessfullyLoaded) {
      // In the unlikely event that the module loaded before the Amiga Forever
      // Essentials check completed we post the message directly.
      loadAmigaForeverEssentialsROMAndKey();
    } else {
      // If module hasn't loaded yet, we queue up the function to be
      // invoked on successful module load.
      moduleInitTasks.push(loadAmigaForeverEssentialsROMAndKey);
    }
  });

  var container = document.getElementById('uae_container');

  container.addEventListener('loadstart', moduleDidStartLoad, true);
  container.addEventListener('progress', moduleLoadProgress, true);
  container.addEventListener('error', moduleLoadError, true);
  container.addEventListener('abort', moduleLoadAbort, true);
  container.addEventListener('load', moduleDidLoad, true);
  container.addEventListener('loadend', moduleDidEndLoad, true);
  container.addEventListener('crash', moduleCrashed, true);
  container.addEventListener('message', handleMessage, true);

  var moduleElement = document.createElement('embed');
  // Any attributes are passed as arguments to UAEInstance::Init().
  moduleElement.setAttribute('name', 'uae');
  moduleElement.setAttribute('id', 'uae');
  moduleElement.setAttribute('width', 720);
  moduleElement.setAttribute('height', 568);
  moduleElement.setAttribute('src', 'uae.nmf');
  moduleElement.setAttribute('type', 'application/x-pnacl');

  var containerDiv = document.getElementById('uae_container');
  containerDiv.appendChild(moduleElement);

}
