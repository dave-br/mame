// license:BSD-3-Clause
// copyright-holders:Vas Crabb
//============================================================
//
//  disassemblyviewer.h - MacOS X Cocoa debug window handling
//
//============================================================

#import "debugosx.h"

#import "debugwindowhandler.h"


#import <Cocoa/Cocoa.h>


@class MAMEDebugConsole, MAMEDisassemblyView, MAMESrcDebugView;

@interface MAMEDisassemblyViewer : MAMEExpressionAuxiliaryDebugWindowHandler
{
	MAMEDisassemblyView *dasmView;
	NSPopUpButton       *subviewButton;
	NSView				*dissasemblyGroupView;
	MAMESrcDebugView	*srcdbgView;
	NSView				*srcdbgGroupView;
}

- (id)initWithMachine:(running_machine &)m console:(MAMEDebugConsole *)c;

- (BOOL)selectSubviewForDevice:(device_t *)device;
- (BOOL)selectSubviewForSpace:(address_space *)space;
- (void)setDisasemblyView:(BOOL)value;

- (IBAction)sourceDebugBarChanged:(id)sender;
- (IBAction)debugToggleBreakpoint:(id)sender;
- (IBAction)debugToggleBreakpointEnable:(id)sender;
- (IBAction)debugRunToCursor:(id)sender;

- (IBAction)changeSubview:(id)sender;

- (void)saveConfigurationToNode:(util::xml::data_node *)node;
- (void)restoreConfigurationFromNode:(util::xml::data_node const *)node;

@end
