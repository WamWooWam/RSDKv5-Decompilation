#ifdef __APPLE__

#import <Foundation/Foundation.h>
#include "cocoaHelpers.hpp"

const char* getResourcesPath(void)
{
	static char pathStorage[256] = {0};
	
	if (!strlen(pathStorage)) {	
		NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];
		NSArray* paths = NSSearchPathForDirectoriesInDomains(NSApplicationSupportDirectory, NSUserDomainMask, YES);
		NSString* applicationSupportDirectory = [paths objectAtIndex:0];		
		NSString* completeDirectory = [applicationSupportDirectory stringByAppendingString:@"/RSDKv5"];
		
		BOOL isDir;
		// check if the directory actually exists before blindly fucking with it
		NSFileManager* fileManager = [NSFileManager defaultManager];
		if(![fileManager fileExistsAtPath:completeDirectory isDirectory: &isDir]) {
			if(![fileManager createDirectoryAtPath:completeDirectory attributes: nil]) {
				printf("Failed to create directory\n");
			}
		}		
		else {	
			printf("Directory exists\n");
		}
		
		strncpy(pathStorage, [completeDirectory UTF8String], 256);
		
		[pool release];
	}
	
	return pathStorage;
}

const char* getAppResourcesPath(void)
{
	static char pathStorage[256] = {0};
	
	if (!strlen(pathStorage)) {	
		NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];
		NSString *resourceDirectory = [[NSBundle mainBundle] resourcePath];
		
		char* str = (char*)[resourceDirectory UTF8String];	
		strncpy(pathStorage, str, 256);
		
		[pool release];
	}
	
	return pathStorage;
}
#endif