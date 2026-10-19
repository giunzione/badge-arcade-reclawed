/*
 * Badge Arcade Reclawed installer - entry point.
 *
 * Step 1 skeleton: shows the app name and version on the top screen and how
 * it was launched, then waits for START. The install logic (copy code.ips,
 * enable Luma3DS game patching, safety checks) comes in later steps.
 */
#include <stdio.h>
#include <3ds.h>

#ifndef APP_VERSION
#define APP_VERSION "dev" /* normally set by the Makefile from VERSION */
#endif

int main(void)
{
	gfxInitDefault();

	/* Text consoles: top screen for information, bottom screen for controls. */
	PrintConsole top, bottom;
	consoleInit(GFX_TOP, &top);
	consoleInit(GFX_BOTTOM, &bottom);

	consoleSelect(&top);
	printf("\x1b[1;1HBadge Arcade Reclawed installer\n");
	printf("Version %s\n\n", APP_VERSION);
	/* envIsHomebrew() is true under the Homebrew Launcher (.3dsx),
	 * false when installed and started from the HOME Menu (.cia). */
	printf("Running as: %s\n", envIsHomebrew() ? "3dsx (Homebrew Launcher)" : "installed CIA");

	consoleSelect(&bottom);
	printf("\x1b[1;1HPress START to exit.\n");

	/* aptMainLoop() returns false when the system asks the app to close
	 * (HOME Menu -> Close, power off), so the loop also ends cleanly then. */
	while (aptMainLoop())
	{
		hidScanInput();
		if (hidKeysDown() & KEY_START)
			break;

		gfxFlushBuffers();
		gfxSwapBuffers();
		gspWaitForVBlank();
	}

	gfxExit();
	return 0;
}
