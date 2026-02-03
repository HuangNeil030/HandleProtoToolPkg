[Defines]
  PLATFORM_NAME                  = HandleProtoToolPkg
  PLATFORM_GUID                  = 2B8A8C7D-2F73-46ED-9F6A-2E2A5C5E7F90
  PLATFORM_VERSION               = 0.1
  DSC_SPECIFICATION              = 0x0001001A
  OUTPUT_DIRECTORY               = Build/HandleProtoToolPkg
  SUPPORTED_ARCHITECTURES        = X64
  BUILD_TARGETS                  = DEBUG|RELEASE
  SKUID_IDENTIFIER               = DEFAULT

[BuildOptions]
  MSFT:DEBUG_VS2019_X64_CC_FLAGS = /GS- /sdl-
  MSFT:*_*_*_CC_FLAGS = /wd4819
  MSFT:*_*_*_CC_FLAGS = /utf-8
  
[Packages]
  MdePkg/MdePkg.dec
  MdeModulePkg/MdeModulePkg.dec
  EmulatorPkg/EmulatorPkg.dec
  HandleProtoToolPkg/HandleProtoToolPkg.dec
  
[LibraryClasses]
 UefiApplicationEntryPoint       | MdePkg/Library/UefiApplicationEntryPoint/UefiApplicationEntryPoint.inf
  UefiLib                        | MdePkg/Library/UefiLib/UefiLib.inf
  UefiBootServicesTableLib       | MdePkg/Library/UefiBootServicesTableLib/UefiBootServicesTableLib.inf
  UefiRuntimeServicesTableLib    | MdePkg/Library/UefiRuntimeServicesTableLib/UefiRuntimeServicesTableLib.inf
  MemoryAllocationLib            | MdePkg/Library/UefiMemoryAllocationLib/UefiMemoryAllocationLib.inf
  BaseLib                        | MdePkg/Library/BaseLib/BaseLib.inf
  BaseMemoryLib                  | MdePkg/Library/BaseMemoryLib/BaseMemoryLib.inf
  DevicePathLib                  | MdePkg/Library/UefiDevicePathLib/UefiDevicePathLib.inf

  DebugLib                       | MdePkg/Library/BaseDebugLibNull/BaseDebugLibNull.inf
  PcdLib                         | MdePkg/Library/BasePcdLibNull/BasePcdLibNull.inf
  PrintLib                       | MdePkg/Library/BasePrintLib/BasePrintLib.inf
  RegisterFilterLib              | MdePkg/Library/RegisterFilterLibNull/RegisterFilterLibNull.inf
  StackCheckLib                  | MdePkg/Library/StackCheckLibNull/StackCheckLibNull.inf


  
  
[Components]
  HandleProtoToolPkg/Applications/HandleProtoTool/HandleProtoTool.inf
