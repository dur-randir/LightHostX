//
//  main.m
//  LightHostX
//
//  Created by dur-randir on 10.08.2024.
//

#import <Cocoa/Cocoa.h>
#import "AppDelegate.h"

AppDelegate * delegate;

int main(int argc, const char * argv[]) {
    NSApplication *applicationObject = [NSApplication sharedApplication];
    delegate = [AppDelegate alloc];
    [delegate init];
    applicationObject.delegate = delegate;
    [applicationObject run];
}
