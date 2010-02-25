/*++

Copyright (c) 2010 Evan Green

Module Name:

    Resource.h

Abstract:

    This header contains resource definitions for Phosphor2 GUI elements.

Author:

    Evan Green 21-Feb-2010

--*/

//
// ---------------------------------------------------------------- Definitions
//

#define ID_ICON 100

//
// Main dialog.
//

#define IDD_MAIN_WINDOW 10000
#define IDE_TEXT_FILES 101
#define IDC_OK 102
#define IDC_CANCEL 103
#define IDE_BACKGROUND_RED 104
#define IDE_BACKGROUND_GREEN 105
#define IDE_BACKGROUND_BLUE 106
#define IDE_FOREGROUND_RED 107
#define IDE_FOREGROUND_GREEN 108
#define IDE_FOREGROUND_BLUE 109
#define IDE_TEXT_SCALE 110
#define IDE_CURSOR_RATE 111
#define IDE_TEXT_RATE 112
#define IDE_FONT 113

//
// Embedded source resources.
//

#define SOURCE_FILE_COUNT 4
#define IDR_SOURCE 10001
#define IDR_RESOURCE_H 10002
#define IDR_RC 10003
#define IDR_MAKEFILE 10004

//
// Define UI element sizes.
//

#define UI_BORDER 1
#define UI_BUTTON_WIDTH 40
#define UI_SMALL_BOX_WIDTH 20
#define UI_MEDIUM_BOX_WIDTH 40
#define UI_LINE_HEIGHT 12
#define UI_INITIAL_WIDTH 200
#define UI_INITIAL_HEIGHT OK_CANCEL_OFFSET_Y + UI_LINE_HEIGHT + UI_BORDER
#define UI_DESCRIPTION_WIDTH (UI_INITIAL_WIDTH / 2) - UI_BORDER

//
// Individual UI element definitions.
//

#define FONT_OFFSET_Y UI_BORDER
#define BACKGROUND_OFFSET_Y \
    FONT_OFFSET_Y + UI_LINE_HEIGHT + (2 * UI_BORDER)

#define FOREGROUND_OFFSET_Y \
    BACKGROUND_OFFSET_Y + UI_LINE_HEIGHT + (2 * UI_BORDER)

#define TEXT_SCALE_OFFSET_Y \
    FOREGROUND_OFFSET_Y + UI_LINE_HEIGHT + (2 * UI_BORDER)

#define CURSOR_RATE_OFFSET_Y \
    TEXT_SCALE_OFFSET_Y + UI_LINE_HEIGHT + (2 * UI_BORDER)

#define TEXT_RATE_OFFSET_Y \
    CURSOR_RATE_OFFSET_Y + UI_LINE_HEIGHT + (2 * UI_BORDER)

#define TEXT_FILES_DESCRIPTION_OFFSET_Y \
    TEXT_RATE_OFFSET_Y + UI_LINE_HEIGHT + (2 * UI_BORDER)

#define TEXT_FILES_OFFSET_Y \
    TEXT_FILES_DESCRIPTION_OFFSET_Y + UI_LINE_HEIGHT + (2 * UI_BORDER)

#define TEXT_FILES_HEIGHT (4 * UI_LINE_HEIGHT)
#define TEXT_FILES_WIDTH UI_INITIAL_WIDTH - (2 * UI_BORDER)
#define OK_OFFSET_X \
    (UI_INITIAL_WIDTH / 2) - UI_BUTTON_WIDTH - (4 * UI_BORDER)

#define OK_CANCEL_OFFSET_Y \
    TEXT_FILES_OFFSET_Y + TEXT_FILES_HEIGHT + (2 * UI_BORDER)

#define CANCEL_OFFSET_X \
    (UI_INITIAL_WIDTH / 2) + (4 * UI_BORDER)

#define EDIT_BOX_OFFSET_X UI_DESCRIPTION_WIDTH + (2 * UI_BORDER)
#define EDIT_GREEN_OFFSET_X EDIT_BOX_OFFSET_X + UI_SMALL_BOX_WIDTH + UI_BORDER
#define EDIT_BLUE_OFFSET_X EDIT_GREEN_OFFSET_X + UI_SMALL_BOX_WIDTH + UI_BORDER

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//
