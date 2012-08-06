/*++

Copyright (c) 2010 Evan Green

Module Name:

    phosphor2.c

Abstract:

    This module implements the Phosphor2 screensaver, based on the original
    Phosphor XScreenSaver by Jamie Zawinski.

Author:

    Evan Green 14-Feb-2010

Environment:

    Windows

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <windows.h>
#include <windowsx.h>
#include <stdio.h>
#include "resource.h"

//
// ---------------------------------------------------------------- Definitions
//

#define APPLICATION_NAME "Phosphor2"

#define DLLEXPORT __declspec(dllexport)

#define ASCII_MIN 32
#define ASCII_MAX 127
#define CURSOR_CHARACTER ASCII_MAX
#define INITIAL_FILE_COUNT 20
#define MAX_STRING (1024 * 12)
#define SETTLING_TIME 500000
#define MOUSE_TOLERANCE 5
#define DECAY_COUNT 10

//
// Configuration key names.
//

#define CONFIGURATION_FILE "Phosphor.ini"
#define KEY_FONT_NAME "FontName"
#define KEY_FOREGROUND_RED "ForegroundRed"
#define KEY_FOREGROUND_GREEN "ForegroundGreen"
#define KEY_FOREGROUND_BLUE "ForegroundBlue"
#define KEY_BACKGROUND_RED "BackgroundRed"
#define KEY_BACKGROUND_GREEN "BackgroundGreen"
#define KEY_BACKGROUND_BLUE "BackgroundBlue"
#define KEY_TEXT_SCALE "TextScale"
#define KEY_CURSOR_RATE "CursorRate"
#define KEY_TEXT_RATE "TextRate"
#define KEY_DECAY "Decay"
#define KEY_TEXT_FILES "TextFiles"

//
// ------------------------------------------------- Data Structure Definitions
//

typedef struct _CHARACTER_BITMAP {
    HDC Character;
    HBITMAP Bitmap;
    HBITMAP OldBitmap;
} CHARACTER_BITMAP, *PCHARACTER_BITMAP;

typedef struct _CELL {
    UCHAR Character;
    BOOLEAN Redraw;
    ULONG FadeIndex;
} CELL, *PCELL;

typedef struct _DIRECTORY_SEARCH {
    PSTR Directory;
    PSTR Pattern;
    BOOLEAN ChildrenEnumerated;
} DIRECTORY_SEARCH, *PDIRECTORY_SEARCH;

//
// -------------------------------------------------------- Function Prototypes
//

LRESULT
DLLEXPORT
WINAPI
ScreenSaverProc (
    HWND hWnd,
    UINT Message,
    WPARAM wParam,
    LPARAM lParam
    );

BOOL
DLLEXPORT
WINAPI
ScreenSaverConfigureDialog (
    HWND Dialog,
    UINT Message,
    WPARAM wParam,
    LPARAM lParam
    );

BOOLEAN
PhoInitialize (
    HWND Window
    );

VOID
PhoDestroy (
    );

VOID
CALLBACK
PhoTimerEvent (
    UINT TimerId,
    UINT Message,
    DWORD_PTR User,
    DWORD_PTR Parameter1,
    DWORD_PTR Parameter2
    );

BOOLEAN
PhopUpdate (
    ULONG Microseconds
    );

VOID
PhopUpdateDisplay (
    HWND Window
    );

VOID
PhopFadeCell (
    HDC Dc,
    ULONG CellX,
    ULONG CellY
    );

BOOL
PhopGetInput (
    PUCHAR Character
    );

BOOL
PhopCreateCharacters (
    HDC WindowDc,
    HFONT Font,
    COLORREF Foreground,
    COLORREF Background
    );

VOID
PhopDestroyCharacters (
    );

VOID
PhopCreateCharacter (
    HDC Source,
    ULONG SourceWidth,
    ULONG SourceHeight,
    HDC Destination,
    COLORREF Foreground,
    double Scale,
    double PenWidthScale
    );

BOOLEAN
PhopOutputCharacter (
    UCHAR Character
    );

VOID
PhopScroll (
    );

VOID
PhopPrintCharacter (
    HDC Window,
    UCHAR Character,
    ULONG XPosition,
    ULONG YPosition,
    ULONG FadeIndex
    );

BOOLEAN
PhopOpenNextFile (
    );

BOOLEAN
PhopEnumerateFiles (
    );

VOID
PhopEnumerateDirectories (
    PSTR Path,
    PSTR FilePattern
    );

VOID
PhopAddFile (
    PSTR Path,
    PSTR Filename
    );

VOID
PhopAddDirectory (
    PSTR Path,
    PSTR Directory,
    PSTR SearchTerm
    );

BOOLEAN
PhopTakeInParameters (
    HWND Window
    );

BOOLEAN
PhopSaveParameters (
    );

BOOLEAN
PhopLoadParameters (
    );

//
// -------------------------------------------------------------------- Globals
//

BOOLEAN ScreenSaverWindowed = FALSE;

//
// Phosphor Configuration.
//

PSTR PhoFontName = "MS Gothic";
ULONG PhoForegroundRed = 0x00;
ULONG PhoForegroundGreen = 0xFF;
ULONG PhoForegroundBlue = 0x30;
ULONG PhoBackgroundRed = 0x00;
ULONG PhoBackgroundGreen = 0x00;
ULONG PhoBackgroundBlue = 0x00;
double PhoScale = 6.0;
ULONG PhoTimerRateMs = 20;
ULONG PhoDecay = 6;
ULONG PhoCursorBlinkMs = 500;
ULONG PhoInputDelayUs = 50000;
PSTR PhoSearchPath = "";

//
// Phosphor State.
//

ULONG PhoCharacterWidth = 0;
ULONG PhoCharacterHeight = 0;
ULONG PhoCharacterMaxWidth = 0;
ULONG PhoXCells = 0;
ULONG PhoYCells = 0;
PCHARACTER_BITMAP PhoBitmaps = NULL;
PCELL PhoCell = NULL;
ULONG PhoCursorX = 0;
ULONG PhoCursorY = 0;
MMRESULT PhoTimer = 0;
HWND PhoWindow = NULL;
BOOLEAN PhoClear = TRUE;
COLORREF PhoBackground;
HANDLE PhoCurrentFile = INVALID_HANDLE_VALUE;
PSTR *PhoFiles = NULL;
ULONG PhoFileCount = 0;
ULONG PhoFileMaxCount = 0;
PDIRECTORY_SEARCH *PhoDirectories = NULL;
ULONG PhoDirectoryCount = 0;
ULONG PhoDirectoryMaxCount = 0;
PUCHAR PhoCurrentResource = NULL;
ULONG PhoCurrentResourceSize = 0;
POINT PhoMousePosition;

//
// Phosphor Timing State.
//

ULONG PhoCursorTimeUs = 0;
ULONG PhoInputTimeUs = 0;
ULONG PhoTotalTimeUs = 0;

//
// ------------------------------------------------------------------ Functions
//

INT
WINAPI
WinMain (
    HINSTANCE hInstance,
    HINSTANCE hPrevInst,
    LPSTR lpszCmdParam,
    INT nCmdShow
    )

/*++

Routine Description:

    This routine is the main entry point for a Win32 application.

Arguments:

    hInstance - Supplies a handle to the current instance of the application.

    hPrevInstance - Supplies a handle to the previous instance of the
        application.

    lpCmdLine - Supplies a pointer to a null-terminated string specifying the
        command line for the application, excluding the program name.

    nCmdShow - Specifies how the window is to be shown.

Return Value:

    Returns TRUE on success, FALSE on failure.

--*/

{

    WNDCLASS Class;
    BOOLEAN Configure;
    MSG Message;
    HWND Parent;
    RECT ParentRect;
    ULONG Properties;
    BOOLEAN Result;
    INT Return;
    HWND Window;
    ULONG WindowHeight;
    ULONG WindowWidth;

    Parent = NULL;
    WindowWidth = 1024;
    WindowHeight = 768;

    //
    // Parse any parameters. C runs the configure dialog.
    //

    Configure = FALSE;
    if ((strstr(lpszCmdParam, "/c") != NULL) ||
        (strstr(lpszCmdParam, "/C") != NULL)) {

        Configure = TRUE;
    }

    //
    // W runs the application in a window.
    //

    if ((strstr(lpszCmdParam, "/w") != NULL) ||
        (strstr(lpszCmdParam, "/W") != NULL)) {

        ScreenSaverWindowed = TRUE;
    }

    //
    // P or I also runs the application in a window with a parent.
    //

    if ((strstr(lpszCmdParam, "/p") != NULL) ||
        (strstr(lpszCmdParam, "/P") != NULL) ||
        (strstr(lpszCmdParam, "/i") != NULL) ||
        (strstr(lpszCmdParam, "/I") != NULL)) {


        Parent = (HWND)atoi(lpszCmdParam + 3);
        GetWindowRect(Parent, &ParentRect);
        if (IsWindow(Parent) == FALSE) {
            Return = 0;
            goto WinMainEnd;
        }

        WindowWidth = ParentRect.right - ParentRect.left;
        WindowHeight = ParentRect.bottom - ParentRect.top;
        ScreenSaverWindowed = TRUE;
    }

    //
    // Register the window class.
    //

    Class.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    Class.lpfnWndProc = ScreenSaverProc;
    Class.cbClsExtra = 0;
    Class.cbWndExtra = 0;
    Class.hInstance = hInstance;
    Class.hIcon = NULL; //LoadIcon(hInstance, MAKEINTRESOURCE(101));
    Class.hCursor = NULL;
    Class.hbrBackground = NULL;
    Class.lpszMenuName = NULL;
    Class.lpszClassName = APPLICATION_NAME;
    Result = RegisterClass(&Class);
    if (Result == FALSE) {
        return 0;
    }

    PhopLoadParameters();

    //
    // For configurations, run the dialog box and quit.
    //

    if (Configure != FALSE) {
        DialogBox(GetModuleHandle(NULL),
                  MAKEINTRESOURCE(IDD_MAIN_WINDOW),
                  NULL,
                  ScreenSaverConfigureDialog);

        PhopSaveParameters();
        goto WinMainEnd;
    }

    //
    // Create the window.
    //

    if (ScreenSaverWindowed != FALSE) {
        if (Parent != NULL) {
            Properties = WS_VISIBLE | WS_CHILD;

        } else {
            Properties = WS_VISIBLE | WS_POPUP;
        }

        Window = CreateWindowEx(WS_EX_TOPMOST,
                                APPLICATION_NAME,
                                APPLICATION_NAME,
                                Properties,
                                0,
                                0,
                                WindowWidth,
                                WindowHeight,
                                Parent,
                                NULL,
                                hInstance,
                                NULL);

    } else {
        Window = CreateWindowEx(WS_EX_TOPMOST,
                                APPLICATION_NAME,
                                APPLICATION_NAME,
                                WS_VISIBLE | WS_POPUP,
                                0,
                                0,
                                GetSystemMetrics(SM_CXVIRTUALSCREEN),
                                GetSystemMetrics(SM_CYVIRTUALSCREEN),
                                NULL,
                                NULL,
                                hInstance,
                                NULL);
    }

    if (Window == NULL) {
        Return = 0;
        goto WinMainEnd;
    }

    if (ScreenSaverWindowed == FALSE) {
        ShowCursor(0);
    }

    SetFocus(Window);
    UpdateWindow(Window);

    //
    // Pump messages to the window.
    //

    while (GetMessage(&Message, NULL, 0, 0) > 0) {
        TranslateMessage(&Message);
        DispatchMessage(&Message);
    }

WinMainEnd:
    ShowCursor(1);
    UnregisterClass(APPLICATION_NAME, hInstance);
    return Return;
}

LRESULT
DLLEXPORT
WINAPI
ScreenSaverProc (
    HWND hWnd,
    UINT Message,
    WPARAM wParam,
    LPARAM lParam
    )

/*++

Routine Description:

    This routine is the main message pump for the screen saver window. It
    receives messages pertaining to the window and handles interesting ones.

Arguments:

    hWnd - Supplies the handle for the overall window.

    Message - Supplies the message being sent to the window.

    WParam - Supplies the "width" parameter, basically the first parameter of
        the message.

    LParam - Supplies the "length" parameter, basically the second parameter of
        the message.

Return Value:

    Returns FALSE if the message was handled, or TRUE if the message was not
    handled and the default handler should be invoked.

--*/

{

    HDC Dc;
    POINT Difference;
    POINT MousePosition;
    BOOL Result;
    RECT ScreenRect;

    switch (Message) {
    case WM_CREATE:
        Result = PhoInitialize(hWnd);
        if (Result == FALSE) {
            PostQuitMessage(0);
        }

        GetCursorPos(&PhoMousePosition);
        break;

    case WM_ERASEBKGND:
        Dc = GetDC(hWnd);
        GetClientRect(hWnd, &ScreenRect);
        FillRect(Dc, &ScreenRect, (HBRUSH)GetStockObject(BLACK_BRUSH));
        ReleaseDC(hWnd, Dc);
        break;

    case WM_DESTROY:
        PhoDestroy();
        PostQuitMessage(0);
        break;

    case WM_SETCURSOR:
        if (ScreenSaverWindowed == FALSE) {
            SetCursor(NULL);
        }

        break;

    case WM_CLOSE:
        if (ScreenSaverWindowed == FALSE) {
            ShowCursor(1);
        }

        break;

    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN:
    case WM_MBUTTONDOWN:
    case WM_KEYDOWN:
    case WM_KEYUP:
        if (PhoTotalTimeUs > SETTLING_TIME) {
            SendMessage(hWnd, WM_CLOSE, 0, 0);
        }

        break;

    case WM_MOUSEMOVE:

        //
        // Ignore mouse movements if the screen saver is in the preview window.
        //

        if (ScreenSaverWindowed != FALSE) {
            break;
        }

        //
        // Random little mouse movements or spurious messages need to be
        // tolerated. If the mouse has moved more than a few pixels in any
        // direction, the user really is controlling it, and the screensaver
        // needs to close.
        //

        GetCursorPos(&MousePosition);
        Difference.x = PhoMousePosition.x - MousePosition.x;
        Difference.y = PhoMousePosition.y - MousePosition.y;
        if (Difference.x < 0) {
            Difference.x = -Difference.x;
        }

        if (Difference.y < 0) {
            Difference.y = -Difference.y;
        }

        if (((Difference.x > MOUSE_TOLERANCE) ||
             (Difference.y > MOUSE_TOLERANCE)) &&
            (PhoTotalTimeUs > SETTLING_TIME)) {

            SendMessage(hWnd, WM_CLOSE, 0, 0);
        }

        break;

    case WM_SYSCOMMAND:
        if((wParam == SC_SCREENSAVE) || (wParam == SC_CLOSE)) {
            return FALSE;
        }

        break;
    }

    return DefWindowProc(hWnd, Message, wParam, lParam);
}

BOOL
DLLEXPORT
WINAPI
ScreenSaverConfigureDialog (
    HWND Dialog,
    UINT Message,
    WPARAM wParam,
    LPARAM lParam
    )

/*++

Routine Description:

    This routine is the main message pump for the screen saver configuration
    window. It processes interesting messages coming from the user interacting
    with the window.

Arguments:

    Dialog - Supplies the handle for the overall window.

    Message - Supplies the message being sent to the window.

    wParam - Supplies the "width" parameter, basically the first parameter of
        the message.

    wParam - Supplies the "length" parameter, basically the second parameter of
        the message.

Return Value:

    Returns FALSE if the message was handled, or TRUE if the message was not
    handled and the default handler should be invoked.

--*/

{

    HWND EditBox;
    BOOLEAN Result;
    PSTR String;

    switch(Message) {
    case WM_INITDIALOG:
        String = malloc(1024);
        if (String == NULL) {
            return TRUE;
        }

        //
        // Load the edits with the current values (or defaults).
        //

        EditBox = GetDlgItem(Dialog, IDE_FONT);
        Edit_SetText(EditBox, PhoFontName);
        EditBox = GetDlgItem(Dialog, IDE_BACKGROUND_RED);
        sprintf(String, "%d", (UINT)PhoBackgroundRed);
        Edit_SetText(EditBox, String);
        EditBox = GetDlgItem(Dialog, IDE_BACKGROUND_GREEN);
        sprintf(String, "%d", (UINT)PhoBackgroundGreen);
        Edit_SetText(EditBox, String);
        EditBox = GetDlgItem(Dialog, IDE_BACKGROUND_BLUE);
        sprintf(String, "%d", (UINT)PhoBackgroundBlue);
        Edit_SetText(EditBox, String);
        EditBox = GetDlgItem(Dialog, IDE_FOREGROUND_RED);
        sprintf(String, "%d", (UINT)PhoForegroundRed);
        Edit_SetText(EditBox, String);
        EditBox = GetDlgItem(Dialog, IDE_FOREGROUND_GREEN);
        sprintf(String, "%d", (UINT)PhoForegroundGreen);
        Edit_SetText(EditBox, String);
        EditBox = GetDlgItem(Dialog, IDE_FOREGROUND_BLUE);
        sprintf(String, "%d", (UINT)PhoForegroundBlue);
        Edit_SetText(EditBox, String);
        EditBox = GetDlgItem(Dialog, IDE_TEXT_SCALE);
        sprintf(String, "%.2f", (float)PhoScale);
        Edit_SetText(EditBox, String);
        EditBox = GetDlgItem(Dialog, IDE_CURSOR_RATE);
        sprintf(String, "%d", (UINT)PhoCursorBlinkMs);
        Edit_SetText(EditBox, String);
        EditBox = GetDlgItem(Dialog, IDE_TEXT_RATE);
        sprintf(String, "%d", (UINT)PhoInputDelayUs);
        Edit_SetText(EditBox, String);
        EditBox = GetDlgItem(Dialog, IDE_DECAY);
        sprintf(String, "%d", (UINT)PhoDecay);
        Edit_SetText(EditBox, String);
        EditBox = GetDlgItem(Dialog, IDE_TEXT_FILES);
        sprintf(String, "%s", PhoSearchPath);
        Edit_SetText(EditBox, String);
        free(String);
        return TRUE;

    case WM_HSCROLL:
        break;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDOK:
        case IDC_OK:

        //
        // Save all the valid entries. Bail on invalid ones.
        //

        Result = PhopTakeInParameters(Dialog);
        if (Result == FALSE) {
            break;
        }

        //
        // Fall through on success.
        //

        case IDCANCEL:
        case IDC_CANCEL:
            EndDialog(Dialog, LOWORD(wParam) == IDOK);
            return TRUE;
        }

        break;
    }

    return FALSE;
}

//
// --------------------------------------------------------- Internal Functions
//

BOOLEAN
PhoInitialize (
    HWND Window
    )

/*++

Routine Description:

    This routine initializes the Phosphor2 screen saver.

Arguments:

    None.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    HDC Dc;
    HFONT Font;
    COLORREF Foreground;
    BOOL Result;
    RECT WindowSize;

    Dc = NULL;

    //
    // Save the window.
    //

    PhoWindow = Window;

    //
    // Load the font.
    //

    Font = CreateFont(0,
                      0,
                      0,
                      0,
                      FW_NORMAL,
                      FALSE,
                      FALSE,
                      FALSE,
                      DEFAULT_CHARSET,
                      OUT_DEFAULT_PRECIS,
                      CLIP_DEFAULT_PRECIS,
                      DEFAULT_QUALITY,
                      DEFAULT_PITCH | FF_DONTCARE,
                      PhoFontName);

    if (Font == NULL) {
        Result = FALSE;
        goto InitializeEnd;
    }

    Foreground = RGB(PhoForegroundRed, PhoForegroundGreen, PhoForegroundBlue);
    PhoBackground = RGB(PhoBackgroundRed,
                        PhoBackgroundGreen,
                        PhoBackgroundBlue);

    //
    // Generate the character bitmaps.
    //

    Dc = GetDC(Window);
    Result = PhopCreateCharacters(Dc, Font, Foreground, PhoBackground);
    if (Result == FALSE) {
        MessageBox(NULL,
                   "Error creating characters.",
                   "Whoopsy!",
                   MB_OK);

        goto InitializeEnd;
    }

    ReleaseDC(Window, Dc);
    Dc = NULL;

    //
    // Determine the size of the cell grid and allocate space for the cells.
    //

    Result = GetWindowRect(Window, &WindowSize);
    if (Result == FALSE) {
        goto InitializeEnd;
    }

    PhoXCells = (WindowSize.right - WindowSize.left) / PhoCharacterWidth;
    PhoYCells = (WindowSize.bottom - WindowSize.top) / PhoCharacterHeight;
    PhoCell = malloc(sizeof(CELL) * PhoXCells * PhoYCells);
    if (PhoCell == NULL) {
        Result = FALSE;
        goto InitializeEnd;
    }

    //
    // Enumerate all files in the search paths.
    //

    Result = PhopEnumerateFiles();
    if (Result == FALSE) {
        goto InitializeEnd;
    }

    //
    // Kick off the timer.
    //

    PhoTimer = timeSetEvent(PhoTimerRateMs,
                            PhoTimerRateMs,
                            PhoTimerEvent,
                            PhoTimerRateMs * 1000,
                            TIME_PERIODIC | TIME_CALLBACK_FUNCTION);

    if (PhoTimer == 0) {
        Result = FALSE;
        goto InitializeEnd;
    }

    Result = TRUE;

InitializeEnd:
    if (Result == FALSE) {
        PhopDestroyCharacters();
    }

    if (Dc != NULL) {
        ReleaseDC(Window, Dc);
    }

    if (Font != NULL) {
        DeleteObject(Font);
    }

    return Result;
}

VOID
PhoDestroy (
    )

/*++

Routine Description:

    This routine tears down the Phosphor runtime.

Arguments:

    None.

Return Value:

    None.

--*/

{

    ULONG Index;

    if (PhoTimer != 0) {
        timeKillEvent(PhoTimer);
    }

    if (PhoCurrentFile != INVALID_HANDLE_VALUE) {
        CloseHandle(PhoCurrentFile);
    }

    //
    // Free all files.
    //

    if (PhoFiles != NULL) {
        for (Index = 0; Index < PhoFileCount; Index += 1) {
            if (PhoFiles[Index] != NULL) {
                free(PhoFiles[Index]);
            }
        }

        free(PhoFiles);
    }

    PhoFiles = NULL;
    PhoFileCount = 0;
    PhoFileMaxCount = 0;

    //
    // Free all directories.
    //

    if (PhoDirectories != NULL) {
        for (Index = 0; Index < PhoDirectoryCount; Index += 1) {
            if (PhoDirectories[Index] != NULL) {
                free(PhoDirectories[Index]);
            }
        }

        free(PhoDirectories);
    }

    PhoDirectories = NULL;
    PhoDirectoryCount = 0;
    PhoDirectoryMaxCount = 0;
    PhopDestroyCharacters();
    return;
}

VOID
CALLBACK
PhoTimerEvent (
    UINT TimerId,
    UINT Message,
    DWORD_PTR User,
    DWORD_PTR Parameter1,
    DWORD_PTR Parameter2
    )

/*++

Routine Description:

    This routine is the callback from the timer.

Arguments:

    TimerId - Supplies the timer ID that fired. Unused.

    Message - Supplies the timer message. Unused.

    User - Supplies the user paramater. In this case this is the period of the
        timer.

    Parameter1 - Supplies an unused parameter.

    Parameter2 - Supplies an unused parameter.

Return Value:

    None.

--*/

{

    BOOLEAN Result;

    Result = PhopUpdate((ULONG)User);
    if (Result == FALSE) {
        PostQuitMessage(0);
    }

    return;
}

BOOLEAN
PhopUpdate (
    ULONG Microseconds
    )

/*++

Routine Description:

    This routine updates the Phosphor runtime.

Arguments:

    Microseconds - Supplies the number of microseconds that have gone by since
        the last update.

Return Value:

    TRUE on success.

    FALSE if a serious failure occurred.

--*/

{

    HBRUSH BackgroundBrush;
    ULONG CursorCellIndex;
    HDC Dc;
    UCHAR Input;
    HBRUSH OriginalBrush;
    BOOLEAN Result;
    ULONG XIndex;
    ULONG YIndex;

    if (Microseconds == 0) {
        return FALSE;
    }

    Dc = GetDC(PhoWindow);

    //
    // Update main time.
    //

    PhoTotalTimeUs += Microseconds;

    //
    // If the window has not been set up, clear everything now.
    //

    if (PhoClear != FALSE) {

        //
        // Initialize the cells to have nothing in them.
        //

        for (YIndex = 0; YIndex < PhoYCells; YIndex += 1) {
            for (XIndex = 0; XIndex < PhoXCells; XIndex += 1) {
                PhoCell[(YIndex * PhoXCells) + XIndex].Character = ' ';
                PhoCell[(YIndex * PhoXCells) + XIndex].Redraw = TRUE;
                PhoCell[(YIndex * PhoXCells) + XIndex].FadeIndex = PhoDecay;
            }
        }

        //
        // Set the cursor at the beginning of the file.
        //

        PhoCursorX = 0;
        PhoCursorY = 0;

        //
        // Wipe the screen.
        //

        BackgroundBrush = CreateSolidBrush(PhoBackground);
        OriginalBrush = SelectObject(Dc, BackgroundBrush);
        ExtFloodFill(Dc, 0, 0, GetPixel(Dc, 0, 0), FLOODFILLSURFACE);
        SelectObject(Dc, OriginalBrush);
        DeleteObject(OriginalBrush);
        PhoClear = FALSE;
    }

    //
    // Update the cursor.
    //

    PhoCursorTimeUs += Microseconds;
    if (PhoCursorTimeUs >= PhoCursorBlinkMs * 1000) {
        CursorCellIndex = (PhoCursorY * PhoXCells) + PhoCursorX;

        //
        // If the cell was empty, fill it with the cursor character.
        //

        if (PhoCell[CursorCellIndex].Character == ' ') {
            PhoCell[CursorCellIndex].Character = CURSOR_CHARACTER;
            PhoCell[CursorCellIndex].FadeIndex = PhoDecay;

        //
        // Otherwise, begin the fade.
        //

        } else {
            if (PhoCell[CursorCellIndex].FadeIndex == 0) {
                PhoCell[CursorCellIndex].Character = ' ';
                PhoCell[CursorCellIndex].FadeIndex = PhoDecay;

            } else if (PhoCell[CursorCellIndex].FadeIndex == PhoDecay) {
                PhoCell[CursorCellIndex].FadeIndex -= 1;
            }
        }

        PhoCell[CursorCellIndex].Redraw = TRUE;
        PhoCursorTimeUs = 0;
    }

    //
    // Update the input timer, and grab more input if it's appropriate.
    //

    PhoInputTimeUs += Microseconds;
    while (PhoInputTimeUs > PhoInputDelayUs) {
        do {
            Result = PhopGetInput(&Input);
            if (Result == FALSE) {

                //
                // Switch to a new input source.
                //

                PhoClear = TRUE;
                Result = PhopOpenNextFile();
                if (Result == FALSE) {
                    goto UpdateEnd;
                }

                continue;

            }

            //
            // Attempt to put the output on the screen. If it was not a
            // printable character, loop.
            //

            Result = PhopOutputCharacter(Input);
        } while (Result == FALSE);

        PhoInputTimeUs -= PhoInputDelayUs;
    }

    //
    // Update the display.
    //

    PhopUpdateDisplay(PhoWindow);
    Result = TRUE;

UpdateEnd:
    ReleaseDC(PhoWindow, Dc);
    return Result;
}

VOID
PhopUpdateDisplay (
    HWND Window
    )

/*++

Routine Description:

    This routine refreshes the display window.

Arguments:

    Window - Supplies a pointer to the screen window.

Return Value:

    None.

--*/

{

    ULONG CellIndex;
    HDC Dc;
    ULONG Shade;
    ULONG XIndex;
    ULONG YIndex;

    Dc = GetDC(PhoWindow);
    for (YIndex = 0; YIndex < PhoYCells; YIndex += 1) {
        for (XIndex = 0; XIndex < PhoXCells; XIndex += 1) {
            CellIndex = (YIndex * PhoXCells) + XIndex;
            if (PhoCell[CellIndex].Redraw != FALSE) {
                Shade = PhoCell[CellIndex].FadeIndex * DECAY_COUNT / PhoDecay;
                PhopPrintCharacter(Dc,
                                   PhoCell[CellIndex].Character,
                                   XIndex,
                                   YIndex,
                                   Shade);

                //
                // If a cell is currently in transition to blank, keep it
                // moving. If this was the last cell before blank, make the
                // cell blank, and print one more time.
                //

                if (PhoCell[CellIndex].FadeIndex < PhoDecay) {
                    if (PhoCell[CellIndex].FadeIndex == 0) {
                        PhoCell[CellIndex].Character = ' ';
                        PhoCell[CellIndex].FadeIndex = PhoDecay;

                    } else {
                        PhoCell[CellIndex].FadeIndex -= 1;
                    }

                //
                // If the cell was not on its way to blank, it was just
                // refreshed, so no more action is needed.
                //

                } else {
                    PhoCell[CellIndex].Redraw = FALSE;
                }
            }
        }
    }

    ReleaseDC(PhoWindow, Dc);
    InvalidateRect(PhoWindow, NULL, FALSE);
    return;
}

BOOL
PhopGetInput (
    PUCHAR Character
    )

/*++

Routine Description:

    This routine gets a character from the input buffer.

Arguments:

    Character - Supplies a pointer that will receive the byte from the input.

Return Value:

    TRUE on success.

    FALSE if no more input is available.

--*/

{

    ULONG BytesRead;
    UCHAR Data;
    BOOLEAN Result;

    //
    // For normal files, read from the file handle.
    //

    if (PhoFileCount != 0) {
        if (PhoCurrentFile == INVALID_HANDLE_VALUE) {
            Result = FALSE;
            goto GetInputEnd;
        }

        Result = ReadFile(PhoCurrentFile, &Data, sizeof(UCHAR), &BytesRead, NULL);
        if ((Result == FALSE) || (BytesRead != sizeof(UCHAR))) {
            Result = FALSE;
            goto GetInputEnd;
        }

        *Character = Data;

    //
    // If no files were listed, then data is read from the embedded resources.
    //

    } else {
        if (PhoCurrentResource == NULL) {
            Result = FALSE;
            goto GetInputEnd;
        }

        if (PhoCurrentResourceSize == 0) {
            Result = FALSE;
            PhoCurrentResource = NULL;
            goto GetInputEnd;
        }

        *Character = *PhoCurrentResource;
        PhoCurrentResource += 1;
        PhoCurrentResourceSize -= 1;
    }

    Result = TRUE;

GetInputEnd:
    if ((Result == FALSE) && (PhoCurrentFile != INVALID_HANDLE_VALUE)) {
        CloseHandle(PhoCurrentFile);
        PhoCurrentFile = INVALID_HANDLE_VALUE;
    }

    return Result;
}

BOOL
PhopCreateCharacters (
    HDC WindowDc,
    HFONT Font,
    COLORREF Foreground,
    COLORREF Background
    )

/*++

Routine Description:

    This routine creates the character bitmaps.

Arguments:

    WindowDc - Supplies a pointer to the device context for the screen.

    Font - Supplies a pointer to the loaded font of the file.

    Foreground - Supplies a pointer to the foreground color.

    Background - Supplies a pointer to the background color.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    HBRUSH BackgroundBrush;
    ULONG BitmapIndex;
    ULONG Blue;
    COLORREF BlurColor;
    COLORREF BrightColor;
    UCHAR Character;
    ULONG CharacterCount;
    ULONG CharacterIndex;
    ULONG FadeIndex;
    HBRUSH ForegroundBrush;
    ULONG Green;
    ULONG ListSize;
    HBRUSH OriginalBrush;
    HFONT OriginalFont;
    ULONG OriginalHeight;
    ULONG OriginalWidth;
    HDC RawCharacter;
    HBITMAP RawCharacterBitmap;
    HBITMAP RawCharacterOriginalBitmap;
    ULONG Red;
    HDC ScaledCharacter;
    HBITMAP ScaledCharacterBitmap;
    HBITMAP ScaledCharacterOriginalBitmap;
    BOOL Result;
    TEXTMETRIC TextMetric;

    CharacterCount = ASCII_MAX - ASCII_MIN + 1;
    RawCharacter = NULL;
    RawCharacterBitmap = NULL;
    RawCharacterOriginalBitmap = NULL;

    //
    // Create the background color brush.
    //

    BackgroundBrush = CreateSolidBrush(Background);

    //
    // Determine the text width and height.
    //

    OriginalFont = SelectObject(WindowDc, Font);
    if (GetTextMetrics(WindowDc, &TextMetric) == 0) {
        SelectObject(WindowDc, OriginalFont);
        Result = FALSE;
        goto CreateCharactersEnd;
    }

    SelectObject(WindowDc, OriginalFont);
    OriginalWidth = TextMetric.tmAveCharWidth;
    OriginalHeight = TextMetric.tmAscent + TextMetric.tmDescent;
    PhoCharacterWidth = OriginalWidth * PhoScale;
    PhoCharacterHeight = OriginalHeight * PhoScale;

    //
    // Allocate space for the bitmap structures.
    //

    ListSize = (DECAY_COUNT + 1) * CharacterCount * sizeof(CHARACTER_BITMAP);
    PhoBitmaps = malloc(ListSize);
    if (PhoBitmaps == NULL) {
        MessageBox(NULL, "Failed to allocate memory.", "Out of memory", MB_OK);
        Result = FALSE;
        goto CreateCharactersEnd;
    }

    memset(PhoBitmaps, 0, ListSize);

    //
    // Create each printable character.
    //

    for (CharacterIndex = 0;
         CharacterIndex < CharacterCount;
         CharacterIndex += 1) {

        Character = ASCII_MIN + CharacterIndex;

        //
        // Create a memory DC and its bitmap.
        //

        RawCharacter = CreateCompatibleDC(WindowDc);
        if (RawCharacter == NULL) {
            Result = FALSE;
            goto CreateCharactersEnd;
        }

        RawCharacterBitmap = CreateCompatibleBitmap(RawCharacter,
                                                    OriginalWidth,
                                                    OriginalHeight);

        if (RawCharacterBitmap == NULL) {
            Result = FALSE;
            goto CreateCharactersEnd;
        }

        RawCharacterOriginalBitmap =
                                SelectObject(RawCharacter, RawCharacterBitmap);

        //
        // For the cursor character, fill the cell with the foreground color.
        //

        if (Character == CURSOR_CHARACTER) {
            ForegroundBrush = GetStockObject(WHITE_BRUSH);
            OriginalBrush = SelectObject(RawCharacter, ForegroundBrush);
            ExtFloodFill(RawCharacter,
                         0,
                         0,
                         GetPixel(RawCharacter, 0, 0),
                         FLOODFILLSURFACE);

            SelectObject(RawCharacter, OriginalBrush);
            DeleteObject(ForegroundBrush);

        //
        // For any normal character, fill the cell with the background color
        // and print the character.
        //

        } else {
            OriginalBrush =
                       SelectObject(RawCharacter, GetStockObject(WHITE_BRUSH));

            ExtFloodFill(RawCharacter,
                         0,
                         0,
                         GetPixel(RawCharacter, 0, 0),
                         FLOODFILLSURFACE);

            SelectObject(RawCharacter, OriginalBrush);

            //
            // Print the given character.
            //

            OriginalFont = SelectObject(RawCharacter, Font);
            SetTextColor(RawCharacter, RGB(0xFF, 0xFF, 0xFF));
            SetBkColor(RawCharacter, RGB(0x00, 0x00, 0x00));
            TextOut(RawCharacter, 0, 0, &Character, 1);
        }

        //
        // Create the larger bitmap for each frame in the fade from all on
        // to the faintest hint.
        //

        for (FadeIndex = 0; FadeIndex <= DECAY_COUNT; FadeIndex += 1) {
            ScaledCharacter = CreateCompatibleDC(WindowDc);
            if (ScaledCharacter == NULL) {
                Result = FALSE;
                goto CreateCharactersEnd;
            }

            ScaledCharacterBitmap = CreateCompatibleBitmap(WindowDc,
                                                           PhoCharacterWidth,
                                                           PhoCharacterHeight);

            if (ScaledCharacterBitmap == NULL) {
                Result = FALSE;
                goto CreateCharactersEnd;
            }

            ScaledCharacterOriginalBitmap =
                             SelectObject(ScaledCharacter, ScaledCharacterBitmap);

            //
            // Fill the destination with the background color.
            //

            OriginalBrush = SelectObject(ScaledCharacter, BackgroundBrush);
            ExtFloodFill(ScaledCharacter,
                         0,
                         0,
                         GetPixel(ScaledCharacter, 0, 0),
                         FLOODFILLSURFACE);

            SelectObject(ScaledCharacter, OriginalBrush);

            //
            // Figure out what the brightest color is for this cell in the fade
            // progression. The brightest color is somewhere between the
            // background and the foreground.
            //

            Red = (UCHAR)((double)(GetRValue(Foreground) -
                                   GetRValue(Background)) *
                          (double)(FadeIndex + 1) / (double)(DECAY_COUNT + 1));

            Green = (UCHAR)((double)(GetGValue(Foreground) -
                                     GetGValue(Background)) *
                            (double)(FadeIndex + 1) /
                            (double)(DECAY_COUNT + 1));

            Blue = (UCHAR)((double)(GetBValue(Foreground) -
                                    GetBValue(Background)) *
                           (double)(FadeIndex + 1) /
                           (double)(DECAY_COUNT + 1));

            BrightColor = RGB(Red + GetRValue(Background),
                              Green + GetGValue(Background),
                              Blue + GetBValue(Background));

            //
            // Create the shadow color half way between the foreground and
            // background.
            //

            Red = (GetRValue(BrightColor) + GetRValue(Background)) / 2;
            Green = (GetGValue(BrightColor) + GetGValue(Background)) / 2;
            Blue = (GetBValue(BrightColor) + GetBValue(Background)) / 2;
            BlurColor = RGB(Red, Green, Blue);

            //
            // Lay down the blur color.
            //

            PhopCreateCharacter(RawCharacter,
                                OriginalWidth,
                                OriginalHeight,
                                ScaledCharacter,
                                BlurColor,
                                PhoScale,
                                1.3);

            //
            // Now print the main color.
            //

            PhopCreateCharacter(RawCharacter,
                                OriginalWidth,
                                OriginalHeight,
                                ScaledCharacter,
                                BrightColor,
                                PhoScale,
                                0.8);

            //
            // Save the bitmaps.
            //

            BitmapIndex = CharacterIndex + (FadeIndex * CharacterCount);
            PhoBitmaps[BitmapIndex].Character = ScaledCharacter;
            PhoBitmaps[BitmapIndex].Bitmap = ScaledCharacterBitmap;
            PhoBitmaps[BitmapIndex].OldBitmap = ScaledCharacterOriginalBitmap;
            ScaledCharacter = NULL;
        }

        //
        // Delete the original bitmap.
        //

        SelectObject(RawCharacter, RawCharacterOriginalBitmap);
        DeleteObject(RawCharacterBitmap);
        DeleteDC(RawCharacter);
        RawCharacter = NULL;
    }

    Result = TRUE;

CreateCharactersEnd:
    DeleteObject(BackgroundBrush);
    if (Result == FALSE) {
        if (RawCharacter != NULL) {
            SelectObject(RawCharacter, RawCharacterOriginalBitmap);
            DeleteObject(RawCharacterBitmap);
            DeleteDC(RawCharacter);
        }

        if (ScaledCharacter != NULL) {
            SelectObject(ScaledCharacter, ScaledCharacterOriginalBitmap);
            DeleteObject(ScaledCharacterBitmap);
            DeleteDC(ScaledCharacter);
        }
    }

    return Result;
}

VOID
PhopDestroyCharacters (
    )

/*++

Routine Description:

    This routine destroys the character bitmaps.

Arguments:

    None.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    ULONG CharacterCount;
    ULONG Index;

    CharacterCount = (ASCII_MAX - ASCII_MIN + 1) * (DECAY_COUNT + 1);
    for (Index = 0; Index < CharacterCount; Index += 1) {
        if (PhoBitmaps[Index].Character != NULL) {
            SelectObject(PhoBitmaps[Index].Character,
                         PhoBitmaps[Index].OldBitmap);

            DeleteObject(PhoBitmaps[Index].Bitmap);
            PhoBitmaps[Index].Bitmap = NULL;
            DeleteDC(PhoBitmaps[Index].Character);
            PhoBitmaps[Index].Character = NULL;
        }
    }

    free(PhoBitmaps);
    PhoBitmaps = NULL;
    return;
}

VOID
PhopCreateCharacter (
    HDC Source,
    ULONG SourceWidth,
    ULONG SourceHeight,
    HDC Destination,
    COLORREF Foreground,
    double Scale,
    double PenWidthScale
    )

/*++

Routine Description:

    This routine creates the terminal-esque bitmap from the standard rendering.

Arguments:

    Sources - Supplies a pointer to the source image.

    SourceWidth - Supplies the width of the source, in pixels.

    SourceHeight - Supplies the height of the source, in pixels.

    Destination - Supplies a pointer to the larger destination image.

    Foreground - Supplies the foreground color of the character.

    Scale - Supplies the scaling factor between source and destination images.

    PenWidthScale - Supplies the scaling factor to apply to the pen width in
        addition to the scale. The pen width will be Scale * PenWidthScale.

Return Value:

    None. The image is written to the destination.

--*/

{

    COLORREF Black;
    LOGBRUSH BrushStyle;
    ULONG LineEndX;
    DWORD LineWidth;
    double Offset;
    HPEN OriginalPen;
    HPEN Pen;
    DWORD PenStyle;
    COLORREF Pixel;
    ULONG SourceX;
    ULONG SourceY;

    Black = RGB(0, 0, 0);

    //
    // Create the pen and select it.
    //

    PenStyle = PS_GEOMETRIC | PS_SOLID | PS_ENDCAP_ROUND | PS_JOIN_ROUND;
    BrushStyle.lbStyle = BS_SOLID;
    BrushStyle.lbColor = Foreground;
    BrushStyle.lbHatch = 0;
    LineWidth = (DWORD)(Scale * PenWidthScale);
    if (LineWidth < 1) {
        LineWidth = 1;
    }

    Pen = ExtCreatePen(PenStyle, LineWidth, &BrushStyle, 0, NULL);
    if (Pen == NULL) {
        goto CreateCharacterEnd;
    }

    OriginalPen = SelectObject(Destination, Pen);

    //
    // Loop over each pixel in the source.
    //

    Offset = Scale / 2.0;
    for (SourceY = 0; SourceY < SourceHeight; SourceY += 1) {
        for (SourceX = 0; SourceX < SourceWidth; SourceX += 1) {
            Pixel = GetPixel(Source, SourceX, SourceY);

            //
            // If the pixel is set, then go right until the pixel is no longer
            // set, then draw the line covering all the set pixels.
            //

            if (Pixel != Black) {
                for (LineEndX = SourceX;
                     LineEndX < SourceWidth;
                     LineEndX += 1) {

                    Pixel = GetPixel(Source, LineEndX, SourceY);
                    if (Pixel == Black) {
                        break;
                    }
                }

                //
                // Back up one because the loop found the first pixel not set,
                // but the line should go to the last pixel that *was* set.
                //

                LineEndX -= 1;

                //
                // Draw the line.
                //

                MoveToEx(Destination,
                         (SourceX * Scale) + Offset,
                         SourceY * Scale,
                         NULL);

                LineTo(Destination,
                       (LineEndX * Scale) + Offset,
                       SourceY * Scale);

                //
                // Advance X to the line ending.
                //

                SourceX = LineEndX;
            }
        }
    }

CreateCharacterEnd:
    if (Pen != NULL) {
        DeleteObject(Pen);
    }

    return;
}

BOOLEAN
PhopOutputCharacter (
    UCHAR Character
    )

/*++

Routine Description:

    This routine prints a character at the current cursor location and
    advances the cursor.

Arguments:

    Character - Supplies a pointer to the character to print.

Return Value:

    TRUE if the character was printed to the screen.

    FALSE if the character was not printable.

--*/

{

    UCHAR BackedCharacter;
    ULONG CellIndex;
    UCHAR CursorCharacter;

    CellIndex = (PhoCursorY * PhoXCells) + PhoCursorX;
    CursorCharacter = PhoCell[CellIndex].Character;
    if (PhoCell[CellIndex].FadeIndex != PhoDecay) {
        CursorCharacter = ' ';
    }

    //
    // Determine if the character is printable.
    //

    if ((Character < ASCII_MIN) && (Character != '\n') && (Character != '\t') &&
        (Character != '\b')) {

        return FALSE;
    }

    if (Character >= ASCII_MAX) {
        return FALSE;
    }

    //
    // Convert tabs to 4 spaces.
    //

    if (Character == '\t') {
        PhopOutputCharacter(' ');
        PhopOutputCharacter(' ');
        PhopOutputCharacter(' ');
        PhopOutputCharacter(' ');
        return TRUE;
    }

    //
    // Handle a backspace.
    //

    if (Character == '\b') {
        if (PhoCursorX == 0) {
            if (PhoCursorY != 0) {
                PhoCursorY -= 1;
                PhoCursorX = PhoXCells - 1;
            }

        } else {
            PhoCursorX -= 1;
        }

        CellIndex = (PhoCursorY * PhoXCells) + PhoCursorX;
        BackedCharacter = PhoCell[CellIndex].Character;
        if (PhoCell[CellIndex].FadeIndex != PhoDecay) {
            BackedCharacter = ' ';
        }

        if (BackedCharacter != CursorCharacter) {
            PhoCell[CellIndex].Character = CursorCharacter;
            PhoCell[CellIndex].Redraw = TRUE;
            PhoCell[CellIndex].FadeIndex = PhoDecay;
        }

        return TRUE;
    }

    //
    // Handle a newline, and don't count it as printing anything.
    //

    if (Character == '\n') {

        //
        // Turn off the cursor if it was there, beginning the fade if needed.
        //

        if (CursorCharacter != ' ') {
            if (PhoCell[CellIndex].FadeIndex != 0) {
                PhoCell[CellIndex].FadeIndex -= 1;

            } else {
                PhoCell[CellIndex].Character = ' ';
                PhoCell[CellIndex].FadeIndex = PhoDecay;
            }

            PhoCell[CellIndex].Redraw = TRUE;
        }

        PhoCursorX = 0;
        if (PhoCursorY < PhoYCells - 1) {
            PhoCursorY += 1;

        } else {
            PhopScroll();
        }

        return FALSE;
    }

    //
    // This is a normal character. Update the cell.
    //

    PhoCell[CellIndex].Character = Character;
    PhoCell[CellIndex].Redraw = TRUE;
    PhoCell[CellIndex].FadeIndex = PhoDecay;

    //
    // Advance the cursor.
    //

    PhoCursorX += 1;
    if (PhoCursorX == PhoXCells) {
        PhoCursorX = 0;
        PhoCursorY += 1;
        if (PhoCursorY == PhoYCells) {
            PhoCursorY = PhoYCells - 1;
            PhopScroll();
        }
    }

    CellIndex = (PhoCursorY * PhoXCells) + PhoCursorX;

    //
    // Restore the cursor if it was previously printed.
    //

    if (PhoCell[CellIndex].Character != CursorCharacter) {
        PhoCell[CellIndex].Character = CursorCharacter;
        PhoCell[CellIndex].Redraw = TRUE;
        PhoCell[CellIndex].FadeIndex = PhoDecay;
    }

    return TRUE;
}

VOID
PhopScroll (
    )

/*++

Routine Description:

    This routine scrolls the screen by one line.

Arguments:

    None.

Return Value:

    None.

--*/

{

    UCHAR Character;
    ULONG DestinationIndex;
    ULONG XIndex;
    ULONG YIndex;
    ULONG SourceIndex;

    //
    // Scroll each line up that has a line below it.
    //

    for (YIndex = 0; YIndex < PhoYCells - 1; YIndex += 1) {
        for (XIndex = 0; XIndex < PhoXCells; XIndex += 1) {
            DestinationIndex = (YIndex * PhoXCells) + XIndex;
            SourceIndex = ((YIndex + 1) * PhoXCells) + XIndex;

            //
            // If the source character is in the process of fading, the
            // character is actually a space.
            //

            Character = PhoCell[SourceIndex].Character;
            if (PhoCell[SourceIndex].FadeIndex != PhoDecay) {
                Character = ' ';
            }

            //
            // If the destination and source are the same and the destination
            // wasn't in the process of fading, no action is needed.
            //

            if ((PhoCell[DestinationIndex].Character == Character) &&
                (PhoCell[DestinationIndex].FadeIndex == PhoDecay)) {

                continue;
            }

            //
            // Set the cell's character to the one below it. If it's a space,
            // begin the fade process.
            //

            if (Character != ' ') {
                PhoCell[DestinationIndex].Character = Character;
                PhoCell[DestinationIndex].FadeIndex = PhoDecay;

            } else if (PhoCell[DestinationIndex].FadeIndex == PhoDecay) {
                PhoCell[DestinationIndex].FadeIndex -= 1;
            }

            PhoCell[DestinationIndex].Redraw = TRUE;
        }
    }

    //
    // Blank out the last line.
    //

    YIndex = PhoYCells - 1;
    for (XIndex = 0; XIndex < PhoXCells; XIndex += 1) {
        DestinationIndex = (YIndex * PhoXCells) + XIndex;

        //
        // If the character is already empty or on its way, no action is needed.
        //

        if ((PhoCell[DestinationIndex].Character == ' ') ||
            (PhoCell[DestinationIndex].FadeIndex != PhoDecay)) {

            continue;
        }

        if (PhoCell[DestinationIndex].FadeIndex != 0) {
            PhoCell[DestinationIndex].FadeIndex -= 1;

        } else {
            PhoCell[DestinationIndex].Character = ' ';
            PhoCell[DestinationIndex].FadeIndex = PhoDecay;
        }

        PhoCell[DestinationIndex].Redraw = TRUE;
    }

    return;
}

VOID
PhopPrintCharacter (
    HDC Window,
    UCHAR Character,
    ULONG XPosition,
    ULONG YPosition,
    ULONG FadeIndex
    )

/*++

Routine Description:

    This routine prints a character on the screen at the given pixel
    coordinates.

Arguments:

    Window - Supplies a pointer to the window's device context.

    Character - Supplies the character to print.

    XPosition - Supplies the horizontal cell offset to print at.

    YPosition - Supplies the vertical cell offset to print at.

    FadeIndex - Supplies the fade index of the character to print.

Return Value:

    None. The image is written to the window.

--*/

{

    ULONG BitmapIndex;
    ULONG CharacterCount;

    //
    // Simply copy the character onto the window.
    //

    CharacterCount = ASCII_MAX - ASCII_MIN + 1;
    BitmapIndex = (Character - ASCII_MIN) + (FadeIndex * CharacterCount);
    BitBlt(Window,
           XPosition * PhoCharacterWidth,
           YPosition * PhoCharacterHeight,
           PhoCharacterWidth,
           PhoCharacterHeight,
           PhoBitmaps[BitmapIndex].Character,
           0,
           0,
           SRCCOPY);

    return;
}

BOOLEAN
PhopOpenNextFile (
    )

/*++

Routine Description:

    This routine opens the next file to be read and printed.

Arguments:

    None.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    ULONG FileIndex;
    LARGE_INTEGER Random;
    HRSRC Resource;
    HGLOBAL ResourceHandle;

    QueryPerformanceCounter(&Random);

    //
    // If files are specified by the user, use them.
    //

    if (PhoFileCount != 0) {
        if (PhoCurrentFile != INVALID_HANDLE_VALUE) {
            CloseHandle(PhoCurrentFile);
            PhoCurrentFile = INVALID_HANDLE_VALUE;
        }

        FileIndex = Random.LowPart % PhoFileCount;
        PhoCurrentFile = CreateFile(PhoFiles[FileIndex],
                                    GENERIC_READ,
                                    FILE_SHARE_READ,
                                    0,
                                    OPEN_EXISTING,
                                    FILE_FLAG_SEQUENTIAL_SCAN,
                                    NULL);

        if (PhoCurrentFile == INVALID_HANDLE_VALUE) {
            return FALSE;
        }

    //
    // If no files were specified, use the resources embedded in the image
    // (this source code!).
    //

    } else {
        FileIndex = Random.LowPart % SOURCE_FILE_COUNT;
        Resource = FindResource(NULL,
                                MAKEINTRESOURCE(IDR_SOURCE + FileIndex),
                                RT_RCDATA);

        if (Resource == NULL) {
            return FALSE;
        }

        ResourceHandle = LoadResource(NULL, Resource);
        PhoCurrentResource = LockResource(ResourceHandle);
        PhoCurrentResourceSize = SizeofResource(NULL, Resource);
    }

    return TRUE;
}

BOOLEAN
PhopEnumerateFiles (
    )

/*++

Routine Description:

    This routine creates a list of all files in the search path.

Arguments:

    None.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    BOOLEAN ChildrenEnumerated;
    PDIRECTORY_SEARCH CurrentDirectory;
    PSTR CurrentPath;
    ULONG DirectoryIndex;
    PSTR FilePattern;
    WIN32_FIND_DATA FindData;
    PSTR FullTerm;
    ULONG FullTermLength;
    PSTR NextSemicolon;
    ULONG OriginalMaxCount;
    BOOLEAN Result;
    HANDLE Search;
    PSTR SearchPath;
    PSTR WildCard;

    FullTerm = NULL;
    SearchPath = NULL;

    //
    // Make a copy of the search path that can be modified.
    //

    SearchPath = malloc(strlen(PhoSearchPath) + 1);
    if (SearchPath == NULL) {
        Result = FALSE;
        goto EnumerateFilesEnd;
    }

    strcpy(SearchPath, PhoSearchPath);

    //
    // Enumerate over all paths.
    //

    CurrentPath = SearchPath;
    while (TRUE) {

        //
        // Stop if this is the end of the string.
        //

        if ((CurrentPath == NULL) || (strlen(CurrentPath) == 0)) {
            break;
        }

        NextSemicolon = strchr(CurrentPath, ';');
        if (NextSemicolon != NULL) {
            *NextSemicolon = '\0';
        }

        //
        // If a * exists, then directories need to be traversed. Otherwise, this
        // points directly to a file.
        //

        WildCard = strchr(CurrentPath, '*');
        if (WildCard == NULL) {
            PhopAddFile(NULL, CurrentPath);

        } else {

            //
            // Add the directory and search term to the directories list. Its
            // children will be enumerated after the full initial list is built.
            //

            FilePattern = strrchr(CurrentPath, '\\');
            *FilePattern = '\0';
            FilePattern += 1;
            if (*FilePattern != '\0') {
                PhopAddDirectory(CurrentPath, NULL, FilePattern);

            } else {
                PhopAddDirectory(CurrentPath, NULL, "*");
            }
        }

        //
        // Advance to the next search path.
        //

        if (NextSemicolon == NULL) {
            break;
        }

        CurrentPath = NextSemicolon + 1;
    }

    //
    // Loop through the directories and flush out all subdirectories
    //

    do {
        ChildrenEnumerated = FALSE;
        OriginalMaxCount = PhoDirectoryMaxCount;
        for (DirectoryIndex = 0;
             DirectoryIndex < PhoDirectoryCount;
             DirectoryIndex += 1) {

            if (PhoDirectories[DirectoryIndex]->ChildrenEnumerated == FALSE) {
                CurrentDirectory = PhoDirectories[DirectoryIndex];
                PhopEnumerateDirectories(CurrentDirectory->Directory,
                                         CurrentDirectory->Pattern);

                CurrentDirectory->ChildrenEnumerated = TRUE;
                ChildrenEnumerated = TRUE;

                //
                // If the array shifted out from under the loop, start over.
                //

                if (PhoDirectoryMaxCount != OriginalMaxCount) {
                    break;
                }
            }
        }

    } while (ChildrenEnumerated != FALSE);

    //
    // Enumerate all files in all directories.
    //

    for (DirectoryIndex = 0;
         DirectoryIndex < PhoDirectoryCount;
         DirectoryIndex += 1) {

        //
        // Create the full search query, which is Directory\FilePattern.
        //

        FullTermLength = strlen(PhoDirectories[DirectoryIndex]->Directory) +
                         strlen(PhoDirectories[DirectoryIndex]->Pattern) + 2;

        FullTerm = malloc(FullTermLength);
        if (FullTerm == NULL) {
            Result = FALSE;
            goto EnumerateFilesEnd;
        }

        strcpy(FullTerm, PhoDirectories[DirectoryIndex]->Directory);
        strcat(FullTerm, "\\");
        strcat(FullTerm, PhoDirectories[DirectoryIndex]->Pattern);

        //
        // Add all files found. If the first term fails, move on to the next
        // search directory.
        //

        Search = FindFirstFileEx(FullTerm,
                                 1, //FindExInfoBasic
                                 &FindData,
                                 FindExSearchNameMatch,
                                 NULL,
                                 0);

        if (Search == INVALID_HANDLE_VALUE) {
            continue;
        }

        do {
            if ((FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
                PhopAddFile(PhoDirectories[DirectoryIndex]->Directory,
                            FindData.cFileName);
            }

            Result = FindNextFile(Search, &FindData);
        } while (Result != FALSE);

        FindClose(Search);
        free(FullTerm);
        FullTerm = NULL;
        Search = INVALID_HANDLE_VALUE;
    }

    Result = TRUE;

EnumerateFilesEnd:
    if (SearchPath != NULL) {
        free(SearchPath);
    }

    if (FullTerm != NULL) {
        free(FullTerm);
    }

    if (Search != INVALID_HANDLE_VALUE) {
        FindClose(Search);
    }

    return Result;
}

VOID
PhopEnumerateDirectories (
    PSTR Path,
    PSTR FilePattern
    )

/*++

Routine Description:

    This routine enumerates all subdirectories of the current path and adds
    them as search directories.

Arguments:

    Path - Supplies the directory path, without a trailing backslash.

    FilePattern - Supplies the file pattern to apply within the current
        directory.

Return Value:

    None.

--*/

{

    PSTR AllFiles;
    WIN32_FIND_DATA FindData;
    BOOLEAN Result;
    HANDLE Search;

    Search = INVALID_HANDLE_VALUE;
    AllFiles = malloc(strlen(Path) + 3);
    if (AllFiles == NULL) {
        goto EnumerateDirectoriesEnd;
    }

    strcpy(AllFiles, Path);
    strcat(AllFiles, "\\*");
    Search = FindFirstFileEx(AllFiles,
                             1, //FindExInfoBasic
                             &FindData,
                             FindExSearchLimitToDirectories,
                             NULL,
                             0);

    if (Search == INVALID_HANDLE_VALUE) {
        goto EnumerateDirectoriesEnd;
    }

    //
    // Loop until all directories in the current one are enumerated. The check
    // for directory attributes are required because the Limit To Directories
    // flag can be silently ignored.
    //

    do {
        if (((FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) &&
            (strcmp(FindData.cFileName, ".") != 0) &&
            (strcmp(FindData.cFileName, "..") != 0)) {

            PhopAddDirectory(Path, FindData.cFileName, FilePattern);
        }

        Result = FindNextFile(Search, &FindData);
    } while (Result != FALSE);

EnumerateDirectoriesEnd:
    if (AllFiles != NULL) {
        free(AllFiles);
    }

    if (Search != INVALID_HANDLE_VALUE) {
        FindClose(Search);
    }

    return;
}

VOID
PhopAddFile (
    PSTR Path,
    PSTR Filename
    )

/*++

Routine Description:

    This routine adds a file to the list of files to be printed.

Arguments:

    Path - Supplies an optional path to the file.

    Filename - Supplies the name of the file.

Return Value:

    None.

--*/

{

    PSTR FullName;
    ULONG FullNameLength;
    PSTR *NewFiles;
    ULONG NewMax;
    BOOLEAN Result;

    FullName = NULL;
    FullNameLength = strlen(Filename);
    if (Path != NULL) {
        FullNameLength += strlen(Path) + 1;
    }

    //
    // Allocate space for the new name and copy it in.
    //

    FullName = malloc(FullNameLength + 1);
    if (FullName == NULL) {
        Result = FALSE;
        goto AddFileEnd;
    }

    if (Path != NULL) {
        strcpy(FullName, Path);
        strcat(FullName, "\\");
        strcat(FullName, Filename);

    } else {
        strcpy(FullName, Filename);
    }

    //
    // If the buffer is full, allocate more space and copy everything in.
    //

    if (PhoFileCount >= PhoFileMaxCount) {
        NewMax = PhoFileMaxCount * 2;
        if (NewMax < INITIAL_FILE_COUNT) {
            NewMax = INITIAL_FILE_COUNT;
        }

        NewFiles = malloc(NewMax * sizeof(PSTR));
        if (NewFiles == NULL) {
            Result = FALSE;
            goto AddFileEnd;
        }

        memset(NewFiles, 0, NewMax * sizeof(PSTR));
        memcpy(NewFiles, PhoFiles, PhoFileCount * sizeof(PSTR));
        free(PhoFiles);
        PhoFiles = NewFiles;
        PhoFileMaxCount = NewMax;
    }

    PhoFiles[PhoFileCount] = FullName;
    PhoFileCount += 1;

    //
    // The operation was successful, don't free the full file name.
    //

    FullName = NULL;
    Result = TRUE;

AddFileEnd:
    if (FullName != NULL) {
        free(FullName);
    }

    return;
}

VOID
PhopAddDirectory (
    PSTR Path,
    PSTR Directory,
    PSTR SearchTerm
    )

/*++

Routine Description:

    This routine adds a directory to the search list.

Arguments:

    Path - Supplies the path of the directory.

    Directory - Supplies the directory to add.

    SearchTerm - Supplies the file term in the directory to search for.

Return Value:

    None.

--*/

{

    ULONG Length;
    PDIRECTORY_SEARCH *NewDirectories;
    PSTR NewDirectory;
    PDIRECTORY_SEARCH NewSearch;
    PSTR NewTerm;
    ULONG NewMax;
    BOOLEAN Result;

    NewDirectory = NULL;
    NewTerm = NULL;

    //
    // Allocate space for the new terms and copy them in.
    //

    Length = sizeof(DIRECTORY_SEARCH) + strlen(Path) + strlen(SearchTerm) + 2;
    if (Directory != NULL) {
        Length += strlen(Directory) + 1;
    }

    NewSearch = malloc(Length);
    if (NewSearch == NULL) {
        Result = FALSE;
        goto AddDirectoryEnd;
    }

    NewDirectory = (PSTR)(NewSearch + 1);
    strcpy(NewDirectory, Path);
    if (Directory != NULL) {
        strcat(NewDirectory, "\\");
        strcat(NewDirectory, Directory);
        NewTerm = NewDirectory + strlen(Path) + strlen(Directory) + 2;

    } else {
        NewTerm = NewDirectory + strlen(Path) + 1;
    }

    strcpy(NewTerm, SearchTerm);
    NewSearch->Directory = NewDirectory;
    NewSearch->Pattern = NewTerm;
    NewSearch->ChildrenEnumerated = FALSE;

    //
    // If the buffer is full, allocate more space and copy everything in.
    //

    if (PhoDirectoryCount >= PhoDirectoryMaxCount) {
        NewMax = PhoDirectoryCount * 2;
        if (NewMax < INITIAL_FILE_COUNT) {
            NewMax = INITIAL_FILE_COUNT;
        }

        NewDirectories = malloc(NewMax * sizeof(PDIRECTORY_SEARCH));
        if (NewDirectories == NULL) {
            Result = FALSE;
            goto AddDirectoryEnd;
        }

        memset(NewDirectories, 0, NewMax * sizeof(PDIRECTORY_SEARCH));
        memcpy(NewDirectories,
               PhoDirectories,
               PhoDirectoryCount * sizeof(PDIRECTORY_SEARCH));

        free(PhoDirectories);
        PhoDirectories = NewDirectories;
        PhoDirectoryMaxCount = NewMax;
    }

    PhoDirectories[PhoDirectoryCount] = NewSearch;
    PhoDirectoryCount += 1;

    //
    // The operation was successful, don't free the full file name.
    //

    NewSearch = NULL;
    Result = TRUE;

AddDirectoryEnd:
    if (NewSearch != NULL) {
        free(NewSearch);
    }

    return;
}

BOOLEAN
PhopTakeInParameters (
    HWND Window
    )

/*++

Routine Description:

    This routine retrieves the values from the configuration edit boxes and
    sets the globals to the new values.

Arguments:

    Window - Supplies a pointer to the dialog window.

Return Value:

    TRUE if all parameters were successfully read.

    FALSE if one or more of the parameters was incorrect.

--*/

{

    UCHAR BackgroundBlue;
    UCHAR BackgroundGreen;
    UCHAR BackgroundRed;
    ULONG CursorRate;
    ULONG Decay;
    HWND EditBox;
    PSTR FontName;
    UCHAR ForegroundBlue;
    UCHAR ForegroundGreen;
    UCHAR ForegroundRed;
    ULONG Length;
    BOOLEAN Result;
    PSTR SearchPath;
    PSTR String;
    float TextScale;
    ULONG TextRate;

    FontName = NULL;
    SearchPath = NULL;

    String = malloc(50);
    if (String == NULL) {
        Result = FALSE;
        goto TakeInParametersEnd;
    }

    //
    // Get the font name.
    //

    EditBox = GetDlgItem(Window, IDE_FONT);
    Length = Edit_GetText(EditBox, String, 50);
    if ((Length == 0) || (Length >= 49)) {
        MessageBox(NULL, "Please enter a valid font name.", "Error", MB_OK);
        Result = FALSE;
        goto TakeInParametersEnd;
    }

    FontName = String;

    //
    // Get the search path string.
    //

    EditBox = GetDlgItem(Window, IDE_TEXT_FILES);
    Length = Edit_GetTextLength(EditBox);
    if (Length > MAX_STRING) {
        MessageBox(NULL,
                   "Please enter a valid set of files to print.",
                   "Error",
                   MB_OK);

        Result = FALSE;
        goto TakeInParametersEnd;
    }

    String = malloc(Length + 1);
    if (String == NULL) {
        Result = FALSE;
        goto TakeInParametersEnd;
    }

    Edit_GetText(EditBox, String, Length + 1);
    SearchPath = String;

    //
    // Get the background and foreground colors.
    //

    String = malloc(50);
    if (String == NULL) {
        Result = FALSE;
        goto TakeInParametersEnd;
    }

    EditBox = GetDlgItem(Window, IDE_BACKGROUND_RED);
    Edit_GetText(EditBox, String, 50);
    BackgroundRed = (UCHAR)strtol(String, NULL, 10);
    EditBox = GetDlgItem(Window, IDE_BACKGROUND_GREEN);
    Edit_GetText(EditBox, String, 50);
    BackgroundGreen = (UCHAR)strtol(String, NULL, 10);
    EditBox = GetDlgItem(Window, IDE_BACKGROUND_BLUE);
    Edit_GetText(EditBox, String, 50);
    BackgroundBlue = (UCHAR)strtol(String, NULL, 10);
    EditBox = GetDlgItem(Window, IDE_FOREGROUND_RED);
    Edit_GetText(EditBox, String, 50);
    ForegroundRed = (UCHAR)strtol(String, NULL, 10);
    EditBox = GetDlgItem(Window, IDE_FOREGROUND_GREEN);
    Edit_GetText(EditBox, String, 50);
    ForegroundGreen = (UCHAR)strtol(String, NULL, 10);
    EditBox = GetDlgItem(Window, IDE_FOREGROUND_BLUE);
    Edit_GetText(EditBox, String, 50);
    ForegroundBlue = (UCHAR)strtol(String, NULL, 10);

    //
    // Get the text scale, cursor rate, text rate, and decay.
    //

    EditBox = GetDlgItem(Window, IDE_TEXT_SCALE);
    Edit_GetText(EditBox, String, 50);
    TextScale = strtof(String, NULL);
    if ((TextScale < 1.0) || (TextScale > 1000.0)) {
        MessageBox(NULL,
                   "Please enter a text scale between 1 and 1000.",
                   "Error",
                   MB_OK);

        Result = FALSE;
        goto TakeInParametersEnd;
    }

    EditBox = GetDlgItem(Window, IDE_CURSOR_RATE);
    Edit_GetText(EditBox, String, 50);
    CursorRate = strtoul(String, NULL, 10);
    if ((CursorRate == 0) || (CursorRate > 100000000)) {
        MessageBox(NULL,
                   "Please enter a cursor rate between 1 and 100 million.",
                   "Error",
                   MB_OK);

        Result = FALSE;
        goto TakeInParametersEnd;
    }

    EditBox = GetDlgItem(Window, IDE_TEXT_RATE);
    Edit_GetText(EditBox, String, 50);
    TextRate = strtoul(String, NULL, 10);
    if ((TextRate == 0) || (TextRate > 100000000)) {
        MessageBox(NULL,
                   "Please enter a text rate between 1 and 100 million.",
                   "Error",
                   MB_OK);

        Result = FALSE;
        goto TakeInParametersEnd;
    }

    EditBox = GetDlgItem(Window, IDE_DECAY);
    Edit_GetText(EditBox, String, 50);
    Decay = strtoul(String, NULL, 10);
    if (Decay > 100) {
        MessageBox(NULL,
                   "Please enter a decay value between 1 and 100.",
                   "Error",
                   MB_OK);

        Result = FALSE;
        goto TakeInParametersEnd;
    }

    free(String);
    String = NULL;

    //
    // All globals are deemed valid. Save them.
    //

    PhoBackgroundRed = BackgroundRed;
    PhoBackgroundGreen = BackgroundGreen;
    PhoBackgroundBlue = BackgroundBlue;
    PhoForegroundRed = ForegroundRed;
    PhoForegroundGreen = ForegroundGreen;
    PhoForegroundBlue = ForegroundBlue;
    PhoFontName = FontName;
    PhoScale = TextScale;
    PhoCursorBlinkMs = CursorRate;
    PhoInputDelayUs = TextRate;
    PhoDecay = Decay;
    PhoSearchPath = SearchPath;
    Result = TRUE;

TakeInParametersEnd:
    if (Result == FALSE) {
        if (FontName != NULL) {
            free(FontName);
        }

        if (SearchPath != NULL) {
            free(SearchPath);
        }
    }

    if (String != NULL) {
        free(String);
    }

    return Result;
}

BOOLEAN
PhopSaveParameters (
    )

/*++

Routine Description:

    This routine writes the current configuration out to disk.

Arguments:

    None.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    BOOLEAN EndResult;
    BOOLEAN Result;
    PSTR String;

    EndResult = TRUE;
    String = malloc(50);
    if (String == NULL) {
        EndResult = FALSE;
        goto SaveParametersEnd;
    }

    //
    // Save the background and foreground colors.
    //

    sprintf(String, "%d", (UINT)PhoBackgroundRed);
    Result = WritePrivateProfileString(APPLICATION_NAME,
                                       KEY_BACKGROUND_RED,
                                       String,
                                       CONFIGURATION_FILE);

    if (Result == FALSE) {
        EndResult = FALSE;
    }

    sprintf(String, "%d", (UINT)PhoBackgroundGreen);
    Result = WritePrivateProfileString(APPLICATION_NAME,
                                       KEY_BACKGROUND_GREEN,
                                       String,
                                       CONFIGURATION_FILE);

    if (Result == FALSE) {
        EndResult = FALSE;
    }

    sprintf(String, "%d", (UINT)PhoBackgroundBlue);
    Result = WritePrivateProfileString(APPLICATION_NAME,
                                       KEY_BACKGROUND_BLUE,
                                       String,
                                       CONFIGURATION_FILE);

    if (Result == FALSE) {
        EndResult = FALSE;
    }

    sprintf(String, "%d", (UINT)PhoForegroundRed);
    Result = WritePrivateProfileString(APPLICATION_NAME,
                                       KEY_FOREGROUND_RED,
                                       String,
                                       CONFIGURATION_FILE);

    if (Result == FALSE) {
        EndResult = FALSE;
    }

    sprintf(String, "%d", (UINT)PhoForegroundGreen);
    Result = WritePrivateProfileString(APPLICATION_NAME,
                                       KEY_FOREGROUND_GREEN,
                                       String,
                                       CONFIGURATION_FILE);

    if (Result == FALSE) {
        EndResult = FALSE;
    }

    sprintf(String, "%d", (UINT)PhoForegroundBlue);
    Result = WritePrivateProfileString(APPLICATION_NAME,
                                       KEY_FOREGROUND_BLUE,
                                       String,
                                       CONFIGURATION_FILE);

    if (Result == FALSE) {
        EndResult = FALSE;
    }

    //
    // Save the cursor rate, text rate, text scale, and decay.
    //

    sprintf(String, "%.2f", PhoScale);
    Result = WritePrivateProfileString(APPLICATION_NAME,
                                       KEY_TEXT_SCALE,
                                       String,
                                       CONFIGURATION_FILE);

    if (Result == FALSE) {
        EndResult = FALSE;
    }

    sprintf(String, "%d", (UINT)PhoCursorBlinkMs);
    Result = WritePrivateProfileString(APPLICATION_NAME,
                                       KEY_CURSOR_RATE,
                                       String,
                                       CONFIGURATION_FILE);

    if (Result == FALSE) {
        EndResult = FALSE;
    }

    sprintf(String, "%d", (UINT)PhoInputDelayUs);
    Result = WritePrivateProfileString(APPLICATION_NAME,
                                       KEY_TEXT_RATE,
                                       String,
                                       CONFIGURATION_FILE);

    if (Result == FALSE) {
        EndResult = FALSE;
    }

    sprintf(String, "%d", (UINT)PhoDecay);
    Result = WritePrivateProfileString(APPLICATION_NAME,
                                       KEY_DECAY,
                                       String,
                                       CONFIGURATION_FILE);

    if (Result == FALSE) {
        EndResult = FALSE;
    }

    //
    // Save the search path and font name.
    //

    Result = WritePrivateProfileString(APPLICATION_NAME,
                                       KEY_TEXT_FILES,
                                       PhoSearchPath,
                                       CONFIGURATION_FILE);

    if (Result == FALSE) {
        EndResult = FALSE;
    }

    Result = WritePrivateProfileString(APPLICATION_NAME,
                                       KEY_FONT_NAME,
                                       PhoFontName,
                                       CONFIGURATION_FILE);

    if (Result == FALSE) {
        EndResult = FALSE;
    }

SaveParametersEnd:
    if (String != NULL) {
        free(String);
    }

    return EndResult;
}

BOOLEAN
PhopLoadParameters (
    )

/*++

Routine Description:

    This routine reads the saved configuration in from disk.

Arguments:

    None.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    BOOLEAN EndResult;
    PSTR String;

    EndResult = TRUE;

    //
    // Get foreground and background colors.
    //

    PhoBackgroundRed = (UCHAR)GetPrivateProfileInt(APPLICATION_NAME,
                                                   KEY_BACKGROUND_RED,
                                                   PhoBackgroundRed,
                                                   CONFIGURATION_FILE);

    PhoBackgroundGreen = (UCHAR)GetPrivateProfileInt(APPLICATION_NAME,
                                                     KEY_BACKGROUND_GREEN,
                                                     PhoBackgroundGreen,
                                                     CONFIGURATION_FILE);

    PhoBackgroundBlue = (UCHAR)GetPrivateProfileInt(APPLICATION_NAME,
                                                    KEY_BACKGROUND_BLUE,
                                                    PhoBackgroundBlue,
                                                    CONFIGURATION_FILE);

    PhoForegroundRed = (UCHAR)GetPrivateProfileInt(APPLICATION_NAME,
                                                   KEY_FOREGROUND_RED,
                                                   PhoForegroundRed,
                                                   CONFIGURATION_FILE);

    PhoForegroundGreen = (UCHAR)GetPrivateProfileInt(APPLICATION_NAME,
                                                     KEY_FOREGROUND_GREEN,
                                                     PhoForegroundGreen,
                                                     CONFIGURATION_FILE);

    PhoForegroundBlue = (UCHAR)GetPrivateProfileInt(APPLICATION_NAME,
                                                    KEY_FOREGROUND_BLUE,
                                                    PhoForegroundBlue,
                                                    CONFIGURATION_FILE);

    //
    // Save the cursor rate, text rate, text scale, and decay.
    //

    PhoCursorBlinkMs = GetPrivateProfileInt(APPLICATION_NAME,
                                            KEY_CURSOR_RATE,
                                            PhoCursorBlinkMs,
                                            CONFIGURATION_FILE);

    PhoInputDelayUs = GetPrivateProfileInt(APPLICATION_NAME,
                                           KEY_TEXT_RATE,
                                           PhoInputDelayUs,
                                           CONFIGURATION_FILE);

    PhoDecay = GetPrivateProfileInt(APPLICATION_NAME,
                                    KEY_DECAY,
                                    PhoDecay,
                                    CONFIGURATION_FILE);

    String = malloc(50);
    if (String == NULL) {
        EndResult = FALSE;
        goto LoadParametersEnd;
    }

    GetPrivateProfileString(APPLICATION_NAME,
                            KEY_TEXT_SCALE,
                            "6.0",
                            String,
                            50,
                            CONFIGURATION_FILE);

    PhoScale = strtof(String, NULL);
    if ((PhoScale < 1.0) || (PhoScale > 1000.0)) {
        PhoScale = 6.0;
    }

    //
    // Get the font name and then the file list.
    //

    GetPrivateProfileString(APPLICATION_NAME,
                            KEY_FONT_NAME,
                            PhoFontName,
                            String,
                            50,
                            CONFIGURATION_FILE);

    PhoFontName = String;
    String = malloc(MAX_STRING);
    if (String == NULL) {
        EndResult = FALSE;
        goto LoadParametersEnd;
    }

    GetPrivateProfileString(APPLICATION_NAME,
                            KEY_TEXT_FILES,
                            PhoSearchPath,
                            String,
                            MAX_STRING,
                            CONFIGURATION_FILE);

    PhoSearchPath = String;
    String = NULL;

LoadParametersEnd:
    if (String != NULL) {
        free(String);
    }

    return EndResult;
}
