#RequireAdmin
#include "lib\Froggy_Includes.au3"

Global Const $BOTNAME = "IPC Test"
Global Const $VERSION = "1.0"
Global Const $AUTHORS[1] = ["Test"]

ConsoleWrite("=== WebIPC Minimal Test ===" & @CRLF)

; Test basic IPC init (no GW client needed)
ConsoleWrite("ScriptDir: " & @ScriptDir & @CRLF)

; Manually create the dir to test
Local $ipcBase = @ScriptDir & "\ipc"
ConsoleWrite("Creating: " & $ipcBase & @CRLF)
DirCreate($ipcBase)
ConsoleWrite("ipc dir exists: " & FileExists($ipcBase) & @CRLF)

Local $ipcDir = $ipcBase & "\testbot"
DirCreate($ipcDir)
ConsoleWrite("testbot dir exists: " & FileExists($ipcDir) & @CRLF)

; Write a test JSON
Local $json = '{"test": true, "character": "TestBot"}'
Local $tmpFile = $ipcDir & "\status.tmp"
Local $statusFile = $ipcDir & "\status.json"
Local $hFile = FileOpen($tmpFile, 2 + 256)
ConsoleWrite("FileOpen result: " & $hFile & @CRLF)
If $hFile <> -1 Then
    FileWrite($hFile, $json)
    FileClose($hFile)
    FileMove($tmpFile, $statusFile, 1)
    ConsoleWrite("status.json written!" & @CRLF)
    ConsoleWrite("Content: " & FileRead($statusFile) & @CRLF)
EndIf

ConsoleWrite("=== DONE ===" & @CRLF)
