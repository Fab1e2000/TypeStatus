param(
    [ValidateRange(50, 5000)]
    [int]$PollIntervalMs = 200,

    [ValidateRange(10, 2000)]
    [int]$MessageTimeoutMs = 100,

    [switch]$ShowDiagnostics,

    [switch]$RestoreOnly
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

Add-Type -AssemblyName System.Drawing

Add-Type -ReferencedAssemblies System.Drawing -TypeDefinition @'
using System;
using System.ComponentModel;
using System.Drawing;
using System.Drawing.Imaging;
using System.Runtime.InteropServices;
using System.Text;

public static class ImeCursorStatusNative
{
    public const uint IMC_GETCONVERSIONMODE = 0x0001;
    public const uint IMC_GETOPENSTATUS = 0x0005;
    public const ulong IME_CMODE_NATIVE = 0x0001;

    private const uint WM_IME_CONTROL = 0x0283;
    private const uint SMTO_BLOCK = 0x0001;
    private const uint SMTO_ABORTIFHUNG = 0x0002;
    private const uint SMTO_ERRORONEXIT = 0x0020;

    [StructLayout(LayoutKind.Sequential)]
    public struct RECT
    {
        public int Left;
        public int Top;
        public int Right;
        public int Bottom;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct GUITHREADINFO
    {
        public uint cbSize;
        public uint flags;
        public IntPtr hwndActive;
        public IntPtr hwndFocus;
        public IntPtr hwndCapture;
        public IntPtr hwndMenuOwner;
        public IntPtr hwndMoveSize;
        public IntPtr hwndCaret;
        public RECT rcCaret;
    }

    public struct ImeQueryResult
    {
        public bool Succeeded;
        public ulong Value;
        public int Win32Error;
    }

    [DllImport("user32.dll")]
    public static extern IntPtr GetForegroundWindow();

    [DllImport("user32.dll")]
    public static extern uint GetWindowThreadProcessId(
        IntPtr window,
        out uint processId);

    [DllImport("user32.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    public static extern bool GetGUIThreadInfo(
        uint threadId,
        ref GUITHREADINFO info);

    [DllImport("user32.dll")]
    public static extern IntPtr GetKeyboardLayout(uint threadId);

    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    public static extern int GetWindowText(
        IntPtr window,
        StringBuilder text,
        int maxCount);

    [DllImport("imm32.dll")]
    public static extern IntPtr ImmGetDefaultIMEWnd(IntPtr window);

    [DllImport("user32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    private static extern IntPtr SendMessageTimeoutW(
        IntPtr window,
        uint message,
        UIntPtr wParam,
        IntPtr lParam,
        uint flags,
        uint timeout,
        out UIntPtr result);

    public static GUITHREADINFO CreateGuiThreadInfo()
    {
        GUITHREADINFO info = new GUITHREADINFO();
        info.cbSize = (uint)Marshal.SizeOf(typeof(GUITHREADINFO));
        return info;
    }

    public static ImeQueryResult QueryImeControl(
        IntPtr imeWindow,
        uint command,
        uint timeout)
    {
        ImeQueryResult query = new ImeQueryResult();
        if (imeWindow == IntPtr.Zero)
            return query;

        UIntPtr value;
        IntPtr sendResult = SendMessageTimeoutW(
            imeWindow,
            WM_IME_CONTROL,
            new UIntPtr(command),
            IntPtr.Zero,
            SMTO_BLOCK | SMTO_ABORTIFHUNG | SMTO_ERRORONEXIT,
            timeout,
            out value);

        query.Succeeded = sendResult != IntPtr.Zero;
        query.Value = value.ToUInt64();
        query.Win32Error = query.Succeeded ? 0 : Marshal.GetLastWin32Error();
        return query;
    }
}

public static class StatusCursorRenderer
{
    private const int IDC_ARROW = 32512;
    private const int IDC_IBEAM = 32513;
    private const uint OCR_NORMAL = 32512;
    private const uint OCR_IBEAM = 32513;
    private const uint DI_NORMAL = 0x0003;
    private const uint SPI_SETCURSORS = 0x0057;

    private static IntPtr chineseIBeam = IntPtr.Zero;
    private static IntPtr englishIBeam = IntPtr.Zero;
    private static IntPtr chineseArrow = IntPtr.Zero;
    private static IntPtr englishArrow = IntPtr.Zero;

    [StructLayout(LayoutKind.Sequential)]
    private struct ICONINFO
    {
        [MarshalAs(UnmanagedType.Bool)]
        public bool fIcon;
        public uint xHotspot;
        public uint yHotspot;
        public IntPtr hbmMask;
        public IntPtr hbmColor;
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct BITMAP
    {
        public int bmType;
        public int bmWidth;
        public int bmHeight;
        public int bmWidthBytes;
        public ushort bmPlanes;
        public ushort bmBitsPixel;
        public IntPtr bmBits;
    }

    [DllImport("user32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    private static extern IntPtr LoadCursorW(IntPtr instance, IntPtr cursorName);

    [DllImport("user32.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    private static extern bool GetIconInfo(IntPtr icon, out ICONINFO info);

    [DllImport("gdi32.dll", SetLastError = true)]
    private static extern int GetObjectW(IntPtr obj, int size, out BITMAP bitmap);

    [DllImport("gdi32.dll")]
    [return: MarshalAs(UnmanagedType.Bool)]
    private static extern bool DeleteObject(IntPtr obj);

    [DllImport("user32.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    private static extern bool DrawIconEx(
        IntPtr dc,
        int x,
        int y,
        IntPtr icon,
        int width,
        int height,
        uint step,
        IntPtr flickerFreeBrush,
        uint flags);

    [DllImport("user32.dll", SetLastError = true)]
    private static extern IntPtr CreateIconIndirect(ref ICONINFO info);

    [DllImport("user32.dll", SetLastError = true)]
    private static extern IntPtr CopyIcon(IntPtr icon);

    [DllImport("user32.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    private static extern bool DestroyIcon(IntPtr icon);

    [DllImport("user32.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    private static extern bool SetSystemCursor(IntPtr cursor, uint id);

    [DllImport("user32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    private static extern bool SystemParametersInfoW(
        uint action,
        uint parameter,
        IntPtr value,
        uint flags);

    public static void Initialize()
    {
        DisposeVariants();
        RestoreSystemCursors();

        IntPtr sourceArrow = LoadCursorW(IntPtr.Zero, new IntPtr(IDC_ARROW));
        if (sourceArrow == IntPtr.Zero)
            throw new Win32Exception(Marshal.GetLastWin32Error(), "LoadCursor(IDC_ARROW) failed.");

        IntPtr sourceIBeam = LoadCursorW(IntPtr.Zero, new IntPtr(IDC_IBEAM));
        if (sourceIBeam == IntPtr.Zero)
            throw new Win32Exception(Marshal.GetLastWin32Error(), "LoadCursor(IDC_IBEAM) failed.");

        try
        {
            // Chinese is red; English is blue.
            chineseIBeam = CreateColorVariant(sourceIBeam, Color.Red, false);
            englishIBeam = CreateColorVariant(sourceIBeam, Color.Blue, false);
            chineseArrow = CreateColorVariant(sourceArrow, Color.Red, true);
            englishArrow = CreateColorVariant(sourceArrow, Color.Blue, true);
        }
        catch
        {
            DisposeVariants();
            throw;
        }
    }

    public static void ApplyChinese()
    {
        Apply(chineseArrow, OCR_NORMAL);
        Apply(chineseIBeam, OCR_IBEAM);
    }

    public static void ApplyEnglish()
    {
        Apply(englishArrow, OCR_NORMAL);
        Apply(englishIBeam, OCR_IBEAM);
    }

    public static void RestoreSystemCursors()
    {
        if (!SystemParametersInfoW(SPI_SETCURSORS, 0, IntPtr.Zero, 0))
            throw new Win32Exception(
                Marshal.GetLastWin32Error(),
                "Restoring the system cursor scheme failed.");
    }

    public static void DisposeVariants()
    {
        DestroyAndClear(ref chineseIBeam);
        DestroyAndClear(ref englishIBeam);
        DestroyAndClear(ref chineseArrow);
        DestroyAndClear(ref englishArrow);
    }

    private static void DestroyAndClear(ref IntPtr cursor)
    {
        if (cursor == IntPtr.Zero)
            return;

        DestroyIcon(cursor);
        cursor = IntPtr.Zero;
    }

    private static void Apply(IntPtr variant, uint systemCursorId)
    {
        if (variant == IntPtr.Zero)
            throw new InvalidOperationException("Cursor variants have not been initialized.");

        IntPtr copy = CopyIcon(variant);
        if (copy == IntPtr.Zero)
            throw new Win32Exception(Marshal.GetLastWin32Error(), "CopyIcon failed.");

        // SetSystemCursor consumes the supplied handle.
        if (!SetSystemCursor(copy, systemCursorId))
        {
            int error = Marshal.GetLastWin32Error();
            DestroyIcon(copy);
            throw new Win32Exception(error, "SetSystemCursor failed.");
        }
    }

    private static IntPtr CreateColorVariant(
        IntPtr source,
        Color targetColor,
        bool replaceDarkPixelsOnly)
    {
        ICONINFO sourceInfo;
        if (!GetIconInfo(source, out sourceInfo))
            throw new Win32Exception(Marshal.GetLastWin32Error(), "GetIconInfo failed.");

        try
        {
            BITMAP bitmapInfo;
            IntPtr sizeBitmap = sourceInfo.hbmColor != IntPtr.Zero
                ? sourceInfo.hbmColor
                : sourceInfo.hbmMask;

            if (sizeBitmap == IntPtr.Zero ||
                GetObjectW(sizeBitmap, Marshal.SizeOf(typeof(BITMAP)), out bitmapInfo) == 0)
                throw new Win32Exception(
                    Marshal.GetLastWin32Error(),
                    "Reading cursor bitmap dimensions failed.");

            int width = Math.Abs(bitmapInfo.bmWidth);
            int height = Math.Abs(bitmapInfo.bmHeight);
            if (sourceInfo.hbmColor == IntPtr.Zero)
                height /= 2;

            if (width <= 0 || height <= 0)
                throw new InvalidOperationException("The system cursor has invalid dimensions.");

            using (Bitmap onBlack = RenderOnBackground(source, width, height, Color.Black))
            using (Bitmap onWhite = RenderOnBackground(source, width, height, Color.White))
            using (Bitmap colored = new Bitmap(width, height, PixelFormat.Format32bppArgb))
            {
                for (int y = 0; y < height; y++)
                {
                    for (int x = 0; x < width; x++)
                    {
                        Color blackPixel = onBlack.GetPixel(x, y);
                        Color whitePixel = onWhite.GetPixel(x, y);
                        int coverage = GetCoverage(blackPixel, whitePixel);

                        if (coverage == 0)
                        {
                            colored.SetPixel(x, y, Color.Transparent);
                            continue;
                        }

                        Color output = targetColor;
                        if (replaceDarkPixelsOnly)
                        {
                            Color sourceColor = RecoverSourceColor(blackPixel, coverage);
                            int luminance = (
                                sourceColor.R * 2126 +
                                sourceColor.G * 7152 +
                                sourceColor.B * 722) / 10000;

                            if (luminance > 96)
                                output = sourceColor;
                        }

                        colored.SetPixel(
                            x,
                            y,
                            Color.FromArgb(coverage, output.R, output.G, output.B));
                    }
                }

                IntPtr temporaryIcon = colored.GetHicon();
                if (temporaryIcon == IntPtr.Zero)
                    throw new Win32Exception(
                        Marshal.GetLastWin32Error(),
                        "Creating the colored bitmap icon failed.");

                try
                {
                    ICONINFO coloredInfo;
                    if (!GetIconInfo(temporaryIcon, out coloredInfo))
                        throw new Win32Exception(
                            Marshal.GetLastWin32Error(),
                            "Reading the colored bitmap icon failed.");

                    try
                    {
                        coloredInfo.fIcon = false;
                        coloredInfo.xHotspot = sourceInfo.xHotspot;
                        coloredInfo.yHotspot = sourceInfo.yHotspot;

                        IntPtr result = CreateIconIndirect(ref coloredInfo);
                        if (result == IntPtr.Zero)
                            throw new Win32Exception(
                                Marshal.GetLastWin32Error(),
                                "CreateIconIndirect failed.");

                        return result;
                    }
                    finally
                    {
                        if (coloredInfo.hbmColor != IntPtr.Zero)
                            DeleteObject(coloredInfo.hbmColor);
                        if (coloredInfo.hbmMask != IntPtr.Zero)
                            DeleteObject(coloredInfo.hbmMask);
                    }
                }
                finally
                {
                    DestroyIcon(temporaryIcon);
                }
            }
        }
        finally
        {
            if (sourceInfo.hbmColor != IntPtr.Zero)
                DeleteObject(sourceInfo.hbmColor);
            if (sourceInfo.hbmMask != IntPtr.Zero)
                DeleteObject(sourceInfo.hbmMask);
        }
    }

    private static Bitmap RenderOnBackground(
        IntPtr cursor,
        int width,
        int height,
        Color background)
    {
        Bitmap bitmap = new Bitmap(width, height, PixelFormat.Format32bppArgb);
        try
        {
            using (Graphics graphics = Graphics.FromImage(bitmap))
            {
                graphics.Clear(background);
                IntPtr dc = graphics.GetHdc();
                try
                {
                    if (!DrawIconEx(
                        dc,
                        0,
                        0,
                        cursor,
                        width,
                        height,
                        0,
                        IntPtr.Zero,
                        DI_NORMAL))
                        throw new Win32Exception(
                            Marshal.GetLastWin32Error(),
                            "DrawIconEx failed.");
                }
                finally
                {
                    graphics.ReleaseHdc(dc);
                }
            }

            return bitmap;
        }
        catch
        {
            bitmap.Dispose();
            throw;
        }
    }

    private static int GetCoverage(Color onBlack, Color onWhite)
    {
        int redDelta = onWhite.R - onBlack.R;
        int greenDelta = onWhite.G - onBlack.G;
        int blueDelta = onWhite.B - onBlack.B;

        if (redDelta < 0 || greenDelta < 0 || blueDelta < 0)
            return 255;

        int transmission = (redDelta + greenDelta + blueDelta) / 3;
        return Math.Max(0, Math.Min(255, 255 - transmission));
    }

    private static Color RecoverSourceColor(Color onBlack, int coverage)
    {
        if (coverage <= 0)
            return Color.Transparent;

        int red = Math.Min(255, onBlack.R * 255 / coverage);
        int green = Math.Min(255, onBlack.G * 255 / coverage);
        int blue = Math.Min(255, onBlack.B * 255 / coverage);
        return Color.FromArgb(255, red, green, blue);
    }
}
'@

function Get-WindowTitle {
    param([IntPtr]$Window)

    $buffer = [Text.StringBuilder]::new(512)
    [void][ImeCursorStatusNative]::GetWindowText(
        $Window,
        $buffer,
        $buffer.Capacity)
    return $buffer.ToString()
}

function Get-InputModeSample {
    $foregroundWindow = [ImeCursorStatusNative]::GetForegroundWindow()
    if ($foregroundWindow -eq [IntPtr]::Zero) {
        return [pscustomobject]@{ State = 'Unknown'; Reason = 'No foreground window' }
    }

    [uint32]$foregroundProcessId = 0
    $foregroundThreadId = [ImeCursorStatusNative]::GetWindowThreadProcessId(
        $foregroundWindow,
        [ref]$foregroundProcessId)
    if ($foregroundThreadId -eq 0) {
        return [pscustomobject]@{ State = 'Unknown'; Reason = 'No foreground thread' }
    }

    $guiInfo = [ImeCursorStatusNative]::CreateGuiThreadInfo()
    $gotGuiInfo = [ImeCursorStatusNative]::GetGUIThreadInfo(
        $foregroundThreadId,
        [ref]$guiInfo)

    $focusWindow = if ($gotGuiInfo -and $guiInfo.hwndFocus -ne [IntPtr]::Zero) {
        $guiInfo.hwndFocus
    }
    else {
        $foregroundWindow
    }

    [uint32]$focusProcessId = 0
    $focusThreadId = [ImeCursorStatusNative]::GetWindowThreadProcessId(
        $focusWindow,
        [ref]$focusProcessId)
    if ($focusThreadId -eq 0) {
        $focusThreadId = $foregroundThreadId
        $focusProcessId = $foregroundProcessId
    }

    $hklPointer = [ImeCursorStatusNative]::GetKeyboardLayout($focusThreadId)
    $rawHkl = $hklPointer.ToInt64()
    $hkl32 = $rawHkl -band 0xffffffffL
    $languageId = [int]($rawHkl -band 0xffffL)
    $primaryLanguageId = $languageId -band 0x03ff

    $imeWindow = [ImeCursorStatusNative]::ImmGetDefaultIMEWnd($focusWindow)
    if ($imeWindow -eq [IntPtr]::Zero -and $focusWindow -ne $foregroundWindow) {
        $imeWindow = [ImeCursorStatusNative]::ImmGetDefaultIMEWnd($foregroundWindow)
    }

    $openResult = [ImeCursorStatusNative]::QueryImeControl(
        $imeWindow,
        [ImeCursorStatusNative]::IMC_GETOPENSTATUS,
        $MessageTimeoutMs)
    $conversionResult = [ImeCursorStatusNative]::QueryImeControl(
        $imeWindow,
        [ImeCursorStatusNative]::IMC_GETCONVERSIONMODE,
        $MessageTimeoutMs)

    $isNative = $conversionResult.Succeeded -and
        (($conversionResult.Value -band [ImeCursorStatusNative]::IME_CMODE_NATIVE) -ne 0)

    if ($primaryLanguageId -eq 0x0009) {
        $state = 'English'
        $reason = 'English-family HKL'
    }
    elseif ($openResult.Succeeded -and $openResult.Value -eq 0) {
        $state = 'English'
        $reason = 'IME reports closed'
    }
    elseif ($conversionResult.Succeeded -and $isNative) {
        $state = 'Chinese'
        $reason = 'IME_CMODE_NATIVE is set'
    }
    elseif ($conversionResult.Succeeded) {
        $state = 'English'
        $reason = 'IME_CMODE_NATIVE is clear'
    }
    else {
        $state = 'Unknown'
        $reason = 'IME status query failed'
    }

    return [pscustomobject]@{
        State             = $state
        Reason            = $reason
        ProcessId         = $focusProcessId
        ThreadId          = $focusThreadId
        Hkl               = ('0x{0:X8}' -f $hkl32)
        LanguageId        = ('0x{0:X4}' -f $languageId)
        ImeWindow         = $imeWindow
        OpenSucceeded     = $openResult.Succeeded
        OpenValue         = $openResult.Value
        OpenError         = $openResult.Win32Error
        ConversionSuccess = $conversionResult.Succeeded
        ConversionValue   = $conversionResult.Value
        ConversionError   = $conversionResult.Win32Error
        IsNative          = $isNative
        Title             = Get-WindowTitle -Window $foregroundWindow
    }
}

if ($RestoreOnly) {
    [StatusCursorRenderer]::RestoreSystemCursors()
    Write-Host 'System cursor scheme restored.' -ForegroundColor Green
    exit 0
}

Write-Host 'Preparing status cursor variants...'
[StatusCursorRenderer]::Initialize()

Write-Host 'IME cursor status MVP started. Press Ctrl+C to stop.' -ForegroundColor Cyan
Write-Host 'Chinese = RED; English = BLUE; Unknown = system default.'
Write-Host ''

$lastState = $null

try {
    while ($true) {
        $sample = Get-InputModeSample

        if ($sample.State -ne $lastState) {
            switch ($sample.State) {
                'Chinese' {
                    [StatusCursorRenderer]::ApplyChinese()
                    $outputColor = 'Red'
                }
                'English' {
                    [StatusCursorRenderer]::ApplyEnglish()
                    $outputColor = 'Blue'
                }
                default {
                    [StatusCursorRenderer]::RestoreSystemCursors()
                    $outputColor = 'Yellow'
                }
            }

            Write-Host ('[{0:HH:mm:ss.fff}] {1} -> {2}' -f
                (Get-Date),
                $sample.State,
                $sample.Reason) -ForegroundColor $outputColor

            if ($ShowDiagnostics -and $sample.PSObject.Properties.Name -contains 'Hkl') {
                Write-Host ('  PID={0} TID={1} HKL={2} LANGID={3} imeWnd=0x{4:X}' -f
                    $sample.ProcessId,
                    $sample.ThreadId,
                    $sample.Hkl,
                    $sample.LanguageId,
                    $sample.ImeWindow.ToInt64())
                Write-Host ('  open: success={0} value={1} error={2}' -f
                    $sample.OpenSucceeded,
                    $sample.OpenValue,
                    $sample.OpenError)
                Write-Host ('  conversion: success={0} value=0x{1:X8} native={2} error={3}' -f
                    $sample.ConversionSuccess,
                    $sample.ConversionValue,
                    $sample.IsNative,
                    $sample.ConversionError)
                Write-Host ('  window: {0}' -f $sample.Title)
            }

            $lastState = $sample.State
        }

        Start-Sleep -Milliseconds $PollIntervalMs
    }
}
finally {
    try {
        [StatusCursorRenderer]::RestoreSystemCursors()
        Write-Host 'System cursor scheme restored.' -ForegroundColor Green
    }
    finally {
        [StatusCursorRenderer]::DisposeVariants()
    }
}
