#include <Uefi.h>

#include <Library/UefiLib.h>
#include <Library/UefiApplicationEntryPoint.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DevicePathLib.h>

#include <Protocol/DevicePath.h>
#include <Protocol/LoadedImage.h>
#include <Protocol/PciIo.h>
#include <Protocol/PciRootBridgeIo.h>
#include <Protocol/SimpleFileSystem.h>

#include <Protocol/SimpleTextIn.h>
#include <Protocol/SimpleTextOut.h>
#include <Protocol/GraphicsOutput.h>
#include <Protocol/DriverBinding.h>
#include <Protocol/ComponentName2.h>

#define MAX_INPUT 128

//
// ---------------- Protocol Name Map ----------------
//

typedef struct {
  CONST CHAR16   *Name;
  CONST EFI_GUID *Guid;
} PROTO_KV;

STATIC PROTO_KV mKnownProtocols[] = {
  { L"Fs",              &gEfiSimpleFileSystemProtocolGuid },
  { L"Dpath",           &gEfiDevicePathProtocolGuid },
  { L"LoadedImage",     &gEfiLoadedImageProtocolGuid },
  { L"PciIo",           &gEfiPciIoProtocolGuid },
  { L"PciRootBridgeIo", &gEfiPciRootBridgeIoProtocolGuid },
  { L"ConIn",           &gEfiSimpleTextInProtocolGuid },
  { L"ConOut",          &gEfiSimpleTextOutProtocolGuid },
  { L"Gop",             &gEfiGraphicsOutputProtocolGuid },
  { L"DriverBinding",   &gEfiDriverBindingProtocolGuid },
  { L"ComponentName2",  &gEfiComponentName2ProtocolGuid },
};

STATIC CONST PROTO_KV*
FindProtocolByName(IN CONST CHAR16 *Name)
{
  for (UINTN i = 0; i < ARRAY_SIZE(mKnownProtocols); i++) {
    if (StrCmp(Name, mKnownProtocols[i].Name) == 0) {
      return &mKnownProtocols[i];
    }
  }
  return NULL;
}

STATIC CONST CHAR16*
GuidToPrettyName(IN CONST EFI_GUID *Guid)
{
  for (UINTN i = 0; i < ARRAY_SIZE(mKnownProtocols); i++) {
    if (CompareGuid(Guid, mKnownProtocols[i].Guid)) {
      return mKnownProtocols[i].Name;
    }
  }
  return L"Unknown GUID";
}

//
// ---------------- Console Helpers ----------------
//

STATIC VOID WaitAnyKey(VOID)
{
  EFI_INPUT_KEY Key;
  while (gST->ConIn->ReadKeyStroke(gST->ConIn, &Key) == EFI_NOT_READY) {
    gBS->Stall(1000);
  }
}

STATIC VOID ReadLine(OUT CHAR16 *Buf, IN UINTN BufChars)
{
  UINTN Idx = 0;
  EFI_INPUT_KEY Key;

  if (BufChars == 0) return;
  Buf[0] = L'\0';

  while (TRUE) {
    while (gST->ConIn->ReadKeyStroke(gST->ConIn, &Key) == EFI_NOT_READY) {
      gBS->Stall(1000);
    }

    if (Key.UnicodeChar == CHAR_CARRIAGE_RETURN) {
      Print(L"\n");
      break;
    }

    if (Key.UnicodeChar == CHAR_BACKSPACE) {
      if (Idx > 0) {
        Idx--;
        Buf[Idx] = L'\0';
        Print(L"\b \b");
      }
      continue;
    }

    if (Key.UnicodeChar >= 0x20 && Key.UnicodeChar <= 0x7E) {
      if (Idx + 1 < BufChars) {
        Buf[Idx++] = Key.UnicodeChar;
        Buf[Idx] = L'\0';
        Print(L"%c", Key.UnicodeChar);
      }
    }
  }
}

STATIC VOID PrintSeparator(VOID)
{
  Print(L"----------------------------------------------------------\n");
}

//
// ---------------- GUID Parse (for menu #2) ----------------
//

STATIC INTN HexVal(CHAR16 c)
{
  if (c >= L'0' && c <= L'9') return (INTN)(c - L'0');
  if (c >= L'a' && c <= L'f') return (INTN)(c - L'a' + 10);
  if (c >= L'A' && c <= L'F') return (INTN)(c - L'A' + 10);
  return -1;
}

STATIC BOOLEAN ParseHexN(CONST CHAR16 *s, UINTN n, UINT64 *out)
{
  UINT64 v = 0;
  for (UINTN i = 0; i < n; i++) {
    INTN h = HexVal(s[i]);
    if (h < 0) return FALSE;
    v = (v << 4) | (UINT64)h;
  }
  *out = v;
  return TRUE;
}

STATIC BOOLEAN ParseGuidString(CONST CHAR16 *Str, EFI_GUID *Guid)
{
  if (Str == NULL || Guid == NULL) return FALSE;
  if (StrLen(Str) < 36) return FALSE;
  if (Str[8] != L'-' || Str[13] != L'-' || Str[18] != L'-' || Str[23] != L'-') return FALSE;

  UINT64 a, b, c, d1, d2;
  if (!ParseHexN(Str + 0,  8,  &a))  return FALSE;
  if (!ParseHexN(Str + 9,  4,  &b))  return FALSE;
  if (!ParseHexN(Str + 14, 4,  &c))  return FALSE;
  if (!ParseHexN(Str + 19, 4,  &d1)) return FALSE;
  if (!ParseHexN(Str + 24, 12, &d2)) return FALSE;

  Guid->Data1 = (UINT32)a;
  Guid->Data2 = (UINT16)b;
  Guid->Data3 = (UINT16)c;

  Guid->Data4[0] = (UINT8)((d1 >> 8) & 0xFF);
  Guid->Data4[1] = (UINT8)( d1       & 0xFF);
  for (UINTN i = 0; i < 6; i++) {
    Guid->Data4[2 + i] = (UINT8)((d2 >> ((5 - i) * 8)) & 0xFF);
  }
  return TRUE;
}

//
// ---------------- Handle Helpers ----------------
//

STATIC EFI_STATUS GetAllHandles(OUT EFI_HANDLE **Handles, OUT UINTN *Count)
{
  if (!Handles || !Count) return EFI_INVALID_PARAMETER;
  *Handles = NULL;
  *Count   = 0;
  return gBS->LocateHandleBuffer(AllHandles, NULL, NULL, Count, Handles);
}

// 用 Handle 值去 AllHandles 找「真正 index（十進制 handle number）」
STATIC
BOOLEAN
FindHandleIndexInAll(
  IN  EFI_HANDLE  Target,
  IN  EFI_HANDLE  *All,
  IN  UINTN       AllCount,
  OUT UINTN       *OutIndex
  )
{
  if (All == NULL || OutIndex == NULL) return FALSE;

  for (UINTN i = 0; i < AllCount; i++) {
    if (All[i] == Target) {
      *OutIndex = i;
      return TRUE;
    }
  }
  return FALSE;
}

//
// ---------------- Handle Dump Core (TXT style) ----------------
//

STATIC VOID PrintDevicePathOrNone(IN EFI_HANDLE Handle)
{
  EFI_DEVICE_PATH_PROTOCOL *Dp = DevicePathFromHandle(Handle);
  if (Dp == NULL) {
    Print(L"  Device Path: [None]\n");
    return;
  }

  CHAR16 *Text = ConvertDevicePathToText(Dp, TRUE, TRUE);
  if (Text == NULL) {
    Print(L"  Device Path: [None]\n");
    return;
  }

  Print(L"  Device Path: %s\n", Text);
  FreePool(Text);
}

STATIC VOID DumpOneHandleTxtStyle(IN EFI_HANDLE Handle, IN UINTN RealIndexDec)
{
  EFI_STATUS Status;
  EFI_GUID   **ProtoGuidArray = NULL;
  UINTN      ProtoCount = 0;

  PrintSeparator();
  Print(L"Handle [%03u] : %p\n", (UINT32)RealIndexDec, Handle);

  PrintDevicePathOrNone(Handle);

  Status = gBS->ProtocolsPerHandle(Handle, &ProtoGuidArray, &ProtoCount);
  if (EFI_ERROR(Status) || ProtoGuidArray == NULL) {
    Print(L"  Protocols (0):\n");
    return;
  }

  Print(L"  Protocols (%u):\n", (UINT32)ProtoCount);
  for (UINTN i = 0; i < ProtoCount; i++) {
    CONST CHAR16 *Name = GuidToPrettyName(ProtoGuidArray[i]);
    Print(L"    - %g (%s)\n", ProtoGuidArray[i], Name);
  }

  FreePool(ProtoGuidArray);
}

//
// ---------------- Menu Actions ----------------
//

STATIC VOID DoSearchAllHandles(VOID)
{
  EFI_STATUS Status;
  EFI_HANDLE *Handles = NULL;
  UINTN Count = 0;

  Status = GetAllHandles(&Handles, &Count);
  if (EFI_ERROR(Status)) {
    Print(L"LocateHandleBuffer(AllHandles): %r\n", Status);
    return;
  }

  Print(L"=== UEFI Handle Dump (TXT Style) ===\n");
  Print(L"Total Handles Found: %u\n", (UINT32)Count);
  Print(L"Processing... Please wait.\n");

  for (UINTN i = 0; i < Count; i++) {
    DumpOneHandleTxtStyle(Handles[i], i); // i 就是 AllHandles 的真實 index（十進位）
  }

  PrintSeparator();
  Print(L"=== Dump Finished ===\n");

  FreePool(Handles);
}

STATIC VOID DoSearchByProtocolGuid(VOID)
{
  CHAR16 In[MAX_INPUT];
  EFI_GUID Guid;
  EFI_STATUS Status;

  EFI_HANDLE *All = NULL;
  UINTN AllCount = 0;

  EFI_HANDLE *Handles = NULL;
  UINTN Count = 0;

  Print(L"GUID:________-____-____-____-____________\n");
  ReadLine(In, MAX_INPUT);

  if (!ParseGuidString(In, &Guid)) {
    Print(L"GUID format error.\n");
    return;
  }

  Status = GetAllHandles(&All, &AllCount);
  if (EFI_ERROR(Status)) {
    Print(L"LocateHandleBuffer(AllHandles): %r\n", Status);
    return;
  }

  Status = gBS->LocateHandleBuffer(ByProtocol, &Guid, NULL, &Count, &Handles);
  if (EFI_ERROR(Status)) {
    Print(L"LocateHandleBuffer(ByProtocol): %r\n", Status);
    FreePool(All);
    return;
  }

  Print(L"Matched Handles: %u\n", (UINT32)Count);

  for (UINTN i = 0; i < Count; i++) {
    UINTN RealIndex = 0;
    if (!FindHandleIndexInAll(Handles[i], All, AllCount, &RealIndex)) {
      RealIndex = i; // fallback
    }
    DumpOneHandleTxtStyle(Handles[i], RealIndex);
  }

  FreePool(Handles);
  FreePool(All);
}

STATIC VOID DoSearchByProtocolName(VOID)
{
  CHAR16 In[MAX_INPUT];
  EFI_STATUS Status;

  EFI_HANDLE *All = NULL;
  UINTN AllCount = 0;

  EFI_HANDLE *Handles = NULL;
  UINTN Count = 0;

  Print(L"Protocol Name (e.g. Fs / Dpath / LoadedImage / PciIo / PciRootBridgeIo):");
  ReadLine(In, MAX_INPUT);

  CONST PROTO_KV *Kv = FindProtocolByName(In);
  if (Kv == NULL) {
    Print(L"Unknown protocol name.\n");
    Print(L"Tip: Fs, Dpath, LoadedImage, PciIo, PciRootBridgeIo, ConIn, ConOut, Gop, DriverBinding, ComponentName2\n");
    return;
  }

  Status = GetAllHandles(&All, &AllCount);
  if (EFI_ERROR(Status)) {
    Print(L"LocateHandleBuffer(AllHandles): %r\n", Status);
    return;
  }

  Status = gBS->LocateHandleBuffer(ByProtocol, (EFI_GUID *)Kv->Guid, NULL, &Count, &Handles);
  if (EFI_ERROR(Status)) {
    Print(L"LocateHandleBuffer(ByProtocol): %r\n", Status);
    FreePool(All);
    return;
  }

  Print(L"Matched Handles: %u\n", (UINT32)Count);

  for (UINTN i = 0; i < Count; i++) {
    UINTN RealIndex = 0;
    if (!FindHandleIndexInAll(Handles[i], All, AllCount, &RealIndex)) {
      RealIndex = i; // fallback
    }
    DumpOneHandleTxtStyle(Handles[i], RealIndex);
  }

  FreePool(Handles);
  FreePool(All);
}

STATIC VOID DoSearchByHandleNumber(VOID)
{
  CHAR16 In[MAX_INPUT];
  EFI_STATUS Status;

  EFI_HANDLE *Handles = NULL;
  UINTN Count = 0;

  Status = GetAllHandles(&Handles, &Count);
  if (EFI_ERROR(Status)) {
    Print(L"LocateHandleBuffer(AllHandles): %r\n", Status);
    return;
  }

  Print(L"Handle Number (DEC like 59):");
  ReadLine(In, MAX_INPUT);

  UINTN Index = (UINTN)StrDecimalToUintn(In);

  Print(L"[DBG] In=\"%s\"  Index(dec)=%u  Total=%u\n",
        In, (UINT32)Index, (UINT32)Count);

  if (Index >= Count) {
    Print(L"Out of range. Total=%u\n", (UINT32)Count);
    FreePool(Handles);
    return;
  }

  Print(L"[DBG] Handles[%u]=%p\n", (UINT32)Index, Handles[Index]);

  DumpOneHandleTxtStyle(Handles[Index], Index);

  FreePool(Handles);
}

STATIC VOID Menu(VOID)
{
  Print(L"\n");
  Print(L"1. Search All Handles\n");
  Print(L"2. Search Specific Handles by Protocol GUID\n");
  Print(L"3. Search Specific Handles by Protocol Name\n");
  Print(L"4. Search Specific Handles by Handle Number\n");
  Print(L"5. Quit Program\n");
  Print(L"Please Select the Function:");
}

//
// ---------------- Entry ----------------
//

EFI_STATUS EFIAPI UefiMain(IN EFI_HANDLE ImageHandle, IN EFI_SYSTEM_TABLE *SystemTable)
{
  CHAR16 In[MAX_INPUT];

  while (TRUE) {
    Menu();
    ReadLine(In, MAX_INPUT);

    if (StrLen(In) == 0) continue;

    switch (In[0]) {
      case L'1': DoSearchAllHandles();       break;
      case L'2': DoSearchByProtocolGuid();   break;
      case L'3': DoSearchByProtocolName();   break;
      case L'4': DoSearchByHandleNumber();   break;
      case L'5': return EFI_SUCCESS;
      default:  Print(L"Unknown selection.\n"); break;
    }

    Print(L"\nPress any key to continue...\n");
    WaitAnyKey();
  }
}
