/*
 * idle: stand-in for vendor daemons that only exist for the Kirin hardware of
 * the donor phone (e.g. CameraDaemon). init keeps the service "running"
 * without it crash-looping. Static, no dependencies.
 */
#include <unistd.h>

int main(void)
{
	for (;;)
		pause();
	return 0;
}
