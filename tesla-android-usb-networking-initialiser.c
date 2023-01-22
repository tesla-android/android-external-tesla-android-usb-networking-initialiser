#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "toys.h"

char* appleVendorId = "05ac";
char* appleInitialisationCommand = "usbmuxd -v -f -X";

int connectedDevicesSize = 0;
char **connectedDeviceIds;

GLOBALS(
	char *i;
	long n;

	void *ids, *class;
	int count;
)

struct dev_ids {
	struct dev_ids *next, *child;
	int id;
	char name[];
};

struct scanloop {
	char *pattern;
	void *d1, *d2;
};

void initialiseDevice(char * deviceId) {
	printf("New USB device detected: %s\n", deviceId);
	if(strstr(deviceId, appleVendorId)) {
		printf("Initialising Apple device with usbmuxd");
		system(appleInitialisationCommand);
	}
}

void resetConnectedDeviceIds() {
	if(connectedDevicesSize != 0) {
		for (int i = 0; i < connectedDevicesSize; i++) {
				free(connectedDeviceIds[i]);
		}
	}
	free(connectedDeviceIds);
	connectedDevicesSize = 0;	
}

void initialiseConnectedDeviceList() {
	//FIXME size should be dynamic
	connectedDeviceIds = malloc(100 * sizeof(char*));
}

int scan_uevent(struct dirtree *new, int len, struct scanloop *sl) {
	int ii, count = 0;
	off_t flen = sizeof(toybuf);
	char *ss, *yy;

	if (*new->name == '.') return 0;
	sprintf(toybuf, "%s/uevent", new->name);
	if (!readfileat(dirtree_parentfd(new), ss = toybuf, toybuf, &flen)) return 0;

	while ((flen = strcspn(ss, "\n"))) {
		if (ss[flen]) ss[flen++] = 0;
		yy = ss+flen;
		for (ii = 0; ii<len; ii++) {
			if (strchr(sl[ii].pattern, '%')) {
				if (2-!sl[ii].d2==sscanf(ss, sl[ii].pattern, sl[ii].d1, sl[ii].d2))
					break;
			} else if (strstart(&ss, sl[ii].pattern)) {
				*(void **)sl[ii].d1 = ss;
				break;
			}
		}
		if (ii!=len) count++;
		ss = yy;
	}

	return count;
}

int processDeviceCallback(struct dirtree *new) {
	int busnum = 0, devnum = 0, pid = 0, vid = 0;

	if (!new->parent) return DIRTREE_RECURSE;
	if (3 == scan_uevent(new, 3, (struct scanloop[]){{"BUSNUM=%u", &busnum, 0},
			{"DEVNUM=%u", &devnum, 0}, {"PRODUCT=%x/%x", &pid, &vid}})) {
		char *deviceId = malloc(10 * sizeof(char));
		snprintf(deviceId, 10, "%04x:%04x", pid, vid);
		connectedDeviceIds[connectedDevicesSize] = deviceId;
		connectedDevicesSize++;
	}
	return 0;
}

int processAndInitialiseDeviceCallback(struct dirtree *new) {
	int busnum = 0, devnum = 0, pid = 0, vid = 0;

	if (!new->parent) return DIRTREE_RECURSE;
	if (3 == scan_uevent(new, 3, (struct scanloop[]){{"BUSNUM=%u", &busnum, 0},
			{"DEVNUM=%u", &devnum, 0}, {"PRODUCT=%x/%x", &pid, &vid}})) {
		char *deviceId = malloc(10 * sizeof(char));
		snprintf(deviceId, 10, "%04x:%04x", pid, vid);
		connectedDeviceIds[connectedDevicesSize] = deviceId;
		connectedDevicesSize++;
		initialiseDevice(deviceId);
	}
	return 0;
}

int initialiseNewlyConnectedDeviceCallback(struct dirtree *new) {
	int busnum = 0, devnum = 0, pid = 0, vid = 0;

	if (!new->parent) return DIRTREE_RECURSE;
	if (3 == scan_uevent(new, 3, (struct scanloop[]){{"BUSNUM=%u", &busnum, 0},
			{"DEVNUM=%u", &devnum, 0}, {"PRODUCT=%x/%x", &pid, &vid}})) {
		char *deviceId = malloc(10 * sizeof(char));
		snprintf(deviceId, 10, "%04x:%04x", pid, vid);
		
		bool isDeviceAlreadyConnected = false;
		for (int j = 0; j < connectedDevicesSize; j++) {
			if(strcmp(connectedDeviceIds[j], deviceId) == 0) {
				isDeviceAlreadyConnected = true;
				break;
			}
		}
	
		if(!isDeviceAlreadyConnected) {
			initialiseDevice(deviceId);
		}	
		free(deviceId);
	}
	return 0;
}

int main(void) {
	while(1) {
		if(connectedDevicesSize == 0) {
			initialiseConnectedDeviceList();
			dirtree_read("/sys/bus/usb/devices/", processAndInitialiseDeviceCallback);
		} else {
			dirtree_read("/sys/bus/usb/devices/", initialiseNewlyConnectedDeviceCallback);
			resetConnectedDeviceIds();
			initialiseConnectedDeviceList();
			dirtree_read("/sys/bus/usb/devices/", processDeviceCallback);
		}
		
		if(connectedDevicesSize == 0) {
			resetConnectedDeviceIds();
		}
		sleep(1);
	}
	resetConnectedDeviceIds();
	return 0;
}
