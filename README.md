# HandleProtoToolPkg
HandleProtoTool 函數使用筆記（README 版）
0. 核心概念：UEFI 沒有「Handle Number」

UEFI 只有 EFI_HANDLE（本質常見是指標 void*），沒有內建的 Handle Number 欄位。

本工具定義的 Handle Number(DEC) = LocateHandleBuffer(AllHandles, ...) 回傳陣列 AllHandles[] 的 index（十進制）。

注意：LocateHandleBuffer(ByProtocol, ...) 回來的是「匹配清單」，其 i 只是 match list 的序號，不是全域 handle number。

因此若要印「全域 Handle Number」，必須用 AllHandles[] 反查 index。

1) LocateHandleBuffer()
用途

依條件取得 handles 陣列：

AllHandles：取得系統所有 handles（建立全域 handle 表）

ByProtocol：取得「支援某 Protocol GUID」的 handles

其他：ByRegisterNotify（進階）

Prototype（重點參數）
EFI_STATUS
LocateHandleBuffer (
  IN EFI_LOCATE_SEARCH_TYPE SearchType,
  IN EFI_GUID               *Protocol OPTIONAL,
  IN VOID                   *SearchKey OPTIONAL,
  IN OUT UINTN              *NoHandles,
  OUT EFI_HANDLE            **Buffer
);

你在專案裡的用法
A. 取得所有 handles（建立 Handle Number）
EFI_HANDLE *All = NULL;
UINTN AllCount = 0;

Status = gBS->LocateHandleBuffer(AllHandles, NULL, NULL, &AllCount, &All);
// All 是一個 EFI_HANDLE 陣列，All[i] 的 i 就是「Handle Number(DEC)」

B. 依 Protocol GUID 查 handles（ByProtocol）
EFI_HANDLE *Handles = NULL;
UINTN Count = 0;

Status = gBS->LocateHandleBuffer(ByProtocol, &SomeGuid, NULL, &Count, &Handles);

常見錯誤碼（你最常遇到）

EFI_SUCCESS：成功，Buffer 必須 FreePool()

EFI_OUT_OF_RESOURCES：系統無法配置陣列記憶體

EFI_INVALID_PARAMETER：參數不合法（常見是 NoHandles/Buffer 指標為 NULL）

EFI_NOT_FOUND：ByProtocol 找不到支援該 GUID 的 handle（有些環境會回這個）

重要注意事項

LocateHandleBuffer() 回傳的 Buffer 是 Boot Services 配置的 pool memory，用完一定要：

FreePool(Buffer);

2) ProtocolsPerHandle()
用途

列出某個 EFI_HANDLE 上安裝的所有 Protocol GUID。

Prototype
EFI_STATUS
ProtocolsPerHandle (
  IN  EFI_HANDLE  Handle,
  OUT EFI_GUID    ***ProtocolBuffer,
  OUT UINTN       *ProtocolBufferCount
);

你在專案裡的用法
EFI_GUID **ProtoGuidArray = NULL;
UINTN ProtoCount = 0;

Status = gBS->ProtocolsPerHandle(Handle, &ProtoGuidArray, &ProtoCount);
if (!EFI_ERROR(Status)) {
  for (UINTN i = 0; i < ProtoCount; i++) {
    Print(L"%g\n", ProtoGuidArray[i]);
  }
  FreePool(ProtoGuidArray);
}

常見錯誤碼

EFI_SUCCESS：成功，ProtocolBuffer 需 FreePool()

EFI_INVALID_PARAMETER：Handle 或輸出指標為 NULL

EFI_OUT_OF_RESOURCES：無法配置 GUID 陣列

注意事項

ProtocolBuffer 是 EFI_GUID** 陣列，每個元素是 EFI_GUID*

用完一定要 FreePool(ProtoGuidArray);

3) HandleProtocol()（你目前沒用到，但需求常會用）
用途

從指定 handle 取得某 protocol 的 interface 指標（protocol instance）。

Prototype
EFI_STATUS
HandleProtocol (
  IN  EFI_HANDLE  Handle,
  IN  EFI_GUID    *Protocol,
  OUT VOID        **Interface
);

典型使用場景

你找到支援某 GUID 的 handle 後，想取得具體 interface 來呼叫方法。

例：取得 EFI_SIMPLE_FILE_SYSTEM_PROTOCOL

EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *Fs;
Status = gBS->HandleProtocol(Handle, &gEfiSimpleFileSystemProtocolGuid, (VOID**)&Fs);

常見錯誤碼

EFI_SUCCESS：成功

EFI_UNSUPPORTED：Handle 不支援該 protocol

EFI_INVALID_PARAMETER：傳入指標為 NULL

4) DevicePathFromHandle() / ConvertDevicePathToText()
DevicePathFromHandle()
用途

從 handle 嘗試取得 EFI_DEVICE_PATH_PROTOCOL*（若該 handle 有安裝 DevicePath）

EFI_DEVICE_PATH_PROTOCOL *Dp = DevicePathFromHandle(Handle);
if (Dp == NULL) {
  // 沒有 device path 很正常
}

ConvertDevicePathToText()
用途

把 device path 轉成人類可讀字串（用來印 log）

CHAR16 *Text = ConvertDevicePathToText(Dp, TRUE, TRUE);
if (Text) {
  Print(L"%s\n", Text);
  FreePool(Text);
}

常見失敗原因

ConvertDevicePathToText() 回 NULL：常見是資源不足（Out of resources）

5) ReadKeyStroke()（Console 輸入核心）
用途

讀一個鍵盤按鍵，非阻塞，沒鍵會回 EFI_NOT_READY

EFI_INPUT_KEY Key;
Status = gST->ConIn->ReadKeyStroke(gST->ConIn, &Key);
if (Status == EFI_NOT_READY) {
  // 尚未有按鍵
}

你專案的模式

ReadLine()：用輪詢 + Stall() 讀入一行字串

WaitAnyKey()：用輪詢等任意鍵

6) 字串/數值解析：StrDecimalToUintn() vs StrHexToUintn()
你遇到的 bug 根因

你輸入 59，如果用 StrHexToUintn("59")：

會被當成十六進制 0x59 → 十進制 89

你已改成十進制輸入，所以要用：

UINTN Index = StrDecimalToUintn(In);

7) CompareGuid() / StrCmp()（查表比對）
CompareGuid(Guid1, Guid2)

用來比較兩個 GUID 是否相等（BOOLEAN）

你用在「GUID → 名字」反查。

StrCmp(FirstString, SecondString)

比較兩個 CHAR16* 字串

區分大小寫

用於使用者輸入協定名字查表（Fs/Dpath/...）

常見 Debug Checklist（你現在最需要的）
A. 你想印「找到的 Handle Number(DEC)」

✅ 正解：用 AllHandles[] 反查 index

ByProtocol 回來的 i 不是全域 index

必須 FindHandleIndexInAll(Target, All, AllCount, &RealIndex);

B. 輸入 handle number 是十進制

✅ 使用 StrDecimalToUintn()

否則 59 -> 0x59 -> 89

C. 任何由 UEFI API 配置的 Buffer，都要 FreePool

LocateHandleBuffer() 回傳 EFI_HANDLE* → FreePool()

ProtocolsPerHandle() 回傳 EFI_GUID** → FreePool()

ConvertDevicePathToText() 回傳 CHAR16* → FreePool()

cd /d D:\BIOS\MyWorkSpace\edk2

edksetup.bat Rebuild

chcp 65001

set PYTHONUTF8=1

set PYTHONIOENCODING=utf-8

rmdir /s /q Build\HandleProtoToolPkg

build -p HandleProtoToolPkg\HandleProtoToolPkg.dsc -a X64 -t VS2019 -b DEBUG

