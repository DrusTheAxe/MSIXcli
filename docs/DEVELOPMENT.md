# Building the Project

## Prerequisites
- Visual Studio 2026
- Windows SDK 10.0 or later

## Build Steps
1. Open `MSIXcli.sln` in Visual Studio
2. Select your desired configuration (Debug/Release) and platform (x86/x64)
3. Build the solution (Build > Build Solution or Ctrl+Shift+B)
4. The DLL will be created in the `Debug` or `Release` folder

# MSIX Property Sheet

## Register the DLL
Open an **elevated command prompt** (Run as Administrator) and execute:

```cmd
msixadmin tool propertysheet install --path=path\to\MSIXPropertySheet.dll
```

By default, ths looks for `MSIXPropertySheet.dll` in the same directory as `msixadmin.exe`. But this
isn't true in the build environment where the exe and dll are in peer directories. Specify the path to the DLL to find it.

### Install via RegSv32

Alternatively, you can directly install the property sheet via regsvr32:

```cmd
regsvr32 "path\to\MSIXPropertySheet.dll"
```

## Unregister the DLL

Likewise, the property sheet can be uninstalled via

```
msixadmin tool propertysheet uninstall --path=path\to\MSIXPropertySheet.dll
```

or

```cmd
regsvr32 /u "path\to\MSIXPropertySheet.dll"
```

# Testing

1. Create a test file with the `.msix` extension
2. Right-click the file and select "Properties"
3. You should see a "MSIX" tab in the properties dialog
4. The tab displays information about the MSIX package

# Debugging

## Debug in Visual Studio
1. Set the project properties:
   - **Debugging > Command**: `C:\Windows\explorer.exe`
   - **Debugging > Command Arguments**: `/select,"C:\path\to\test.msix"`
2. Set breakpoints in your code
3. Press F5 to start debugging
4. Right-click the test file and select Properties

Alternatively, run `explorer.exe /select,C:\path\to\test.msix`, then in Visual Studio select
**Debug > Attach to Process...** and select the newly started explorer.exe process.

## Common Issues
- **Handler not appearing**: Make sure the DLL is registered correctly
- **Access denied**: Run regsvr32 as Administrator
- **Wrong architecture**: Match DLL architecture (x64/arm64) with Explorer process
- **Changes not taking effect**: Restart Windows Explorer (Task Manager > explorer.exe > Restart)

## Restart Windows Explorer

```cmd
taskkill /f /im explorer.exe
start explorer.exe
```
**WARNING**: This terminates ALL instances. I prefer to run a new (separate) instance of Explorer to host the property sheet for testing and debugging.


# License

See the LICENSE file in the root directory.

# References

- [Implementing Shell Extension Handlers](https://docs.microsoft.com/en-us/windows/win32/shell/handlers)
- [IShellPropSheetExt Interface](https://docs.microsoft.com/en-us/windows/win32/api/shobjidl_core/nn-shobjidl_core-ishellpropsheetext)
- [Property Sheet Handlers](https://docs.microsoft.com/en-us/windows/win32/shell/propsheet-handlers)
