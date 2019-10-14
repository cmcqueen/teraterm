/*
 * Copyright (C) 2019 TeraTerm Project
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * W to A Wrapper
 *
 * API名はW版の頭に '_' を付けたものを使用する
 */

#include <windows.h>

#include "codeconv.h"
#include "compat_win.h"

#include "layer_for_unicode.h"

BOOL _SetDlgItemTextW(HWND hDlg, int nIDDlgItem, LPCWSTR lpString)
{
	if (pSetDlgItemTextW != NULL) {
		return pSetDlgItemTextW(hDlg, nIDDlgItem, lpString);
	}

	char *strA = ToCharW(lpString);
	BOOL retval = SetDlgItemTextA(hDlg, nIDDlgItem, strA);
	free(strA);
	return retval;
}

UINT _DragQueryFileW(HDROP hDrop, UINT iFile, LPWSTR lpszFile, UINT cch)
{
	if (pDragQueryFileW != NULL) {
		return pDragQueryFileW(hDrop, iFile, lpszFile, cch);
	}

	UINT retval;
	if (iFile == 0xffffffff) {
		// ファイル数問い合わせ
		retval = DragQueryFileA(hDrop, iFile, NULL, 0);
	}
	else if (lpszFile == NULL) {
		// ファイル名の文字数問い合わせ
		char FileNameA[MAX_PATH];
		retval = DragQueryFileA(hDrop, iFile, FileNameA, MAX_PATH);
		if (retval != 0) {
			wchar_t *FileNameW = ToWcharA(FileNameA);
			retval = (UINT)(wcslen(FileNameW) + 1);
			free(FileNameW);
		}
	}
	else {
		// ファイル名取得
		char FileNameA[MAX_PATH];
		retval = DragQueryFileA(hDrop, iFile, FileNameA, MAX_PATH);
		if (retval != 0) {
			wchar_t *FileNameW = ToWcharA(FileNameA);
			wcscpy_s(lpszFile, cch, FileNameW);
			free(FileNameW);
		}
	}
	return retval;
}

DWORD _GetFileAttributesW(LPCWSTR lpFileName)
{
	if (pGetFileAttributesW != NULL) {
		return pGetFileAttributesW(lpFileName);
	}

	char *FileNameA;
	if (lpFileName == NULL) {
		FileNameA = NULL;
	} else {
		FileNameA = ToCharW(lpFileName);
	}
	const DWORD attr = GetFileAttributesA(FileNameA);
	free(FileNameA);
	return attr;
}

LRESULT _SendDlgItemMessageW(HWND hDlg, int nIDDlgItem, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	if (pSendDlgItemMessageW != NULL) {
		return pSendDlgItemMessageW(hDlg, nIDDlgItem, Msg, wParam, lParam);
	}

	LRESULT retval;
	if (Msg == CB_ADDSTRING || Msg == LB_ADDSTRING) {
		char *strA = ToCharW((wchar_t *)lParam);
		retval = SendDlgItemMessageA(hDlg, nIDDlgItem, Msg, wParam, (LPARAM)strA);
		free(strA);
	} else {
		retval = SendDlgItemMessageA(hDlg, nIDDlgItem, Msg, wParam, lParam);
	}
	return retval;
}

HPROPSHEETPAGE _CreatePropertySheetPageW(LPCPROPSHEETPAGEW_V1 psp)
{
	if (pCreatePropertySheetPageW != NULL) {
		return pCreatePropertySheetPageW((LPCPROPSHEETPAGEW)psp);
	}

	char *titleA = ToCharW(psp->pszTitle);

	PROPSHEETPAGEA_V1 pspA;
	memset(&pspA, 0, sizeof(pspA));
	pspA.dwSize = sizeof(pspA);
	pspA.dwFlags = psp->dwFlags;
	pspA.hInstance = psp->hInstance;
	pspA.pResource = psp->pResource;
	pspA.pszTitle = titleA;
	pspA.pfnDlgProc = psp->pfnDlgProc;
	pspA.lParam = psp->lParam;

	HPROPSHEETPAGE retval = CreatePropertySheetPageA((LPCPROPSHEETPAGEA)&pspA);

	free(titleA);
	return retval;
}

// リリース用SDKのヘッダに
//	PROPSHEETHEADERW_V1 がないため
//	PROPSHEETHEADERW を使用
//		SDK: Windows Server 2003 R2 Platform SDK
//			 (Microsoft Windows SDK for Windows 7 and .NET Framework 3.5 SP1)
//INT_PTR _PropertySheetW(PROPSHEETHEADERW_V1 *psh)
INT_PTR _PropertySheetW(PROPSHEETHEADERW *psh)
{
	if (pPropertySheetW != NULL) {
		return pPropertySheetW((PROPSHEETHEADERW *)psh);
	}

	char *captionA = ToCharW(psh->pszCaption);

//	PROPSHEETHEADERA_V1 pshA;
	PROPSHEETHEADERA pshA;
	memset(&pshA, 0, sizeof(pshA));
	pshA.dwSize = sizeof(pshA);
	pshA.dwFlags = psh->dwFlags;
	pshA.hwndParent = psh->hwndParent;
	pshA.hInstance = psh->hInstance;
	pshA.pszCaption = captionA;
	pshA.nPages = psh->nPages;
	pshA.phpage = psh->phpage;
	pshA.pfnCallback = psh->pfnCallback;

	INT_PTR retval = PropertySheetA(&pshA);

	free(captionA);
	return retval;
}

HWND _CreateWindowExW(DWORD dwExStyle, LPCWSTR lpClassName, LPCWSTR lpWindowName, DWORD dwStyle, int X, int Y,
							 int nWidth, int nHeight, HWND hWndParent, HMENU hMenu, HINSTANCE hInstance, LPVOID lpParam)
{
	if (pCreateWindowExW != NULL) {
		return pCreateWindowExW(dwExStyle, lpClassName, lpWindowName, dwStyle, X, Y, nWidth, nHeight, hWndParent, hMenu,
								hInstance, lpParam);
	}

	char *lpClassNameA = ToCharW(lpClassName);
	char *lpWindowNameA = ToCharW(lpWindowName);
	HWND hWnd = CreateWindowExA(dwExStyle, lpClassNameA, lpWindowNameA, dwStyle, X, Y, nWidth, nHeight, hWndParent,
								hMenu, hInstance, lpParam);
	free(lpClassNameA);
	if (lpWindowNameA != NULL) {
		free(lpWindowNameA);
	}
	return hWnd;
}

ATOM _RegisterClassW(const WNDCLASSW *lpWndClass)
{
	if (pRegisterClassW != NULL) {
		return pRegisterClassW(lpWndClass);
	}

	char *menu_nameA = ToCharW(lpWndClass->lpszMenuName);
	char *class_nameA = ToCharW(lpWndClass->lpszClassName);

	WNDCLASSA WndClassA;
	WndClassA.style = lpWndClass->style;
	WndClassA.lpfnWndProc = lpWndClass->lpfnWndProc;
	WndClassA.cbClsExtra = lpWndClass->cbClsExtra;
	WndClassA.cbWndExtra = lpWndClass->cbWndExtra;
	WndClassA.hInstance = lpWndClass->hInstance;
	WndClassA.hIcon = lpWndClass->hIcon;
	WndClassA.hCursor = lpWndClass->hCursor;
	WndClassA.hbrBackground = lpWndClass->hbrBackground;
	WndClassA.lpszMenuName = menu_nameA;
	WndClassA.lpszClassName = class_nameA;
	ATOM atom = RegisterClassA(&WndClassA);

	if (menu_nameA != NULL) {
		free(menu_nameA);
	}
	if (class_nameA != NULL) {
		free(class_nameA);
	}
	return atom;
}

BOOL _SetWindowTextW(HWND hWnd, LPCWSTR lpString)
{
	if (pSetWindowTextW != NULL) {
		return pSetWindowTextW(hWnd, lpString);
	}

	char *strA = ToCharW(lpString);
	BOOL retval = SetWindowTextA(hWnd, strA);
	free(strA);
	return retval;
}
