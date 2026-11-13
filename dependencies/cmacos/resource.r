#include "Processes.r"
#include "Dialogs.r"
#include "MacTypes.r"

resource 'SIZE' (-1) {
	reserved,
	acceptSuspendResumeEvents,
	reserved,
	canBackground,
	needsActivateOnFGSwitch,
	backgroundAndForeground,
	dontGetFrontClicks,
	ignoreChildDiedEvents,
	is32BitCompatible,
	isHighLevelEventAware,
	onlyLocalHLEvents,
	notStationeryAware,
	dontUseTextEditServices,
	reserved,
	reserved,
	reserved,
	/*
	  to save every rsdk game from needing cmacos code we just ask the OS for 256mb RAM minimum and 384MB preferred.
		Sonic Mania in particular needs 128MB minimum so it's not like this is far off
	*/
	384 * 1024 * 1024,
	256 * 1024 * 1024
};

resource 'ALRT' (128, purgeable) {
	{60, 81, 180, 431},
	128,
	{
		OK, visible, sound1,
		OK, visible, sound1,
		OK, visible, sound1,
		OK, visible, sound1
	},
	alertPositionMainScreen
};

resource 'DITL' (128, purgeable) {
	{
		{88, 268, 108, 336}, Button { enabled, "OK" },
		{10, 72, 78, 336}, StaticText { disabled, "^0" }
	}
};

resource 'vers' (1) {
	0x01, 0x00, development, 0x00, verUS,
	"1.0d",
	"1.0d, RSDKv5 decompilation for Classic Mac OS"
};
