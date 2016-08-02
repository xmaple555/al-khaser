#include "Generic.h"

/*
Check of following dll are loaded
 - sbiedll.dll (Sandboxie)
 - dbghelp.dll (vmware)
 - api_log.dll (SunBelt SandBox)
 - dir_watch.dll (SunBelt SandBox)
 - pstorec.dll (SunBelt Sandbox)
 - vmcheck.dll (Virtual PC)
 - wpespy.dll (WPE Pro)
*/
VOID loaded_dlls()
{
	/* Some vars */
	HMODULE hDll;

	/* Array of strings of blacklisted dlls */
	TCHAR* szDlls[] = {
		_T("sbiedll.dll"),
		_T("dbghelp.dll"),
		_T("api_log.dll"),
		_T("dir_watch.dll"),
		_T("pstorec.dll"),
		_T("vmcheck.dll"),
		_T("wpespy.dll"),

	};

	WORD dwlength = sizeof(szDlls) / sizeof(szDlls[0]);
	for (int i = 0; i < dwlength; i++)
	{
		_tprintf(TEXT("[*] Checking if process loaded modules contains: %s "), szDlls[i]);

		/* Check if process loaded modules contains the blacklisted dll */
		hDll = GetModuleHandle(szDlls[i]);
		if (hDll == NULL)
			print_not_detected();
		else
			print_detected();	
	}
}


/*
Number of Processors in VM
*/

BOOL NumberOfProcessors()
{
#if defined (ENV64BIT)
	PULONG ulNumberProcessors = (PULONG)(__readgsqword(0x30) + 0xB8);

#elif defined(ENV32BIT)
	PULONG ulNumberProcessors = (PULONG)(__readfsdword(0x30) + 0x64) ;

#endif

	if (*ulNumberProcessors < 2)
		return TRUE;
	else
		return FALSE;
}


/*
This trick  involves looking at pointers to critical operating system tables
that are typically relocated on a virtual machine. One such table is the
Interrupt Descriptor Table (IDT), which tells the system where various operating
system interrupt handlers are located in memory. On real machines, the IDT is
located lower in memory than it is on guest (i.e., virtual) machines
PS: Does not seem to work on newer version of VMWare Workstation (Tested on v12)
*/
BOOL idt_trick()
{
	UINT idt_base = get_idt_base();
	if ((idt_base >> 24) == 0xff) 
		return TRUE;

	else
		return FALSE;
}

/*
Same for Local Descriptor Table (LDT) 
*/
BOOL ldt_trick()
{
	UINT ldt_base = get_ldt_base();

	if (ldt_base == 0xdead0000) 
		return FALSE;
	else 
		return TRUE; // VMWare detected	
}


/*
Same for Global Descriptor Table (GDT)
*/
BOOL gdt_trick()
{
	UINT gdt_base = get_gdt_base();

	if ((gdt_base >> 24) == 0xff)
		return TRUE; // VMWare detected	

	else
		return FALSE;
}


/*
The instruction STR (Store Task Register) stores the selector segment of the TR
register (Task Register) in the specified operand (memory or other general purpose register).
All x86 processors can manage tasks in the same way as an operating system would do it.
That is, keeping the task state and recovering it when that task is executed again. All 
the states of a task are kept in its TSS; there is one TSS per task. How can we know which
is the TSS associated to the execution task? Using STR instruction, due to the fact that
the selector segment that was brought back points into the TSS of the present task.
In all the tests that were done, the value brought back by STR from within a virtual machine
was different to the obtained from a native system, so apparently, it can be used as a another
mechanism of a unique instruction in assembler to detect virtual machines.
*/
BOOL str_trick()
{
	UCHAR *mem = get_str_base();

	if ((mem[0] == 0x00) && (mem[1] == 0x40))
		return TRUE; // VMWare detected	
	else
		return FALSE;
}


/*
Check number of cores using WMI
*/
BOOL number_cores_wmi()
{
	IWbemServices *pSvc = NULL;
	IWbemLocator *pLoc = NULL;
	IEnumWbemClassObject* pEnumerator = NULL;
	BOOL bStatus = FALSE;
	HRESULT hRes;
	BOOL bFound = FALSE;

	// Init WMI
	bStatus = InitWMI(&pSvc, &pLoc);
	if (bStatus)
	{
		// If success, execute the desired query
		bStatus = ExecWMIQuery(&pSvc, &pLoc, &pEnumerator, _T("SELECT * FROM Win32_Processor"));
		if (bStatus)
		{
			// Get the data from the query
			IWbemClassObject *pclsObj = NULL;
			ULONG uReturn = 0;
			VARIANT vtProp;

			// Iterate over our enumator
			while (pEnumerator)
			{
				hRes = pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);
				if (0 == uReturn)
					break;

				// Get the value of the Name property
				hRes = pclsObj->Get(_T("NumberOfCores"), 0, &vtProp, 0, 0);
				if (V_VT(&vtProp) != VT_NULL) {

					// Do our comparaison
					if (vtProp.uintVal < 2) {
						bFound = TRUE; break;
					}

					// release the current result object
					VariantClear(&vtProp);
					pclsObj->Release();
				}
			}

			// Cleanup
			pEnumerator->Release();
			pSvc->Release();
			pLoc->Release();
			CoUninitialize();
		}
	}

	return bFound;
}


/*
Check hard disk size using WMI
*/
BOOL disk_size_wmi()
{
	IWbemServices *pSvc = NULL;
	IWbemLocator *pLoc = NULL;
	IEnumWbemClassObject* pEnumerator = NULL;
	BOOL bStatus = FALSE;
	HRESULT hRes;
	BOOL bFound = FALSE;

	// Init WMI
	bStatus = InitWMI(&pSvc, &pLoc);
	if (bStatus)
	{
		// If success, execute the desired query
		bStatus = ExecWMIQuery(&pSvc, &pLoc, &pEnumerator, _T("SELECT * FROM Win32_LogicalDisk"));
		if (bStatus)
		{
			// Get the data from the query
			IWbemClassObject *pclsObj = NULL;
			ULONG uReturn = 0;
			VARIANT vtProp;

			// Iterate over our enumator
			while (pEnumerator)
			{
				hRes = pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);
				if (0 == uReturn)
					break;

				// Get the value of the Name property
				hRes = pclsObj->Get(_T("Size"), 0, &vtProp, 0, 0);
				if (V_VT(&vtProp) != VT_NULL) {

					// Do our comparaison
					if (vtProp.uintVal < 80 * 1024 * 1024 * 1024) { // Less than 80GB
						bFound = TRUE; break;
					}

					// release the current result object
					VariantClear(&vtProp);
					pclsObj->Release();
				}
			}

			// Cleanup
			pEnumerator->Release();
			pSvc->Release();
			pLoc->Release();
			CoUninitialize();
		}
	}

	return bFound;
}



BOOL setupdi_diskdrive()
{
	HDEVINFO hDevInfo;
	SP_DEVINFO_DATA DeviceInfoData;
	DWORD i;
	BOOL bFound = FALSE;

	// Create a HDEVINFO with all present devices.
	hDevInfo = SetupDiGetClassDevs((LPGUID)&GUID_DEVCLASS_DISKDRIVE,
		0, // Enumerator
		0,
		DIGCF_PRESENT);

	if (hDevInfo == INVALID_HANDLE_VALUE)
		return FALSE;

	// Enumerate through all devices in Set.
	DeviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
	for (i = 0; SetupDiEnumDeviceInfo(hDevInfo, i, &DeviceInfoData); i++)
	{
		DWORD dwPropertyRegDataType;
		LPTSTR buffer = NULL;
		DWORD dwSize = 0;

		while (!SetupDiGetDeviceRegistryProperty(hDevInfo, &DeviceInfoData, SPDRP_HARDWAREID,
			&dwPropertyRegDataType, (PBYTE)buffer, dwSize, &dwSize))
		{
			if (GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
				// Change the buffer size.
				if (buffer)LocalFree(buffer);
				// Double the size to avoid problems on 
				// W2k MBCS systems per KB 888609. 
				buffer = (LPTSTR)LocalAlloc(LPTR, dwSize * 2);
			}
			else
				break;

		}

		// Do our comparaison
		if (!StrStrI(buffer, _T("vbox")) || !StrStrI(buffer, _T("vmware")) || !StrStrI(buffer, _T("qemu"))
			|| !StrStrI(buffer, _T("vbox"))) {
			LocalFree(buffer);
			bFound =  TRUE;
			break;

		}
	}

	if (GetLastError() != NO_ERROR && GetLastError() != ERROR_NO_MORE_ITEMS)
		return FALSE;

	//  Cleanup
	SetupDiDestroyDeviceInfoList(hDevInfo);

	if (bFound)
		return TRUE;

	else
		return FALSE;
}

