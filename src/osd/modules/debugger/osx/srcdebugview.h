// license:BSD-3-Clause
// copyright-holders:tim lindner
//============================================================
//
//  srcdebugview.h - MacOS X Cocoa source debug window handling
//
//============================================================

#import "debugosx.h"

#import "debugview.h"

#include "debug/dvsourcecode.h"

#import <Cocoa/Cocoa.h>

@interface MAMESrcDebugView : MAMEDebugView <MAMEDebugViewSubviewSupport>
{
}

- (id)initWithFrame:(NSRect)f machine:(running_machine &)m;

- (NSSize)maximumFrameSize;

- (NSString *)selectedSubviewName;
- (int)selectedSubviewIndex;
- (void)selectSubviewAtIndex:(int)index;
// - (BOOL)selectSubviewForDevice:(device_t *)device;
// - (BOOL)selectSubviewForSpace:(address_space *)space;

// - (NSString *)expression;
// - (void)setExpression:(NSString *)exp;

// - (debug_view_sourcecode const *)source;
// - (offs_t)selectedLine;

// - (IBAction)showRightColumn:(id)sender;
- (IBAction)sourceDebugBarChanged:(id)sender;

- (void)insertActionItemsInMenu:(NSMenu *)menu atIndex:(NSInteger)index;
- (void)insertSubviewItemsInMenu:(NSMenu *)menu atIndex:(NSInteger)index;

- (void)saveConfigurationToNode:(util::xml::data_node *)node;
- (void)restoreConfigurationFromNode:(util::xml::data_node const *)node;

@end
