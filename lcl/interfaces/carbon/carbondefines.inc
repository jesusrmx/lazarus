{%MainUnit carbonint.pas}
{
 /***************************************************************************
                    carbondefines.inc  -  Carbon defines
 ***************************************************************************/

 *****************************************************************************
  This file is part of the Lazarus Component Library (LCL)

  See the file COPYING.modifiedLGPL.txt, included in this distribution,
  for details about the license.
 *****************************************************************************
}

// Cocoa related defines

{$ifndef CarbonDontUseCocoa}
  {$define CarbonUseCocoa}
  {$define CarbonUseCocoaAll}
{$endif}

// Show debug info when tracing:

{off $define DebugBitmaps}

{off $DEFINE DebugEventLoop}

{off $DEFINE VerboseObject}    // Carbon object
{off $DEFINE VerboseTimer}
{off $DEFINE VerboseWinAPI}    // Carbon WinAPI
{off $DEFINE VerboseLCLIntf}   // Carbon LCLIntf
{off $DEFINE VerboseMouse}
{off $DEFINE VerboseCursor}    // Carbon cursor
{off $DEFINE VerboseKeyboard}
{off $DEFINE VerbosePaint}
{off $DEFINE VerboseCanvas}
{off $DEFINE VerboseMenu}      // Carbon menu
{off $DEFINE VerboseScroll}      // Carbon scrollable
{off $DEFINE VerboseBounds}      // debug bounding and client rects of LCL vs. intf

{off $DEFINE VerboseWidget}    // Carbon widget
{off $DEFINE VerboseAppEvent}  // Carbon application event handlers
{off $DEFINE VerboseControlEvent}  // Carbon control event handlers
{off $DEFINE VerboseWindowEvent}   // Carbon window event handlers
{off $DEFINE VerboseCommonEvent}   // Carbon common event handlers

{off $DEFINE VerboseWSClass}   // Carbon WS class

