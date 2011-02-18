/*
 * Rebug XMB Selector v1.1- Swaps files for different XMB in Rebug
 *
 * Author: jjolano
 *       : Cyberskunk
 *
 * ICON0 : evilsperm
 */

#include <psl1ght/lv2.h>
#include <psl1ght/lv2/filesystem.h>

#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <fcntl.h>

#include <sysutil/events.h>
#include <sysutil/video.h>

#include <rsx/gcm.h>
#include <rsx/reality.h>

#include <io/pad.h>

#include "sconsole.h"

int exitapp = 0;

typedef struct {
	int height;
	int width;
	uint32_t *ptr;
	// Internal stuff
	uint32_t offset;
} buffer;

gcmContextData *context;
VideoResolution res;
int currentBuffer = 0;
buffer *buffers[2];

void waitFlip() { // Block the PPU thread untill the previous flip operation has finished.
	while(gcmGetFlipStatus() != 0) 
		usleep(200);
	gcmResetFlipStatus();
}

void flip(s32 buffer)
{
	assert(gcmSetFlip(context, buffer) == 0);
	realityFlushBuffer(context);
	gcmSetWaitFlip(context);
}

void makeBuffer(int id, int size)
{
	buffer *buf = malloc(sizeof(buffer));
	buf->ptr = rsxMemAlign(16, size);
	assert(buf->ptr != NULL);

	assert(realityAddressToOffset(buf->ptr, &buf->offset) == 0);
	assert(gcmSetDisplayBuffer(id, buf->offset, res.width * 4, res.width, res.height) == 0);
	
	buf->width = res.width;
	buf->height = res.height;
	buffers[id] = buf;
}

void init_screen()
{
	void *host_addr = memalign(1024*1024, 1024*1024);
	assert(host_addr != NULL);

	context = realityInit(0x10000, 1024*1024, host_addr); 
	assert(context != NULL);

	VideoState state;
	assert(videoGetState(0, 0, &state) == 0);
	assert(state.state == 0);

	assert(videoGetResolution(state.displayMode.resolution, &res) == 0);
	
	VideoConfiguration vconfig;
	memset(&vconfig, 0, sizeof(VideoConfiguration));
	vconfig.resolution = state.displayMode.resolution;
	vconfig.format = VIDEO_BUFFER_FORMAT_XRGB;
	vconfig.pitch = res.width * 4;

	assert(videoConfigure(0, &vconfig, NULL, 0) == 0);
	assert(videoGetState(0, 0, &state) == 0); 

	s32 buffer_size = 4 * res.width * res.height;
	
	gcmSetFlipMode(GCM_FLIP_VSYNC);
	makeBuffer(0, buffer_size);
	makeBuffer(1, buffer_size);

	gcmResetFlipStatus();
	flip(1);
}

void eventHandler(u64 status, u64 param, void * userdata)
{
	if(status == EVENT_REQUEST_EXITAPP) // 0x101
	{
		exitapp = 1;
	}
}

int copyfile(const char* fn_src, const char* fn_dst)
{
	u64 pos;
	u64 read;
	u64 write;
	
	Lv2FsFile src = -1;
	Lv2FsFile dst = -1;
	
	if(lv2FsOpen(fn_src, LV2_O_RDONLY, &src, 0, NULL, 0) != 0 || lv2FsOpen(fn_dst, LV2_O_WRONLY | LV2_O_CREAT | LV2_O_TRUNC, &dst, 0, NULL, 0) != 0)
	{
		return -1;
	}
	
	lv2FsChmod(fn_dst, S_IFMT | 0777);
	
	lv2FsLSeek64(src, 0, 0, &pos);
	lv2FsLSeek64(dst, 0, 0, &pos);
	
	char buffer[32768];
	
	while(lv2FsRead(src, buffer, sizeof(buffer) - 1, &read) == 0 && read > 0)
	{
		lv2FsWrite(dst, buffer, read, &write);
	}
	
	if(read != 0)
	{
		return -1;
	}
	
	lv2FsClose(src);
	lv2FsClose(dst);
	
	return 0;
}

int main(int argc, const char* argv[])
{
	PadInfo padinfo;
	PadData paddata;
	int i, j;
	char status[128];
	
	sysRegisterCallback(EVENT_SLOT0, eventHandler, NULL);
	
	init_screen();
	ioPadInit(7);
	
	sconsoleInit(FONT_COLOR_MOROON, FONT_COLOR_YELLOW, res.width, res.height);
	
	waitFlip();
	
	Lv2FsStat entry;
	int is_mounted = lv2FsStat("/dev_blind", &entry);
	
	if(is_mounted != 0)
	{
		Lv2Syscall8(837, (u64)"CELL_FS_IOS:BUILTIN_FLSH1", (u64)"CELL_FS_FAT", (u64)"/dev_blind", 0, 0, 0, 0, 0);
	}
	
	strcpy(status, "  Status: Ready - dev_blind mounted");
	
	while(exitapp == 0)
	{
		sysCheckCallback();
		ioPadGetInfo(&padinfo);
		for(i = 0; i < MAX_PADS; i++)
		{
			if(padinfo.status[i])
			{
				ioPadGetData(i, &paddata);
				if(paddata.BTN_CROSS)
				{
					if(copyfile("/dev_flash/rebug/debug_menu_1/sysconf_plugin.sprx", "/dev_blind/vsh/module/sysconf_plugin.sprx") == 0)
					{
						strcpy(status, "Status: Debug Menu 1 installed");
					}
					else
					{
						strcpy(status, "Status: Debug Menu 1 install failed");
					}
					
					sleep(1);
				}
				if(paddata.BTN_CIRCLE)
				{
					if(copyfile("/dev_flash/rebug/debug_menu_2/sysconf_plugin.sprx", "/dev_blind/vsh/module/sysconf_plugin.sprx") == 0)
					{
						strcpy(status, "Status: Debug Menu 2 installed");
					}
					else
					{
						strcpy(status, "Status: Debug Menu 2 install failed");
					}
					
					sleep(1);
				}
				if(paddata.BTN_SQUARE)
				{
					if(copyfile("/dev_flash/rebug/debug_xmb/vsh.self", "/dev_blind/vsh/module/vsh.self") == 0)
					{
						strcpy(status, "Status: Debug XMB installed");
					}
					else
					{
						strcpy(status, "Status: Debug XMB install failed");
					}
					
					sleep(1);
				}
				if(paddata.BTN_TRIANGLE)
				{
					if(copyfile("/dev_flash/rebug/retail_xmb/vsh.self", "/dev_blind/vsh/module/vsh.self") == 0)
					{
						strcpy(status, "Status: Retail XMB installed");
					}
					else
					{
						strcpy(status, "Status: Retail XMB install failed");
					}
					
					sleep(1);
				}
				if(paddata.BTN_START)
				{
					if(copyfile("/dev_usb000/xRegistry.sys", "/dev_flash2/etc/xRegistry.sys") == 0)
					{
						strcpy(status, "Status: xRegistry.sys RESTORE SUCCESSFUL - PLEASE REBOOT");
					}
					else
					{
						strcpy(status, "Status: xRegistry.sys RESTORE FAILED");
					}
					
					sleep(1);
				}
				if(paddata.BTN_SELECT)
				{
					if(copyfile("/dev_flash2/etc/xRegistry.sys", "/dev_usb000/xRegistry.sys") == 0)
					{
						strcpy(status, "Status: xRegistry.sys BACKUP SUCCESSFULL");
					}
					else
					{
						strcpy(status, "Status: xRegistry.sys BACKUP FAILED");
					}
					if(copyfile("/dev_flash2/etc/xRegistry.sys", "/dev_usb000/xRegistry.sys") == 0)
					{
						strcpy(status, "Status: xRegistry.sys BACKUP SUCCESSFULL");
					}
					else
					{
						strcpy(status, "Status: xRegistry.sys BACKUP FAILED");
					}
					
					sleep(1);
				}
			}		
		}
		
		for(i = 0; i < res.height; i++)
		{
			for(j = 0; j < res.width; j++)
			{
				buffers[currentBuffer]->ptr[i* res.width + j] = FONT_COLOR_MOROON;
			}
		}
		
		print(60, 50, "**** Rebug XMB Selector v1.1 ****", buffers[currentBuffer]->ptr);
		print(70, 100, status, buffers[currentBuffer]->ptr);
		
		print(60, 150, "Press CROSS to use: [Debug Menu 1]", buffers[currentBuffer]->ptr);
		print(60, 180, "Press CIRCLE to use: [Debug Menu 2]", buffers[currentBuffer]->ptr);
		print(60, 210, "Press SQUARE to use: [Debug XMB]", buffers[currentBuffer]->ptr);
		print(60, 240, "Press TRIANGLE to use: [Retail XMB]", buffers[currentBuffer]->ptr);

		print(60, 315, "** xRegistry.sys - Backup/Restore **", buffers[currentBuffer]->ptr);
		print(60, 355, "Press SELECT to BACKUP xRegistry to USB000", buffers[currentBuffer]->ptr);
		print(60, 385, "Press START to RESTORE xRegistry from USB000", buffers[currentBuffer]->ptr);
		
		flip(currentBuffer);
		waitFlip();
		currentBuffer = !currentBuffer;
	}
	
	if(is_mounted != 0)
	{
		Lv2Syscall1(838, (u64)"/dev_blind");
	}
	
	return 0;
}
