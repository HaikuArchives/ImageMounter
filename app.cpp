/*
 
ImageMounter -- mounts file system images via console or graphical user interface,
using drag'n'drop.

Copyright (C) 2002 Maurice Michalski, http://fetal.de, http://maurice-michalski.de

Permission is hereby granted, free of charge, to any person obtaining a copy of this
software and associated documentation files (the "Software"), to deal in the Software
without restriction, including without limitation the rights to use, copy, modify,
merge, publish, distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to the following
conditions:

The above copyright notice and this permission notice shall be included in all copies
or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE
OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 
*/

#include "app.h"
#include <Alert.h>
#include <Entry.h>
#include <unistd.h>
#include <Path.h>
#include <stdio.h>
#include <Directory.h>
#include <Window.h>
#include <View.h>
#include <MenuBar.h>
#include <Menu.h>
#include <MenuItem.h>
#include <File.h>
#include "AboutWindow.h"
#include <NodeInfo.h>
#include <app/Roster.h>

#define MOUNT_IMAGE 'EMmt'

app::app():BApplication("application/x-ImageMounter") {
	quit=false;
	readytorun=false;
}

app::~app() {
}

// Creates the user interface.
void app::ReadyToRun() {
	readytorun=true;
	if (!quit) {
		w=new win();
		BView *view=new BView(BRect(0,0,150,70), "view", B_FOLLOW_TOP | B_FOLLOW_LEFT, B_WILL_DRAW);
		BMenuBar *mb=new BMenuBar(BRect(0,0,150,12), "mb");
		BMenu	*fm=new BMenu("File");
		sv=new BStringView(BRect(0,40, 150, 65), "sv", "drop image files here");
		
		sv->SetAlignment(B_ALIGN_CENTER);
		sv->SetHighColor(150, 150, 150);
		mb->AddItem(fm);
		fm->AddItem(new BMenuItem("About...", new BMessage(B_ABOUT_REQUESTED)));
		fm->AddSeparatorItem();
		fm->AddItem(new BMenuItem("Quit", new BMessage(B_QUIT_REQUESTED), 'Q'));
		view->SetViewColor(216, 216, 216);
		view->AddChild(mb);
		w->AddChild(view);
		mb->SetTargetForItems(be_app_messenger);
		fm->SetTargetForItems(be_app_messenger);
		view->AddChild(sv);
		
		w->Show();
	}
}

// Closes all windows when the user requested the application to quit.
bool app::QuitRequested() {
	while (CountWindows()>0) {WindowAt(0)->Lock();WindowAt(0)->Quit();}
	return true;
}

// Called when a message has been received.
void app::MessageReceived(BMessage *msg) {
	bool failed;
	switch(msg->what) {
		case B_SIMPLE_DATA:
			RefsReceived(msg);
			break;
		case MOUNT_IMAGE: {
			failed=false;
			if (MountImage(imagepath.String(), "bfs")<0)
			if (MountImage(imagepath.String(), "hfs")<0)
			if (MountImage(imagepath.String(), "iso9660")<0)
			if (MountImage(imagepath.String(), "dos")<0) {
				failed=true;
			}
			if (quit) PostMessage(new BMessage(B_QUIT_REQUESTED)); else
					if (w->Lock()) {
						if (failed) sv->SetHighColor(150, 100, 100); else
							sv->SetHighColor(100, 150, 100);
						sv->SetText(failed ? "Not an image file!" : "Image mounted successfully.");
						w->Unlock();
					}
			break;
		}
	
		default:
			BApplication::MessageReceived(msg);
			break;
	}
}

// Called, when a file has been dropped over the window.
void app::RefsReceived(BMessage *msg) {
	uint32 		type;
	int32 		count;
	BEntry		*entry=new BEntry();
	entry_ref	ref;
	BPath		path;

	msg->GetInfo("refs", &type, &count);
	if (!readytorun) quit=true;
	// not a entry_ref?
	if (type != B_REF_TYPE) {
		delete entry;
		return;
	}
	
	if (msg->FindRef("refs", 0, &ref) == B_OK)
		if (entry->SetTo(&ref,true)==B_OK) {
			// entry is ok. use it here.
			entry->GetPath(&path);
			imagepath.SetTo(path.Path());
			PostMessage(new BMessage(MOUNT_IMAGE));
		}
	delete entry;	
}

// Show some information about this software.
void app::AboutRequested() {
	AboutWindow	*aw=new AboutWindow(BRect(0,0,300,180), "About");
	app_info	info;

	if (GetAppInfo(&info)==B_OK) {
		BBitmap bmp(BRect(0,0,31,31), B_CMAP8);
		if (BNodeInfo::GetTrackerIcon(&info.ref, &bmp, B_LARGE_ICON)==B_OK)
			aw->SetIcon(&bmp);
	}

	aw->SetApplicationName("ImageMounter");
	aw->SetVersionNumber("0.3");
	aw->SetCopyrightString(B_UTF8_COPYRIGHT" by Maurice Michalski");
	aw->SetText("ImageMounter is freeware.\nThanks to Brent \"misza\" Miszalski.");
	aw->Show();
}

// Find out the image label and use it as the folder name for the mount point.
const char *app::FindMountPointName(const char *filename) {
	BFile	*image=new BFile(filename, B_READ_ONLY);
	uchar	id;
	char	volumename[255];
	
	image->Read((void *)(&id), 1);
	switch(id) {
		case 0xb8: {
			// Be Volume
			image->Seek(512, SEEK_SET);
			image->Read((void *)(&volumename), 255);
			return strdup(volumename);
			break;
		}
		
		case 0: {
			// ISO Volume
			image->Seek(32808, SEEK_SET);
			image->Read((void *)(&volumename), 255);
			return strdup(volumename);
			break;
		}
		
		default: {
			// unknown / read error
			// try to number it, as it has been done in ImageMounter 0.2
			
			break;
		}
	}
	
	return strdup("ImageMounter");
}

// Mounts the specified image file (filename) assuming it contains the 
// specified file system.
int app::MountImage(const char *filename, const char *filesystem) {
	int	timeout=3;
	int retvalue;
	BString	mountpoint;
	BEntry	entry;
	int mountpoint_counter=0;
	
	do {
		mountpoint="/";
		mountpoint.Append(FindMountPointName(filename));
		while (mountpoint[mountpoint.Length()-1]==0x20)
			mountpoint.RemoveLast(" ");
		if (mountpoint_counter>0)
			mountpoint << " " << mountpoint_counter;
		entry.SetTo(mountpoint.String());
		mountpoint_counter++;
	} while(entry.Exists());
	CreateDirectory(mountpoint.String());
	
	while ((retvalue=mount(filesystem,mountpoint.String(),filename,2,NULL,0))<0) {
		snooze(100000); // omg! ;)
		timeout--;
		if (timeout==0) {
			entry.Remove();
			mountpoint_counter--;
			entry.Unset();
			return -1;
		}
	};
	printf("mounted.\nfilename:\t%s\nfilesystem:\t%s\nmountpoint:\t%s\n", filename, filesystem, mountpoint.String());
	if (retvalue<0) {
		entry.Remove();
		mountpoint_counter--;
	}
	entry.Unset();
	return retvalue;
}

// un-mounts the image mounted at the specified mount point
int app::UnmountImage(const char *mountpoint) {
	int	timeout=10;
	int retvalue;
	BString s;

	while ((retvalue=unmount(mountpoint))<0) {
		snooze(1000000);
		timeout--;
		if (timeout==0) return -1;
	};
	s.SetTo("/boot/beos/bin/rmdir ").Append(mountpoint);
	system(s.String());
	return retvalue;
}

void app::CreateDirectory(const char *folder_name) {
	BDirectory dir;
	
	dir.CreateDirectory(folder_name, &dir);
}

void process_refs(entry_ref dir_ref, BMessage* msg, void *) {
	BEntry		*entry=new BEntry();
	entry_ref	ref;
	BPath		path;
	BString 	imagepath;
	
	if (msg->FindRef("refs", 0, &ref) == B_OK)
		if (entry->SetTo(&ref,true)==B_OK) {
			entry->GetPath(&path);
			 imagepath.SetTo(path.Path());
			if (app::MountImage(imagepath.String(), "bfs")<0)
			if (app::MountImage(imagepath.String(), "hfs")<0)
			if (app::MountImage(imagepath.String(), "iso9660")<0)
			 app::MountImage(imagepath.String(), "dos");
		}
	delete entry;	
}

win::win() :BWindow(BRect(300,250,450,320), "ImageMounter", B_TITLED_WINDOW, B_NOT_ZOOMABLE | B_NOT_RESIZABLE) {
}

win::~win() {
}

bool win::QuitRequested() {
	be_app->PostMessage(new BMessage(B_QUIT_REQUESTED));
	return false;
}

void win::MessageReceived(BMessage *msg) {
	switch(msg->what) {
		case B_SIMPLE_DATA: be_app->PostMessage(msg);
		default: BWindow::MessageReceived(msg);
	}
}


int main() {
	//printf("ImageMounter 0.1  (c) 2002 by Maurice Michalski <moscht@gmx.de>\n");
	new app();
	be_app->Run();
	delete be_app;
	return 0;
}
